//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// QueryGL.cpp: Implements the class methods for QueryGL.

#include "libANGLE/renderer/gl/QueryGL.h"

#include "common/debug.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"

namespace
{

GLuint MergeQueryResults(GLenum type, GLuint currentResult, GLuint newResult)
{
    switch (type)
    {
        case GL_ANY_SAMPLES_PASSED:
        case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
            return (currentResult == GL_TRUE || newResult == GL_TRUE) ? GL_TRUE : GL_FALSE;

        case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
            return currentResult + newResult;

        default:
            UNREACHABLE();
            return 0;
    }
}

}  // anonymous namespace

namespace rx
{

QueryGL::QueryGL(GLenum type, const FunctionsGL *functions, StateManagerGL *stateManager)
    : QueryImpl(type),
      mType(type),
      mFunctions(functions),
      mStateManager(stateManager),
      mActiveQuery(0),
      mPendingQueries(),
      mResultSum(0)
{
}

QueryGL::~QueryGL()
{
    mStateManager->deleteQuery(mActiveQuery);
    mStateManager->onDeleteQueryObject(this);
    while (!mPendingQueries.empty())
    {
        mStateManager->deleteQuery(mPendingQueries.front());
        mPendingQueries.pop_front();
    }
}

gl::Error QueryGL::begin()
{
    mResultSum = 0;
    return gl::Error(GL_NO_ERROR);
}

gl::Error QueryGL::end()
{
    return pause();
}

gl::Error QueryGL::getResult(GLuint *params)
{
    ASSERT(mActiveQuery == 0);

    gl::Error error = flush(true);
    if (error.isError())
    {
        return error;
    }

    ASSERT(mPendingQueries.empty());
    *params = mResultSum;

    return gl::Error(GL_NO_ERROR);
}

gl::Error QueryGL::isResultAvailable(GLuint *available)
{
    ASSERT(mActiveQuery == 0);

    gl::Error error = flush(false);
    if (error.isError())
    {
        return error;
    }

    *available = mPendingQueries.empty() ? GL_TRUE : GL_FALSE;
    return gl::Error(GL_NO_ERROR);
}

gl::Error QueryGL::pause()
{
    if (mActiveQuery != 0)
    {
        mStateManager->endQuery(mType, mActiveQuery);

        mPendingQueries.push_back(mActiveQuery);
        mActiveQuery = 0;
    }

    // Flush to make sure the pending queries don't add up too much.
    gl::Error error = flush(false);
    if (error.isError())
    {
        return error;
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error QueryGL::resume()
{
    if (mActiveQuery == 0)
    {
        // Flush to make sure the pending queries don't add up too much.
        gl::Error error = flush(false);
        if (error.isError())
        {
            return error;
        }

        mFunctions->genQueries(1, &mActiveQuery);
        mStateManager->beginQuery(mType, mActiveQuery);
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error QueryGL::flush(bool force)
{
    while (!mPendingQueries.empty())
    {
        GLuint id = mPendingQueries.front();
        if (!force)
        {
            GLuint resultAvailable = 0;
            mFunctions->getQueryObjectuiv(id, GL_QUERY_RESULT_AVAILABLE, &resultAvailable);
            if (resultAvailable == GL_FALSE)
            {
                return gl::Error(GL_NO_ERROR);
            }
        }

        GLuint result = 0;
        mFunctions->getQueryObjectuiv(id, GL_QUERY_RESULT, &result);
        mResultSum = MergeQueryResults(mType, mResultSum, result);

        mStateManager->deleteQuery(id);

        mPendingQueries.pop_front();
    }

    return gl::Error(GL_NO_ERROR);
}

}
