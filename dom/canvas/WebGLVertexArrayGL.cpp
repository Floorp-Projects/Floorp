/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLVertexArrayGL.h"

#include "GLContext.h"
#include "WebGLContext.h"

namespace mozilla {

WebGLVertexArrayGL::WebGLVertexArrayGL(WebGLContext* webgl)
    : WebGLVertexArray(webgl)
#if defined(XP_LINUX)
    , mIsVAO(false)
#endif
{ }

WebGLVertexArrayGL::~WebGLVertexArrayGL()
{
    DeleteOnce();
}

void
WebGLVertexArrayGL::DeleteImpl()
{
    mElementArrayBuffer = nullptr;

    mContext->MakeContextCurrent();
    mContext->gl->fDeleteVertexArrays(1, &mGLName);

#if defined(XP_LINUX)
    mIsVAO = false;
#endif
}

void
WebGLVertexArrayGL::BindVertexArrayImpl()
{
    mContext->mBoundVertexArray = this;
    mContext->gl->fBindVertexArray(mGLName);

#if defined(XP_LINUX)
    mIsVAO = true;
#endif
}

void
WebGLVertexArrayGL::GenVertexArray()
{
    mContext->gl->fGenVertexArrays(1, &mGLName);
}

bool
WebGLVertexArrayGL::IsVertexArrayImpl()
{
#if defined(XP_LINUX)
    gl::GLContext* gl = mContext->gl;
    if (gl->WorkAroundDriverBugs() &&
        gl->Vendor() == gl::GLVendor::VMware &&
        gl->Renderer() == gl::GLRenderer::GalliumLlvmpipe)
    {
        return mIsVAO;
    }
#endif

    mContext->MakeContextCurrent();
    return mContext->gl->fIsVertexArray(mGLName) != 0;
}

} // namespace mozilla
