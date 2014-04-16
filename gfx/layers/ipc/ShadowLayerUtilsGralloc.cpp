/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "mozilla/gfx/Point.h"
#include "mozilla/layers/PGrallocBufferChild.h"
#include "mozilla/layers/PGrallocBufferParent.h"
#include "mozilla/layers/LayerTransactionChild.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/TextureHost.h"
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

  if (!aMsg->ReadSize(aIter, &nbytes) ||
      !aMsg->ReadSize(aIter, &nfds) ||
      !aMsg->ReadBytes(aIter, &data, nbytes)) {
    return false;
  }

  int fds[nfds];

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
    bool sameProcess = (XRE_GetProcessType() == GeckoProcessType_Default);
    int dupFd = sameProcess ? dup(fd.fd) : fd.fd;
    fds[n] = dupFd;
  }

  sp<GraphicBuffer> buffer(new GraphicBuffer());
#if ANDROID_VERSION >= 19
  // Make a copy of "data" and "fds" for unflatten() to avoid casting problem
  void const *pdata = (void const *)data;
  int const *pfds = fds;

  if (NO_ERROR == buffer->unflatten(pdata, nbytes, pfds, nfds)) {
#else
  Flattenable *flattenable = buffer.get();

  if (NO_ERROR == flattenable->unflatten(data, nbytes, fds, nfds)) {
#endif
    aResult->mGraphicBuffer = buffer;
    return true;
  }
  return false;
}

} // namespace IPC

namespace mozilla {
namespace layers {

MagicGrallocBufferHandle::MagicGrallocBufferHandle(const sp<GraphicBuffer>& aGraphicBuffer)
  : mGraphicBuffer(aGraphicBuffer)
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

class GrallocReporter MOZ_FINAL : public nsIMemoryReporter
{
  friend class GrallocBufferActor;

public:
  NS_DECL_ISUPPORTS

  GrallocReporter()
  {
#ifdef DEBUG
    // There must be only one instance of this class, due to |sAmount|
    // being static.  Assert this.
    static bool hasRun = false;
    MOZ_ASSERT(!hasRun);
    hasRun = true;
#endif
  }

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData)
  {
    return MOZ_COLLECT_REPORT(
      "gralloc", KIND_OTHER, UNITS_BYTES, sAmount,
"Special RAM that can be shared between processes and directly accessed by "
"both the CPU and GPU. Gralloc memory is usually a relatively precious "
"resource, with much less available than generic RAM. When it's exhausted, "
"graphics performance can suffer. This value can be incorrect because of race "
"conditions.");
  }

private:
  static int64_t sAmount;
};

NS_IMPL_ISUPPORTS1(GrallocReporter, nsIMemoryReporter)

int64_t GrallocReporter::sAmount = 0;

void InitGralloc() {
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  RegisterStrongMemoryReporter(new GrallocReporter());
}

GrallocBufferActor::GrallocBufferActor()
: mAllocBytes(0)
, mTextureHost(nullptr)
{
}

GrallocBufferActor::~GrallocBufferActor()
{
  if (mAllocBytes > 0) {
    GrallocReporter::sAmount -= mAllocBytes;
  }
}

/*static*/ PGrallocBufferParent*
GrallocBufferActor::Create(const gfx::IntSize& aSize,
                           const uint32_t& aFormat,
                           const uint32_t& aUsage,
                           MaybeMagicGrallocBufferHandle* aOutHandle)
{
  PROFILER_LABEL("GrallocBufferActor", "Create");
  GrallocBufferActor* actor = new GrallocBufferActor();
  *aOutHandle = null_t();
  uint32_t format = aFormat;
  uint32_t usage = aUsage;

  if (format == 0 || usage == 0) {
    printf_stderr("GrallocBufferActor::Create -- format and usage must be non-zero");
    return actor;
  }

  // If the requested size is too big (i.e. exceeds the commonly used max GL texture size)
  // then we risk OOMing the parent process. It's better to just deny the allocation and
  // kill the child process, which is what the following code does.
  // TODO: actually use GL_MAX_TEXTURE_SIZE instead of hardcoding 4096
  if (aSize.width > 4096 || aSize.height > 4096) {
    printf_stderr("GrallocBufferActor::Create -- requested gralloc buffer is too big. Killing child instead.");
    delete actor;
    return nullptr;
  }

  sp<GraphicBuffer> buffer(new GraphicBuffer(aSize.width, aSize.height, format, usage));
  if (buffer->initCheck() != OK)
    return actor;

  size_t bpp = BytesPerPixelForPixelFormat(format);
  actor->mAllocBytes = aSize.width * aSize.height * bpp;
  GrallocReporter::sAmount += actor->mAllocBytes;

  actor->mGraphicBuffer = buffer;
  *aOutHandle = MagicGrallocBufferHandle(buffer);

  return actor;
}

void GrallocBufferActor::ActorDestroy(ActorDestroyReason)
{
  // Used only for hacky fix for bug 966446.
  if (mTextureHost) {
    mTextureHost->ForgetBufferActor();
    mTextureHost = nullptr;
  }
}

void GrallocBufferActor::AddTextureHost(TextureHost* aTextureHost)
{
  mTextureHost = aTextureHost;
}

void GrallocBufferActor::RemoveTextureHost()
{
  mTextureHost = nullptr;
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
// Child process

/*static*/ PGrallocBufferChild*
GrallocBufferActor::Create()
{
  return new GrallocBufferActor();
}

void
GrallocBufferActor::InitFromHandle(const MagicGrallocBufferHandle& aHandle)
{
  MOZ_ASSERT(!mGraphicBuffer.get());
  MOZ_ASSERT(aHandle.mGraphicBuffer.get());

  mGraphicBuffer = aHandle.mGraphicBuffer;
}

PGrallocBufferChild*
ShadowLayerForwarder::AllocGrallocBuffer(const gfx::IntSize& aSize,
                                         uint32_t aFormat,
                                         uint32_t aUsage,
                                         MaybeMagicGrallocBufferHandle* aHandle)
{
  return mShadowManager->SendPGrallocBufferConstructor(aSize, aFormat, aUsage, aHandle);
}

void
ShadowLayerForwarder::DeallocGrallocBuffer(PGrallocBufferChild* aChild)
{
  MOZ_ASSERT(aChild);
  PGrallocBufferChild::Send__delete__(aChild);
}

//-----------------------------------------------------------------------------
// Both processes

android::GraphicBuffer*
GrallocBufferActor::GetGraphicBuffer()
{
  return mGraphicBuffer.get();
}

/*static*/ void
ShadowLayerForwarder::PlatformSyncBeforeUpdate()
{
  // Nothing to be done for gralloc.
}

} // namespace layers
} // namespace mozilla
