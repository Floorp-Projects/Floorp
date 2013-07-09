/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ISurfaceAllocator.h"
#include "mozilla/ipc/SharedMemory.h"
#include "gfxSharedImageSurface.h"
#include "gfxPlatform.h"
#include "gfxASurface.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/layers/SharedPlanarYCbCrImage.h"
#include "mozilla/layers/SharedRGBImage.h"
#include "nsXULAppAPI.h"
#include "nsExpirationTracker.h"
#include "nsContentUtils.h"

#ifdef DEBUG
#include "prenv.h"
#endif

using namespace mozilla::ipc;

namespace mozilla {
namespace layers {

// We use MemoryImageData to hold the memory contained in MemoryImage objects. An
// expiration tracker keeps MemoryImageData objects around for a short period
// of time to avoid constantly re-allocating memory.
class MemoryImageData {
  friend class MemoryImageDataExpirationTracker;

  unsigned char *mData;
  size_t mSize;
  nsExpirationState mExpirationState;

  MemoryImageData() :
    mData(nullptr), mSize(0) { }
  ~MemoryImageData();

  void Init(size_t aSize) {
    mData = (unsigned char *)moz_malloc(aSize);
    mSize = aSize;
  }

  size_t Size() const {
    return mSize;
  }

public:
  nsExpirationState* GetExpirationState() {
    return &mExpirationState;
  }

  unsigned char *Data() const {
    return mData;
  }
  
  void Clear()
  {
    memset(Data(), 0, mSize);
  }

  static MemoryImageData *Alloc(size_t aSize);
  static void Destroy(MemoryImageData *);
};

class MemoryImageDataExpirationTracker MOZ_FINAL :
  public nsExpirationTracker<MemoryImageData,2>,
  public nsIObserver {
  friend class MemoryImageData;
  friend class DestroyOnMainThread;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

private:

  // With K = 2, this means that memory images will be released when they are not
  // used for 1-2 seconds.
  enum { TIMEOUT_MS = 1000 };
  MemoryImageDataExpirationTracker()
    : nsExpirationTracker<MemoryImageData,2>(TIMEOUT_MS)
  {
    nsContentUtils::RegisterShutdownObserver(this);
  }

  ~MemoryImageDataExpirationTracker() {
    AgeAllGenerations();
  }

  virtual void NotifyExpired(MemoryImageData *aData) {
    delete aData;
  }

  static void AddData(MemoryImageData* aData) {
    NS_ASSERTION(!aData->GetExpirationState()->IsTracked(), "must not be tracked yet");
    if (!sExpirationTracker) {
      sExpirationTracker = new MemoryImageDataExpirationTracker();
    }
    sExpirationTracker->AddObject(aData);
  }

  static void RemoveData(MemoryImageData* aData) {
    NS_ASSERTION(aData->GetExpirationState()->IsTracked(), "must be tracked");
    if (!sExpirationTracker)
      return;
    if (aData->GetExpirationState()->IsTracked()) {
      sExpirationTracker->RemoveObject(aData);
    }
  }

  static MemoryImageData *GetData(size_t aSize) {
    // Try to reuse a memory image data object already the tracker.
    if (NS_IsMainThread() && sExpirationTracker) {
      Iterator i = Iterator(sExpirationTracker);
      while (MemoryImageData *data = i.Next()) {
        if (data->Size() >= aSize && aSize > data->Size()/2) {
          RemoveData(data);
          return data;
        }
      }
    }
    return nullptr;
  }

  // Return a memory image data object to the tracker for reuse until it expires.
  static void DestroyData(MemoryImageData *aData) {
    AddData(aData);
  }

  static void Shutdown()
  {
    sExpirationTracker->AgeAllGenerations();
    sExpirationTracker = nullptr;
  }

private:
  static nsRefPtr<MemoryImageDataExpirationTracker> sExpirationTracker;
  uint32_t sConsumers;
};

NS_IMPL_ISUPPORTS1(MemoryImageDataExpirationTracker, nsIObserver)

NS_IMETHODIMP
MemoryImageDataExpirationTracker::Observe(nsISupports *aSubject,
                                          const char *aTopic,
                                          const PRUnichar *aData)
{
  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    Shutdown();

    nsContentUtils::UnregisterShutdownObserver(this);
  }
  return NS_OK;
}

nsRefPtr<MemoryImageDataExpirationTracker>
MemoryImageDataExpirationTracker::sExpirationTracker = nullptr;

MemoryImageData::~MemoryImageData()
{
  MemoryImageDataExpirationTracker::RemoveData(this);
}

MemoryImageData *
MemoryImageData::Alloc(size_t aSize)
{
  // The expiration tracker can only be used from the main thread. This matches our usage
  // pattern since we allocate buffers on the main thread and send them to the compositor
  // thread.
  if (NS_IsMainThread()) {
    if (MemoryImageData *data = MemoryImageDataExpirationTracker::GetData(aSize))
      return data;
  }
  // No luck, allocate a new one.
  MemoryImageData *data = new MemoryImageData();
  data->Init(aSize);
  return data;
}

class DestroyOnMainThread : public nsRunnable {
public:
  DestroyOnMainThread(MemoryImageData *aData)
    : mData(aData)
  {}

  NS_IMETHOD Run()
  {
    MemoryImageDataExpirationTracker::DestroyData(mData);
    return NS_OK;
  }
private:
  MemoryImageData *mData;
};

void
MemoryImageData::Destroy(MemoryImageData *aData)
{
  // The compositor destroys MemoryImageData objects, so send them back to the main thread
  // instead of stomping on the non-threadsafe expiration tracker.
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(new DestroyOnMainThread(aData));
    return;
  }
  return MemoryImageDataExpirationTracker::DestroyData(aData);
}

unsigned char *
GetMemoryImageData(const MemoryImage &aMemoryImage)
{
  return ((MemoryImageData *)aMemoryImage.data())->Data();
}

SharedMemory::SharedMemoryType OptimalShmemType()
{
#if defined(MOZ_PLATFORM_MAEMO) && defined(MOZ_HAVE_SHAREDMEMORYSYSV)
  // Use SysV memory because maemo5 on the N900 only allots 64MB to
  // /dev/shm, even though it has 1GB(!!) of system memory.  Sys V shm
  // is allocated from a different pool.  We don't want an arbitrary
  // cap that's much much lower than available memory on the memory we
  // use for layers.
  return SharedMemory::TYPE_SYSV;
#else
  return SharedMemory::TYPE_BASIC;
#endif
}

bool
IsSurfaceDescriptorValid(const SurfaceDescriptor& aSurface)
{
  return aSurface.type() != SurfaceDescriptor::T__None &&
         aSurface.type() != SurfaceDescriptor::Tnull_t;
}

bool
ISurfaceAllocator::AllocSharedImageSurface(const gfxIntSize& aSize,
                               gfxASurface::gfxContentType aContent,
                               gfxSharedImageSurface** aBuffer)
{
  SharedMemory::SharedMemoryType shmemType = OptimalShmemType();
  gfxASurface::gfxImageFormat format = gfxPlatform::GetPlatform()->OptimalFormatForContent(aContent);

  nsRefPtr<gfxSharedImageSurface> back =
    gfxSharedImageSurface::CreateUnsafe(this, aSize, format, shmemType);
  if (!back)
    return false;

  *aBuffer = nullptr;
  back.swap(*aBuffer);
  return true;
}

bool
ISurfaceAllocator::AllocSurfaceDescriptor(const gfxIntSize& aSize,
                                          gfxASurface::gfxContentType aContent,
                                          SurfaceDescriptor* aBuffer)
{
  return AllocSurfaceDescriptorWithCaps(aSize, aContent, DEFAULT_BUFFER_CAPS, aBuffer);
}

bool
ISurfaceAllocator::AllocSurfaceDescriptorWithCaps(const gfxIntSize& aSize,
                                                  gfxASurface::gfxContentType aContent,
                                                  uint32_t aCaps,
                                                  SurfaceDescriptor* aBuffer)
{
  bool tryPlatformSurface = true;
#ifdef DEBUG
  tryPlatformSurface = !PR_GetEnv("MOZ_LAYERS_FORCE_SHMEM_SURFACES");
#endif
  if (tryPlatformSurface &&
      PlatformAllocSurfaceDescriptor(aSize, aContent, aCaps, aBuffer)) {
    return true;
  }

  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    gfxImageFormat format =
      gfxPlatform::GetPlatform()->OptimalFormatForContent(aContent);
    int32_t stride = gfxASurface::FormatStrideForWidth(format, aSize.width);
    MemoryImageData *data = MemoryImageData::Alloc(stride * aSize.height);
#ifdef XP_MACOSX
    // Workaround a bug in Quartz where drawing an a8 surface to another a8
    // surface with OPERATOR_SOURCE still requires the destination to be clear.
    if (format == gfxASurface::ImageFormatA8) {
      data->Clear();
    }
#endif
    *aBuffer = MemoryImage((uintptr_t)data, aSize, stride, format);
    return true;
  }

  nsRefPtr<gfxSharedImageSurface> buffer;
  if (!AllocSharedImageSurface(aSize, aContent,
                               getter_AddRefs(buffer))) {
    return false;
  }

  *aBuffer = buffer->GetShmem();
  return true;
}


void
ISurfaceAllocator::DestroySharedSurface(SurfaceDescriptor* aSurface)
{
  MOZ_ASSERT(aSurface);
  if (!aSurface) {
    return;
  }
  if (!IsOnCompositorSide() && ReleaseOwnedSurfaceDescriptor(*aSurface)) {
    *aSurface = SurfaceDescriptor();
    return;
  }
  if (PlatformDestroySharedSurface(aSurface)) {
    return;
  }
  switch (aSurface->type()) {
    case SurfaceDescriptor::TShmem:
      DeallocShmem(aSurface->get_Shmem());
      break;
    case SurfaceDescriptor::TYCbCrImage:
      DeallocShmem(aSurface->get_YCbCrImage().data());
      break;
    case SurfaceDescriptor::TRGBImage:
      DeallocShmem(aSurface->get_RGBImage().data());
      break;
    case SurfaceDescriptor::TSurfaceDescriptorD3D10:
      break;
    case SurfaceDescriptor::TMemoryImage:
      MemoryImageData::Destroy(((MemoryImageData *)aSurface->get_MemoryImage().data()));
      break;
    case SurfaceDescriptor::Tnull_t:
    case SurfaceDescriptor::T__None:
      break;
    default:
      NS_RUNTIMEABORT("surface type not implemented!");
  }
  *aSurface = SurfaceDescriptor();
}

bool IsSurfaceDescriptorOwned(const SurfaceDescriptor& aDescriptor)
{
  switch (aDescriptor.type()) {
    case SurfaceDescriptor::TYCbCrImage: {
      const YCbCrImage& ycbcr = aDescriptor.get_YCbCrImage();
      return ycbcr.owner() != 0;
    }
    case SurfaceDescriptor::TRGBImage: {
      const RGBImage& rgb = aDescriptor.get_RGBImage();
      return rgb.owner() != 0;
    }
    default:
      return false;
  }
  return false;
}
bool ReleaseOwnedSurfaceDescriptor(const SurfaceDescriptor& aDescriptor)
{
  SharedPlanarYCbCrImage* sharedYCbCr = SharedPlanarYCbCrImage::FromSurfaceDescriptor(aDescriptor);
  if (sharedYCbCr) {
    sharedYCbCr->Release();
    return true;
  }
  SharedRGBImage* sharedRGB = SharedRGBImage::FromSurfaceDescriptor(aDescriptor);
  if (sharedRGB) {
    sharedRGB->Release();
    return true;
  }
  return false;
}

#if !defined(MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS)
bool
ISurfaceAllocator::PlatformAllocSurfaceDescriptor(const gfxIntSize&,
                                                  gfxASurface::gfxContentType,
                                                  uint32_t,
                                                  SurfaceDescriptor*)
{
  return false;
}
#endif

} // namespace
} // namespace
