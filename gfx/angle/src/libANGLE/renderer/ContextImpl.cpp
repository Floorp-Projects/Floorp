//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ContextImpl:
//   Implementation-specific functionality associated with a GL Context.
//

#include "libANGLE/renderer/ContextImpl.h"

namespace rx
{

ContextImpl::ContextImpl(const gl::ContextState &state) : mState(state)
{
}

ContextImpl::~ContextImpl()
{
}

void ContextImpl::stencilFillPath(const gl::Path *path, GLenum fillMode, GLuint mask)
{
    UNREACHABLE();
}

void ContextImpl::stencilStrokePath(const gl::Path *path, GLint reference, GLuint mask)
{
    UNREACHABLE();
}

void ContextImpl::coverFillPath(const gl::Path *path, GLenum coverMode)
{
    UNREACHABLE();
}

void ContextImpl::coverStrokePath(const gl::Path *path, GLenum coverMode)
{
    UNREACHABLE();
}

void ContextImpl::stencilThenCoverFillPath(const gl::Path *path,
                                           GLenum fillMode,
                                           GLuint mask,
                                           GLenum coverMode)
{
    UNREACHABLE();
}

void ContextImpl::stencilThenCoverStrokePath(const gl::Path *path,
                                             GLint reference,
                                             GLuint mask,
                                             GLenum coverMode)
{
    UNREACHABLE();
}

}  // namespace rx
