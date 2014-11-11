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

namespace mozilla {

using namespace dom;
using namespace gfx;

struct ImageCacheKey {
  ImageCacheKey(Element* aImage, HTMLCanvasElement* aCanvas)
    : mImage(aImage), mCanvas(aCanvas) {}
  Element* mImage;
  HTMLCanvasElement* mCanvas;
};

struct ImageCacheEntryData {
  ImageCacheEntryData(const ImageCacheEntryData& aOther)
    : mImage(aOther.mImage)
    , mILC(aOther.mILC)
    , mCanvas(aOther.mCanvas)
    , mRequest(aOther.mRequest)
    , mSourceSurface(aOther.mSourceSurface)
    , mSize(aOther.mSize)
  {}
  explicit ImageCacheEntryData(const ImageCacheKey& aKey)
    : mImage(aKey.mImage)
    , mILC(nullptr)
    , mCanvas(aKey.mCanvas)
  {}

  nsExpirationState* GetExpirationState() { return &mState; }

  size_t SizeInBytes() { return mSize.width * mSize.height * 4; }

  // Key
  nsRefPtr<Element> mImage;
  nsIImageLoadingContent* mILC;
  nsRefPtr<HTMLCanvasElement> mCanvas;
  // Value
  nsCOMPtr<imgIRequest> mRequest;
  RefPtr<SourceSurface> mSourceSurface;
  gfxIntSize mSize;
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
    return mData->mImage == key->mImage && mData->mCanvas == key->mCanvas;
  }

  static KeyTypePointer KeyToPointer(KeyType& key) { return &key; }
  static PLDHashNumber HashKey(KeyTypePointer key)
  {
    return HashGeneric(key->mImage, key->mCanvas);
  }
  enum { ALLOW_MEMMOVE = true };

  nsAutoPtr<ImageCacheEntryData> mData;
};

class SimpleImageCacheEntry : public PLDHashEntryHdr {
public:
  typedef imgIRequest& KeyType;
  typedef const imgIRequest* KeyTypePointer;

  explicit SimpleImageCacheEntry(KeyTypePointer aKey)
    : mRequest(const_cast<imgIRequest*>(aKey))
  {}
  SimpleImageCacheEntry(const SimpleImageCacheEntry &toCopy)
    : mRequest(toCopy.mRequest)
    , mSourceSurface(toCopy.mSourceSurface)
  {}
  ~SimpleImageCacheEntry() {}

  bool KeyEquals(KeyTypePointer key) const
  {
    return key == mRequest;
  }

  static KeyTypePointer KeyToPointer(KeyType key) { return &key; }
  static PLDHashNumber HashKey(KeyTypePointer key)
  {
    return NS_PTR_TO_UINT32(key) >> 2;
  }
  enum { ALLOW_MEMMOVE = true };

  nsCOMPtr<imgIRequest> mRequest;
  RefPtr<SourceSurface> mSourceSurface;
};

static bool sPrefsInitialized = false;
static int32_t sCanvasImageCacheLimit = 0;

class ImageCacheObserver;

class ImageCache MOZ_FINAL : public nsExpirationTracker<ImageCacheEntryData,4> {
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
    mSimpleCache.RemoveEntry(*aObject->mRequest);
    mCache.RemoveEntry(ImageCacheKey(aObject->mImage, aObject->mCanvas));
  }

  nsTHashtable<ImageCacheEntry> mCache;
  nsTHashtable<SimpleImageCacheEntry> mSimpleCache;
  size_t mTotal;
  nsRefPtr<ImageCacheObserver> mImageCacheObserver;
};

static ImageCache* gImageCache = nullptr;

// Listen memory-pressure event for image cache purge
class ImageCacheObserver MOZ_FINAL : public nsIObserver
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
                        const char16_t* aSomeData)
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

class CanvasImageCacheShutdownObserver MOZ_FINAL : public nsIObserver
{
  ~CanvasImageCacheShutdownObserver() {}
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

ImageCache::ImageCache()
  : nsExpirationTracker<ImageCacheEntryData,4>(GENERATION_MS)
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
                                  const gfxIntSize& aSize)
{
  if (!gImageCache) {
    gImageCache = new ImageCache();
    nsContentUtils::RegisterShutdownObserver(new CanvasImageCacheShutdownObserver());
  }

  ImageCacheEntry* entry = gImageCache->mCache.PutEntry(ImageCacheKey(aImage, aCanvas));
  if (entry) {
    if (entry->mData->mSourceSurface) {
      // We are overwriting an existing entry.
      gImageCache->mTotal -= entry->mData->SizeInBytes();
      gImageCache->RemoveObject(entry->mData);
      gImageCache->mSimpleCache.RemoveEntry(*entry->mData->mRequest);
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
        gImageCache->mSimpleCache.PutEntry(*entry->mData->mRequest);
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
                         gfxIntSize* aSize)
{
  if (!gImageCache)
    return nullptr;

  ImageCacheEntry* entry = gImageCache->mCache.GetEntry(ImageCacheKey(aImage, aCanvas));
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
CanvasImageCache::SimpleLookup(Element* aImage)
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

  SimpleImageCacheEntry* entry = gImageCache->mSimpleCache.GetEntry(*request);
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
