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
    , mIsVAO(false)
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

    mIsVAO = false;
}

void
WebGLVertexArrayGL::BindVertexArrayImpl()
{
    mContext->mBoundVertexArray = this;
    mContext->gl->fBindVertexArray(mGLName);

    mIsVAO = true;
}

void
WebGLVertexArrayGL::GenVertexArray()
{
    mContext->gl->fGenVertexArrays(1, &mGLName);
}

bool
WebGLVertexArrayGL::IsVertexArrayImpl() const
{
    gl::GLContext* gl = mContext->gl;
    if (gl->WorkAroundDriverBugs())
    {
        return mIsVAO;
    }

    mContext->MakeContextCurrent();
    return mContext->gl->fIsVertexArray(mGLName) != 0;
}

} // namespace mozilla
