/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_BUFFER_H_
#define WEBGL_BUFFER_H_

#include <map>

#include "GLDefs.h"
#include "mozilla/LinkedList.h"
#include "nsWrapperCache.h"
#include "WebGLObjectModel.h"
#include "WebGLTypes.h"

namespace mozilla {

class WebGLBuffer final
    : public nsWrapperCache
    , public WebGLRefCountedObject<WebGLBuffer>
    , public LinkedListElement<WebGLBuffer>
{
    friend class WebGLContext;
    friend class WebGL2Context;
    friend class WebGLTexture;

public:
    enum class Kind {
        Undefined,
        ElementArray,
        OtherData
    };

    WebGLBuffer(WebGLContext* webgl, GLuint buf);

    void SetContentAfterBind(GLenum target);
    Kind Content() const { return mContent; }

    void Delete();

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

    GLenum Usage() const { return mUsage; }
    size_t ByteLength() const { return mByteLength; }

    bool ValidateIndexedFetch(GLenum type, uint32_t max_allowed, size_t first, size_t count) const;
    bool ValidateRange(const char* funcName, size_t byteOffset, size_t byteLen) const;

    WebGLContext* GetParentObject() const {
        return mContext;
    }

    virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto) override;

    bool ValidateCanBindToTarget(const char* funcName, GLenum target);
    void BufferData(GLenum target, size_t size, const void* data, GLenum usage);
    void BufferSubData(GLenum target, size_t dstByteOffset, size_t dataLen,
                       const void* data) const;

    ////

    static void AddBindCount(GLenum target, WebGLBuffer* buffer, int8_t addVal) {
        if (!buffer)
            return;

        if (target == LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER) {
            MOZ_ASSERT_IF(addVal < 0, buffer->mTFBindCount >= size_t(-addVal));
            buffer->mTFBindCount += addVal;
        } else {
            MOZ_ASSERT_IF(addVal < 0, buffer->mNonTFBindCount >= size_t(-addVal));
            buffer->mNonTFBindCount += addVal;
        }
    }

    static void SetSlot(GLenum target, WebGLBuffer* newBuffer,
                        WebGLRefPtr<WebGLBuffer>* const out_slot)
    {
        WebGLBuffer* const oldBuffer = *out_slot;
        AddBindCount(target, oldBuffer, -1);
        AddBindCount(target, newBuffer, +1);
        *out_slot = newBuffer;
    }

    bool IsBoundForTF() const { return bool(mTFBindCount); }
    bool IsBoundForNonTF() const { return bool(mNonTFBindCount); }

    ////

    const GLenum mGLName;

    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLBuffer)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLBuffer)

protected:
    ~WebGLBuffer();

    void InvalidateCacheRange(size_t offset, size_t length) const;

    Kind mContent;
    GLenum mUsage;
    size_t mByteLength;
    size_t mTFBindCount;
    size_t mNonTFBindCount;

    struct IndexRange final {
        GLenum type;
        size_t first;
        size_t count;

        bool operator<(const IndexRange& x) const {
            if (type != x.type)
                return type < x.type;

            if (first != x.first)
                return first < x.first;

            return count < x.count;
        }
    };

    UniqueBuffer mIndexCache;
    mutable std::map<IndexRange, size_t> mIndexRanges;
};

} // namespace mozilla

#endif // WEBGL_BUFFER_H_
