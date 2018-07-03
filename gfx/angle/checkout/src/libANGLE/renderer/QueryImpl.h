//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// QueryImpl.h: Defines the abstract rx::QueryImpl class.

#ifndef LIBANGLE_RENDERER_QUERYIMPL_H_
#define LIBANGLE_RENDERER_QUERYIMPL_H_


#include "common/angleutils.h"
#include "libANGLE/Error.h"
#include "libANGLE/PackedEnums.h"

namespace rx
{

class QueryImpl : angle::NonCopyable
{
  public:
    explicit QueryImpl(gl::QueryType type) { mType = type; }
    virtual ~QueryImpl() { }

    virtual gl::Error begin() = 0;
    virtual gl::Error end() = 0;
    virtual gl::Error queryCounter() = 0;
    virtual gl::Error getResult(GLint *params) = 0;
    virtual gl::Error getResult(GLuint *params) = 0;
    virtual gl::Error getResult(GLint64 *params) = 0;
    virtual gl::Error getResult(GLuint64 *params) = 0;
    virtual gl::Error isResultAvailable(bool *available) = 0;

    gl::QueryType getType() const { return mType; }

  private:
    gl::QueryType mType;
};

}

#endif // LIBANGLE_RENDERER_QUERYIMPL_H_
