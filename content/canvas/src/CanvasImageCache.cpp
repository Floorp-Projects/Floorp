/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan <robert@ocallahan.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "CanvasImageCache.h"
#include "nsIImageLoadingContent.h"
#include "nsExpirationTracker.h"
#include "imgIRequest.h"
#include "gfxASurface.h"
#include "gfxPoint.h"
#include "nsIDOMElement.h"
#include "nsTHashtable.h"
#include "nsHTMLCanvasElement.h"
#include "nsContentUtils.h"

namespace mozilla {

struct ImageCacheKey {
  ImageCacheKey(nsIDOMElement* aImage, nsHTMLCanvasElement* aCanvas)
    : mImage(aImage), mCanvas(aCanvas) {}
  nsIDOMElement* mImage;
  nsHTMLCanvasElement* mCanvas;
};

struct ImageCacheEntryData {
  ImageCacheEntryData(const ImageCacheEntryData& aOther)
    : mImage(aOther.mImage)
    , mILC(aOther.mILC)
    , mCanvas(aOther.mCanvas)
    , mRequest(aOther.mRequest)
    , mSurface(aOther.mSurface)
    , mSize(aOther.mSize)
  {}
  ImageCacheEntryData(const ImageCacheKey& aKey)
    : mImage(aKey.mImage)
    , mILC(nsnull)
    , mCanvas(aKey.mCanvas)
  {}

  nsExpirationState* GetExpirationState() { return &mState; }

  // Key
  nsCOMPtr<nsIDOMElement> mImage;
  nsIImageLoadingContent* mILC;
  nsRefPtr<nsHTMLCanvasElement> mCanvas;
  // Value
  nsCOMPtr<imgIRequest> mRequest;
  nsRefPtr<gfxASurface> mSurface;
  gfxIntSize mSize;
  nsExpirationState mState;
};

class ImageCacheEntry : public PLDHashEntryHdr {
public:
  typedef ImageCacheKey KeyType;
  typedef const ImageCacheKey* KeyTypePointer;

  ImageCacheEntry(const KeyType *key) :
      mData(new ImageCacheEntryData(*key)) {}
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
    return (NS_PTR_TO_INT32(key->mImage) ^ NS_PTR_TO_INT32(key->mCanvas)) >> 2;
  }
  enum { ALLOW_MEMMOVE = true };

  nsAutoPtr<ImageCacheEntryData> mData;
};

class ImageCache MOZ_FINAL : public nsExpirationTracker<ImageCacheEntryData,4> {
public:
  // We use 3 generations of 1 second each to get a 2-3 seconds timeout.
  enum { GENERATION_MS = 1000 };
  ImageCache()
    : nsExpirationTracker<ImageCacheEntryData,4>(GENERATION_MS)
  {
    mCache.Init();
  }
  ~ImageCache() {
    AgeAllGenerations();
  }

  virtual void NotifyExpired(ImageCacheEntryData* aObject)
  {
    RemoveObject(aObject);
    // Deleting the entry will delete aObject since the entry owns aObject
    mCache.RemoveEntry(ImageCacheKey(aObject->mImage, aObject->mCanvas));
  }

  nsTHashtable<ImageCacheEntry> mCache;
};

static ImageCache* gImageCache = nsnull;

class CanvasImageCacheShutdownObserver MOZ_FINAL : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

void
CanvasImageCache::NotifyDrawImage(nsIDOMElement* aImage,
                                  nsHTMLCanvasElement* aCanvas,
                                  imgIRequest* aRequest,
                                  gfxASurface* aSurface,
                                  const gfxIntSize& aSize)
{
  if (!gImageCache) {
    gImageCache = new ImageCache();
    nsContentUtils::RegisterShutdownObserver(new CanvasImageCacheShutdownObserver());
  }

  ImageCacheEntry* entry = gImageCache->mCache.PutEntry(ImageCacheKey(aImage, aCanvas));
  if (entry) {
    if (entry->mData->mSurface) {
      // We are overwriting an existing entry.
      gImageCache->RemoveObject(entry->mData);
    }
    gImageCache->AddObject(entry->mData);

    nsCOMPtr<nsIImageLoadingContent> ilc = do_QueryInterface(aImage);
    if (ilc) {
      ilc->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                      getter_AddRefs(entry->mData->mRequest));
    }
    entry->mData->mILC = ilc;
    entry->mData->mSurface = aSurface;
    entry->mData->mSize = aSize;
  }
}

gfxASurface*
CanvasImageCache::Lookup(nsIDOMElement* aImage,
                         nsHTMLCanvasElement* aCanvas,
                         gfxIntSize* aSize)
{
  if (!gImageCache)
    return nsnull;

  ImageCacheEntry* entry = gImageCache->mCache.GetEntry(ImageCacheKey(aImage, aCanvas));
  if (!entry || !entry->mData->mILC)
    return nsnull;

  nsCOMPtr<imgIRequest> request;
  entry->mData->mILC->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST, getter_AddRefs(request));
  if (request != entry->mData->mRequest)
    return nsnull;

  gImageCache->MarkUsed(entry->mData);

  *aSize = entry->mData->mSize;
  return entry->mData->mSurface;
}

NS_IMPL_ISUPPORTS1(CanvasImageCacheShutdownObserver, nsIObserver)

NS_IMETHODIMP
CanvasImageCacheShutdownObserver::Observe(nsISupports *aSubject,
                                          const char *aTopic,
                                          const PRUnichar *aData)
{
  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    delete gImageCache;
    gImageCache = nsnull;

    nsContentUtils::UnregisterShutdownObserver(this);
  }

  return NS_OK;
}

} // namespace mozilla
