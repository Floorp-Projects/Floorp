/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "mozilla/gfx/Point.h"
#include "mozilla/layers/LayerTransactionChild.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/layers/SharedBufferManagerChild.h"
#include "mozilla/layers/SharedBufferManagerParent.h"
#include "mozilla/unused.h"
#include "nsXULAppAPI.h"

#include "ShadowLayerUtilsGralloc.h"

#include "nsIMemoryReporter.h"

#include "gfxPlatform.h"
#include "gfx2DGlue.h"
#include "GLContext.h"

#include "GeckoProfiler.h"

#include "cutils/properties.h"

#include "MainThreadUtils.h"

using namespace android;
using namespace base;
using namespace mozilla::layers;
using namespace mozilla::gl;

namespace IPC {

void
ParamTraits<GrallocBufferRef>::Write(Message* aMsg,
                                     const paramType& aParam)
{
  aMsg->WriteInt(aParam.mOwner);
  aMsg->WriteInt32(aParam.mKey);
}

bool
ParamTraits<GrallocBufferRef>::Read(const Message* aMsg, void** aIter,
                                    paramType* aParam)
{
  int owner;
  int index;
  if (!aMsg->ReadInt(aIter, &owner) ||
      !aMsg->ReadInt32(aIter, &index))
    return false;
  aParam->mOwner = owner;
  aParam->mKey = index;
  return true;
}

void
ParamTraits<MagicGrallocBufferHandle>::Write(Message* aMsg,
                                             const paramType& aParam)
{
#if ANDROID_VERSION >= 19
  sp<GraphicBuffer> flattenable = aParam.mGraphicBuffer;
#else
  Flattenable *flattenable = aParam.mGraphicBuffer.get();
#endif
  size_t nbytes = flattenable->getFlattenedSize();
  size_t nfds = flattenable->getFdCount();

  char data[nbytes];
  int fds[nfds];

#if ANDROID_VERSION >= 19
  // Make a copy of "data" and "fds" for flatten() to avoid casting problem
  void *pdata = (void *)data;
  int *pfds = fds;

  flattenable->flatten(pdata, nbytes, pfds, nfds);

  // In Kitkat, flatten() will change the value of nbytes and nfds, which dues
  // to multiple parcelable object consumption. The actual size and fd count
  // which returned by getFlattenedSize() and getFdCount() are not changed.
  // So we change nbytes and nfds back by call corresponding calls.
  nbytes = flattenable->getFlattenedSize();
  nfds = flattenable->getFdCount();
#else
  flattenable->flatten(data, nbytes, fds, nfds);
#endif
  aMsg->WriteInt(aParam.mRef.mOwner);
  aMsg->WriteInt32(aParam.mRef.mKey);
  aMsg->WriteSize(nbytes);
  aMsg->WriteSize(nfds);

  aMsg->WriteBytes(data, nbytes);
  for (size_t n = 0; n < nfds; ++n) {
    // These buffers can't die in transit because they're created
    // synchonously and the parent-side buffer can only be dropped if
    // there's a crash.
    aMsg->WriteFileDescriptor(FileDescriptor(fds[n], false));
  }
}

bool
ParamTraits<MagicGrallocBufferHandle>::Read(const Message* aMsg,
                                            void** aIter, paramType* aResult)
{
  size_t nbytes;
  size_t nfds;
  const char* data;
  int owner;
  int index;

  if (!aMsg->ReadInt(aIter, &owner) ||
      !aMsg->ReadInt32(aIter, &index) ||
      !aMsg->ReadSize(aIter, &nbytes) ||
      !aMsg->ReadSize(aIter, &nfds) ||
      !aMsg->ReadBytes(aIter, &data, nbytes)) {
    return false;
  }

  int fds[nfds];
  bool sameProcess = (XRE_GetProcessType() == GeckoProcessType_Default);
  for (size_t n = 0; n < nfds; ++n) {
    FileDescriptor fd;
    if (!aMsg->ReadFileDescriptor(aIter, &fd)) {
      return false;
    }
    // If the GraphicBuffer was shared cross-process, SCM_RIGHTS does
    // the right thing and dup's the fd.  If it's shared cross-thread,
    // SCM_RIGHTS doesn't dup the fd.  That's surprising, but we just
    // deal with it here.  NB: only the "default" (master) process can
    // alloc gralloc buffers.
    int dupFd = sameProcess && index < 0 ? dup(fd.fd) : fd.fd;
    fds[n] = dupFd;
  }

  aResult->mRef.mOwner = owner;
  aResult->mRef.mKey = index;
  if (sameProcess)
    aResult->mGraphicBuffer = SharedBufferManagerParent::GetGraphicBuffer(aResult->mRef);
  else {
    aResult->mGraphicBuffer = SharedBufferManagerChild::GetSingleton()->GetGraphicBuffer(index);
    if (index >= 0 && aResult->mGraphicBuffer == nullptr) {
      //Only newly created GraphicBuffer should deserialize
#if ANDROID_VERSION >= 19
      sp<GraphicBuffer> flattenable(new GraphicBuffer());
      const void* datap = (const void*)data;
      const int* fdsp = &fds[0];
      if (NO_ERROR == flattenable->unflatten(datap, nbytes, fdsp, nfds)) {
        aResult->mGraphicBuffer = flattenable;
      }
#else
      sp<GraphicBuffer> buffer(new GraphicBuffer());
      Flattenable *flattenable = buffer.get();

      if (NO_ERROR == flattenable->unflatten(data, nbytes, fds, nfds)) {
        aResult->mGraphicBuffer = buffer;
      }
#endif
    }
  }

  if (aResult->mGraphicBuffer == nullptr) {
    return false;
  }

  return true;
}

} // namespace IPC

namespace mozilla {
namespace layers {

MagicGrallocBufferHandle::MagicGrallocBufferHandle(const sp<GraphicBuffer>& aGraphicBuffer, GrallocBufferRef ref)
  : mGraphicBuffer(aGraphicBuffer)
  , mRef(ref)
{
}

//-----------------------------------------------------------------------------
// Parent process

static gfxImageFormat
ImageFormatForPixelFormat(android::PixelFormat aFormat)
{
  switch (aFormat) {
  case PIXEL_FORMAT_RGBA_8888:
    return gfxImageFormat::ARGB32;
  case PIXEL_FORMAT_RGBX_8888:
    return gfxImageFormat::RGB24;
  case PIXEL_FORMAT_RGB_565:
    return gfxImageFormat::RGB16_565;
  default:
    MOZ_CRASH("Unknown gralloc pixel format");
  }
  return gfxImageFormat::ARGB32;
}

static android::PixelFormat
PixelFormatForImageFormat(gfxImageFormat aFormat)
{
  switch (aFormat) {
  case gfxImageFormat::ARGB32:
    return android::PIXEL_FORMAT_RGBA_8888;
  case gfxImageFormat::RGB24:
    return android::PIXEL_FORMAT_RGBX_8888;
  case gfxImageFormat::RGB16_565:
    return android::PIXEL_FORMAT_RGB_565;
  case gfxImageFormat::A8:
    NS_WARNING("gralloc does not support gfxImageFormat::A8");
    return android::PIXEL_FORMAT_UNKNOWN;
  default:
    MOZ_CRASH("Unknown gralloc pixel format");
  }
  return android::PIXEL_FORMAT_RGBA_8888;
}

static size_t
BytesPerPixelForPixelFormat(android::PixelFormat aFormat)
{
  switch (aFormat) {
  case PIXEL_FORMAT_RGBA_8888:
  case PIXEL_FORMAT_RGBX_8888:
  case PIXEL_FORMAT_BGRA_8888:
    return 4;
  case PIXEL_FORMAT_RGB_888:
    return 3;
  case PIXEL_FORMAT_RGB_565:
  case PIXEL_FORMAT_RGBA_5551:
  case PIXEL_FORMAT_RGBA_4444:
    return 2;
  default:
    return 0;
  }
  return 0;
}

static android::PixelFormat
PixelFormatForContentType(gfxContentType aContentType)
{
  return PixelFormatForImageFormat(
    gfxPlatform::GetPlatform()->OptimalFormatForContent(aContentType));
}

static gfxContentType
ContentTypeFromPixelFormat(android::PixelFormat aFormat)
{
  return gfxASurface::ContentFromFormat(ImageFormatForPixelFormat(aFormat));
}

/*static*/ bool
LayerManagerComposite::SupportsDirectTexturing()
{
  return true;
}

/*static*/ void
LayerManagerComposite::PlatformSyncBeforeReplyUpdate()
{
  // Nothing to be done for gralloc.
}

//-----------------------------------------------------------------------------
// Both processes

/*static*/ sp<GraphicBuffer>
GetGraphicBufferFrom(MaybeMagicGrallocBufferHandle aHandle)
{
  if (aHandle.type() != MaybeMagicGrallocBufferHandle::TMagicGrallocBufferHandle) {
    if (aHandle.type() == MaybeMagicGrallocBufferHandle::TGrallocBufferRef) {
      if (XRE_GetProcessType() == GeckoProcessType_Default) {
        return SharedBufferManagerParent::GetGraphicBuffer(aHandle.get_GrallocBufferRef());
      }
      return SharedBufferManagerChild::GetSingleton()->GetGraphicBuffer(aHandle.get_GrallocBufferRef().mKey);
    }
  } else {
    MagicGrallocBufferHandle realHandle = aHandle.get_MagicGrallocBufferHandle();
    return realHandle.mGraphicBuffer;
  }
  return nullptr;
}

android::sp<android::GraphicBuffer>
GetGraphicBufferFromDesc(SurfaceDescriptor aDesc)
{
  MaybeMagicGrallocBufferHandle handle;
  if (aDesc.type() == SurfaceDescriptor::TNewSurfaceDescriptorGralloc) {
    handle = aDesc.get_NewSurfaceDescriptorGralloc().buffer();
  }
  return GetGraphicBufferFrom(handle);
}

/*static*/ void
ShadowLayerForwarder::PlatformSyncBeforeUpdate()
{
  // Nothing to be done for gralloc.
}

} // namespace layers
} // namespace mozilla
