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
#include "mozilla/Maybe.h"
#include "mozilla/SystemGroup.h"
#include "nsExpirationTracker.h"
#include "nsClassHashtable.h"
#include "gfxUtils.h"

using namespace mozilla;
using namespace mozilla::gfx;

gfxAlphaBoxBlur::gfxAlphaBoxBlur()
  : mData(nullptr),
    mAccelerated(false)
{
}

gfxAlphaBoxBlur::~gfxAlphaBoxBlur()
{
}

already_AddRefed<gfxContext>
gfxAlphaBoxBlur::Init(gfxContext* aDestinationCtx,
                      const gfxRect& aRect,
                      const IntSize& aSpreadRadius,
                      const IntSize& aBlurRadius,
                      const gfxRect* aDirtyRect,
                      const gfxRect* aSkipRect,
                      bool aUseHardwareAccel)
{
  DrawTarget* refDT = aDestinationCtx->GetDrawTarget();
  Maybe<Rect> dirtyRect = aDirtyRect ? Some(ToRect(*aDirtyRect)) : Nothing();
  Maybe<Rect> skipRect = aSkipRect ? Some(ToRect(*aSkipRect)) : Nothing();
  RefPtr<DrawTarget> dt =
    InitDrawTarget(refDT, ToRect(aRect), aSpreadRadius, aBlurRadius,
                   dirtyRect.ptrOr(nullptr), skipRect.ptrOr(nullptr),
                   aUseHardwareAccel);
  if (!dt) {
    return nullptr;
  }

  RefPtr<gfxContext> context = gfxContext::CreateOrNull(dt);
  MOZ_ASSERT(context); // already checked for target above
  context->SetMatrix(Matrix::Translation(-mBlur.GetRect().TopLeft()));
  return context.forget();
}

already_AddRefed<DrawTarget>
gfxAlphaBoxBlur::InitDrawTarget(const DrawTarget* aReferenceDT,
                                const Rect& aRect,
                                const IntSize& aSpreadRadius,
                                const IntSize& aBlurRadius,
                                const Rect* aDirtyRect,
                                const Rect* aSkipRect,
                                bool aUseHardwareAccel)
{
  mBlur.Init(aRect, aSpreadRadius, aBlurRadius, aDirtyRect, aSkipRect);
  size_t blurDataSize = mBlur.GetSurfaceAllocationSize();
  if (blurDataSize == 0) {
    return nullptr;
  }

  BackendType backend = aReferenceDT->GetBackendType();

  // Check if the backend has an accelerated DrawSurfaceWithShadow.
  // Currently, only D2D1.1 supports this.
  // Otherwise, DrawSurfaceWithShadow only supports square blurs without spread.
  // When blurring small draw targets such as short spans text, the cost of
  // creating and flushing an accelerated draw target may exceed the speedup
  // gained from the faster blur. It's up to the users of this blur
  // to determine whether they want to use hardware acceleration.
  if (aBlurRadius.IsSquare() && aSpreadRadius.IsEmpty() &&
      aUseHardwareAccel &&
      backend == BackendType::DIRECT2D1_1) {
    mAccelerated = true;
  }

  if (aReferenceDT->IsCaptureDT()) {
    if (mAccelerated) {
      mDrawTarget = Factory::CreateCaptureDrawTarget(backend, mBlur.GetSize(), SurfaceFormat::A8);
    } else {
      mDrawTarget =
        Factory::CreateCaptureDrawTargetForData(backend, mBlur.GetSize(), SurfaceFormat::A8,
                                                mBlur.GetStride(), blurDataSize);
    }
  } else if (mAccelerated) {
    // Note: CreateShadowDrawTarget is only implemented for Cairo, so we don't
    // care about mimicking this in the DrawTargetCapture case.
    mDrawTarget =
      aReferenceDT->CreateShadowDrawTarget(mBlur.GetSize(),
                                           SurfaceFormat::A8,
                                           AlphaBoxBlur::CalculateBlurSigma(aBlurRadius.width));
  } else {
    // Make an alpha-only surface to draw on. We will play with the data after
    // everything is drawn to create a blur effect.
    // This will be freed when the DrawTarget dies
    mData = static_cast<uint8_t*>(calloc(1, blurDataSize));
    if (!mData) {
      return nullptr;
    }
    mDrawTarget =
      Factory::DoesBackendSupportDataDrawtarget(backend) ?
        Factory::CreateDrawTargetForData(backend,
                                         mData,
                                         mBlur.GetSize(),
                                         mBlur.GetStride(),
                                         SurfaceFormat::A8) :
        gfxPlatform::CreateDrawTargetForData(mData,
                                             mBlur.GetSize(),
                                             mBlur.GetStride(),
                                             SurfaceFormat::A8);
  }

  if (!mDrawTarget || !mDrawTarget->IsValid()) {
    if (mData) {
      free(mData);
    }

    return nullptr;
  }

  if (mData) {
    mDrawTarget->AddUserData(reinterpret_cast<UserDataKey*>(mDrawTarget.get()),
                              mData,
                              free);
  }

  mDrawTarget->SetTransform(Matrix::Translation(-mBlur.GetRect().TopLeft()));
  return do_AddRef(mDrawTarget);
}

already_AddRefed<SourceSurface>
gfxAlphaBoxBlur::DoBlur(const Color* aShadowColor, IntPoint* aOutTopLeft)
{
  if (aOutTopLeft) {
    *aOutTopLeft = mBlur.GetRect().TopLeft();
  }

  RefPtr<SourceSurface> blurMask;
  if (mData) {
    mBlur.Blur(mData);
    blurMask = mDrawTarget->Snapshot();
  } else if (mAccelerated) {
    blurMask = mDrawTarget->Snapshot();
    RefPtr<DrawTarget> blurDT =
      mDrawTarget->CreateSimilarDrawTarget(blurMask->GetSize(), SurfaceFormat::A8);
    if (!blurDT) {
      return nullptr;
    }
    blurDT->DrawSurfaceWithShadow(blurMask, Point(0, 0), Color(1, 1, 1), Point(0, 0),
                                  AlphaBoxBlur::CalculateBlurSigma(mBlur.GetBlurRadius().width),
                                  CompositionOp::OP_OVER);
    blurMask = blurDT->Snapshot();
  } else if (mDrawTarget->IsCaptureDT()) {
    mDrawTarget->Blur(mBlur);
    blurMask = mDrawTarget->Snapshot();
  }

  if (!aShadowColor) {
    return blurMask.forget();
  }

  RefPtr<DrawTarget> shadowDT =
    mDrawTarget->CreateSimilarDrawTarget(blurMask->GetSize(), SurfaceFormat::B8G8R8A8);
  if (!shadowDT) {
    return nullptr;
  }
  ColorPattern shadowColor(ToDeviceColor(*aShadowColor));
  shadowDT->MaskSurface(shadowColor, blurMask, Point(0, 0));

  return shadowDT->Snapshot();
}

void
gfxAlphaBoxBlur::Paint(gfxContext* aDestinationCtx)
{
  if ((mDrawTarget && !mDrawTarget->IsCaptureDT()) && !mAccelerated && !mData) {
    return;
  }

  DrawTarget *dest = aDestinationCtx->GetDrawTarget();
  if (!dest) {
    NS_WARNING("Blurring not supported for Thebes contexts!");
    return;
  }

  RefPtr<gfxPattern> thebesPat = aDestinationCtx->GetPattern();
  Pattern* pat = thebesPat->GetPattern(dest, nullptr);
  if (!pat) {
    NS_WARNING("Failed to get pattern for blur!");
    return;
  }

  IntPoint topLeft;
  RefPtr<SourceSurface> mask = DoBlur(nullptr, &topLeft);
  if (!mask) {
    NS_ERROR("Failed to create mask!");
    return;
  }

  // Avoid a semi-expensive clip operation if we can, otherwise
  // clip to the dirty rect
  Rect* dirtyRect = mBlur.GetDirtyRect();
  if (dirtyRect) {
    dest->PushClipRect(*dirtyRect);
  }

  Matrix oldTransform = dest->GetTransform();
  Matrix newTransform = oldTransform;
  newTransform.PreTranslate(topLeft);
  dest->SetTransform(newTransform);

  dest->MaskSurface(*pat, mask, Point(0, 0));

  dest->SetTransform(oldTransform);

  if (dirtyRect) {
    dest->PopClip();
  }
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
  IntSize mInnerMinSize;

  BlurCacheKey(const IntSize& aMinSize, const IntSize& aBlurRadius,
               const RectCornerRadii* aCornerRadii, const Color& aShadowColor,
               BackendType aBackendType)
    : BlurCacheKey(aMinSize, IntSize(0, 0),
                   aBlurRadius, aCornerRadii,
                   aShadowColor, false,
                   aBackendType)
  {}

  explicit BlurCacheKey(const BlurCacheKey* aOther)
    : mMinSize(aOther->mMinSize)
    , mBlurRadius(aOther->mBlurRadius)
    , mShadowColor(aOther->mShadowColor)
    , mBackend(aOther->mBackend)
    , mCornerRadii(aOther->mCornerRadii)
    , mIsInset(aOther->mIsInset)
    , mInnerMinSize(aOther->mInnerMinSize)
  { }

  explicit BlurCacheKey(const IntSize& aOuterMinSize, const IntSize& aInnerMinSize,
                        const IntSize& aBlurRadius,
                        const RectCornerRadii* aCornerRadii,
                        const Color& aShadowColor, bool aIsInset,
                        BackendType aBackendType)
    : mMinSize(aOuterMinSize)
    , mBlurRadius(aBlurRadius)
    , mShadowColor(aShadowColor)
    , mBackend(aBackendType)
    , mCornerRadii(aCornerRadii ? *aCornerRadii : RectCornerRadii())
    , mIsInset(aIsInset)
    , mInnerMinSize(aInnerMinSize)
  { }

  BlurCacheKey(BlurCacheKey&&) = default;

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
      hash = AddToHash(hash, aKey->mInnerMinSize.width, aKey->mInnerMinSize.height);
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
        return (mInnerMinSize == aKey->mInnerMinSize);
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
  BlurCacheData(SourceSurface* aBlur, const IntMargin& aBlurMargin, BlurCacheKey&& aKey)
    : mBlur(aBlur)
    , mBlurMargin(aBlurMargin)
    , mKey(std::move(aKey))
  {}

  BlurCacheData(BlurCacheData&& aOther) = default;

  nsExpirationState *GetExpirationState() {
    return &mExpirationState;
  }

  nsExpirationState mExpirationState;
  RefPtr<SourceSurface> mBlur;
  IntMargin mBlurMargin;
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
      : nsExpirationTracker<BlurCacheData, 4>(GENERATION_MS, "BlurCache",
                                              SystemGroup::EventTargetFor(TaskCategory::Other))
    {
    }

    virtual void NotifyExpired(BlurCacheData* aObject) override
    {
      RemoveObject(aObject);
      mHashEntries.Remove(aObject->mKey);
    }

    BlurCacheData* Lookup(const IntSize& aMinSize,
                          const IntSize& aBlurRadius,
                          const RectCornerRadii* aCornerRadii,
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

    BlurCacheData* LookupInsetBoxShadow(const IntSize& aOuterMinSize,
                                        const IntSize& aInnerMinSize,
                                        const IntSize& aBlurRadius,
                                        const RectCornerRadii* aCornerRadii,
                                        const Color& aShadowColor,
                                        BackendType aBackendType)
    {
      bool insetBoxShadow = true;
      BlurCacheKey key(aOuterMinSize, aInnerMinSize,
                       aBlurRadius, aCornerRadii,
                       aShadowColor, insetBoxShadow,
                       aBackendType);
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
ComputeMinSizeForShadowShape(const RectCornerRadii* aCornerRadii,
                             const IntSize& aBlurRadius,
                             IntMargin& aOutSlice,
                             const IntSize& aRectSize)
{
  Size cornerSize(0, 0);
  if (aCornerRadii) {
    const RectCornerRadii& corners = *aCornerRadii;
    NS_FOR_CSS_FULL_CORNERS(i) {
      cornerSize.width = std::max(cornerSize.width, corners[i].width);
      cornerSize.height = std::max(cornerSize.height, corners[i].height);
    }
  }

  IntSize margin = IntSize::Ceil(cornerSize) + aBlurRadius;
  aOutSlice = IntMargin(margin.height, margin.width,
                        margin.height, margin.width);

  IntSize minSize(aOutSlice.LeftRight() + 1,
                  aOutSlice.TopBottom() + 1);

  // If aRectSize is smaller than minSize, the border-image approach won't
  // work; there's no way to squeeze parts of the min box-shadow source
  // image such that the result looks correct. So we need to adjust minSize
  // in such a way that we can later draw it without stretching in the affected
  // dimension. We also need to adjust "slice" to ensure that we're not trying
  // to slice away more than we have.
  if (aRectSize.width < minSize.width) {
    minSize.width = aRectSize.width;
    aOutSlice.left = 0;
    aOutSlice.right = 0;
  }
  if (aRectSize.height < minSize.height) {
    minSize.height = aRectSize.height;
    aOutSlice.top = 0;
    aOutSlice.bottom = 0;
  }

  MOZ_ASSERT(aOutSlice.LeftRight() <= minSize.width);
  MOZ_ASSERT(aOutSlice.TopBottom() <= minSize.height);
  return minSize;
}

void
CacheBlur(DrawTarget* aDT,
          const IntSize& aMinSize,
          const IntSize& aBlurRadius,
          const RectCornerRadii* aCornerRadii,
          const Color& aShadowColor,
          const IntMargin& aBlurMargin,
          SourceSurface* aBoxShadow)
{
  BlurCacheKey key(aMinSize, aBlurRadius, aCornerRadii, aShadowColor, aDT->GetBackendType());
  BlurCacheData* data = new BlurCacheData(aBoxShadow, aBlurMargin, std::move(key));
  if (!gBlurCache->RegisterEntry(data)) {
    delete data;
  }
}

// Blurs a small surface and creates the colored box shadow.
static already_AddRefed<SourceSurface>
CreateBoxShadow(DrawTarget* aDestDrawTarget,
                const IntSize& aMinSize,
                const RectCornerRadii* aCornerRadii,
                const IntSize& aBlurRadius,
                const Color& aShadowColor,
                bool aMirrorCorners,
                IntMargin& aOutBlurMargin)
{
  gfxAlphaBoxBlur blur;
  Rect minRect(Point(0, 0), Size(aMinSize));
  Rect blurRect(minRect);
  // If mirroring corners, we only need to draw the top-left quadrant.
  // Use ceil to preserve the remaining 1x1 middle area for minimized box
  // shadows.
  if (aMirrorCorners) {
    blurRect.SizeTo(ceil(blurRect.Width() * 0.5f), ceil(blurRect.Height() * 0.5f));
  }
  IntSize zeroSpread(0, 0);
  RefPtr<DrawTarget> blurDT =
    blur.InitDrawTarget(aDestDrawTarget, blurRect, zeroSpread, aBlurRadius);
  if (!blurDT) {
    return nullptr;
  }

  ColorPattern black(Color(0.f, 0.f, 0.f, 1.f));

  if (aCornerRadii) {
    RefPtr<Path> roundedRect =
      MakePathForRoundedRect(*blurDT, minRect, *aCornerRadii);
    blurDT->Fill(roundedRect, black);
  } else {
    blurDT->FillRect(minRect, black);
  }

  IntPoint topLeft;
  RefPtr<SourceSurface> result = blur.DoBlur(&aShadowColor, &topLeft);
  if (!result) {
    return nullptr;
  }

  // Since blurRect is at (0, 0), we can find the inflated margin by
  // negating the new rect origin, which would have been negative if
  // the rect was inflated.
  aOutBlurMargin = IntMargin(-topLeft.y, -topLeft.x, -topLeft.y, -topLeft.x);

  return result.forget();
}

static already_AddRefed<SourceSurface>
GetBlur(gfxContext* aDestinationCtx,
        const IntSize& aRectSize,
        const IntSize& aBlurRadius,
        const RectCornerRadii* aCornerRadii,
        const Color& aShadowColor,
        bool aMirrorCorners,
        IntMargin& aOutBlurMargin,
        IntMargin& aOutSlice,
        IntSize& aOutMinSize)
{
  if (!gBlurCache) {
    gBlurCache = new BlurCache();
  }

  IntSize minSize =
    ComputeMinSizeForShadowShape(aCornerRadii, aBlurRadius, aOutSlice, aRectSize);

  // We can get seams using the min size rect when drawing to the destination rect
  // if we have a non-pixel aligned destination transformation. In those cases,
  // fallback to just rendering the destination rect.
  // During printing, we record all the Moz 2d commands and replay them on the parent side
  // with Cairo. Cairo printing uses StretchDIBits to stretch the surface. However,
  // since our source image is only 1px for some parts, we make thousands of calls.
  // Instead just render the blur ourself here as one image and send it over for printing.
  // TODO: May need to change this with the blob renderer in WR since it also records.
  Matrix destMatrix = aDestinationCtx->CurrentMatrix();
  bool useDestRect = !destMatrix.IsRectilinear() || destMatrix.HasNonIntegerTranslation() ||
                     aDestinationCtx->GetDrawTarget()->IsRecording();
  if (useDestRect) {
    minSize = aRectSize;
  }
  aOutMinSize = minSize;

  DrawTarget* destDT = aDestinationCtx->GetDrawTarget();

  if (!useDestRect) {
    BlurCacheData* cached = gBlurCache->Lookup(minSize, aBlurRadius,
                                               aCornerRadii, aShadowColor,
                                               destDT->GetBackendType());
    if (cached) {
      // See CreateBoxShadow() for these values
      aOutBlurMargin = cached->mBlurMargin;
      RefPtr<SourceSurface> blur = cached->mBlur;
      return blur.forget();
    }
  }

  RefPtr<SourceSurface> boxShadow =
    CreateBoxShadow(destDT, minSize, aCornerRadii, aBlurRadius,
                    aShadowColor, aMirrorCorners, aOutBlurMargin);
  if (!boxShadow) {
    return nullptr;
  }

  if (RefPtr<SourceSurface> opt = destDT->OptimizeSourceSurface(boxShadow)) {
    boxShadow = opt;
  }

  if (!useDestRect) {
    CacheBlur(destDT, minSize, aBlurRadius, aCornerRadii, aShadowColor,
              aOutBlurMargin, boxShadow);
  }
  return boxShadow.forget();
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

static bool
ShouldStretchSurface(DrawTarget* aDT, SourceSurface* aSurface)
{
  // Use stretching if possible, since it leads to less seams when the
  // destination is transformed. However, don't do this if we're using cairo,
  // because if cairo is using pixman it won't render anything for large
  // stretch factors because pixman's internal fixed point precision is not
  // high enough to handle those scale factors.
  // Calling FillRect on a D2D backend with a repeating pattern is much slower
  // than DrawSurface, so special case the D2D backend here.
  return (!aDT->GetTransform().IsRectilinear() &&
          aDT->GetBackendType() != BackendType::CAIRO) ||
         (aDT->GetBackendType() == BackendType::DIRECT2D1_1);
}

static void
RepeatOrStretchSurface(DrawTarget* aDT, SourceSurface* aSurface,
                       const Rect& aDest, const Rect& aSrc, const Rect& aSkipRect)
{
  if (aSkipRect.Contains(aDest)) {
    return;
  }

  if (ShouldStretchSurface(aDT, aSurface)) {
    aDT->DrawSurface(aSurface, aDest, aSrc);
    return;
  }

  SurfacePattern pattern(aSurface, ExtendMode::REPEAT,
                         Matrix::Translation(aDest.TopLeft() - aSrc.TopLeft()),
                         SamplingFilter::GOOD, RoundedToInt(aSrc));
  aDT->FillRect(aDest, pattern);
}

static void
DrawCorner(DrawTarget* aDT, SourceSurface* aSurface,
           const Rect& aDest, const Rect& aSrc, const Rect& aSkipRect)
{
  if (aSkipRect.Contains(aDest)) {
    return;
  }

  aDT->DrawSurface(aSurface, aDest, aSrc);
}

static void
DrawMinBoxShadow(DrawTarget* aDestDrawTarget, SourceSurface* aSourceBlur,
                 const Rect& aDstOuter, const Rect& aDstInner,
                 const Rect& aSrcOuter, const Rect& aSrcInner,
                 const Rect& aSkipRect, bool aMiddle = false)
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

  // Middle part
  if (aMiddle) {
    RepeatOrStretchSurface(aDestDrawTarget, aSourceBlur,
                           RectWithEdgesTRBL(aDstInner.Y(), aDstInner.XMost(),
                                             aDstInner.YMost(), aDstInner.X()),
                           RectWithEdgesTRBL(aSrcInner.Y(), aSrcInner.XMost(),
                                             aSrcInner.YMost(), aSrcInner.X()),
                           aSkipRect);
  }
}

static void
DrawMirroredRect(DrawTarget* aDT,
                 SourceSurface* aSurface,
                 const Rect& aDest, const Point& aSrc,
                 Float aScaleX, Float aScaleY)
{
  SurfacePattern pattern(aSurface, ExtendMode::CLAMP,
                         Matrix::Scaling(aScaleX, aScaleY)
                           .PreTranslate(-aSrc)
                           .PostTranslate(
                             aScaleX < 0 ? aDest.XMost() : aDest.X(),
                             aScaleY < 0 ? aDest.YMost() : aDest.Y()));
  aDT->FillRect(aDest, pattern);
}

static void
DrawMirroredBoxShadow(DrawTarget* aDT,
                      SourceSurface* aSurface,
                      const Rect& aDestRect)
{
  Point center(ceil(aDestRect.X() + aDestRect.Width() / 2),
               ceil(aDestRect.Y() + aDestRect.Height() / 2));
  Rect topLeft(aDestRect.X(), aDestRect.Y(),
               center.x - aDestRect.X(),
               center.y - aDestRect.Y());
  Rect bottomRight(topLeft.BottomRight(), aDestRect.Size() - topLeft.Size());
  Rect topRight(bottomRight.X(), topLeft.Y(), bottomRight.Width(), topLeft.Height());
  Rect bottomLeft(topLeft.X(), bottomRight.Y(), topLeft.Width(), bottomRight.Height());
  DrawMirroredRect(aDT, aSurface, topLeft, Point(), 1, 1);
  DrawMirroredRect(aDT, aSurface, topRight, Point(), -1, 1);
  DrawMirroredRect(aDT, aSurface, bottomLeft, Point(), 1, -1);
  DrawMirroredRect(aDT, aSurface, bottomRight, Point(), -1, -1);
}

static void
DrawMirroredCorner(DrawTarget* aDT, SourceSurface* aSurface,
                   const Rect& aDest, const Point& aSrc,
                   const Rect& aSkipRect, Float aScaleX, Float aScaleY)
{
  if (aSkipRect.Contains(aDest)) {
    return;
  }

  DrawMirroredRect(aDT, aSurface, aDest, aSrc, aScaleX, aScaleY);
}

static void
RepeatOrStretchMirroredSurface(DrawTarget* aDT, SourceSurface* aSurface,
                               const Rect& aDest, const Rect& aSrc,
                               const Rect& aSkipRect, Float aScaleX, Float aScaleY)
{
  if (aSkipRect.Contains(aDest)) {
    return;
  }

  if (ShouldStretchSurface(aDT, aSurface)) {
    aScaleX *= aDest.Width() / aSrc.Width();
    aScaleY *= aDest.Height() / aSrc.Height();
    DrawMirroredRect(aDT, aSurface, aDest, aSrc.TopLeft(), aScaleX, aScaleY);
    return;
  }

  SurfacePattern pattern(aSurface, ExtendMode::REPEAT,
                         Matrix::Scaling(aScaleX, aScaleY)
                           .PreTranslate(-aSrc.TopLeft())
                           .PostTranslate(
                             aScaleX < 0 ? aDest.XMost() : aDest.X(),
                             aScaleY < 0 ? aDest.YMost() : aDest.Y()),
                         SamplingFilter::GOOD, RoundedToInt(aSrc));
  aDT->FillRect(aDest, pattern);
}

static void
DrawMirroredMinBoxShadow(DrawTarget* aDestDrawTarget, SourceSurface* aSourceBlur,
                         const Rect& aDstOuter, const Rect& aDstInner,
                         const Rect& aSrcOuter, const Rect& aSrcInner,
                         const Rect& aSkipRect, bool aMiddle = false)
{
  // Corners: top left, top right, bottom left, bottom right
  // Compute quadrant bounds and then clip them to corners along
  // dimensions where we need to stretch from min size.
  Point center(ceil(aDstOuter.X() + aDstOuter.Width() / 2),
               ceil(aDstOuter.Y() + aDstOuter.Height() / 2));
  Rect topLeft(aDstOuter.X(), aDstOuter.Y(),
               center.x - aDstOuter.X(),
               center.y - aDstOuter.Y());
  Rect bottomRight(topLeft.BottomRight(), aDstOuter.Size() - topLeft.Size());
  Rect topRight(bottomRight.X(), topLeft.Y(), bottomRight.Width(), topLeft.Height());
  Rect bottomLeft(topLeft.X(), bottomRight.Y(), topLeft.Width(), bottomRight.Height());

  // Check if the middle part has been minimized along each dimension.
  // If so, those will be strecthed/drawn separately and need to be clipped out.
  if (aSrcInner.Width() == 1) {
    topLeft.SetRightEdge(aDstInner.X());
    topRight.SetLeftEdge(aDstInner.XMost());
    bottomLeft.SetRightEdge(aDstInner.X());
    bottomRight.SetLeftEdge(aDstInner.XMost());
  }
  if (aSrcInner.Height() == 1) {
    topLeft.SetBottomEdge(aDstInner.Y());
    topRight.SetBottomEdge(aDstInner.Y());
    bottomLeft.SetTopEdge(aDstInner.YMost());
    bottomRight.SetTopEdge(aDstInner.YMost());
  }

  DrawMirroredCorner(aDestDrawTarget, aSourceBlur, topLeft,
                     aSrcOuter.TopLeft(), aSkipRect, 1, 1);
  DrawMirroredCorner(aDestDrawTarget, aSourceBlur, topRight,
                     aSrcOuter.TopLeft(), aSkipRect, -1, 1);
  DrawMirroredCorner(aDestDrawTarget, aSourceBlur, bottomLeft,
                     aSrcOuter.TopLeft(), aSkipRect, 1, -1);
  DrawMirroredCorner(aDestDrawTarget, aSourceBlur, bottomRight,
                     aSrcOuter.TopLeft(), aSkipRect, -1, -1);

  // Edges: top, bottom, left, right
  // Draw middle edges where they need to be stretched. The top and left
  // sections that are part of the top-left quadrant will be mirrored to
  // the bottom and right sections, respectively.
  if (aSrcInner.Width() == 1) {
    Rect dstTop = RectWithEdgesTRBL(aDstOuter.Y(), aDstInner.XMost(),
                                    aDstInner.Y(), aDstInner.X());
    Rect srcTop = RectWithEdgesTRBL(aSrcOuter.Y(), aSrcInner.XMost(),
                                    aSrcInner.Y(), aSrcInner.X());
    Rect dstBottom = RectWithEdgesTRBL(aDstInner.YMost(), aDstInner.XMost(),
                                       aDstOuter.YMost(), aDstInner.X());
    Rect srcBottom = RectWithEdgesTRBL(aSrcOuter.Y(), aSrcInner.XMost(),
                                       aSrcInner.Y(), aSrcInner.X());
    // If we only need to stretch along the X axis and we're drawing
    // the middle section, just sample all the way to the center of the
    // source on the Y axis to avoid extra draw calls.
    if (aMiddle && aSrcInner.Height() != 1) {
      dstTop.SetBottomEdge(center.y);
      srcTop.SetHeight(dstTop.Height());
      dstBottom.SetTopEdge(dstTop.YMost());
      srcBottom.SetHeight(dstBottom.Height());
    }
    RepeatOrStretchMirroredSurface(aDestDrawTarget, aSourceBlur,
                                   dstTop, srcTop, aSkipRect, 1, 1);
    RepeatOrStretchMirroredSurface(aDestDrawTarget, aSourceBlur,
                                   dstBottom, srcBottom, aSkipRect, 1, -1);
  }

  if (aSrcInner.Height() == 1) {
    Rect dstLeft = RectWithEdgesTRBL(aDstInner.Y(), aDstInner.X(),
                                     aDstInner.YMost(), aDstOuter.X());
    Rect srcLeft = RectWithEdgesTRBL(aSrcInner.Y(), aSrcInner.X(),
                                     aSrcInner.YMost(), aSrcOuter.X());
    Rect dstRight = RectWithEdgesTRBL(aDstInner.Y(), aDstOuter.XMost(),
                                      aDstInner.YMost(), aDstInner.XMost());
    Rect srcRight = RectWithEdgesTRBL(aSrcInner.Y(), aSrcInner.X(),
                                      aSrcInner.YMost(), aSrcOuter.X());
    // Only stretching on Y axis, so sample source to the center of the X axis.
    if (aMiddle && aSrcInner.Width() != 1) {
      dstLeft.SetRightEdge(center.x);
      srcLeft.SetWidth(dstLeft.Width());
      dstRight.SetLeftEdge(dstLeft.XMost());
      srcRight.SetWidth(dstRight.Width());
    }
    RepeatOrStretchMirroredSurface(aDestDrawTarget, aSourceBlur,
                                   dstLeft, srcLeft, aSkipRect, 1, 1);
    RepeatOrStretchMirroredSurface(aDestDrawTarget, aSourceBlur,
                                   dstRight, srcRight, aSkipRect, -1, 1);
  }

  // If we need to stretch along both dimensions, then the middle part
  // must be drawn separately.
  if (aMiddle && aSrcInner.Width() == 1 && aSrcInner.Height() == 1) {
    RepeatOrStretchSurface(aDestDrawTarget, aSourceBlur,
                           RectWithEdgesTRBL(aDstInner.Y(), aDstInner.XMost(),
                                             aDstInner.YMost(), aDstInner.X()),
                           RectWithEdgesTRBL(aSrcInner.Y(), aSrcInner.XMost(),
                                             aSrcInner.YMost(), aSrcInner.X()),
                           aSkipRect);
  }
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
                               const RectCornerRadii* aCornerRadii,
                               const gfxPoint& aBlurStdDev,
                               const Color& aShadowColor,
                               const gfxRect& aDirtyRect,
                               const gfxRect& aSkipRect)
{
  IntSize blurRadius = CalculateBlurRadius(aBlurStdDev);
  bool mirrorCorners = !aCornerRadii || aCornerRadii->AreRadiiSame();

  IntRect rect = RoundedToInt(ToRect(aRect));
  IntMargin blurMargin;
  IntMargin slice;
  IntSize minSize;
  RefPtr<SourceSurface> boxShadow = GetBlur(aDestinationCtx,
                                            rect.Size(), blurRadius,
                                            aCornerRadii, aShadowColor, mirrorCorners,
                                            blurMargin, slice, minSize);
  if (!boxShadow) {
    return;
  }

  DrawTarget* destDrawTarget = aDestinationCtx->GetDrawTarget();
  destDrawTarget->PushClipRect(ToRect(aDirtyRect));

  // Copy the right parts from boxShadow into destDrawTarget. The middle parts
  // will be stretched, border-image style.

  Rect srcOuter(Point(blurMargin.left, blurMargin.top), Size(minSize));
  Rect srcInner(srcOuter);
  srcOuter.Inflate(Margin(blurMargin));
  srcInner.Deflate(Margin(slice));

  Rect dstOuter(rect);
  Rect dstInner(rect);
  dstOuter.Inflate(Margin(blurMargin));
  dstInner.Deflate(Margin(slice));

  Rect skipRect = ToRect(aSkipRect);

  if (minSize == rect.Size()) {
    // The target rect is smaller than the minimal size so just draw the surface
    if (mirrorCorners) {
      DrawMirroredBoxShadow(destDrawTarget, boxShadow, dstOuter);
    } else {
      destDrawTarget->DrawSurface(boxShadow, dstOuter, srcOuter);
    }
  } else {
    if (mirrorCorners) {
      DrawMirroredMinBoxShadow(destDrawTarget, boxShadow, dstOuter, dstInner,
                               srcOuter, srcInner, skipRect, true);
    } else {
      DrawMinBoxShadow(destDrawTarget, boxShadow, dstOuter, dstInner,
                       srcOuter, srcInner, skipRect, true);
    }
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

  destDrawTarget->PopClip();
}

static already_AddRefed<Path>
GetBoxShadowInsetPath(DrawTarget* aDrawTarget,
                      const Rect aOuterRect, const Rect aInnerRect,
                      const RectCornerRadii* aInnerClipRadii)
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

  if (aInnerClipRadii) {
    AppendRoundedRectToPath(builder, aInnerRect, *aInnerClipRadii, false);
  } else {
    AppendRectToPath(builder, aInnerRect, false);
  }
  return builder->Finish();
}

static void
FillDestinationPath(gfxContext* aDestinationCtx,
                    const Rect& aDestinationRect,
                    const Rect& aShadowClipRect,
                    const Color& aShadowColor,
                    const RectCornerRadii* aInnerClipRadii = nullptr)
{
  // When there is no blur radius, fill the path onto the destination
  // surface.
  aDestinationCtx->SetColor(aShadowColor);
  DrawTarget* destDrawTarget = aDestinationCtx->GetDrawTarget();
  RefPtr<Path> shadowPath = GetBoxShadowInsetPath(destDrawTarget, aDestinationRect,
                                                  aShadowClipRect, aInnerClipRadii);

  aDestinationCtx->SetPath(shadowPath);
  aDestinationCtx->Fill();
}

static void
CacheInsetBlur(const IntSize& aMinOuterSize,
               const IntSize& aMinInnerSize,
               const IntSize& aBlurRadius,
               const RectCornerRadii* aCornerRadii,
               const Color& aShadowColor,
               BackendType aBackendType,
               SourceSurface* aBoxShadow)
{
  bool isInsetBlur = true;
  BlurCacheKey key(aMinOuterSize, aMinInnerSize,
                   aBlurRadius, aCornerRadii,
                   aShadowColor, isInsetBlur,
                   aBackendType);
  IntMargin blurMargin(0, 0, 0, 0);
  BlurCacheData* data = new BlurCacheData(aBoxShadow, blurMargin, std::move(key));
  if (!gBlurCache->RegisterEntry(data)) {
    delete data;
  }
}

already_AddRefed<SourceSurface>
gfxAlphaBoxBlur::GetInsetBlur(const Rect& aOuterRect,
                              const Rect& aWhitespaceRect,
                              bool aIsDestRect,
                              const Color& aShadowColor,
                              const IntSize& aBlurRadius,
                              const RectCornerRadii* aInnerClipRadii,
                              DrawTarget* aDestDrawTarget,
                              bool aMirrorCorners)
{
  if (!gBlurCache) {
    gBlurCache = new BlurCache();
  }

  IntSize outerSize = IntSize::Truncate(aOuterRect.Size());
  IntSize whitespaceSize = IntSize::Truncate(aWhitespaceRect.Size());
  if (!aIsDestRect) {
    BlurCacheData* cached =
      gBlurCache->LookupInsetBoxShadow(outerSize, whitespaceSize,
                                       aBlurRadius, aInnerClipRadii,
                                       aShadowColor, aDestDrawTarget->GetBackendType());
    if (cached) {
      // So we don't forget the actual cached blur
      RefPtr<SourceSurface> cachedBlur = cached->mBlur;
      return cachedBlur.forget();
    }
  }

  // If we can do a min rect, the whitespace rect will be expanded in Init to
  // aOuterRect.
  Rect blurRect = aIsDestRect ? aOuterRect : aWhitespaceRect;
  // If mirroring corners, we only need to draw the top-left quadrant.
  // Use ceil to preserve the remaining 1x1 middle area for minimized box
  // shadows.
  if (aMirrorCorners) {
    blurRect.SizeTo(ceil(blurRect.Width() * 0.5f), ceil(blurRect.Height() * 0.5f));
  }
  IntSize zeroSpread(0, 0);
  RefPtr<DrawTarget> minDrawTarget =
    InitDrawTarget(aDestDrawTarget, blurRect, zeroSpread, aBlurRadius);
  if (!minDrawTarget) {
    return nullptr;
  }

  // This is really annoying. When we create the AlphaBoxBlur, the DrawTarget
  // has a translation applied to it that is the topLeft point. This is actually
  // the rect we gave it plus the blur radius. The rects we give this for the outer
  // and whitespace rects are based at (0, 0). We could either translate those rects
  // when we don't have a destination rect or ignore the translation when using
  // the dest rect. The dest rects layout gives us expect this translation.
  if (!aIsDestRect) {
    minDrawTarget->SetTransform(Matrix());
  }

  // Fill in the path between the inside white space / outer rects
  // NOT the inner frame
  RefPtr<Path> maskPath =
    GetBoxShadowInsetPath(minDrawTarget, aOuterRect,
                          aWhitespaceRect, aInnerClipRadii);

  ColorPattern black(Color(0.f, 0.f, 0.f, 1.f));
  minDrawTarget->Fill(maskPath, black);

  // Blur and fill in with the color we actually wanted
  RefPtr<SourceSurface> minInsetBlur = DoBlur(&aShadowColor);
  if (!minInsetBlur) {
    return nullptr;
  }

  if (RefPtr<SourceSurface> opt = aDestDrawTarget->OptimizeSourceSurface(minInsetBlur)) {
    minInsetBlur = opt;
  }

  if (!aIsDestRect) {
    CacheInsetBlur(outerSize, whitespaceSize,
                   aBlurRadius, aInnerClipRadii,
                   aShadowColor, aDestDrawTarget->GetBackendType(),
                   minInsetBlur);
  }

  return minInsetBlur.forget();
}

/***
 * We create our minimal rect with 2 rects.
 * The first is the inside whitespace rect, that is "cut out"
 * from the box. This is (1). This must be the size
 * of the blur radius + corner radius so we can have a big enough
 * inside cut.
 *
 * The second (2) is one blur radius surrounding the inner
 * frame of (1). This is the amount of blur space required
 * to get a proper blend.
 *
 * B = one blur size
 * W = one blur + corner radii - known as inner margin
 * ___________________________________
 * |                                |
 * |          |             |       |
 * |      (2) |    (1)      |  (2)  |
 * |       B  |     W       |   B   |
 * |          |             |       |
 * |          |             |       |
 * |          |                     |
 * |________________________________|
 */
static void GetBlurMargins(const RectCornerRadii* aInnerClipRadii,
                           const IntSize& aBlurRadius,
                           Margin& aOutBlurMargin,
                           Margin& aOutInnerMargin)
{
  Size cornerSize(0, 0);
  if (aInnerClipRadii) {
    const RectCornerRadii& corners = *aInnerClipRadii;
    NS_FOR_CSS_FULL_CORNERS(i) {
      cornerSize.width = std::max(cornerSize.width, corners[i].width);
      cornerSize.height = std::max(cornerSize.height, corners[i].height);
    }
  }

  // Only the inside whitespace size cares about the border radius size.
  // Outer sizes only care about blur.
  IntSize margin = IntSize::Ceil(cornerSize) + aBlurRadius;

  aOutInnerMargin.SizeTo(margin.height, margin.width,
                         margin.height, margin.width);
  aOutBlurMargin.SizeTo(aBlurRadius.height, aBlurRadius.width,
                        aBlurRadius.height, aBlurRadius.width);
}

static bool
GetInsetBoxShadowRects(const Margin& aBlurMargin,
                       const Margin& aInnerMargin,
                       const Rect& aShadowClipRect,
                       const Rect& aDestinationRect,
                       Rect& aOutWhitespaceRect,
                       Rect& aOutOuterRect)
{
  // We always copy (2 * blur radius) + corner radius worth of data to the destination rect
  // This covers the blend of the path + the actual blur
  // Need +1 so that we copy the edges correctly as we'll copy
  // over the min box shadow corners then the +1 for the edges between
  // Note, the (x,y) coordinates are from the blur margin
  // since the frame outside the whitespace rect is 1 blur radius extra space.
  Rect insideWhiteSpace(aBlurMargin.left,
                        aBlurMargin.top,
                        aInnerMargin.LeftRight() + 1,
                        aInnerMargin.TopBottom() + 1);

  // If the inner white space rect is larger than the shadow clip rect
  // our approach does not work as we'll just copy one corner
  // and cover the destination. In those cases, fallback to the destination rect
  bool useDestRect = (aShadowClipRect.Width() <= aInnerMargin.LeftRight()) ||
                     (aShadowClipRect.Height() <= aInnerMargin.TopBottom());

  if (useDestRect) {
    aOutWhitespaceRect = aShadowClipRect;
    aOutOuterRect = aDestinationRect;
  } else {
    aOutWhitespaceRect = insideWhiteSpace;
    aOutOuterRect = aOutWhitespaceRect;
    aOutOuterRect.Inflate(aBlurMargin);
  }

  return useDestRect;
}

void
gfxAlphaBoxBlur::BlurInsetBox(gfxContext* aDestinationCtx,
                              const Rect& aDestinationRect,
                              const Rect& aShadowClipRect,
                              const IntSize& aBlurRadius,
                              const Color& aShadowColor,
                              const RectCornerRadii* aInnerClipRadii,
                              const Rect& aSkipRect,
                              const Point& aShadowOffset)
{
  if ((aBlurRadius.width == 0 && aBlurRadius.height == 0)
      || aShadowClipRect.IsEmpty()) {
    FillDestinationPath(aDestinationCtx, aDestinationRect, aShadowClipRect,
        aShadowColor, aInnerClipRadii);
    return;
  }

  DrawTarget* destDrawTarget = aDestinationCtx->GetDrawTarget();

  Margin innerMargin;
  Margin blurMargin;
  GetBlurMargins(aInnerClipRadii, aBlurRadius, blurMargin, innerMargin);

  Rect whitespaceRect;
  Rect outerRect;
  bool useDestRect =
    GetInsetBoxShadowRects(blurMargin, innerMargin, aShadowClipRect,
                           aDestinationRect, whitespaceRect, outerRect);

  // Check that the inset margin between the outer and whitespace rects is symmetric,
  // and that all corner radii are the same, in which case the blur can be mirrored.
  Margin checkMargin = outerRect - whitespaceRect;
  bool mirrorCorners =
    checkMargin.left == checkMargin.right &&
    checkMargin.top == checkMargin.bottom &&
    (!aInnerClipRadii || aInnerClipRadii->AreRadiiSame());
  RefPtr<SourceSurface> minBlur =
    GetInsetBlur(outerRect, whitespaceRect, useDestRect, aShadowColor,
                 aBlurRadius, aInnerClipRadii, destDrawTarget, mirrorCorners);
  if (!minBlur) {
    return;
  }

  if (useDestRect) {
    Rect destBlur = aDestinationRect;
    destBlur.Inflate(blurMargin);
    if (mirrorCorners) {
      DrawMirroredBoxShadow(destDrawTarget, minBlur.get(), destBlur);
    } else {
      Rect srcBlur(Point(0, 0), Size(minBlur->GetSize()));
      MOZ_ASSERT(srcBlur.Size() == destBlur.Size());
      destDrawTarget->DrawSurface(minBlur, destBlur, srcBlur);
    }
  } else {
    Rect srcOuter(outerRect);
    Rect srcInner(srcOuter);
    srcInner.Deflate(blurMargin);   // The outer color fill
    srcInner.Deflate(innerMargin);  // The inner whitespace

    // The shadow clip rect already takes into account the spread radius
    Rect outerFillRect(aShadowClipRect);
    outerFillRect.Inflate(blurMargin);
    FillDestinationPath(aDestinationCtx, aDestinationRect, outerFillRect, aShadowColor);

    // Inflate once for the frame around the whitespace
    Rect destRect(aShadowClipRect);
    destRect.Inflate(blurMargin);

    // Deflate for the blurred in white space
    Rect destInnerRect(aShadowClipRect);
    destInnerRect.Deflate(innerMargin);

    if (mirrorCorners) {
      DrawMirroredMinBoxShadow(destDrawTarget, minBlur,
                               destRect, destInnerRect,
                               srcOuter, srcInner,
                               aSkipRect);
    } else {
      DrawMinBoxShadow(destDrawTarget, minBlur,
                       destRect, destInnerRect,
                       srcOuter, srcInner,
                       aSkipRect);
    }
  }
}
