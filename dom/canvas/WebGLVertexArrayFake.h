/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_VERTEX_ARRAY_FAKE_H_
#define WEBGL_VERTEX_ARRAY_FAKE_H_

#include "WebGLVertexArray.h"

namespace mozilla {

class WebGLVertexArrayFake final
    : public WebGLVertexArray
{
    friend class WebGLVertexArray;

protected:
    virtual void BindVertexArrayImpl() override;
    virtual void DeleteImpl() override;
    virtual void GenVertexArray() override {};
    virtual bool IsVertexArrayImpl() override;

private:
    explicit WebGLVertexArrayFake(WebGLContext* webgl);

    ~WebGLVertexArrayFake() {
        DeleteOnce();
    }

    bool mIsVAO;
};

} // namespace mozilla

#endif // WEBGL_VERTEX_ARRAY_FAKE_H_
