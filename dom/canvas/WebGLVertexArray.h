/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_VERTEX_ARRAY_H_
#define WEBGL_VERTEX_ARRAY_H_

#include "nsTArray.h"
#include "mozilla/LinkedList.h"
#include "nsWrapperCache.h"

#include "CacheInvalidator.h"
#include "WebGLObjectModel.h"
#include "WebGLStrongTypes.h"
#include "WebGLVertexAttribData.h"

namespace mozilla {

class WebGLVertexArrayFake;
namespace webgl {
struct LinkedProgramInfo;
}

class WebGLVertexArray
    : public nsWrapperCache
    , public WebGLRefCountedObject<WebGLVertexArray>
    , public LinkedListElement<WebGLVertexArray>
    , public CacheInvalidator
{
public:
    static WebGLVertexArray* Create(WebGLContext* webgl);

    void Delete();

    WebGLContext* GetParentObject() const {
        return mContext;
    }

    virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto) override;

    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLVertexArray)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLVertexArray)

    void AddBufferBindCounts(int8_t addVal) const;

protected:
    WebGLVertexArray(WebGLContext* webgl, GLuint name);
    virtual ~WebGLVertexArray();

    virtual void BindVertexArray() = 0;
    virtual void DeleteImpl() = 0;

public:
    const GLuint mGLName;
    bool mHasBeenBound = false;
protected:
    nsTArray<WebGLVertexAttribData> mAttribs;
    WebGLRefPtr<WebGLBuffer> mElementArrayBuffer;

    friend class ScopedDrawHelper;
    friend class WebGLContext;
    friend class WebGLVertexArrayFake;
    friend class WebGL2Context;
    friend struct webgl::LinkedProgramInfo;
};

} // namespace mozilla

#endif // WEBGL_VERTEX_ARRAY_H_
