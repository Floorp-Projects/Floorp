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
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
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
using namespace mozilla::layers;
using namespace mozilla::gl;

using base::FileDescriptor;

namespace IPC {

void
ParamTraits<GrallocBufferRef>::Write(Message* aMsg,
                                     const paramType& aParam)
{
  aMsg->WriteInt(aParam.mOwner);
  aMsg->WriteInt64(aParam.mKey);
}

bool
ParamTraits<GrallocBufferRef>::Read(const Message* aMsg, PickleIterator* aIter,
                                    paramType* aParam)
{
  int owner;
  int64_t index;
  if (!aMsg->ReadInt(aIter, &owner) ||
      !aMsg->ReadInt64(aIter, &index)) {
    printf_stderr("ParamTraits<GrallocBufferRef>::Read() failed to read a message\n");
    return false;
  }
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
  aMsg->WriteInt64(aParam.mRef.mKey);
  aMsg->WriteSize(nbytes);
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
                                            PickleIterator* aIter, paramType* aResult)
{
  MOZ_ASSERT(!aResult->mGraphicBuffer.get());
  MOZ_ASSERT(aResult->mRef.mOwner == 0);
  MOZ_ASSERT(aResult->mRef.mKey == -1);

  size_t nbytes;
  int owner;
  int64_t index;

  if (!aMsg->ReadInt(aIter, &owner) ||
      !aMsg->ReadInt64(aIter, &index) ||
      !aMsg->ReadSize(aIter, &nbytes)) {
    printf_stderr("ParamTraits<MagicGrallocBufferHandle>::Read() failed to read a message\n");
    return false;
  }

  auto data = mozilla::MakeUnique<char[]>(nbytes);

  if (!aMsg->ReadBytesInto(aIter, data.get(), nbytes)) {
    printf_stderr("ParamTraits<MagicGrallocBufferHandle>::Read() failed to read a message\n");
    return false;
  }

  size_t nfds = aMsg->num_fds();
  int fds[nfds];

  for (size_t n = 0; n < nfds; ++n) {
    FileDescriptor fd;
    if (!aMsg->ReadFileDescriptor(aIter, &fd)) {
      printf_stderr("ParamTraits<MagicGrallocBufferHandle>::Read() failed to read file descriptors\n");
      return false;
    }
    // If the GraphicBuffer was shared cross-process, SCM_RIGHTS does
    // the right thing and dup's the fd. If it's shared cross-thread,
    // SCM_RIGHTS doesn't dup the fd.
    // But in shared cross-thread, dup fd is not necessary because we get
    // a pointer to the GraphicBuffer directly from SharedBufferManagerParent
    // and don't create a new GraphicBuffer around the fd.
    fds[n] = fd.fd;
  }

  aResult->mRef.mOwner = owner;
  aResult->mRef.mKey = index;
  if (XRE_IsParentProcess()) {
    // If we are in chrome process, we can just get GraphicBuffer directly from
    // SharedBufferManagerParent.
    aResult->mGraphicBuffer = SharedBufferManagerParent::GetGraphicBuffer(aResult->mRef);
  } else {
    // Deserialize GraphicBuffer
#if ANDROID_VERSION >= 19
    sp<GraphicBuffer> buffer(new GraphicBuffer());
    const void* datap = (const void*)data.get();
    const int* fdsp = &fds[0];
    if (NO_ERROR != buffer->unflatten(datap, nbytes, fdsp, nfds)) {
      buffer = nullptr;
    }
#else
    sp<GraphicBuffer> buffer(new GraphicBuffer());
    Flattenable *flattenable = buffer.get();
    if (NO_ERROR != flattenable->unflatten(data.get(), nbytes, fds, nfds)) {
      buffer = nullptr;
    }
#endif
    if (buffer.get()) {
      aResult->mGraphicBuffer = buffer;
    }
  }

  if (!aResult->mGraphicBuffer.get()) {
    printf_stderr("ParamTraits<MagicGrallocBufferHandle>::Read() failed to get gralloc buffer\n");
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
      if (XRE_IsParentProcess()) {
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
  if (aDesc.type() == SurfaceDescriptor::TSurfaceDescriptorGralloc) {
    handle = aDesc.get_SurfaceDescriptorGralloc().buffer();
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
