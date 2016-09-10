/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_ELEMENT_ARRAY_CACHE_H
#define WEBGL_ELEMENT_ARRAY_CACHE_H

#include "GLDefs.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/UniquePtr.h"
#include "nscore.h"
#include "nsTArray.h"
#include <stdint.h>

namespace mozilla {

template<typename T>
struct WebGLElementArrayCacheTree;

/* WebGLElementArrayCache implements WebGL element array buffer validation for
 * drawElements.
 *
 * Its exposes methods meant to be called by WebGL method implementations:
 *
 * - Validate, to be called by WebGLContext::DrawElements, is where we use the
 *   cache.
 *
 * - BufferData and BufferSubData, to be called by eponymous WebGL methods, are
 *   how data is fed into the cache.
 *
 * Most of the implementation is hidden in the auxilary class template,
 * WebGLElementArrayCacheTree. Refer to its code for design comments.
 */
class WebGLElementArrayCache {
public:
    bool BufferData(const void* ptr, size_t byteLength);
    bool BufferSubData(size_t pos, const void* ptr, size_t updateByteSize);

    bool Validate(GLenum type, uint32_t maxAllowed, size_t first, size_t count);

    template<typename T>
    T Element(size_t i) const { return Elements<T>()[i]; }

    WebGLElementArrayCache();
    ~WebGLElementArrayCache();

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
    bool BeenUsedWithMultipleTypes() const;

private:
    /* Returns true if a drawElements call with the given parameters should
     * succeed, false otherwise.
     *
     * In other words, this returns true if all entries in the element array at
     * positions:
     *
     *    first .. first+count-1
     *
     * are less than or equal to maxAllowed.
     *
     * Input parameters:
     *   maxAllowed: Maximum value to be allowed in the specificied portion of
     *               the element array.
     *   first: Start of the portion of the element array to consume.
     *   count: Number of entries from the element array to consume.
     *
     * Output parameter:
     *   out_upperBound: Upon success, is set to the actual maximum value in the
     *                   specified range, which is then guaranteed to be less
     *                   than or equal to maxAllowed. upon failure, is set to
     *                   the first value in the specified range, that is greater
     *                   than maxAllowed.
     */
    template<typename T>
    bool Validate(uint32_t maxAllowed, size_t first, size_t count);

    template<typename T>
    const T* Elements() const {
        return reinterpret_cast<const T*>(mBytes.Elements());
    }

    template<typename T>
    T* Elements() { return reinterpret_cast<T*>(mBytes.Elements()); }

    bool UpdateTrees(size_t firstByte, size_t lastByte);

    template<typename T>
    friend struct WebGLElementArrayCacheTree;
    template<typename T>
    friend struct TreeForType;

    FallibleTArray<uint8_t> mBytes;
    UniquePtr<WebGLElementArrayCacheTree<uint8_t>> mUint8Tree;
    UniquePtr<WebGLElementArrayCacheTree<uint16_t>> mUint16Tree;
    UniquePtr<WebGLElementArrayCacheTree<uint32_t>> mUint32Tree;
};

} // end namespace mozilla

#endif // WEBGL_ELEMENT_ARRAY_CACHE_H
