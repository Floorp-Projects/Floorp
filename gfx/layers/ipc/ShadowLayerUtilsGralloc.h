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

#include "IPC/IPCMessageUtils.h"
#include "mozilla/layers/PGrallocBufferChild.h"
#include "mozilla/layers/PGrallocBufferParent.h"

#define MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
#define MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS

class gfxASurface;

namespace mozilla {
namespace layers {

class MaybeMagicGrallocBufferHandle;
class SurfaceDescriptorGralloc;

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

  MagicGrallocBufferHandle()
  { }

  MagicGrallocBufferHandle(const android::sp<GraphicBuffer>& aGraphicBuffer);

  // Default copy ctor and operator= are OK

  bool operator==(const MagicGrallocBufferHandle& aOther) const {
    return mGraphicBuffer == aOther.mGraphicBuffer;
  }

  android::sp<GraphicBuffer> mGraphicBuffer;
};

/**
 * GrallocBufferActor is an "IPC wrapper" for an underlying
 * GraphicBuffer (pmem region).  It allows us to cheaply and
 * conveniently share gralloc handles between processes.
 */
class GrallocBufferActor : public PGrallocBufferChild
                         , public PGrallocBufferParent
{
  friend class ShadowLayerForwarder;
  typedef android::GraphicBuffer GraphicBuffer;

public:
  virtual ~GrallocBufferActor() {}

  static PGrallocBufferParent*
  Create(const gfxIntSize& aSize, const gfxContentType& aContent,
         MaybeMagicGrallocBufferHandle* aOutHandle);

  static PGrallocBufferChild*
  Create();

private:
  GrallocBufferActor() {}

  void InitFromHandle(const MagicGrallocBufferHandle& aHandle);

  static android::sp<GraphicBuffer>
  GetFrom(const SurfaceDescriptorGralloc& aDescriptor);

  android::sp<GraphicBuffer> mGraphicBuffer;
};

} // namespace layers
} // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::layers::MagicGrallocBufferHandle> {
  typedef mozilla::layers::MagicGrallocBufferHandle paramType;

  static void Write(Message* aMsg, const paramType& aParam);
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult);
};

} // namespace IPC

#endif  // mozilla_layers_ShadowLayerUtilsGralloc_h
