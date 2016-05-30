/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ShadowLayerUtilsGralloc_h
#define mozilla_layers_ShadowLayerUtilsGralloc_h

#include <unistd.h>
#include <ui/GraphicBuffer.h>

#include "base/process.h"
#include "ipc/IPCMessageUtils.h"

#define MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
#define MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS

namespace mozilla {
namespace layers {

class MaybeMagicGrallocBufferHandle;
class SurfaceDescriptor;

struct GrallocBufferRef {
  base::ProcessId mOwner;
  int64_t mKey;

  GrallocBufferRef()
    : mOwner(0)
    , mKey(-1)
  {

  }

  bool operator== (const GrallocBufferRef rhs) const{
    return mOwner == rhs.mOwner && mKey == rhs.mKey;
  }
};
/**
 * This class exists to share the underlying GraphicBuffer resources
 * from one thread context to another.  This requires going through
 * slow paths in the kernel so can be somewhat expensive.
 *
 * This is not just platform-specific, but also
 * gralloc-implementation-specific.
 */
struct MagicGrallocBufferHandle {
  typedef android::GraphicBuffer GraphicBuffer;
  MagicGrallocBufferHandle() {}

  MagicGrallocBufferHandle(const android::sp<GraphicBuffer>& aGraphicBuffer, GrallocBufferRef ref);

  // Default copy ctor and operator= are OK

  bool operator==(const MagicGrallocBufferHandle& aOther) const {
    return mGraphicBuffer == aOther.mGraphicBuffer;
  }

  android::sp<GraphicBuffer> mGraphicBuffer;
  GrallocBufferRef mRef;
};

/**
 * Util function to find GraphicBuffer from SurfaceDescriptor, caller of this function should check origin
 * to make sure not corrupt others buffer
 */
android::sp<android::GraphicBuffer> GetGraphicBufferFrom(MaybeMagicGrallocBufferHandle aHandle);
android::sp<android::GraphicBuffer> GetGraphicBufferFromDesc(SurfaceDescriptor aDesc);

} // namespace layers
} // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::layers::MagicGrallocBufferHandle> {
  typedef mozilla::layers::MagicGrallocBufferHandle paramType;

  static void Write(Message* aMsg, const paramType& aParam);
  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult);
};

template<>
struct ParamTraits<mozilla::layers::GrallocBufferRef> {
  typedef mozilla::layers::GrallocBufferRef paramType;
  static void Write(Message* aMsg, const paramType& aParam);
  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult);
};


} // namespace IPC

#endif  // mozilla_layers_ShadowLayerUtilsGralloc_h
