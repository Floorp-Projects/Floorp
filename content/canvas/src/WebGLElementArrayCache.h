/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLELEMENTARRAYCACHE_H
#define WEBGLELEMENTARRAYCACHE_H

#include "mozilla/MemoryReporting.h"
#include <stdint.h>
#include "nscore.h"
#include "GLDefs.h"

namespace mozilla {

template<typename T>
struct WebGLElementArrayCacheTree;

/*
 * WebGLElementArrayCache implements WebGL element array buffer validation for drawElements.
 *
 * Its exposes methods meant to be called by WebGL method implementations:
 *  - Validate, to be called by WebGLContext::DrawElements, is where we use the cache
 *  - BufferData and BufferSubData, to be called by eponymous WebGL methods, are how
 *    data is fed into the cache
 *
 * Most of the implementation is hidden in the auxilary class template, WebGLElementArrayCacheTree.
 * Refer to its code for design comments.
 */
class WebGLElementArrayCache {

public:
  bool BufferData(const void* ptr, size_t byteSize);
  void BufferSubData(size_t pos, const void* ptr, size_t updateByteSize);

  bool Validate(GLenum type, uint32_t maxAllowed, size_t first, size_t count);

  template<typename T>
  T Element(size_t i) const { return Elements<T>()[i]; }

  WebGLElementArrayCache()
    : mUntypedData(nullptr)
    , mByteSize(0)
    , mUint8Tree(nullptr)
    , mUint16Tree(nullptr)
    , mUint32Tree(nullptr)
  {}

  ~WebGLElementArrayCache();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:

  template<typename T>
  bool Validate(uint32_t maxAllowed, size_t first, size_t count);

  size_t ByteSize() const {
    return mByteSize;
  }

  template<typename T>
  const T* Elements() const { return static_cast<const T*>(mUntypedData); }
  template<typename T>
  T* Elements() { return static_cast<T*>(mUntypedData); }

  void InvalidateTrees(size_t firstByte, size_t lastByte);

  template<typename T>
  friend struct WebGLElementArrayCacheTree;
  template<typename T>
  friend struct TreeForType;

  void* mUntypedData;
  size_t mByteSize;
  WebGLElementArrayCacheTree<uint8_t>* mUint8Tree;
  WebGLElementArrayCacheTree<uint16_t>* mUint16Tree;
  WebGLElementArrayCacheTree<uint32_t>* mUint32Tree;
};


} // end namespace mozilla

#endif // WEBGLELEMENTARRAYCACHE_H
