/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_BUFFER_H_
#define WEBGL_BUFFER_H_

#include "GLDefs.h"
#include "mozilla/LinkedList.h"
#include "nsAutoPtr.h"
#include "nsWrapperCache.h"

#include "WebGLObjectModel.h"
#include "WebGLTypes.h"

namespace mozilla {

class WebGLElementArrayCache;

class WebGLBuffer final
    : public nsWrapperCache
    , public WebGLRefCountedObject<WebGLBuffer>
    , public LinkedListElement<WebGLBuffer>
    , public WebGLContextBoundObject
{
public:

    enum class Kind {
        Undefined,
        ElementArray,
        OtherData
    };

    WebGLBuffer(WebGLContext* webgl, GLuint buf);

    void BindTo(GLenum target);
    Kind Content() const { return mContent; }

    void Delete();

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

    WebGLsizeiptr ByteLength() const { return mByteLength; }
    void SetByteLength(WebGLsizeiptr byteLength) { mByteLength = byteLength; }

    bool ElementArrayCacheBufferData(const void* ptr, size_t bufferSizeInBytes);

    void ElementArrayCacheBufferSubData(size_t pos, const void* ptr,
                                        size_t updateSizeInBytes);

    bool Validate(GLenum type, uint32_t max_allowed, size_t first, size_t count,
                  uint32_t* const out_upperBound);

    bool IsElementArrayUsedWithMultipleTypes() const;

    WebGLContext* GetParentObject() const {
        return mContext;
    }

    virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto) override;

    const GLenum mGLName;

    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLBuffer)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLBuffer)

protected:
    ~WebGLBuffer();

    Kind mContent;
    WebGLsizeiptr mByteLength;
    nsAutoPtr<WebGLElementArrayCache> mCache;
};

} // namespace mozilla

#endif // WEBGL_BUFFER_H_
