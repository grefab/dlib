// Copyright (C) 2015  Davis E. King (davis@dlib.net)
// License: Boost Software License   See LICENSE.txt for the full license.
#ifndef DLIB_DNN_CuBLAS_CPP_
#define DLIB_DNN_CuBLAS_CPP_

#ifdef DLIB_USE_CUDA

#include "cublas_dlibapi.h"
#include "cuda_utils.h"

#include <cublas_v2.h>
#include <vector>
#include "../threads/thread_specific_data_extension.h"

static const char* cublas_get_error_string(cublasStatus_t s)
{
    switch(s)
    {
        case CUBLAS_STATUS_NOT_INITIALIZED: 
            return "CUDA Runtime API initialization failed.";
        case CUBLAS_STATUS_ALLOC_FAILED: 
            return "CUDA Resources could not be allocated.";
        default:
            return "A call to cuBLAS failed";
    }
}

// Check the return value of a call to the cuBLAS runtime for an error condition.
#define CHECK_CUBLAS(call)                                                      \
do{                                                                              \
    const cublasStatus_t error = call;                                         \
    if (error != CUBLAS_STATUS_SUCCESS)                                        \
    {                                                                          \
        std::ostringstream sout;                                               \
        sout << "Error while calling " << #call << " in file " << __FILE__ << ":" << __LINE__ << ". ";\
        sout << "code: " << error << ", reason: " << cublas_get_error_string(error);\
        throw dlib::cublas_error(sout.str());                            \
    }                                                                          \
}while(false)

namespace dlib
{
    namespace cuda 
    {

    // -----------------------------------------------------------------------------------

        class cublas_context
        {
        public:
            // not copyable
            cublas_context(const cublas_context&) = delete;
            cublas_context& operator=(const cublas_context&) = delete;

            cublas_context()
            {
                handles.resize(16);
            }
            ~cublas_context()
            {
                for (auto h : handles)
                {
                    if (h)
                        cublasDestroy(h);
                }
            }

            cublasHandle_t get_handle (
            )  
            { 
                int new_device_id;
                CHECK_CUDA(cudaGetDevice(&new_device_id));
                // make room for more devices if needed
                if (new_device_id >= (long)handles.size())
                    handles.resize(new_device_id+16);

                // If we don't have a handle already for this device then make one
                if (!handles[new_device_id])
                    CHECK_CUBLAS(cublasCreate(&handles[new_device_id]));

                // Finally, return the handle for the current device
                return handles[new_device_id];
            }

        private:

            std::vector<cublasHandle_t> handles;
        };

        static cublasHandle_t context()
        {
            static thread_specific_data<cublas_context> c;
            return c.data().get_handle();
        }

    // -----------------------------------------------------------------------------------

        void gemm (
            float beta,
            tensor& dest,
            float alpha,
            const tensor& lhs,
            bool trans_lhs,
            const tensor& rhs,
            bool trans_rhs
        )
        {
            // Recall that BLAS uses column major order so to deal with that we flip the
            // order of the lhs and rhs arguments.
            const auto transa = trans_lhs ? CUBLAS_OP_T : CUBLAS_OP_N;
            const auto transb = trans_rhs ? CUBLAS_OP_T : CUBLAS_OP_N;

            const int dest_nr = dest.num_samples();
            const int dest_nc = dest.size()/dest_nr;
            const int lhs_nr = lhs.num_samples();
            const int lhs_nc = lhs.size()/lhs_nr;
            const int rhs_nr = rhs.num_samples();
            const int rhs_nc = rhs.size()/rhs_nr;
            if (trans_lhs && trans_rhs)
            {
                DLIB_ASSERT( dest_nr == lhs_nc &&
                              dest_nc == rhs_nr &&
                              lhs_nr == rhs_nc)
            }
            else if (!trans_lhs && trans_rhs)
            {
                DLIB_ASSERT( dest_nr == lhs_nr &&
                              dest_nc == rhs_nr &&
                              lhs_nc == rhs_nc)
            }
            else if (trans_lhs && !trans_rhs)
            {
                DLIB_ASSERT( dest_nr == lhs_nc &&
                              dest_nc == rhs_nc &&
                              lhs_nr == rhs_nr)
            }
            else
            {
                DLIB_ASSERT( dest_nr == lhs_nr &&
                              dest_nc == rhs_nc &&
                              lhs_nc == rhs_nr)
            }

            const int k = trans_rhs ? rhs_nc : rhs_nr;
            CHECK_CUBLAS(cublasSgemm(context(),
                              transb,
                              transa, 
                              dest_nc, dest_nr, k,
                              &alpha,
                              rhs.device(), rhs_nc,
                              lhs.device(), lhs_nc,
                              &beta,
                              dest.device(),dest_nc));
        }

    // ------------------------------------------------------------------------------------

    }  
}

#endif // DLIB_USE_CUDA

#endif // DLIB_DNN_CuBLAS_CPP_



