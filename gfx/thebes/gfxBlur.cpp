/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxBlur.h"
#include "gfxContext.h"
#include "gfxPlatform.h"

#include "mozilla/gfx/Blur.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/UniquePtr.h"
#include "nsExpirationTracker.h"
#include "nsClassHashtable.h"

using namespace mozilla;
using namespace mozilla::gfx;

gfxAlphaBoxBlur::gfxAlphaBoxBlur()
{
}

gfxAlphaBoxBlur::~gfxAlphaBoxBlur()
{
  mContext = nullptr;
}

gfxContext*
gfxAlphaBoxBlur::Init(const gfxRect& aRect,
                      const gfxIntSize& aSpreadRadius,
                      const gfxIntSize& aBlurRadius,
                      const gfxRect* aDirtyRect,
                      const gfxRect* aSkipRect)
{
    mozilla::gfx::Rect rect(Float(aRect.x), Float(aRect.y),
                            Float(aRect.width), Float(aRect.height));
    IntSize spreadRadius(aSpreadRadius.width, aSpreadRadius.height);
    IntSize blurRadius(aBlurRadius.width, aBlurRadius.height);
    UniquePtr<Rect> dirtyRect;
    if (aDirtyRect) {
      dirtyRect = MakeUnique<Rect>(Float(aDirtyRect->x),
                                   Float(aDirtyRect->y),
                                   Float(aDirtyRect->width),
                                   Float(aDirtyRect->height));
    }
    UniquePtr<Rect> skipRect;
    if (aSkipRect) {
      skipRect = MakeUnique<Rect>(Float(aSkipRect->x),
                                  Float(aSkipRect->y),
                                  Float(aSkipRect->width),
                                  Float(aSkipRect->height));
    }

    mBlur = MakeUnique<AlphaBoxBlur>(rect, spreadRadius, blurRadius, dirtyRect.get(), skipRect.get());
    int32_t blurDataSize = mBlur->GetSurfaceAllocationSize();
    if (blurDataSize <= 0)
        return nullptr;

    IntSize size = mBlur->GetSize();

    // Make an alpha-only surface to draw on. We will play with the data after
    // everything is drawn to create a blur effect.
    mData = new unsigned char[blurDataSize];
    memset(mData, 0, blurDataSize);

    mozilla::RefPtr<DrawTarget> dt =
        gfxPlatform::GetPlatform()->CreateDrawTargetForData(mData, size,
                                                            mBlur->GetStride(),
                                                            SurfaceFormat::A8);
    if (!dt) {
        return nullptr;
    }

    IntRect irect = mBlur->GetRect();
    gfxPoint topleft(irect.TopLeft().x, irect.TopLeft().y);

    mContext = new gfxContext(dt);
    mContext->Translate(-topleft);

    return mContext;
}

void
DrawBlur(gfxContext* aDestinationCtx,
         SourceSurface* aBlur,
         const IntPoint& aTopLeft,
         const Rect* aDirtyRect)
{
    DrawTarget *dest = aDestinationCtx->GetDrawTarget();

    nsRefPtr<gfxPattern> thebesPat = aDestinationCtx->GetPattern();
    Pattern* pat = thebesPat->GetPattern(dest, nullptr);

    Matrix oldTransform = dest->GetTransform();
    Matrix newTransform = oldTransform;
    newTransform.PreTranslate(aTopLeft.x, aTopLeft.y);

    // Avoid a semi-expensive clip operation if we can, otherwise
    // clip to the dirty rect
    if (aDirtyRect) {
        dest->PushClipRect(*aDirtyRect);
    }

    dest->SetTransform(newTransform);
    dest->MaskSurface(*pat, aBlur, Point(0, 0));
    dest->SetTransform(oldTransform);

    if (aDirtyRect) {
        dest->PopClip();
    }
}

TemporaryRef<SourceSurface>
gfxAlphaBoxBlur::DoBlur(DrawTarget* aDT, IntPoint* aTopLeft)
{
    mBlur->Blur(mData);

    *aTopLeft = mBlur->GetRect().TopLeft();

    return aDT->CreateSourceSurfaceFromData(mData,
                                            mBlur->GetSize(),
                                            mBlur->GetStride(),
                                            SurfaceFormat::A8);
}

void
gfxAlphaBoxBlur::Paint(gfxContext* aDestinationCtx)
{
    if (!mContext)
        return;

    DrawTarget *dest = aDestinationCtx->GetDrawTarget();
    if (!dest) {
      NS_WARNING("Blurring not supported for Thebes contexts!");
      return;
    }

    Rect* dirtyRect = mBlur->GetDirtyRect();

    IntPoint topLeft;
    RefPtr<SourceSurface> mask = DoBlur(dest, &topLeft);
    if (!mask) {
      NS_ERROR("Failed to create mask!");
      return;
    }

    DrawBlur(aDestinationCtx, mask, topLeft, dirtyRect);
}

gfxIntSize gfxAlphaBoxBlur::CalculateBlurRadius(const gfxPoint& aStd)
{
    mozilla::gfx::Point std(Float(aStd.x), Float(aStd.y));
    IntSize size = AlphaBoxBlur::CalculateBlurRadius(std);
    return gfxIntSize(size.width, size.height);
}

struct BlurCacheKey : public PLDHashEntryHdr {
  typedef const BlurCacheKey& KeyType;
  typedef const BlurCacheKey* KeyTypePointer;
  enum { ALLOW_MEMMOVE = true };

  gfxRect mRect;
  gfxIntSize mBlurRadius;
  gfxRect mSkipRect;
  BackendType mBackend;

  BlurCacheKey(const gfxRect& aRect, const gfxIntSize &aBlurRadius, const gfxRect& aSkipRect, BackendType aBackend)
    : mRect(aRect)
    , mBlurRadius(aBlurRadius)
    , mSkipRect(aSkipRect)
    , mBackend(aBackend)
  { }

  explicit BlurCacheKey(const BlurCacheKey* aOther)
    : mRect(aOther->mRect)
    , mBlurRadius(aOther->mBlurRadius)
    , mSkipRect(aOther->mSkipRect)
    , mBackend(aOther->mBackend)
  { }

  static PLDHashNumber
  HashKey(const KeyTypePointer aKey)
  {
    PLDHashNumber hash = HashBytes(&aKey->mRect.x, 4 * sizeof(gfxFloat));
    hash = AddToHash(hash, aKey->mBlurRadius.width, aKey->mBlurRadius.height);
    hash = AddToHash(hash, HashBytes(&aKey->mSkipRect.x, 4 * sizeof(gfxFloat)));
    hash = AddToHash(hash, (uint32_t)aKey->mBackend);
    return hash;
  }

  bool KeyEquals(KeyTypePointer aKey) const
  {
    if (aKey->mRect.IsEqualInterior(mRect) &&
        aKey->mBlurRadius == mBlurRadius &&
        aKey->mSkipRect.IsEqualInterior(mSkipRect) &&
        aKey->mBackend == mBackend) {
      return true;
    }
    return false;
  }
  static KeyTypePointer KeyToPointer(KeyType aKey)
  {
    return &aKey;
  }
};

/**
 * This class is what is cached. It need to be allocated in an object separated
 * to the cache entry to be able to be tracked by the nsExpirationTracker.
 * */
struct BlurCacheData {
  BlurCacheData(SourceSurface* aBlur, const IntPoint& aTopLeft, const gfxRect& aDirtyRect, const BlurCacheKey& aKey)
    : mBlur(aBlur)
    , mTopLeft(aTopLeft)
    , mDirtyRect(aDirtyRect)
    , mKey(aKey)
  {}

  BlurCacheData(const BlurCacheData& aOther)
    : mBlur(aOther.mBlur)
    , mTopLeft(aOther.mTopLeft)
    , mDirtyRect(aOther.mDirtyRect)
    , mKey(aOther.mKey)
  { }

  nsExpirationState *GetExpirationState() {
    return &mExpirationState;
  }

  nsExpirationState mExpirationState;
  RefPtr<SourceSurface> mBlur;
  IntPoint mTopLeft;
  gfxRect mDirtyRect;
  BlurCacheKey mKey;
};

/**
 * This class implements a cache with no maximum size, that retains the
 * SourceSurfaces used to draw the blurs.
 *
 * An entry stays in the cache as long as it is used often.
 */
class BlurCache MOZ_FINAL : public nsExpirationTracker<BlurCacheData,4>
{
  public:
    BlurCache()
      : nsExpirationTracker<BlurCacheData, 4>(GENERATION_MS)
    {
    }

    virtual void NotifyExpired(BlurCacheData* aObject)
    {
      RemoveObject(aObject);
      mHashEntries.Remove(aObject->mKey);
    }

    BlurCacheData* Lookup(const gfxRect& aRect,
                          const gfxIntSize& aBlurRadius,
                          const gfxRect& aSkipRect,
                          BackendType aBackendType,
                          const gfxRect* aDirtyRect)
    {
      BlurCacheData* blur =
        mHashEntries.Get(BlurCacheKey(aRect, aBlurRadius, aSkipRect, aBackendType));

      if (blur) {
        if (aDirtyRect && !blur->mDirtyRect.Contains(*aDirtyRect)) {
          return nullptr;
        }
        MarkUsed(blur);
      }

      return blur;
    }

    // Returns true if we successfully register the blur in the cache, false
    // otherwise.
    bool RegisterEntry(BlurCacheData* aValue)
    {
      nsresult rv = AddObject(aValue);
      if (NS_FAILED(rv)) {
        // We are OOM, and we cannot track this object. We don't want stall
        // entries in the hash table (since the expiration tracker is responsible
        // for removing the cache entries), so we avoid putting that entry in the
        // table, which is a good things considering we are short on memory
        // anyway, we probably don't want to retain things.
        return false;
      }
      mHashEntries.Put(aValue->mKey, aValue);
      return true;
    }

  protected:
    static const uint32_t GENERATION_MS = 1000;
    /**
     * FIXME use nsTHashtable to avoid duplicating the BlurCacheKey.
     * https://bugzilla.mozilla.org/show_bug.cgi?id=761393#c47
     */
    nsClassHashtable<BlurCacheKey, BlurCacheData> mHashEntries;
};

static BlurCache* gBlurCache = nullptr;

SourceSurface*
GetCachedBlur(DrawTarget *aDT,
              const gfxRect& aRect,
              const gfxIntSize& aBlurRadius,
              const gfxRect& aSkipRect,
              const gfxRect& aDirtyRect,
              IntPoint* aTopLeft)
{
  if (!gBlurCache) {
    gBlurCache = new BlurCache();
  }
  BlurCacheData* cached = gBlurCache->Lookup(aRect, aBlurRadius, aSkipRect,
                                             aDT->GetBackendType(),
                                             &aDirtyRect);
  if (cached) {
    *aTopLeft = cached->mTopLeft;
    return cached->mBlur;
  }
  return nullptr;
}

void
CacheBlur(DrawTarget *aDT,
          const gfxRect& aRect,
          const gfxIntSize& aBlurRadius,
          const gfxRect& aSkipRect,
          SourceSurface* aBlur,
          const IntPoint& aTopLeft,
          const gfxRect& aDirtyRect)
{
  // If we already had a cached value with this key, but an incorrect dirty region then just update
  // the existing entry
  if (BlurCacheData* cached = gBlurCache->Lookup(aRect, aBlurRadius, aSkipRect,
                                                 aDT->GetBackendType(),
                                                 nullptr)) {
    cached->mBlur = aBlur;
    cached->mTopLeft = aTopLeft;
    cached->mDirtyRect = aDirtyRect;
    return;
  }

  BlurCacheKey key(aRect, aBlurRadius, aSkipRect, aDT->GetBackendType());
  BlurCacheData* data = new BlurCacheData(aBlur, aTopLeft, aDirtyRect, key);
  if (!gBlurCache->RegisterEntry(data)) {
    delete data;
  }
}

void
gfxAlphaBoxBlur::ShutdownBlurCache()
{
  delete gBlurCache;
  gBlurCache = nullptr;
}

/* static */ void
gfxAlphaBoxBlur::BlurRectangle(gfxContext *aDestinationCtx,
                               const gfxRect& aRect,
                               gfxCornerSizes* aCornerRadii,
                               const gfxPoint& aBlurStdDev,
                               const gfxRGBA& aShadowColor,
                               const gfxRect& aDirtyRect,
                               const gfxRect& aSkipRect)
{
  gfxIntSize blurRadius = CalculateBlurRadius(aBlurStdDev);
    
  DrawTarget *dt = aDestinationCtx->GetDrawTarget();
  if (!dt) {
    NS_WARNING("Blurring not supported for Thebes contexts!");
    return;
  }

  IntPoint topLeft;
  RefPtr<SourceSurface> surface = GetCachedBlur(dt, aRect, blurRadius, aSkipRect, aDirtyRect, &topLeft);
  if (!surface) {
    // Create the temporary surface for blurring
    gfxAlphaBoxBlur blur;
    gfxContext *dest = blur.Init(aRect, gfxIntSize(), blurRadius, &aDirtyRect, &aSkipRect);

    if (!dest) {
      return;
    }

    gfxRect shadowGfxRect = aRect;
    shadowGfxRect.Round();

    dest->NewPath();
    if (aCornerRadii) {
      dest->RoundedRectangle(shadowGfxRect, *aCornerRadii);
    } else {
      dest->Rectangle(shadowGfxRect);
    }
    dest->Fill();

    surface = blur.DoBlur(dt, &topLeft);
    if (!surface) {
      return;
    }
    CacheBlur(dt, aRect, blurRadius, aSkipRect, surface, topLeft, aDirtyRect);
  }

  aDestinationCtx->SetColor(aShadowColor);
  Rect dirtyRect(aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
  DrawBlur(aDestinationCtx, surface, topLeft, &dirtyRect);
}

