/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "mozilla/layers/PGrallocBufferChild.h"
#include "mozilla/layers/PGrallocBufferParent.h"
#include "mozilla/layers/PLayersChild.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/unused.h"
#include "nsXULAppAPI.h"

#include "ShadowLayerUtilsGralloc.h"

#include "nsIMemoryReporter.h"

#include "gfxImageSurface.h"
#include "gfxPlatform.h"

#include "sampler.h"

using namespace android;
using namespace base;
using namespace mozilla::layers;
using namespace mozilla::gl;

namespace IPC {

void
ParamTraits<MagicGrallocBufferHandle>::Write(Message* aMsg,
                                             const paramType& aParam)
{
  Flattenable *flattenable = aParam.mGraphicBuffer.get();
  size_t nbytes = flattenable->getFlattenedSize();
  size_t nfds = flattenable->getFdCount();

  char data[nbytes];
  int fds[nfds];
  flattenable->flatten(data, nbytes, fds, nfds);

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
  Flattenable *flattenable = buffer.get();

  if (NO_ERROR == flattenable->unflatten(data, nbytes, fds, nfds)) {
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

static gfxASurface::gfxImageFormat
ImageFormatForPixelFormat(android::PixelFormat aFormat)
{
  switch (aFormat) {
  case PIXEL_FORMAT_RGBA_8888:
    return gfxASurface::ImageFormatARGB32;
  case PIXEL_FORMAT_RGBX_8888:
    return gfxASurface::ImageFormatRGB24;
  case PIXEL_FORMAT_RGB_565:
    return gfxASurface::ImageFormatRGB16_565;
  case PIXEL_FORMAT_A_8:
    return gfxASurface::ImageFormatA8;
  default:
    MOZ_NOT_REACHED("Unknown gralloc pixel format");
  }
  return gfxASurface::ImageFormatARGB32;
}

static android::PixelFormat
PixelFormatForImageFormat(gfxASurface::gfxImageFormat aFormat)
{
  switch (aFormat) {
  case gfxASurface::ImageFormatARGB32:
    return android::PIXEL_FORMAT_RGBA_8888;
  case gfxASurface::ImageFormatRGB24:
    return android::PIXEL_FORMAT_RGBX_8888;
  case gfxASurface::ImageFormatRGB16_565:
    return android::PIXEL_FORMAT_RGB_565;
  case gfxASurface::ImageFormatA8:
    return android::PIXEL_FORMAT_A_8;
  default:
    MOZ_NOT_REACHED("Unknown gralloc pixel format");
  }
  return gfxASurface::ImageFormatARGB32;
}

static android::PixelFormat
PixelFormatForContentType(gfxASurface::gfxContentType aContentType)
{
  return PixelFormatForImageFormat(
    gfxPlatform::GetPlatform()->OptimalFormatForContent(aContentType));
}

static gfxASurface::gfxContentType
ContentTypeFromPixelFormat(android::PixelFormat aFormat)
{
  return gfxASurface::ContentFromFormat(ImageFormatForPixelFormat(aFormat));
}

static size_t sCurrentAlloc;
static int64_t GetGrallocSize() { return sCurrentAlloc; }

NS_MEMORY_REPORTER_IMPLEMENT(GrallocBufferActor,
  "gralloc",
  KIND_OTHER,
  UNITS_BYTES,
  GetGrallocSize,
  "Special RAM that can be shared between processes and directly "
  "accessed by both the CPU and GPU.  Gralloc memory is usually a "
  "relatively precious resource, with much less available than generic "
  "RAM.  When it's exhausted, graphics performance can suffer. "
  "This value can be incorrect because of race conditions.");

GrallocBufferActor::GrallocBufferActor()
: mAllocBytes(0)
{
  static bool registered;
  if (!registered) {
    // We want to be sure that the first call here will always run on
    // the main thread.
    NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

    NS_RegisterMemoryReporter(new NS_MEMORY_REPORTER_NAME(GrallocBufferActor));
    registered = true;
  }
}

GrallocBufferActor::~GrallocBufferActor()
{
  if (mAllocBytes > 0) {
    sCurrentAlloc -= mAllocBytes;
  }
}

/*static*/ PGrallocBufferParent*
GrallocBufferActor::Create(const gfxIntSize& aSize,
                           const gfxContentType& aContent,
                           MaybeMagicGrallocBufferHandle* aOutHandle)
{
  SAMPLE_LABEL("GrallocBufferActor", "Create");
  GrallocBufferActor* actor = new GrallocBufferActor();
  *aOutHandle = null_t();
  android::PixelFormat format = PixelFormatForContentType(aContent);
  sp<GraphicBuffer> buffer(
    new GraphicBuffer(aSize.width, aSize.height, format,
                      GraphicBuffer::USAGE_SW_READ_OFTEN |
                      GraphicBuffer::USAGE_SW_WRITE_OFTEN |
                      GraphicBuffer::USAGE_HW_TEXTURE));
  if (buffer->initCheck() != OK)
    return actor;

  size_t bpp = gfxASurface::BytePerPixelFromFormat(
      gfxPlatform::GetPlatform()->OptimalFormatForContent(aContent));
  actor->mAllocBytes = aSize.width * aSize.height * bpp;
  sCurrentAlloc += actor->mAllocBytes;

  actor->mGraphicBuffer = buffer;
  *aOutHandle = MagicGrallocBufferHandle(buffer);
  return actor;
}

/*static*/ already_AddRefed<TextureImage>
ShadowLayerManager::OpenDescriptorForDirectTexturing(GLContext* aGL,
                                                     const SurfaceDescriptor& aDescriptor,
                                                     GLenum aWrapMode)
{
  SAMPLE_LABEL("ShadowLayerManager", "OpenDescriptorForDirectTexturing");
  if (SurfaceDescriptor::TSurfaceDescriptorGralloc != aDescriptor.type()) {
    return nullptr;
  }
  sp<GraphicBuffer> buffer = GrallocBufferActor::GetFrom(aDescriptor);
  return aGL->CreateDirectTextureImage(buffer.get(), aWrapMode);
}

/*static*/ void
ShadowLayerManager::PlatformSyncBeforeReplyUpdate()
{
  // Nothing to be done for gralloc.
}

/*static*/ PGrallocBufferParent*
GrallocBufferActor::Create(const gfxIntSize& aSize,
                           const uint32_t& aFormat,
                           const uint32_t& aUsage,
                           MaybeMagicGrallocBufferHandle* aOutHandle)
{
  GrallocBufferActor* actor = new GrallocBufferActor();
  *aOutHandle = null_t();
  sp<GraphicBuffer> buffer(
    new GraphicBuffer(aSize.width, aSize.height, aFormat, aUsage));
  if (buffer->initCheck() != OK)
    return actor;

  actor->mGraphicBuffer = buffer;
  *aOutHandle = MagicGrallocBufferHandle(buffer);
  return actor;
}

bool
ShadowLayerManager::PlatformDestroySharedSurface(SurfaceDescriptor* aSurface)
{
  if (SurfaceDescriptor::TSurfaceDescriptorGralloc != aSurface->type()) {
    return false;
  }

  PGrallocBufferParent* gbp =
    aSurface->get_SurfaceDescriptorGralloc().bufferParent();
  unused << PGrallocBufferParent::Send__delete__(gbp);
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

bool
ShadowLayerForwarder::PlatformAllocBuffer(const gfxIntSize& aSize,
                                          gfxASurface::gfxContentType aContent,
                                          uint32_t aCaps,
                                          SurfaceDescriptor* aBuffer)
{
  SAMPLE_LABEL("ShadowLayerForwarder", "PlatformAllocBuffer");
  // Gralloc buffers are efficiently mappable as gfxImageSurface, so
  // no need to check |aCaps & MAP_AS_IMAGE_SURFACE|.
  MaybeMagicGrallocBufferHandle handle;
  PGrallocBufferChild* gc =
    mShadowManager->SendPGrallocBufferConstructor(aSize, aContent, &handle);
  if (handle.Tnull_t == handle.type()) {
    PGrallocBufferChild::Send__delete__(gc);
    return false;
  }

  GrallocBufferActor* gba = static_cast<GrallocBufferActor*>(gc);
  gba->InitFromHandle(handle.get_MagicGrallocBufferHandle());

  *aBuffer = SurfaceDescriptorGralloc(nullptr, gc, /* external */ false);
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


/*static*/ already_AddRefed<gfxASurface>
ShadowLayerForwarder::PlatformOpenDescriptor(OpenMode aMode,
                                             const SurfaceDescriptor& aSurface)
{
  SAMPLE_LABEL("ShadowLayerForwarder", "PlatformOpenDescriptor");
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

  gfxIntSize size = gfxIntSize(buffer->getWidth(), buffer->getHeight());
  gfxImageFormat format = ImageFormatForPixelFormat(buffer->getPixelFormat());
  long pixelStride = buffer->getStride();
  long byteStride = pixelStride * gfxASurface::BytePerPixelFromFormat(format);

  nsRefPtr<gfxASurface> surf =
    new gfxImageSurface((unsigned char*)vaddr, size, byteStride, format);
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
  gfxIntSize* aSize,
  gfxASurface** aSurface)
{
  if (SurfaceDescriptor::TSurfaceDescriptorGralloc != aDescriptor.type()) {
    return false;
  }

  sp<GraphicBuffer> buffer =
    GrallocBufferActor::GetFrom(aDescriptor.get_SurfaceDescriptorGralloc());
  *aSize = gfxIntSize(buffer->getWidth(), buffer->getHeight());
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
  SAMPLE_LABEL("ShadowLayerForwarder", "PlatformCloseDescriptor");
  if (SurfaceDescriptor::TSurfaceDescriptorGralloc != aDescriptor.type()) {
    return false;
  }

  sp<GraphicBuffer> buffer = GrallocBufferActor::GetFrom(aDescriptor);
  // If the buffer wasn't lock()d, this probably blows up.  But since
  // PlatformCloseDescriptor() is private and only used by
  // AutoOpenSurface, we want to know if the logic is wrong there.
  buffer->unlock();
  return true;
}

/*static*/ void
ShadowLayerForwarder::PlatformSyncBeforeUpdate()
{
  // Nothing to be done for gralloc.
}

} // namespace layers
} // namespace mozilla
