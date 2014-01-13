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

#include "gfxImageSurface.h"
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
    return gfxImageFormatARGB32;
  case PIXEL_FORMAT_RGBX_8888:
    return gfxImageFormatRGB24;
  case PIXEL_FORMAT_RGB_565:
    return gfxImageFormatRGB16_565;
  default:
    MOZ_CRASH("Unknown gralloc pixel format");
  }
  return gfxImageFormatARGB32;
}

static android::PixelFormat
PixelFormatForImageFormat(gfxImageFormat aFormat)
{
  switch (aFormat) {
  case gfxImageFormatARGB32:
    return android::PIXEL_FORMAT_RGBA_8888;
  case gfxImageFormatRGB24:
    return android::PIXEL_FORMAT_RGBX_8888;
  case gfxImageFormatRGB16_565:
    return android::PIXEL_FORMAT_RGB_565;
  default:
    MOZ_CRASH("Unknown gralloc pixel format");
  }
  return gfxImageFormatARGB32;
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

GrallocBufferActor::GrallocBufferActor()
: mAllocBytes(0)
{
  static bool registered;
  if (!registered) {
    // We want to be sure that the first call here will always run on
    // the main thread.
    NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

    RegisterStrongMemoryReporter(new GrallocReporter());
    registered = true;
  }
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

// used only for hacky fix for bug 862324
void GrallocBufferActor::ActorDestroy(ActorDestroyReason)
{
  for (size_t i = 0; i < mDeprecatedTextureHosts.Length(); i++) {
    mDeprecatedTextureHosts[i]->ForgetBuffer();
  }
}

// used only for hacky fix for bug 862324
void GrallocBufferActor::AddDeprecatedTextureHost(DeprecatedTextureHost* aDeprecatedTextureHost)
{
  mDeprecatedTextureHosts.AppendElement(aDeprecatedTextureHost);
}

// used only for hacky fix for bug 862324
void GrallocBufferActor::RemoveDeprecatedTextureHost(DeprecatedTextureHost* aDeprecatedTextureHost)
{
  mDeprecatedTextureHosts.RemoveElement(aDeprecatedTextureHost);
  // that should be the only occurence, otherwise we'd leak this TextureHost...
  // assert that that's not happening.
  MOZ_ASSERT(!mDeprecatedTextureHosts.Contains(aDeprecatedTextureHost));
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

bool
ISurfaceAllocator::PlatformDestroySharedSurface(SurfaceDescriptor* aSurface)
{
  if (SurfaceDescriptor::TSurfaceDescriptorGralloc != aSurface->type()) {
    return false;
  }

  // we should have either a bufferParent or bufferChild
  // depending on whether we're on the parent or child side.
  PGrallocBufferParent* gbp =
    aSurface->get_SurfaceDescriptorGralloc().bufferParent();
  if (gbp) {
    unused << PGrallocBufferParent::Send__delete__(gbp);
  } else {
    PGrallocBufferChild* gbc =
      aSurface->get_SurfaceDescriptorGralloc().bufferChild();
    unused << PGrallocBufferChild::Send__delete__(gbc);
  }

  *aSurface = SurfaceDescriptor();
  return true;
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

bool
ISurfaceAllocator::PlatformAllocSurfaceDescriptor(const gfx::IntSize& aSize,
                                                  gfxContentType aContent,
                                                  uint32_t aCaps,
                                                  SurfaceDescriptor* aBuffer)
{

  // Check for devices that have problems with gralloc. We only check for
  // this on ICS or earlier, in hopes that JB will work.
#if ANDROID_VERSION <= 15
  static bool checkedDevice = false;
  static bool disableGralloc = false;

  if (!checkedDevice) {
    char propValue[PROPERTY_VALUE_MAX];
    property_get("ro.product.device", propValue, "None");

    if (strcmp("crespo",propValue) == 0) {
      NS_WARNING("Nexus S has issues with gralloc, falling back to shmem");
      disableGralloc = true;
    }

    checkedDevice = true;
  }

  if (disableGralloc) {
    return false;
  }
#endif

  // Some GL implementations fail to render gralloc textures with
  // width < 64.  There's not much point in gralloc'ing buffers that
  // small anyway, so fall back on shared memory plus a texture
  // upload.
  if (aSize.width < 64) {
    return false;
  }
  PROFILER_LABEL("ShadowLayerForwarder", "PlatformAllocSurfaceDescriptor");
  // Gralloc buffers are efficiently mappable as gfxImageSurface, so
  // no need to check |aCaps & MAP_AS_IMAGE_SURFACE|.
  MaybeMagicGrallocBufferHandle handle;
  PGrallocBufferChild* gc;
  bool defaultRBSwap;

  if (aCaps & USING_GL_RENDERING_ONLY) {
    gc = AllocGrallocBuffer(aSize,
                            PixelFormatForContentType(aContent),
                            GraphicBuffer::USAGE_HW_RENDER |
                            GraphicBuffer::USAGE_HW_TEXTURE,
                            &handle);
    // If you're allocating for USING_GL_RENDERING_ONLY, then we don't flag
    // this for RB swap.
    defaultRBSwap = false;
  } else {
    gc = AllocGrallocBuffer(aSize,
                            PixelFormatForContentType(aContent),
                            GraphicBuffer::USAGE_SW_READ_OFTEN |
                            GraphicBuffer::USAGE_SW_WRITE_OFTEN |
                            GraphicBuffer::USAGE_HW_TEXTURE,
                            &handle);
    // But if you're allocating for non-GL-only rendering, we flag for
    // RB swap to preserve old behaviour and proper interaction with
    // cairo.
    defaultRBSwap = true;
  }

  if (!gc) {
    NS_ERROR("GrallocBufferConstructor failed by returned null");
    return false;
  } else if (handle.Tnull_t == handle.type()) {
    NS_ERROR("GrallocBufferConstructor failed by returning handle with type Tnull_t");
    PGrallocBufferChild::Send__delete__(gc);
    return false;
  }

  GrallocBufferActor* gba = static_cast<GrallocBufferActor*>(gc);
  gba->InitFromHandle(handle.get_MagicGrallocBufferHandle());

  *aBuffer = SurfaceDescriptorGralloc(nullptr, gc, aSize,
                                      /* external */ false,
                                      defaultRBSwap);
  return true;
}

//-----------------------------------------------------------------------------
// Both processes

/*static*/ sp<GraphicBuffer>
GrallocBufferActor::GetFrom(const SurfaceDescriptorGralloc& aDescriptor)
{
  GrallocBufferActor* gba = nullptr;
  if (PGrallocBufferChild* child = aDescriptor.bufferChild()) {
    gba = static_cast<GrallocBufferActor*>(child);
  } else if (PGrallocBufferParent* parent = aDescriptor.bufferParent()) {
    gba = static_cast<GrallocBufferActor*>(parent);
  }
  return gba->mGraphicBuffer;
}

android::GraphicBuffer*
GrallocBufferActor::GetGraphicBuffer()
{
  return mGraphicBuffer.get();
}

/*static*/ already_AddRefed<gfxASurface>
ShadowLayerForwarder::PlatformOpenDescriptor(OpenMode aMode,
                                             const SurfaceDescriptor& aSurface)
{
  PROFILER_LABEL("ShadowLayerForwarder", "PlatformOpenDescriptor");
  if (SurfaceDescriptor::TSurfaceDescriptorGralloc != aSurface.type()) {
    return nullptr;
  }

  sp<GraphicBuffer> buffer =
    GrallocBufferActor::GetFrom(aSurface.get_SurfaceDescriptorGralloc());
  uint32_t usage = GRALLOC_USAGE_SW_READ_OFTEN;
  if (OPEN_READ_WRITE == aMode) {
    usage |= GRALLOC_USAGE_SW_WRITE_OFTEN;
  }
  void *vaddr;
  DebugOnly<status_t> status = buffer->lock(usage, &vaddr);
  // If we fail to lock, we'll just end up aborting anyway.
  MOZ_ASSERT(status == OK);

  gfx::IntSize size = aSurface.get_SurfaceDescriptorGralloc().size();
  gfxImageFormat format = ImageFormatForPixelFormat(buffer->getPixelFormat());
  long pixelStride = buffer->getStride();
  long byteStride = pixelStride * gfxASurface::BytePerPixelFromFormat(format);

  nsRefPtr<gfxASurface> surf =
    new gfxImageSurface((unsigned char*)vaddr,
                        gfx::ThebesIntSize(size),
                        byteStride,
                        format);
  return surf->CairoStatus() ? nullptr : surf.forget();
}

/*static*/ bool
ShadowLayerForwarder::PlatformGetDescriptorSurfaceContentType(
  const SurfaceDescriptor& aDescriptor, OpenMode aMode,
  gfxContentType* aContent,
  gfxASurface** aSurface)
{
  if (SurfaceDescriptor::TSurfaceDescriptorGralloc != aDescriptor.type()) {
    return false;
  }

  sp<GraphicBuffer> buffer =
    GrallocBufferActor::GetFrom(aDescriptor.get_SurfaceDescriptorGralloc());
  *aContent = ContentTypeFromPixelFormat(buffer->getPixelFormat());
  return true;
}

/*static*/ bool
ShadowLayerForwarder::PlatformGetDescriptorSurfaceSize(
  const SurfaceDescriptor& aDescriptor, OpenMode aMode,
  gfx::IntSize* aSize,
  gfxASurface** aSurface)
{
  if (SurfaceDescriptor::TSurfaceDescriptorGralloc != aDescriptor.type()) {
    return false;
  }

  sp<GraphicBuffer> buffer =
    GrallocBufferActor::GetFrom(aDescriptor.get_SurfaceDescriptorGralloc());
  *aSize = aDescriptor.get_SurfaceDescriptorGralloc().size();
  return true;
}

/*static*/ bool
ShadowLayerForwarder::PlatformGetDescriptorSurfaceImageFormat(
  const SurfaceDescriptor& aDescriptor,
  OpenMode aMode,
  gfxImageFormat* aImageFormat,
  gfxASurface** aSurface)
{
  if (SurfaceDescriptor::TSurfaceDescriptorGralloc != aDescriptor.type()) {
    return false;
  }

  sp<GraphicBuffer> buffer =
    GrallocBufferActor::GetFrom(aDescriptor.get_SurfaceDescriptorGralloc());
  *aImageFormat = ImageFormatForPixelFormat(buffer->getPixelFormat());
  return true;
}

/*static*/ bool
ShadowLayerForwarder::PlatformDestroySharedSurface(SurfaceDescriptor* aSurface)
{
  if (SurfaceDescriptor::TSurfaceDescriptorGralloc != aSurface->type()) {
    return false;
  }

  PGrallocBufferChild* gbp =
    aSurface->get_SurfaceDescriptorGralloc().bufferChild();
  PGrallocBufferChild::Send__delete__(gbp);
  *aSurface = SurfaceDescriptor();
  return true;
}

/*static*/ bool
ShadowLayerForwarder::PlatformCloseDescriptor(const SurfaceDescriptor& aDescriptor)
{
  PROFILER_LABEL("ShadowLayerForwarder", "PlatformCloseDescriptor");
  if (SurfaceDescriptor::TSurfaceDescriptorGralloc != aDescriptor.type()) {
    return false;
  }

  sp<GraphicBuffer> buffer = GrallocBufferActor::GetFrom(aDescriptor);
  DebugOnly<status_t> status = buffer->unlock();
  // If we fail to unlock, we'll subsequently fail to lock and end up aborting anyway.
  MOZ_ASSERT(status == OK);
  return true;
}

/*static*/ void
ShadowLayerForwarder::PlatformSyncBeforeUpdate()
{
  // Nothing to be done for gralloc.
}

} // namespace layers
} // namespace mozilla
