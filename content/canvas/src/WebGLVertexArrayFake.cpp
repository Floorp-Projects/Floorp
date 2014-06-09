/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLVertexArrayFake.h"

#include "WebGLContext.h"
#include "GLContext.h"

namespace mozilla {

void
WebGLVertexArrayFake::BindVertexArrayImpl()
{
    // Go through and re-bind all buffers and setup all
    // vertex attribute pointers
    gl::GLContext* gl = mContext->gl;

    WebGLRefPtr<WebGLBuffer> prevBuffer = mContext->mBoundArrayBuffer;
    mContext->BindBuffer(LOCAL_GL_ELEMENT_ARRAY_BUFFER, mElementArrayBuffer);

    for (size_t i = 0; i < mAttribs.Length(); ++i) {
        const WebGLVertexAttribData& vd = mAttribs[i];

        mContext->BindBuffer(LOCAL_GL_ARRAY_BUFFER, vd.buf);

        gl->fVertexAttribPointer(i, vd.size, vd.type, vd.normalized,
                                 vd.stride, reinterpret_cast<void*>(vd.byteOffset));

        if (vd.enabled) {
            gl->fEnableVertexAttribArray(i);
        } else {
            gl->fDisableVertexAttribArray(i);
        }
    }

    mContext->BindBuffer(LOCAL_GL_ARRAY_BUFFER, prevBuffer);
}

} // namespace mozilla

