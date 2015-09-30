/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxBlur.h"

#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Blur.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/UniquePtr.h"
#include "nsExpirationTracker.h"
#include "nsClassHashtable.h"
#include "gfxUtils.h"

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
                      const IntSize& aSpreadRadius,
                      const IntSize& aBlurRadius,
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
    size_t blurDataSize = mBlur->GetSurfaceAllocationSize();
    if (blurDataSize == 0)
        return nullptr;

    IntSize size = mBlur->GetSize();

    // Make an alpha-only surface to draw on. We will play with the data after
    // everything is drawn to create a blur effect.
    mData = new (std::nothrow) unsigned char[blurDataSize];
    if (!mData) {
        return nullptr;
    }
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
    mContext->SetMatrix(gfxMatrix::Translation(-topleft));

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

already_AddRefed<SourceSurface>
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

IntSize gfxAlphaBoxBlur::CalculateBlurRadius(const gfxPoint& aStd)
{
    mozilla::gfx::Point std(Float(aStd.x), Float(aStd.y));
    IntSize size = AlphaBoxBlur::CalculateBlurRadius(std);
    return IntSize(size.width, size.height);
}

struct BlurCacheKey : public PLDHashEntryHdr {
  typedef const BlurCacheKey& KeyType;
  typedef const BlurCacheKey* KeyTypePointer;
  enum { ALLOW_MEMMOVE = true };

  IntSize mMinSize;
  IntSize mBlurRadius;
  Color mShadowColor;
  BackendType mBackend;
  RectCornerRadii mCornerRadii;
  bool mIsInset;

  // Only used for inset blurs
  bool mHasBorderRadius;
  IntSize mSpreadRadius;
  IntSize mInnerMinSize;

  BlurCacheKey(IntSize aMinSize, IntSize aBlurRadius,
               RectCornerRadii* aCornerRadii, const Color& aShadowColor,
               BackendType aBackendType)
    : BlurCacheKey(aMinSize, IntSize(0, 0),
                   aBlurRadius, IntSize(0, 0),
                   aCornerRadii, aShadowColor,
                   false, false, aBackendType)
  {}

  explicit BlurCacheKey(const BlurCacheKey* aOther)
    : mMinSize(aOther->mMinSize)
    , mBlurRadius(aOther->mBlurRadius)
    , mShadowColor(aOther->mShadowColor)
    , mBackend(aOther->mBackend)
    , mCornerRadii(aOther->mCornerRadii)
    , mIsInset(aOther->mIsInset)
    , mHasBorderRadius(aOther->mHasBorderRadius)
    , mSpreadRadius(aOther->mSpreadRadius)
    , mInnerMinSize(aOther->mInnerMinSize)
  { }

  explicit BlurCacheKey(IntSize aOuterMinSize, IntSize aInnerMinSize,
                        IntSize aBlurRadius, IntSize aSpreadRadius,
                        const RectCornerRadii* aCornerRadii,
                        const Color& aShadowColor, bool aIsInset,
                        bool aHasBorderRadius, BackendType aBackendType)
    : mMinSize(aOuterMinSize)
    , mBlurRadius(aBlurRadius)
    , mShadowColor(aShadowColor)
    , mBackend(aBackendType)
    , mCornerRadii(aCornerRadii ? *aCornerRadii : RectCornerRadii())
    , mIsInset(aIsInset)
    , mHasBorderRadius(aHasBorderRadius)
    , mSpreadRadius(aSpreadRadius)
    , mInnerMinSize(aInnerMinSize)
  { }

  static PLDHashNumber
  HashKey(const KeyTypePointer aKey)
  {
    PLDHashNumber hash = 0;
    hash = AddToHash(hash, aKey->mMinSize.width, aKey->mMinSize.height);
    hash = AddToHash(hash, aKey->mBlurRadius.width, aKey->mBlurRadius.height);

    hash = AddToHash(hash, HashBytes(&aKey->mShadowColor.r,
                                     sizeof(aKey->mShadowColor.r)));
    hash = AddToHash(hash, HashBytes(&aKey->mShadowColor.g,
                                     sizeof(aKey->mShadowColor.g)));
    hash = AddToHash(hash, HashBytes(&aKey->mShadowColor.b,
                                     sizeof(aKey->mShadowColor.b)));
    hash = AddToHash(hash, HashBytes(&aKey->mShadowColor.a,
                                     sizeof(aKey->mShadowColor.a)));

    for (int i = 0; i < 4; i++) {
      hash = AddToHash(hash, aKey->mCornerRadii[i].width, aKey->mCornerRadii[i].height);
    }

    hash = AddToHash(hash, (uint32_t)aKey->mBackend);

    if (aKey->mIsInset) {
      hash = AddToHash(hash, aKey->mSpreadRadius.width, aKey->mSpreadRadius.height);
      hash = AddToHash(hash, aKey->mInnerMinSize.width, aKey->mInnerMinSize.height);
      hash = AddToHash(hash, HashBytes(&aKey->mHasBorderRadius, sizeof(bool)));
    }
    return hash;
  }

  bool
  KeyEquals(KeyTypePointer aKey) const
  {
    if (aKey->mMinSize == mMinSize &&
        aKey->mBlurRadius == mBlurRadius &&
        aKey->mCornerRadii == mCornerRadii &&
        aKey->mShadowColor == mShadowColor &&
        aKey->mBackend == mBackend) {

      if (mIsInset) {
        return (mHasBorderRadius == aKey->mHasBorderRadius) &&
                (mInnerMinSize == aKey->mInnerMinSize) &&
                (mSpreadRadius == aKey->mSpreadRadius);
      }

      return true;
     }

     return false;
  }

  static KeyTypePointer
  KeyToPointer(KeyType aKey)
  {
    return &aKey;
  }
};

/**
 * This class is what is cached. It need to be allocated in an object separated
 * to the cache entry to be able to be tracked by the nsExpirationTracker.
 * */
struct BlurCacheData {
  BlurCacheData(SourceSurface* aBlur, IntMargin aExtendDestBy, const BlurCacheKey& aKey)
    : mBlur(aBlur)
    , mExtendDest(aExtendDestBy)
    , mKey(aKey)
  {}

  BlurCacheData(const BlurCacheData& aOther)
    : mBlur(aOther.mBlur)
    , mExtendDest(aOther.mExtendDest)
    , mKey(aOther.mKey)
  { }

  nsExpirationState *GetExpirationState() {
    return &mExpirationState;
  }

  nsExpirationState mExpirationState;
  RefPtr<SourceSurface> mBlur;
  IntMargin mExtendDest;
  BlurCacheKey mKey;
};

/**
 * This class implements a cache with no maximum size, that retains the
 * SourceSurfaces used to draw the blurs.
 *
 * An entry stays in the cache as long as it is used often.
 */
class BlurCache final : public nsExpirationTracker<BlurCacheData,4>
{
  public:
    BlurCache()
      : nsExpirationTracker<BlurCacheData, 4>(GENERATION_MS, "BlurCache")
    {
    }

    virtual void NotifyExpired(BlurCacheData* aObject)
    {
      RemoveObject(aObject);
      mHashEntries.Remove(aObject->mKey);
    }

    BlurCacheData* Lookup(const IntSize aMinSize,
                          const IntSize& aBlurRadius,
                          RectCornerRadii* aCornerRadii,
                          const Color& aShadowColor,
                          BackendType aBackendType)
    {
      BlurCacheData* blur =
        mHashEntries.Get(BlurCacheKey(aMinSize, aBlurRadius,
                                      aCornerRadii, aShadowColor,
                                      aBackendType));
      if (blur) {
        MarkUsed(blur);
      }

      return blur;
    }

    BlurCacheData* LookupInsetBoxShadow(const IntSize aOuterMinSize,
                                        const IntSize aInnerMinSize,
                                        const IntSize& aBlurRadius,
                                        const IntSize& aSpreadRadius,
                                        const RectCornerRadii* aCornerRadii,
                                        const Color& aShadowColor,
                                        const bool& aHasBorderRadius,
                                        BackendType aBackendType)
    {
      BlurCacheKey key(aOuterMinSize, aInnerMinSize,
                       aBlurRadius, aSpreadRadius,
                       aCornerRadii, aShadowColor,
                       true, aHasBorderRadius, aBackendType);
      BlurCacheData* blur = mHashEntries.Get(key);
      if (blur) {
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

static IntSize
ComputeMinSizeForShadowShape(RectCornerRadii* aCornerRadii,
                             IntSize aBlurRadius,
                             IntMargin& aSlice,
                             const IntSize& aRectSize)
{
  float cornerWidth = 0;
  float cornerHeight = 0;
  if (aCornerRadii) {
    RectCornerRadii corners = *aCornerRadii;
    for (size_t i = 0; i < 4; i++) {
      cornerWidth = std::max(cornerWidth, corners[i].width);
      cornerHeight = std::max(cornerHeight, corners[i].height);
    }
  }

  aSlice = IntMargin(ceil(cornerHeight) + aBlurRadius.height,
                     ceil(cornerWidth) + aBlurRadius.width,
                     ceil(cornerHeight) + aBlurRadius.height,
                     ceil(cornerWidth) + aBlurRadius.width);

  IntSize minSize(aSlice.LeftRight() + 1,
                      aSlice.TopBottom() + 1);

  // If aRectSize is smaller than minSize, the border-image approach won't
  // work; there's no way to squeeze parts of the min box-shadow source
  // image such that the result looks correct. So we need to adjust minSize
  // in such a way that we can later draw it without stretching in the affected
  // dimension. We also need to adjust "slice" to ensure that we're not trying
  // to slice away more than we have.
  if (aRectSize.width < minSize.width) {
    minSize.width = aRectSize.width;
    aSlice.left = 0;
    aSlice.right = 0;
  }
  if (aRectSize.height < minSize.height) {
    minSize.height = aRectSize.height;
    aSlice.top = 0;
    aSlice.bottom = 0;
  }

  MOZ_ASSERT(aSlice.LeftRight() <= minSize.width);
  MOZ_ASSERT(aSlice.TopBottom() <= minSize.height);
  return minSize;
}

void
CacheBlur(DrawTarget& aDT,
          const IntSize& aMinSize,
          const IntSize& aBlurRadius,
          RectCornerRadii* aCornerRadii,
          const Color& aShadowColor,
          IntMargin aExtendDest,
          SourceSurface* aBoxShadow)
{
  BlurCacheKey key(aMinSize, aBlurRadius, aCornerRadii, aShadowColor, aDT.GetBackendType());
  BlurCacheData* data = new BlurCacheData(aBoxShadow, aExtendDest, key);
  if (!gBlurCache->RegisterEntry(data)) {
    delete data;
  }
}

// Blurs a small surface and creates the mask.
static already_AddRefed<SourceSurface>
CreateBlurMask(const IntSize& aRectSize,
               RectCornerRadii* aCornerRadii,
               IntSize aBlurRadius,
               IntMargin& aExtendDestBy,
               IntMargin& aSliceBorder,
               DrawTarget& aDestDrawTarget)
{
  IntMargin slice;
  gfxAlphaBoxBlur blur;
  IntSize minSize =
    ComputeMinSizeForShadowShape(aCornerRadii, aBlurRadius, slice, aRectSize);
  IntRect minRect(IntPoint(), minSize);

  gfxContext* blurCtx = blur.Init(ThebesRect(Rect(minRect)), IntSize(),
                                  aBlurRadius, nullptr, nullptr);

  if (!blurCtx) {
    return nullptr;
  }

  DrawTarget* blurDT = blurCtx->GetDrawTarget();
  ColorPattern black(Color(0.f, 0.f, 0.f, 1.f));

  if (aCornerRadii) {
    RefPtr<Path> roundedRect =
      MakePathForRoundedRect(*blurDT, Rect(minRect), *aCornerRadii);
    blurDT->Fill(roundedRect, black);
  } else {
    blurDT->FillRect(Rect(minRect), black);
  }

  IntPoint topLeft;
  RefPtr<SourceSurface> result = blur.DoBlur(&aDestDrawTarget, &topLeft);
  if (!result) {
    return nullptr;
  }

  IntRect expandedMinRect(topLeft, result->GetSize());
  aExtendDestBy = expandedMinRect - minRect;
  aSliceBorder = slice + aExtendDestBy;

  MOZ_ASSERT(aSliceBorder.LeftRight() <= expandedMinRect.width);
  MOZ_ASSERT(aSliceBorder.TopBottom() <= expandedMinRect.height);

  return result.forget();
}

static already_AddRefed<SourceSurface>
CreateBoxShadow(SourceSurface* aBlurMask, const Color& aShadowColor)
{
  IntSize blurredSize = aBlurMask->GetSize();
  gfxPlatform* platform = gfxPlatform::GetPlatform();
  RefPtr<DrawTarget> boxShadowDT =
    platform->CreateOffscreenContentDrawTarget(blurredSize, SurfaceFormat::B8G8R8A8);

  if (!boxShadowDT) {
    return nullptr;
  }

  ColorPattern shadowColor(ToDeviceColor(aShadowColor));
  boxShadowDT->MaskSurface(shadowColor, aBlurMask, Point(0, 0));
  return boxShadowDT->Snapshot();
}

static SourceSurface*
GetBlur(DrawTarget& aDT,
        const IntSize& aRectSize,
        const IntSize& aBlurRadius,
        RectCornerRadii* aCornerRadii,
        const Color& aShadowColor,
        IntMargin& aExtendDestBy,
        IntMargin& aSlice)
{
  if (!gBlurCache) {
    gBlurCache = new BlurCache();
  }

  IntSize minSize =
    ComputeMinSizeForShadowShape(aCornerRadii, aBlurRadius, aSlice, aRectSize);

  BlurCacheData* cached = gBlurCache->Lookup(minSize, aBlurRadius,
                                             aCornerRadii, aShadowColor,
                                             aDT.GetBackendType());
  if (cached) {
    // See CreateBlurMask() for these values
    aExtendDestBy = cached->mExtendDest;
    aSlice = aSlice + aExtendDestBy;
    return cached->mBlur;
  }

  RefPtr<SourceSurface> blurMask =
    CreateBlurMask(aRectSize, aCornerRadii, aBlurRadius, aExtendDestBy, aSlice, aDT);

  if (!blurMask) {
    return nullptr;
  }

  RefPtr<SourceSurface> boxShadow = CreateBoxShadow(blurMask, aShadowColor);
  if (!boxShadow) {
    return nullptr;
  }

  CacheBlur(aDT, minSize, aBlurRadius, aCornerRadii, aShadowColor, aExtendDestBy, boxShadow);
  return boxShadow;
}

void
gfxAlphaBoxBlur::ShutdownBlurCache()
{
  delete gBlurCache;
  gBlurCache = nullptr;
}

static Rect
RectWithEdgesTRBL(Float aTop, Float aRight, Float aBottom, Float aLeft)
{
  return Rect(aLeft, aTop, aRight - aLeft, aBottom - aTop);
}

static void
RepeatOrStretchSurface(DrawTarget& aDT, SourceSurface* aSurface,
                       const Rect& aDest, const Rect& aSrc, Rect& aSkipRect)
{
  if (aSkipRect.Contains(aDest)) {
    return;
  }

  if ((!aDT.GetTransform().IsRectilinear() &&
       aDT.GetBackendType() != BackendType::CAIRO) ||
      (aDT.GetBackendType() == BackendType::DIRECT2D)) {
    // Use stretching if possible, since it leads to less seams when the
    // destination is transformed. However, don't do this if we're using cairo,
    // because if cairo is using pixman it won't render anything for large
    // stretch factors because pixman's internal fixed point precision is not
    // high enough to handle those scale factors.
    // Calling FillRect on a D2D backend with a repeating pattern is much slower
    // than DrawSurface, so special case the D2D backend here.
    aDT.DrawSurface(aSurface, aDest, aSrc);
    return;
  }

  SurfacePattern pattern(aSurface, ExtendMode::REPEAT,
                         Matrix::Translation(aDest.TopLeft() - aSrc.TopLeft()),
                         Filter::GOOD, RoundedToInt(aSrc));
  aDT.FillRect(aDest, pattern);
}

static void
DrawCorner(DrawTarget& aDT, SourceSurface* aSurface,
           const Rect& aDest, const Rect& aSrc, Rect& aSkipRect)
{
  if (aSkipRect.Contains(aDest)) {
    return;
  }

  aDT.DrawSurface(aSurface, aDest, aSrc);
}

static void
DrawBoxShadows(DrawTarget& aDestDrawTarget, SourceSurface* aSourceBlur,
               Rect aDstOuter, Rect aDstInner, Rect aSrcOuter, Rect aSrcInner,
               Rect aSkipRect)
{
  // Corners: top left, top right, bottom left, bottom right
  DrawCorner(aDestDrawTarget, aSourceBlur,
             RectWithEdgesTRBL(aDstOuter.Y(), aDstInner.X(),
                               aDstInner.Y(), aDstOuter.X()),
             RectWithEdgesTRBL(aSrcOuter.Y(), aSrcInner.X(),
                               aSrcInner.Y(), aSrcOuter.X()),
             aSkipRect);

  DrawCorner(aDestDrawTarget, aSourceBlur,
             RectWithEdgesTRBL(aDstOuter.Y(), aDstOuter.XMost(),
                               aDstInner.Y(), aDstInner.XMost()),
             RectWithEdgesTRBL(aSrcOuter.Y(), aSrcOuter.XMost(),
                               aSrcInner.Y(), aSrcInner.XMost()),
             aSkipRect);

  DrawCorner(aDestDrawTarget, aSourceBlur,
             RectWithEdgesTRBL(aDstInner.YMost(), aDstInner.X(),
                               aDstOuter.YMost(), aDstOuter.X()),
             RectWithEdgesTRBL(aSrcInner.YMost(), aSrcInner.X(),
                               aSrcOuter.YMost(), aSrcOuter.X()),
             aSkipRect);

  DrawCorner(aDestDrawTarget, aSourceBlur,
             RectWithEdgesTRBL(aDstInner.YMost(), aDstOuter.XMost(),
                               aDstOuter.YMost(), aDstInner.XMost()),
             RectWithEdgesTRBL(aSrcInner.YMost(), aSrcOuter.XMost(),
                               aSrcOuter.YMost(), aSrcInner.XMost()),
             aSkipRect);

  // Edges: top, left, right, bottom
  RepeatOrStretchSurface(aDestDrawTarget, aSourceBlur,
                         RectWithEdgesTRBL(aDstOuter.Y(), aDstInner.XMost(),
                                           aDstInner.Y(), aDstInner.X()),
                         RectWithEdgesTRBL(aSrcOuter.Y(), aSrcInner.XMost(),
                                           aSrcInner.Y(), aSrcInner.X()),
                         aSkipRect);
  RepeatOrStretchSurface(aDestDrawTarget, aSourceBlur,
                         RectWithEdgesTRBL(aDstInner.Y(), aDstInner.X(),
                                           aDstInner.YMost(), aDstOuter.X()),
                         RectWithEdgesTRBL(aSrcInner.Y(), aSrcInner.X(),
                                           aSrcInner.YMost(), aSrcOuter.X()),
                         aSkipRect);
  RepeatOrStretchSurface(aDestDrawTarget, aSourceBlur,
                         RectWithEdgesTRBL(aDstInner.Y(), aDstOuter.XMost(),
                                           aDstInner.YMost(), aDstInner.XMost()),
                         RectWithEdgesTRBL(aSrcInner.Y(), aSrcOuter.XMost(),
                                           aSrcInner.YMost(), aSrcInner.XMost()),
                         aSkipRect);
  RepeatOrStretchSurface(aDestDrawTarget, aSourceBlur,
                         RectWithEdgesTRBL(aDstInner.YMost(), aDstInner.XMost(),
                                           aDstOuter.YMost(), aDstInner.X()),
                         RectWithEdgesTRBL(aSrcInner.YMost(), aSrcInner.XMost(),
                                           aSrcOuter.YMost(), aSrcInner.X()),
                         aSkipRect);
}


/***
 * We draw a blurred a rectangle by only blurring a smaller rectangle and
 * splitting the rectangle into 9 parts.
 * First, a small minimum source rect is calculated and used to create a blur
 * mask since the actual blurring itself is expensive. Next, we use the mask
 * with the given shadow color to create a minimally-sized box shadow of the
 * right color. Finally, we cut out the 9 parts from the box-shadow source and
 * paint each part in the right place, stretching the non-corner parts to fill
 * the space between the corners.
 */


/* static */ void
gfxAlphaBoxBlur::BlurRectangle(gfxContext* aDestinationCtx,
                               const gfxRect& aRect,
                               RectCornerRadii* aCornerRadii,
                               const gfxPoint& aBlurStdDev,
                               const Color& aShadowColor,
                               const gfxRect& aDirtyRect,
                               const gfxRect& aSkipRect)
{
  DrawTarget& destDrawTarget = *aDestinationCtx->GetDrawTarget();
  IntSize blurRadius = CalculateBlurRadius(aBlurStdDev);

  IntRect rect = RoundedToInt(ToRect(aRect));
  IntMargin extendDestBy;
  IntMargin slice;

  RefPtr<SourceSurface> boxShadow = GetBlur(destDrawTarget,
                                            rect.Size(), blurRadius,
                                            aCornerRadii, aShadowColor,
                                            extendDestBy, slice);
  if (!boxShadow) {
    return;
  }

  destDrawTarget.PushClipRect(ToRect(aDirtyRect));

  // Copy the right parts from boxShadow into destDrawTarget. The middle parts
  // will be stretched, border-image style.

  Rect srcOuter(Point(), Size(boxShadow->GetSize()));
  Rect srcInner = srcOuter;
  srcInner.Deflate(Margin(slice));

  rect.Inflate(extendDestBy);
  Rect dstOuter(rect);
  Rect dstInner(rect);
  dstInner.Deflate(Margin(slice));

  Rect skipRect = ToRect(aSkipRect);

  if (srcInner.IsEqualInterior(srcOuter)) {
    MOZ_ASSERT(dstInner.IsEqualInterior(dstOuter));
    // The target rect is smaller than the minimal size so just draw the surface
    destDrawTarget.DrawSurface(boxShadow, dstInner, srcInner);
  } else {
    DrawBoxShadows(destDrawTarget, boxShadow, dstOuter, dstInner,
                   srcOuter, srcInner, skipRect);

    // Middle part
    RepeatOrStretchSurface(destDrawTarget, boxShadow,
                           RectWithEdgesTRBL(dstInner.Y(), dstInner.XMost(),
                                             dstInner.YMost(), dstInner.X()),
                           RectWithEdgesTRBL(srcInner.Y(), srcInner.XMost(),
                                             srcInner.YMost(), srcInner.X()),
                           skipRect);
  }

  // A note about anti-aliasing and seems between adjacent parts:
  // We don't explicitly disable anti-aliasing in the DrawSurface calls above,
  // so if there's a transform on destDrawTarget that is not pixel-aligned,
  // there will be seams between adjacent parts of the box-shadow. It's hard to
  // avoid those without the use of an intermediate surface.
  // You might think that we could avoid those by just turning of AA, but there
  // is a problem with that: Box-shadow rendering needs to clip out the
  // element's border box, and we'd like that clip to have anti-aliasing -
  // especially if the element has rounded corners! So we can't do that unless
  // we have a way to say "Please anti-alias the clip, but don't antialias the
  // destination rect of the DrawSurface call".
  // On OS X there is an additional problem with turning off AA: CoreGraphics
  // will not just fill the pixels that have their pixel center inside the
  // filled shape. Instead, it will fill all the pixels which are partially
  // covered by the shape. So for pixels on the edge between two adjacent parts,
  // all those pixels will be painted to by both parts, which looks very bad.

  destDrawTarget.PopClip();
}

static already_AddRefed<Path>
GetBoxShadowInsetPath(DrawTarget* aDrawTarget,
                      const Rect aOuterRect, const Rect aInnerRect,
                      const bool aHasBorderRadius, const RectCornerRadii& aInnerClipRadii)
{
  /***
   * We create an inset path by having two rects.
   *
   *  -----------------------
   *  |  ________________   |
   *  | |                |  |
   *  | |                |  |
   *  | ------------------  |
   *  |_____________________|
   *
   * The outer rect and the inside rect. The path
   * creates a frame around the content where we draw the inset shadow.
   */
  RefPtr<PathBuilder> builder =
    aDrawTarget->CreatePathBuilder(FillRule::FILL_EVEN_ODD);
  AppendRectToPath(builder, aOuterRect, true);

  if (aHasBorderRadius) {
    AppendRoundedRectToPath(builder, aInnerRect, aInnerClipRadii, false);
  } else {
    AppendRectToPath(builder, aInnerRect, false);
  }
  return builder->Finish();
}

static void
ComputeRectsForInsetBoxShadow(IntSize aBlurRadius,
                              IntSize aSpreadRadius,
                              Rect& aOutOuterRect,
                              Rect& aOutInnerRect,
                              Margin& aOutPathMargins,
                              const Rect& aDestRect,
                              const Rect& aShadowClipRect,
                              bool aHasBorderRadius,
                              const RectCornerRadii& aInnerClipRectRadii)
{
  IntSize marginSize = aBlurRadius + aSpreadRadius;
  aOutPathMargins.SizeTo(marginSize.height, marginSize.width, marginSize.height, marginSize.width);
  aOutPathMargins += aOutPathMargins;

  aOutOuterRect.x = 0;
  aOutInnerRect.x = marginSize.width;

  aOutOuterRect.y = 0;
  aOutInnerRect.y = marginSize.height;

  // + 1 for the middle edges so we can sample them
  aOutInnerRect.width = aOutPathMargins.LeftRight() + 1;
  aOutInnerRect.height = aOutPathMargins.TopBottom() + 1;

  // The outer path rect needs to be 1 blur radius past the inner edges
  aOutOuterRect.width = aOutInnerRect.XMost() + marginSize.width;
  aOutOuterRect.height = aOutInnerRect.YMost() + marginSize.height;

  if (aHasBorderRadius) {
    for (int i = 0; i < 4; i++) {
      aOutInnerRect.width += aInnerClipRectRadii[i].width;
      aOutInnerRect.height += aInnerClipRectRadii[i].height;

      aOutOuterRect.width += aInnerClipRectRadii[i].width;
      aOutOuterRect.height += aInnerClipRectRadii[i].height;
    }
  }

  if ((aOutOuterRect.width >= aDestRect.width) ||
      (aOutOuterRect.height >= aDestRect.height) ||
      (aOutInnerRect.width >= aShadowClipRect.width) ||
      (aOutInnerRect.height >= aShadowClipRect.height))
  {
    aOutOuterRect.width = aDestRect.width;
    aOutOuterRect.height = aDestRect.height;
    aOutInnerRect.width = aShadowClipRect.width;
    aOutInnerRect.height = aShadowClipRect.height;
    aOutPathMargins.SizeTo(0, 0, 0, 0);
  }
}

static void
FillDestinationPath(gfxContext* aDestinationCtx,
                    const Rect aDestinationRect,
                    const Rect aShadowClipRect,
                    const Color& aShadowColor,
                    const bool aHasBorderRadius,
                    const RectCornerRadii& aInnerClipRadii)
{
  // When there is no blur radius, fill the path onto the destination
  // surface.
  aDestinationCtx->SetColor(aShadowColor);
  DrawTarget* destDrawTarget = aDestinationCtx->GetDrawTarget();
  RefPtr<Path> shadowPath = GetBoxShadowInsetPath(destDrawTarget, aDestinationRect,
                                                  aShadowClipRect, aHasBorderRadius,
                                                  aInnerClipRadii);

  aDestinationCtx->SetPath(shadowPath);
  aDestinationCtx->Fill();
}

void
CacheInsetBlur(const IntSize aMinOuterSize,
               const IntSize aMinInnerSize,
               const IntSize& aBlurRadius,
               const IntSize& aSpreadRadius,
               const RectCornerRadii* aCornerRadii,
               const Color& aShadowColor,
               const bool& aHasBorderRadius,
               BackendType aBackendType,
               IntMargin aExtendBy,
               SourceSurface* aBoxShadow)
{
  BlurCacheKey key(aMinOuterSize, aMinInnerSize,
                   aBlurRadius, aSpreadRadius,
                   aCornerRadii, aShadowColor,
                   true, aHasBorderRadius, aBackendType);
  BlurCacheData* data = new BlurCacheData(aBoxShadow, aExtendBy, key);
  if (!gBlurCache->RegisterEntry(data)) {
    delete data;
  }
}

already_AddRefed<mozilla::gfx::SourceSurface>
gfxAlphaBoxBlur::GetInsetBlur(Rect& aOuterRect,
                              Rect& aInnerRect,
                              const IntSize& aBlurRadius,
                              const IntSize& aSpreadRadius,
                              const RectCornerRadii& aInnerClipRadii,
                              const Color& aShadowColor,
                              const bool& aHasBorderRadius,
                              IntPoint& aOutTopLeft,
                              gfxContext* aDestinationCtx)

{
  if (!gBlurCache) {
    gBlurCache = new BlurCache();
  }

  IntSize outerRectSize = RoundedToInt(aOuterRect).Size();
  IntSize innerRectSize = RoundedToInt(aInnerRect).Size();
  DrawTarget* destDrawTarget = aDestinationCtx->GetDrawTarget();

  BlurCacheData* cached =
      gBlurCache->LookupInsetBoxShadow(outerRectSize, innerRectSize, aBlurRadius, aSpreadRadius,
                                       &aInnerClipRadii, aShadowColor,
                                       aHasBorderRadius, destDrawTarget->GetBackendType());

  if (cached) {
    IntMargin extends = cached->mExtendDest;
    aOutTopLeft.x = extends.left;
    aOutTopLeft.y = extends.top;
    // So we don't forget the actual cached blur
    RefPtr<SourceSurface> cachedBlur = cached->mBlur;
    return cachedBlur.forget();
  }

  // Dirty rect and skip rect are null for the min inset shadow.
  // When rendering inset box shadows, we respect the spread radius by changing
  //  the shape of the unblurred shadow, and can pass a spread radius of zero here.
  IntSize zeroSpread(0, 0);
  gfxContext* minGfxContext = Init(ThebesRect(aOuterRect), zeroSpread, aBlurRadius, nullptr, nullptr);
  if (!minGfxContext) {
    return nullptr;
  }

  DrawTarget* minDrawTarget = minGfxContext->GetDrawTarget();
  RefPtr<Path> maskPath = GetBoxShadowInsetPath(minDrawTarget, aOuterRect,
                                                aInnerRect, aHasBorderRadius,
                                                aInnerClipRadii);

  minGfxContext->SetColor(aShadowColor);
  minGfxContext->SetPath(maskPath);
  minGfxContext->Fill();

  RefPtr<SourceSurface> minMask = DoBlur(minDrawTarget, &aOutTopLeft);
  if (!minMask) {
    return nullptr;
  }

  RefPtr<SourceSurface> minInsetBlur = CreateBoxShadow(minMask, aShadowColor);
  if (!minInsetBlur) {
    return nullptr;
  }

  IntMargin extendBy(aOutTopLeft.y, 0, 0, aOutTopLeft.x);
  CacheInsetBlur(outerRectSize, innerRectSize,
                 aBlurRadius, aSpreadRadius,
                 &aInnerClipRadii, aShadowColor,
                 aHasBorderRadius, destDrawTarget->GetBackendType(),
                 extendBy, minInsetBlur);
  return minInsetBlur.forget();
}

/***
 * Blur an inset box shadow by doing:
 * 1) Create a minimal box shadow path that creates a frame.
 * 2) Draw the box shadow portion over the destination surface.
 * 3) The "inset" part is created by a clip rect that properly clips
 *    the alpha mask so that it has clean edges. We still create the full
 *    proper alpha mask, but let the clip deal with the clean edges.
 *
 * All parameters should already be in device pixels.
 */
void
gfxAlphaBoxBlur::BlurInsetBox(gfxContext* aDestinationCtx,
                              const Rect aDestinationRect,
                              const Rect aShadowClipRect,
                              const IntSize aBlurRadius,
                              const IntSize aSpreadRadius,
                              const Color& aShadowColor,
                              bool aHasBorderRadius,
                              const RectCornerRadii& aInnerClipRadii,
                              const Rect aSkipRect)
{
  // Blur inset shadows ALWAYS have a 0 spread radius.
  if ((aBlurRadius.width <= 0 && aBlurRadius.height <= 0)) {
    FillDestinationPath(aDestinationCtx, aDestinationRect, aShadowClipRect,
        aShadowColor, aHasBorderRadius, aInnerClipRadii);
    return;
  }

  DrawTarget* destDrawTarget = aDestinationCtx->GetDrawTarget();
  Rect outerRect;
  Rect innerRect;
  Margin pathMargins;
  ComputeRectsForInsetBoxShadow(aBlurRadius, aSpreadRadius,
                                outerRect, innerRect,
                                pathMargins, aDestinationRect,
                                aShadowClipRect, aHasBorderRadius,
                                aInnerClipRadii);

  IntPoint topLeft;
  RefPtr<SourceSurface> minInsetBlur = GetInsetBlur(outerRect, innerRect,
                                                    aBlurRadius, aSpreadRadius,
                                                    aInnerClipRadii, aShadowColor,
                                                    aHasBorderRadius,
                                                    topLeft, aDestinationCtx);
  if (!minInsetBlur) {
    return;
  }

  Rect destRectOuter(aDestinationRect);
  Rect destRectInner(destRectOuter);
  destRectInner.Deflate(pathMargins);

  Rect srcRectOuter(outerRect);
  srcRectOuter.MoveBy(abs(topLeft.x), abs(topLeft.y));
  Rect srcRectInner(srcRectOuter);
  srcRectInner.Deflate(pathMargins);

  if (srcRectOuter.IsEqualInterior(srcRectInner)) {
    destDrawTarget->DrawSurface(minInsetBlur, destRectOuter, srcRectOuter);
  } else {
    DrawBoxShadows(*destDrawTarget, minInsetBlur,
                   destRectOuter, destRectInner,
                   srcRectOuter, srcRectInner,
                   aSkipRect);
 }
}
