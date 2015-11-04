/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasImageCache.h"
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

struct ImageCacheKey {
  ImageCacheKey(Element* aImage,
                HTMLCanvasElement* aCanvas,
                bool aIsAccelerated)
    : mImage(aImage)
    , mCanvas(aCanvas)
    , mIsAccelerated(aIsAccelerated)
  {}
  Element* mImage;
  HTMLCanvasElement* mCanvas;
  bool mIsAccelerated;
};

struct ImageCacheEntryData {
  ImageCacheEntryData(const ImageCacheEntryData& aOther)
    : mImage(aOther.mImage)
    , mILC(aOther.mILC)
    , mCanvas(aOther.mCanvas)
    , mIsAccelerated(aOther.mIsAccelerated)
    , mRequest(aOther.mRequest)
    , mSourceSurface(aOther.mSourceSurface)
    , mSize(aOther.mSize)
  {}
  explicit ImageCacheEntryData(const ImageCacheKey& aKey)
    : mImage(aKey.mImage)
    , mILC(nullptr)
    , mCanvas(aKey.mCanvas)
    , mIsAccelerated(aKey.mIsAccelerated)
  {}

  nsExpirationState* GetExpirationState() { return &mState; }

  size_t SizeInBytes() { return mSize.width * mSize.height * 4; }

  // Key
  RefPtr<Element> mImage;
  nsIImageLoadingContent* mILC;
  RefPtr<HTMLCanvasElement> mCanvas;
  bool mIsAccelerated;
  // Value
  nsCOMPtr<imgIRequest> mRequest;
  RefPtr<SourceSurface> mSourceSurface;
  IntSize mSize;
  nsExpirationState mState;
};

class ImageCacheEntry : public PLDHashEntryHdr {
public:
  typedef ImageCacheKey KeyType;
  typedef const ImageCacheKey* KeyTypePointer;

  explicit ImageCacheEntry(const KeyType* aKey) :
      mData(new ImageCacheEntryData(*aKey)) {}
  ImageCacheEntry(const ImageCacheEntry &toCopy) :
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
    return HashGeneric(key->mImage, key->mCanvas, key->mIsAccelerated);
  }
  enum { ALLOW_MEMMOVE = true };

  nsAutoPtr<ImageCacheEntryData> mData;
};

struct SimpleImageCacheKey {
  SimpleImageCacheKey(const imgIRequest* aImage,
                      bool aIsAccelerated)
    : mImage(aImage)
    , mIsAccelerated(aIsAccelerated)
  {}
  const imgIRequest* mImage;
  bool mIsAccelerated;
};

class SimpleImageCacheEntry : public PLDHashEntryHdr {
public:
  typedef SimpleImageCacheKey KeyType;
  typedef const SimpleImageCacheKey* KeyTypePointer;

  explicit SimpleImageCacheEntry(KeyTypePointer aKey)
    : mRequest(const_cast<imgIRequest*>(aKey->mImage))
    , mIsAccelerated(aKey->mIsAccelerated)
  {}
  SimpleImageCacheEntry(const SimpleImageCacheEntry &toCopy)
    : mRequest(toCopy.mRequest)
    , mIsAccelerated(toCopy.mIsAccelerated)
    , mSourceSurface(toCopy.mSourceSurface)
  {}
  ~SimpleImageCacheEntry() {}

  bool KeyEquals(KeyTypePointer key) const
  {
    return key->mImage == mRequest && key->mIsAccelerated == mIsAccelerated;
  }

  static KeyTypePointer KeyToPointer(KeyType& key) { return &key; }
  static PLDHashNumber HashKey(KeyTypePointer key)
  {
    return HashGeneric(key->mImage, key->mIsAccelerated);
  }
  enum { ALLOW_MEMMOVE = true };

  nsCOMPtr<imgIRequest> mRequest;
  bool mIsAccelerated;
  RefPtr<SourceSurface> mSourceSurface;
};

static bool sPrefsInitialized = false;
static int32_t sCanvasImageCacheLimit = 0;

class ImageCacheObserver;

class ImageCache final : public nsExpirationTracker<ImageCacheEntryData,4> {
public:
  // We use 3 generations of 1 second each to get a 2-3 seconds timeout.
  enum { GENERATION_MS = 1000 };
  ImageCache();
  ~ImageCache();

  virtual void NotifyExpired(ImageCacheEntryData* aObject)
  {
    mTotal -= aObject->SizeInBytes();
    RemoveObject(aObject);
    // Deleting the entry will delete aObject since the entry owns aObject
    mSimpleCache.RemoveEntry(SimpleImageCacheKey(aObject->mRequest, aObject->mIsAccelerated));
    mCache.RemoveEntry(ImageCacheKey(aObject->mImage, aObject->mCanvas, aObject->mIsAccelerated));
  }

  nsTHashtable<ImageCacheEntry> mCache;
  nsTHashtable<SimpleImageCacheEntry> mSimpleCache;
  size_t mTotal;
  RefPtr<ImageCacheObserver> mImageCacheObserver;
};

static ImageCache* gImageCache = nullptr;

// Listen memory-pressure event for image cache purge
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
  MOZ_RELEASE_ASSERT(mImageCacheObserver, "Can't alloc ImageCacheObserver");
}

ImageCache::~ImageCache() {
  AgeAllGenerations();
  mImageCacheObserver->Destroy();
}

void
CanvasImageCache::NotifyDrawImage(Element* aImage,
                                  HTMLCanvasElement* aCanvas,
                                  imgIRequest* aRequest,
                                  SourceSurface* aSource,
                                  const IntSize& aSize,
                                  bool aIsAccelerated)
{
  if (!gImageCache) {
    gImageCache = new ImageCache();
    nsContentUtils::RegisterShutdownObserver(new CanvasImageCacheShutdownObserver());
  }

  ImageCacheEntry* entry =
    gImageCache->mCache.PutEntry(ImageCacheKey(aImage, aCanvas, aIsAccelerated));
  if (entry) {
    if (entry->mData->mSourceSurface) {
      // We are overwriting an existing entry.
      gImageCache->mTotal -= entry->mData->SizeInBytes();
      gImageCache->RemoveObject(entry->mData);
      gImageCache->mSimpleCache.RemoveEntry(SimpleImageCacheKey(entry->mData->mRequest, entry->mData->mIsAccelerated));
    }
    gImageCache->AddObject(entry->mData);

    nsCOMPtr<nsIImageLoadingContent> ilc = do_QueryInterface(aImage);
    if (ilc) {
      ilc->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                      getter_AddRefs(entry->mData->mRequest));
    }
    entry->mData->mILC = ilc;
    entry->mData->mSourceSurface = aSource;
    entry->mData->mSize = aSize;

    gImageCache->mTotal += entry->mData->SizeInBytes();

    if (entry->mData->mRequest) {
      SimpleImageCacheEntry* simpleentry =
        gImageCache->mSimpleCache.PutEntry(SimpleImageCacheKey(entry->mData->mRequest, aIsAccelerated));
      simpleentry->mSourceSurface = aSource;
    }
  }

  if (!sCanvasImageCacheLimit)
    return;

  // Expire the image cache early if its larger than we want it to be.
  while (gImageCache->mTotal > size_t(sCanvasImageCacheLimit))
    gImageCache->AgeOneGeneration();
}

SourceSurface*
CanvasImageCache::Lookup(Element* aImage,
                         HTMLCanvasElement* aCanvas,
                         gfx::IntSize* aSize,
                         bool aIsAccelerated)
{
  if (!gImageCache)
    return nullptr;

  ImageCacheEntry* entry =
    gImageCache->mCache.GetEntry(ImageCacheKey(aImage, aCanvas, aIsAccelerated));
  if (!entry || !entry->mData->mILC)
    return nullptr;

  nsCOMPtr<imgIRequest> request;
  entry->mData->mILC->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST, getter_AddRefs(request));
  if (request != entry->mData->mRequest)
    return nullptr;

  gImageCache->MarkUsed(entry->mData);

  *aSize = entry->mData->mSize;
  return entry->mData->mSourceSurface;
}

SourceSurface*
CanvasImageCache::SimpleLookup(Element* aImage,
                               bool aIsAccelerated)
{
  if (!gImageCache)
    return nullptr;

  nsCOMPtr<imgIRequest> request;
  nsCOMPtr<nsIImageLoadingContent> ilc = do_QueryInterface(aImage);
  if (!ilc)
    return nullptr;

  ilc->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                  getter_AddRefs(request));
  if (!request)
    return nullptr;

  SimpleImageCacheEntry* entry = gImageCache->mSimpleCache.GetEntry(SimpleImageCacheKey(request, aIsAccelerated));
  if (!entry)
    return nullptr;

  return entry->mSourceSurface;
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
