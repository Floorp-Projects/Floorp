/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLVERTEXARRAYGL_H_
#define WEBGLVERTEXARRAYGL_H_

#include "WebGLVertexArray.h"

namespace mozilla {

class WebGLVertexArrayGL MOZ_FINAL
    : public WebGLVertexArray
{
public:
    virtual void DeleteImpl() MOZ_OVERRIDE;
    virtual void BindVertexArrayImpl() MOZ_OVERRIDE;
    virtual void GenVertexArray() MOZ_OVERRIDE;

private:
    WebGLVertexArrayGL(WebGLContext* context)
        : WebGLVertexArray(context)
    { }

    ~WebGLVertexArrayGL() {
        DeleteOnce();
    }

    friend class WebGLVertexArray;
};

} // namespace mozilla

#endif
