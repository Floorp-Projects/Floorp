/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_VERTEX_ARRAY_GL_H_
#define WEBGL_VERTEX_ARRAY_GL_H_

#include "WebGLVertexArray.h"

namespace mozilla {

class WebGLVertexArrayGL final
    : public WebGLVertexArray
{
public:
    virtual void DeleteImpl() override;
    virtual void BindVertexArrayImpl() override;
    virtual void GenVertexArray() override;

private:
    explicit WebGLVertexArrayGL(WebGLContext* webgl)
        : WebGLVertexArray(webgl)
    { }

    ~WebGLVertexArrayGL() {
        DeleteOnce();
    }

    friend class WebGLVertexArray;
};

} // namespace mozilla

#endif // WEBGL_VERTEX_ARRAY_GL_H_
