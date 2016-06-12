/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasImageCache.h"
#include "nsAutoPtr.h"
#include "nsIImageLoadingContent.h"
#include "nsExpirationTracker.h"
#include "imgIRequest.h"
#include "mozilla/dom/Element.h"
#include "nsTHashtable.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "nsContentUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/gfx/2D.h"
#include "gfx2DGlue.h"

namespace mozilla {

using namespace dom;
using namespace gfx;

/**
 * Used for images specific to this one canvas. Required
 * due to CORS security.
 */
struct ImageCacheKey
{
  ImageCacheKey(imgIContainer* aImage,
                HTMLCanvasElement* aCanvas,
                bool aIsAccelerated)
    : mImage(aImage)
    , mCanvas(aCanvas)
    , mIsAccelerated(aIsAccelerated)
  {}
  nsCOMPtr<imgIContainer> mImage;
  HTMLCanvasElement* mCanvas;
  bool mIsAccelerated;
};

/**
 * Cache data needs to be separate from the entry
 * for nsExpirationTracker.
 */
struct ImageCacheEntryData
{
  ImageCacheEntryData(const ImageCacheEntryData& aOther)
    : mImage(aOther.mImage)
    , mCanvas(aOther.mCanvas)
    , mIsAccelerated(aOther.mIsAccelerated)
    , mSourceSurface(aOther.mSourceSurface)
    , mSize(aOther.mSize)
  {}
  explicit ImageCacheEntryData(const ImageCacheKey& aKey)
    : mImage(aKey.mImage)
    , mCanvas(aKey.mCanvas)
    , mIsAccelerated(aKey.mIsAccelerated)
  {}

  nsExpirationState* GetExpirationState() { return &mState; }
  size_t SizeInBytes() { return mSize.width * mSize.height * 4; }

  // Key
  nsCOMPtr<imgIContainer> mImage;
  HTMLCanvasElement* mCanvas;
  bool mIsAccelerated;
  // Value
  RefPtr<SourceSurface> mSourceSurface;
  IntSize mSize;
  nsExpirationState mState;
};

class ImageCacheEntry : public PLDHashEntryHdr
{
public:
  typedef ImageCacheKey KeyType;
  typedef const ImageCacheKey* KeyTypePointer;

  explicit ImageCacheEntry(const KeyType* aKey) :
      mData(new ImageCacheEntryData(*aKey)) {}
  ImageCacheEntry(const ImageCacheEntry& toCopy) :
      mData(new ImageCacheEntryData(*toCopy.mData)) {}
  ~ImageCacheEntry() {}

  bool KeyEquals(KeyTypePointer key) const
  {
    return mData->mImage == key->mImage &&
           mData->mCanvas == key->mCanvas &&
           mData->mIsAccelerated == key->mIsAccelerated;
  }

  static KeyTypePointer KeyToPointer(KeyType& key) { return &key; }
  static PLDHashNumber HashKey(KeyTypePointer key)
  {
    return HashGeneric(key->mImage.get(), key->mCanvas, key->mIsAccelerated);
  }
  enum { ALLOW_MEMMOVE = true };

  nsAutoPtr<ImageCacheEntryData> mData;
};


/**
 * Used for all images across all canvases.
 */
struct AllCanvasImageCacheKey
{
  AllCanvasImageCacheKey(imgIContainer* aImage,
                         bool aIsAccelerated)
    : mImage(aImage)
    , mIsAccelerated(aIsAccelerated)
  {}

  nsCOMPtr<imgIContainer> mImage;
  bool mIsAccelerated;
};

class AllCanvasImageCacheEntry : public PLDHashEntryHdr {
public:
  typedef AllCanvasImageCacheKey KeyType;
  typedef const AllCanvasImageCacheKey* KeyTypePointer;

  explicit AllCanvasImageCacheEntry(const KeyType* aKey)
    : mImage(aKey->mImage)
    , mIsAccelerated(aKey->mIsAccelerated)
  {}

  AllCanvasImageCacheEntry(const AllCanvasImageCacheEntry &toCopy)
    : mImage(toCopy.mImage)
    , mIsAccelerated(toCopy.mIsAccelerated)
    , mSourceSurface(toCopy.mSourceSurface)
  {}

  ~AllCanvasImageCacheEntry() {}

  bool KeyEquals(KeyTypePointer key) const
  {
    return mImage == key->mImage &&
           mIsAccelerated == key->mIsAccelerated;
  }

  static KeyTypePointer KeyToPointer(KeyType& key) { return &key; }
  static PLDHashNumber HashKey(KeyTypePointer key)
  {
    return HashGeneric(key->mImage.get(), key->mIsAccelerated);
  }
  enum { ALLOW_MEMMOVE = true };

  nsCOMPtr<imgIContainer> mImage;
  bool mIsAccelerated;
  RefPtr<SourceSurface> mSourceSurface;
};

static bool sPrefsInitialized = false;
static int32_t sCanvasImageCacheLimit = 0;

class ImageCacheObserver;

class ImageCache final : public nsExpirationTracker<ImageCacheEntryData,4>
{
public:
  // We use 3 generations of 1 second each to get a 2-3 seconds timeout.
  enum { GENERATION_MS = 1000 };
  ImageCache();
  ~ImageCache();

  virtual void NotifyExpired(ImageCacheEntryData* aObject)
  {
    mTotal -= aObject->SizeInBytes();
    RemoveObject(aObject);

    // Remove from the all canvas cache entry first since nsExpirationTracker
    // will delete aObject.
    mAllCanvasCache.RemoveEntry(AllCanvasImageCacheKey(aObject->mImage, aObject->mIsAccelerated));

    // Deleting the entry will delete aObject since the entry owns aObject.
    mCache.RemoveEntry(ImageCacheKey(aObject->mImage, aObject->mCanvas, aObject->mIsAccelerated));
  }

  nsTHashtable<ImageCacheEntry> mCache;
  nsTHashtable<AllCanvasImageCacheEntry> mAllCanvasCache;
  size_t mTotal;
  RefPtr<ImageCacheObserver> mImageCacheObserver;
};

static ImageCache* gImageCache = nullptr;

// Listen memory-pressure event for image cache purge.
class ImageCacheObserver final : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS

  explicit ImageCacheObserver(ImageCache* aImageCache)
    : mImageCache(aImageCache)
  {
    RegisterMemoryPressureEvent();
  }

  void Destroy()
  {
    UnregisterMemoryPressureEvent();
    mImageCache = nullptr;
  }

  NS_IMETHODIMP Observe(nsISupports* aSubject,
                        const char* aTopic,
                        const char16_t* aSomeData) override
  {
    if (!mImageCache || strcmp(aTopic, "memory-pressure")) {
      return NS_OK;
    }

    mImageCache->AgeAllGenerations();
    return NS_OK;
  }

private:
  virtual ~ImageCacheObserver()
  {
  }

  void RegisterMemoryPressureEvent()
  {
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();

    MOZ_ASSERT(observerService);

    if (observerService) {
      observerService->AddObserver(this, "memory-pressure", false);
    }
  }

  void UnregisterMemoryPressureEvent()
  {
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();

    // Do not assert on observerService here. This might be triggered by
    // the cycle collector at a late enough time, that XPCOM services are
    // no longer available. See bug 1029504.
    if (observerService) {
        observerService->RemoveObserver(this, "memory-pressure");
    }
  }

  ImageCache* mImageCache;
};

NS_IMPL_ISUPPORTS(ImageCacheObserver, nsIObserver)

class CanvasImageCacheShutdownObserver final : public nsIObserver
{
  ~CanvasImageCacheShutdownObserver() {}
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

ImageCache::ImageCache()
  : nsExpirationTracker<ImageCacheEntryData,4>(GENERATION_MS, "ImageCache")
  , mTotal(0)
{
  if (!sPrefsInitialized) {
    sPrefsInitialized = true;
    Preferences::AddIntVarCache(&sCanvasImageCacheLimit, "canvas.image.cache.limit", 0);
  }
  mImageCacheObserver = new ImageCacheObserver(this);
  MOZ_RELEASE_ASSERT(mImageCacheObserver, "GFX: Can't alloc ImageCacheObserver");
}

ImageCache::~ImageCache() {
  AgeAllGenerations();
  mImageCacheObserver->Destroy();
}

static already_AddRefed<imgIContainer>
GetImageContainer(dom::Element* aImage)
{
  nsCOMPtr<imgIRequest> request;
  nsCOMPtr<nsIImageLoadingContent> ilc = do_QueryInterface(aImage);
  if (!ilc) {
    return nullptr;
  }

  ilc->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                  getter_AddRefs(request));
  if (!request) {
    return nullptr;
  }

  nsCOMPtr<imgIContainer> imgContainer;
  request->GetImage(getter_AddRefs(imgContainer));
  if (!imgContainer) {
    return nullptr;
  }

  return imgContainer.forget();
}

void
CanvasImageCache::NotifyDrawImage(Element* aImage,
                                  HTMLCanvasElement* aCanvas,
                                  SourceSurface* aSource,
                                  const IntSize& aSize,
                                  bool aIsAccelerated)
{
  if (!gImageCache) {
    gImageCache = new ImageCache();
    nsContentUtils::RegisterShutdownObserver(new CanvasImageCacheShutdownObserver());
  }

  nsCOMPtr<imgIContainer> imgContainer = GetImageContainer(aImage);
  if (!imgContainer) {
    return;
  }

  AllCanvasImageCacheKey allCanvasCacheKey(imgContainer, aIsAccelerated);
  ImageCacheKey canvasCacheKey(imgContainer, aCanvas, aIsAccelerated);
  ImageCacheEntry* entry = gImageCache->mCache.PutEntry(canvasCacheKey);

  if (entry) {
    if (entry->mData->mSourceSurface) {
      // We are overwriting an existing entry.
      gImageCache->mTotal -= entry->mData->SizeInBytes();
      gImageCache->RemoveObject(entry->mData);
      gImageCache->mAllCanvasCache.RemoveEntry(allCanvasCacheKey);
    }

    gImageCache->AddObject(entry->mData);
    entry->mData->mSourceSurface = aSource;
    entry->mData->mSize = aSize;
    gImageCache->mTotal += entry->mData->SizeInBytes();

    AllCanvasImageCacheEntry* allEntry = gImageCache->mAllCanvasCache.PutEntry(allCanvasCacheKey);
    if (allEntry) {
      allEntry->mSourceSurface = aSource;
    }
  }

  if (!sCanvasImageCacheLimit)
    return;

  // Expire the image cache early if its larger than we want it to be.
  while (gImageCache->mTotal > size_t(sCanvasImageCacheLimit))
    gImageCache->AgeOneGeneration();
}

SourceSurface*
CanvasImageCache::LookupAllCanvas(Element* aImage,
                                  bool aIsAccelerated)
{
  if (!gImageCache) {
    return nullptr;
  }

  nsCOMPtr<imgIContainer> imgContainer = GetImageContainer(aImage);
  if (!imgContainer) {
    return nullptr;
  }

  AllCanvasImageCacheEntry* entry =
    gImageCache->mAllCanvasCache.GetEntry(AllCanvasImageCacheKey(imgContainer, aIsAccelerated));
  if (!entry) {
    return nullptr;
  }

  return entry->mSourceSurface;
}

SourceSurface*
CanvasImageCache::LookupCanvas(Element* aImage,
                               HTMLCanvasElement* aCanvas,
                               IntSize* aSizeOut,
                               bool aIsAccelerated)
{
  if (!gImageCache) {
    return nullptr;
  }

  nsCOMPtr<imgIContainer> imgContainer = GetImageContainer(aImage);
  if (!imgContainer) {
    return nullptr;
  }

  ImageCacheEntry* entry =
    gImageCache->mCache.GetEntry(ImageCacheKey(imgContainer, aCanvas, aIsAccelerated));
  if (!entry) {
    return nullptr;
  }

  MOZ_ASSERT(aSizeOut);

  gImageCache->MarkUsed(entry->mData);
  *aSizeOut = entry->mData->mSize;
  return entry->mData->mSourceSurface;
}


NS_IMPL_ISUPPORTS(CanvasImageCacheShutdownObserver, nsIObserver)

NS_IMETHODIMP
CanvasImageCacheShutdownObserver::Observe(nsISupports *aSubject,
                                          const char *aTopic,
                                          const char16_t *aData)
{
  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    delete gImageCache;
    gImageCache = nullptr;

    nsContentUtils::UnregisterShutdownObserver(this);
  }

  return NS_OK;
}

} // namespace mozilla
