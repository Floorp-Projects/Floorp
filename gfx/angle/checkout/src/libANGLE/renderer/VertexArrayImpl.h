//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// VertexAttribImpl.h: Defines the abstract rx::VertexAttribImpl class.

#ifndef LIBANGLE_RENDERER_VERTEXARRAYIMPL_H_
#define LIBANGLE_RENDERER_VERTEXARRAYIMPL_H_

#include "common/angleutils.h"
#include "libANGLE/Buffer.h"
#include "libANGLE/VertexArray.h"

namespace rx
{
class ContextImpl;

class VertexArrayImpl : angle::NonCopyable
{
  public:
    VertexArrayImpl(const gl::VertexArrayState &state) : mState(state) {}
    virtual gl::Error syncState(const gl::Context *context,
                                const gl::VertexArray::DirtyBits &dirtyBits,
                                const gl::VertexArray::DirtyAttribBitsArray &attribBits,
                                const gl::VertexArray::DirtyBindingBitsArray &bindingBits)
    {
        return gl::NoError();
    }

    virtual void destroy(const gl::Context *context) {}
    virtual ~VertexArrayImpl() {}

  protected:
    const gl::VertexArrayState &mState;
};

}  // namespace rx

#endif // LIBANGLE_RENDERER_VERTEXARRAYIMPL_H_
