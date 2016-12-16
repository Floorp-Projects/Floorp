/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_VERTEX_ARRAY_GL_H_
#define WEBGL_VERTEX_ARRAY_GL_H_

#include "WebGLVertexArray.h"

namespace mozilla {

class WebGLVertexArrayGL
    : public WebGLVertexArray
{
    friend class WebGLVertexArray;

public:
    virtual void DeleteImpl() override;
    virtual void BindVertexArrayImpl() override;
    virtual void GenVertexArray() override;
    virtual bool IsVertexArrayImpl() const override;

protected:
    explicit WebGLVertexArrayGL(WebGLContext* webgl);
    ~WebGLVertexArrayGL();

    // Bug 1140459: Some drivers (including our test slaves!) don't
    // give reasonable answers for IsVertexArray, maybe others.
    //
    // So we track the `is a VAO` state ourselves.
    bool mIsVAO;
};

} // namespace mozilla

#endif // WEBGL_VERTEX_ARRAY_GL_H_
