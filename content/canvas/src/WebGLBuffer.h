/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLBUFFER_H_
#define WEBGLBUFFER_H_

#include "GLDefs.h"
#include "mozilla/LinkedList.h"
#include "mozilla/MemoryReporting.h"
#include "nsWrapperCache.h"
#include "WebGLObjectModel.h"
#include "WebGLTypes.h"

namespace mozilla {

class WebGLElementArrayCache;

class WebGLBuffer MOZ_FINAL
    : public nsWrapperCache
    , public WebGLRefCountedObject<WebGLBuffer>
    , public LinkedListElement<WebGLBuffer>
    , public WebGLContextBoundObject
{
public:
    WebGLBuffer(WebGLContext *context);

    ~WebGLBuffer();

    void Delete();

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

    bool HasEverBeenBound() { return mHasEverBeenBound; }
    void SetHasEverBeenBound(bool x) { mHasEverBeenBound = x; }
    GLuint GLName() const { return mGLName; }
    WebGLsizeiptr ByteLength() const { return mByteLength; }
    GLenum Target() const { return mTarget; }

    void SetByteLength(WebGLsizeiptr byteLength) { mByteLength = byteLength; }

    void SetTarget(GLenum target);

    bool ElementArrayCacheBufferData(const void* ptr, size_t buffer_size_in_bytes);

    void ElementArrayCacheBufferSubData(size_t pos, const void* ptr, size_t update_size_in_bytes);

    bool Validate(GLenum type, uint32_t max_allowed, size_t first, size_t count,
                  uint32_t* out_upperBound);

    WebGLContext *GetParentObject() const {
        return Context();
    }

    virtual JSObject* WrapObject(JSContext *cx) MOZ_OVERRIDE;

    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLBuffer)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLBuffer)

protected:

    GLuint mGLName;
    bool mHasEverBeenBound;
    WebGLsizeiptr mByteLength;
    GLenum mTarget;

    nsAutoPtr<WebGLElementArrayCache> mCache;
};
}
#endif //WEBGLBUFFER_H_
