//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Query11.cpp: Defines the rx::Query11 class which implements rx::QueryImpl.

#include "libANGLE/renderer/d3d/d3d11/Query11.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"
#include "common/utilities.h"

#include <GLES2/gl2ext.h>

namespace rx
{

Query11::Query11(Renderer11 *renderer, GLenum type)
    : QueryImpl(type),
      mResult(0),
      mQueryFinished(false),
      mRenderer(renderer),
      mQuery(nullptr),
      mTimestampBeginQuery(nullptr),
      mTimestampEndQuery(nullptr)
{
}

Query11::~Query11()
{
    SafeRelease(mQuery);
    SafeRelease(mTimestampBeginQuery);
    SafeRelease(mTimestampEndQuery);
}

gl::Error Query11::begin()
{
    if (mQuery == nullptr)
    {
        D3D11_QUERY_DESC queryDesc;
        queryDesc.Query = gl_d3d11::ConvertQueryType(getType());
        queryDesc.MiscFlags = 0;

        ID3D11Device *device = mRenderer->getDevice();

        HRESULT result = device->CreateQuery(&queryDesc, &mQuery);
        if (FAILED(result))
        {
            return gl::Error(GL_OUT_OF_MEMORY, "Internal query creation failed, result: 0x%X.", result);
        }

        // If we are doing time elapsed we also need a query to actually query the timestamp
        if (getType() == GL_TIME_ELAPSED_EXT)
        {
            D3D11_QUERY_DESC desc;
            desc.Query     = D3D11_QUERY_TIMESTAMP;
            desc.MiscFlags = 0;
            result = device->CreateQuery(&desc, &mTimestampBeginQuery);
            if (FAILED(result))
            {
                return gl::Error(GL_OUT_OF_MEMORY, "Internal query creation failed, result: 0x%X.",
                                 result);
            }
            result = device->CreateQuery(&desc, &mTimestampEndQuery);
            if (FAILED(result))
            {
                return gl::Error(GL_OUT_OF_MEMORY, "Internal query creation failed, result: 0x%X.",
                                 result);
            }
        }
    }

    ID3D11DeviceContext *context = mRenderer->getDeviceContext();

    context->Begin(mQuery);

    // If we are doing time elapsed query the begin timestamp
    if (getType() == GL_TIME_ELAPSED_EXT)
    {
        context->End(mTimestampBeginQuery);
    }
    return gl::Error(GL_NO_ERROR);
}

gl::Error Query11::end()
{
    ASSERT(mQuery);

    ID3D11DeviceContext *context = mRenderer->getDeviceContext();

    // If we are doing time elapsed query the end timestamp
    if (getType() == GL_TIME_ELAPSED_EXT)
    {
        context->End(mTimestampEndQuery);
    }

    context->End(mQuery);

    mQueryFinished = false;
    mResult = GL_FALSE;

    return gl::Error(GL_NO_ERROR);
}

gl::Error Query11::queryCounter()
{
    // This doesn't do anything for D3D11 as we don't support timestamps
    ASSERT(getType() == GL_TIMESTAMP_EXT);
    mQueryFinished = true;
    mResult = 0;
    return gl::Error(GL_NO_ERROR);
}

template <typename T>
gl::Error Query11::getResultBase(T *params)
{
    while (!mQueryFinished)
    {
        gl::Error error = testQuery();
        if (error.isError())
        {
            return error;
        }

        if (!mQueryFinished)
        {
            ScheduleYield();
        }
    }

    ASSERT(mQueryFinished);
    *params = static_cast<T>(mResult);

    return gl::Error(GL_NO_ERROR);
}

gl::Error Query11::getResult(GLint *params)
{
    return getResultBase(params);
}

gl::Error Query11::getResult(GLuint *params)
{
    return getResultBase(params);
}

gl::Error Query11::getResult(GLint64 *params)
{
    return getResultBase(params);
}

gl::Error Query11::getResult(GLuint64 *params)
{
    return getResultBase(params);
}

gl::Error Query11::isResultAvailable(bool *available)
{
    gl::Error error = testQuery();
    if (error.isError())
    {
        return error;
    }

    *available = mQueryFinished;

    return gl::Error(GL_NO_ERROR);
}

gl::Error Query11::testQuery()
{
    if (!mQueryFinished)
    {
        ASSERT(mQuery);

        ID3D11DeviceContext *context = mRenderer->getDeviceContext();
        switch (getType())
        {
          case GL_ANY_SAMPLES_PASSED_EXT:
          case GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT:
            {
                UINT64 numPixels = 0;
                HRESULT result = context->GetData(mQuery, &numPixels, sizeof(numPixels), 0);
                if (FAILED(result))
                {
                    return gl::Error(GL_OUT_OF_MEMORY, "Failed to get the data of an internal query, result: 0x%X.", result);
                }

                if (result == S_OK)
                {
                    mQueryFinished = true;
                    mResult = (numPixels > 0) ? GL_TRUE : GL_FALSE;
                }
            }
            break;

          case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
            {
                D3D11_QUERY_DATA_SO_STATISTICS soStats = { 0 };
                HRESULT result = context->GetData(mQuery, &soStats, sizeof(soStats), 0);
                if (FAILED(result))
                {
                    return gl::Error(GL_OUT_OF_MEMORY, "Failed to get the data of an internal query, result: 0x%X.", result);
                }

                if (result == S_OK)
                {
                    mQueryFinished = true;
                    mResult        = static_cast<GLuint64>(soStats.NumPrimitivesWritten);
                }
            }
            break;

            case GL_TIME_ELAPSED_EXT:
            {
                D3D11_QUERY_DATA_TIMESTAMP_DISJOINT timeStats = {0};
                HRESULT result = context->GetData(mQuery, &timeStats, sizeof(timeStats), 0);
                if (FAILED(result))
                {
                    return gl::Error(GL_OUT_OF_MEMORY,
                                     "Failed to get the data of an internal query, result: 0x%X.",
                                     result);
                }

                if (result == S_OK)
                {
                    UINT64 beginTime = 0;
                    HRESULT beginRes =
                        context->GetData(mTimestampBeginQuery, &beginTime, sizeof(UINT64), 0);
                    if (FAILED(beginRes))
                    {
                        return gl::Error(
                            GL_OUT_OF_MEMORY,
                            "Failed to get the data of an internal query, result: 0x%X.", beginRes);
                    }
                    UINT64 endTime = 0;
                    HRESULT endRes =
                        context->GetData(mTimestampEndQuery, &endTime, sizeof(UINT64), 0);
                    if (FAILED(endRes))
                    {
                        return gl::Error(
                            GL_OUT_OF_MEMORY,
                            "Failed to get the data of an internal query, result: 0x%X.", endRes);
                    }

                    if (beginRes == S_OK && endRes == S_OK)
                    {
                        mQueryFinished = true;
                        if (timeStats.Disjoint)
                        {
                            mRenderer->setGPUDisjoint();
                        }
                        static_assert(sizeof(UINT64) == sizeof(unsigned long long),
                                      "D3D UINT64 isn't 64 bits");
                        if (rx::IsUnsignedMultiplicationSafe(endTime - beginTime, 1000000000ull))
                        {
                            mResult = ((endTime - beginTime) * 1000000000ull) / timeStats.Frequency;
                        }
                        else
                        {
                            mResult = std::numeric_limits<GLuint64>::max() / timeStats.Frequency;
                            // If an overflow does somehow occur, there is no way the elapsed time
                            // is accurate, so we generate a disjoint event
                            mRenderer->setGPUDisjoint();
                        }
                    }
                }
            }
            break;

            case GL_TIMESTAMP_EXT:
            {
                // D3D11 doesn't support GL timestamp queries as D3D timestamps are not guaranteed
                // to have any sort of continuity outside of a disjoint timestamp query block, which
                // GL depends on
                mResult = 0;
            }
            break;

        default:
            UNREACHABLE();
            break;
        }

        if (!mQueryFinished && mRenderer->testDeviceLost())
        {
            mRenderer->notifyDeviceLost();
            return gl::Error(GL_OUT_OF_MEMORY, "Failed to test get query result, device is lost.");
        }
    }

    return gl::Error(GL_NO_ERROR);
}

}
