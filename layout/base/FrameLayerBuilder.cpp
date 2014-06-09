/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "FrameLayerBuilder.h"

#include "mozilla/gfx/Matrix.h"
#include "nsDisplayList.h"
#include "nsPresContext.h"
#include "nsLayoutUtils.h"
#include "Layers.h"
#include "BasicLayers.h"
#include "gfxUtils.h"
#include "nsRenderingContext.h"
#include "MaskLayerImageCache.h"
#include "nsIScrollableFrame.h"
#include "nsPrintfCString.h"
#include "LayerTreeInvalidation.h"
#include "nsSVGIntegrationUtils.h"
#include "ImageContainer.h"
#include "ActiveLayerTracker.h"
#include "gfx2DGlue.h"
#include "mozilla/LookAndFeel.h"

#include "GeckoProfiler.h"
#include "mozilla/gfx/Tools.h"
#include "mozilla/gfx/2D.h"
#include "gfxPrefs.h"

#include <algorithm>

using namespace mozilla::layers;
using namespace mozilla::gfx;

namespace mozilla {

class ContainerState;

FrameLayerBuilder::DisplayItemData::DisplayItemData(LayerManagerData* aParent, uint32_t aKey,
                                                    Layer* aLayer, LayerState aLayerState, uint32_t aGeneration)

  : mParent(aParent)
  , mLayer(aLayer)
  , mDisplayItemKey(aKey)
  , mContainerLayerGeneration(aGeneration)
  , mLayerState(aLayerState)
  , mUsed(true)
  , mIsInvalid(false)
{
}

FrameLayerBuilder::DisplayItemData::DisplayItemData(DisplayItemData &toCopy)
{
  // This isn't actually a copy-constructor; notice that it steals toCopy's
  // mGeometry pointer.  Be careful.
  mParent = toCopy.mParent;
  mLayer = toCopy.mLayer;
  mInactiveManager = toCopy.mInactiveManager;
  mFrameList = toCopy.mFrameList;
  mGeometry = toCopy.mGeometry;
  mDisplayItemKey = toCopy.mDisplayItemKey;
  mClip = toCopy.mClip;
  mContainerLayerGeneration = toCopy.mContainerLayerGeneration;
  mLayerState = toCopy.mLayerState;
  mUsed = toCopy.mUsed;
}

void
FrameLayerBuilder::DisplayItemData::AddFrame(nsIFrame* aFrame)
{
  mFrameList.AppendElement(aFrame);

  nsTArray<DisplayItemData*> *array =
    reinterpret_cast<nsTArray<DisplayItemData*>*>(aFrame->Properties().Get(FrameLayerBuilder::LayerManagerDataProperty()));
  if (!array) {
    array = new nsTArray<DisplayItemData*>();
    aFrame->Properties().Set(FrameLayerBuilder::LayerManagerDataProperty(), array);
  }
  array->AppendElement(this);
}

void
FrameLayerBuilder::DisplayItemData::RemoveFrame(nsIFrame* aFrame)
{
  DebugOnly<bool> result = mFrameList.RemoveElement(aFrame);
  NS_ASSERTION(result, "Can't remove a frame that wasn't added!");

  nsTArray<DisplayItemData*> *array =
    reinterpret_cast<nsTArray<DisplayItemData*>*>(aFrame->Properties().Get(FrameLayerBuilder::LayerManagerDataProperty()));
  NS_ASSERTION(array, "Must be already stored on the frame!");
  array->RemoveElement(this);
}

void
FrameLayerBuilder::DisplayItemData::UpdateContents(Layer* aLayer, LayerState aState,
                                                   uint32_t aContainerLayerGeneration,
                                                   nsDisplayItem* aItem /* = nullptr */)
{
  mLayer = aLayer;
  mOptLayer = nullptr;
  mInactiveManager = nullptr;
  mLayerState = aState;
  mContainerLayerGeneration = aContainerLayerGeneration;
  mGeometry = nullptr;
  mClip = DisplayItemClip();
  mUsed = true;

  if (!aItem) {
    return;
  }

  nsAutoTArray<nsIFrame*, 4> copy(mFrameList);
  if (!copy.RemoveElement(aItem->Frame())) {
    AddFrame(aItem->Frame());
  }

  nsAutoTArray<nsIFrame*,4> mergedFrames;
  aItem->GetMergedFrames(&mergedFrames);
  for (uint32_t i = 0; i < mergedFrames.Length(); ++i) {
    if (!copy.RemoveElement(mergedFrames[i])) {
      AddFrame(mergedFrames[i]);
    }
  }

  for (uint32_t i = 0; i < copy.Length(); i++) {
    RemoveFrame(copy[i]);
  }
}

static nsIFrame* sDestroyedFrame = nullptr;
FrameLayerBuilder::DisplayItemData::~DisplayItemData()
{
  for (uint32_t i = 0; i < mFrameList.Length(); i++) {
    nsIFrame* frame = mFrameList[i];
    if (frame == sDestroyedFrame) {
      continue;
    }
    nsTArray<DisplayItemData*> *array =
      reinterpret_cast<nsTArray<DisplayItemData*>*>(frame->Properties().Get(LayerManagerDataProperty()));
    array->RemoveElement(this);
  }
}

void
FrameLayerBuilder::DisplayItemData::GetFrameListChanges(nsDisplayItem* aOther,
                                                        nsTArray<nsIFrame*>& aOut)
{
  aOut = mFrameList;
  nsAutoTArray<nsIFrame*, 4> added;
  if (!aOut.RemoveElement(aOther->Frame())) {
    added.AppendElement(aOther->Frame());
  }

  nsAutoTArray<nsIFrame*,4> mergedFrames;
  aOther->GetMergedFrames(&mergedFrames);
  for (uint32_t i = 0; i < mergedFrames.Length(); ++i) {
    if (!aOut.RemoveElement(mergedFrames[i])) {
      added.AppendElement(mergedFrames[i]);
    }
  }

  aOut.AppendElements(added);
}

/**
 * This is the userdata we associate with a layer manager.
 */
class LayerManagerData : public LayerUserData {
public:
  LayerManagerData(LayerManager *aManager)
    : mLayerManager(aManager)
#ifdef DEBUG_DISPLAY_ITEM_DATA
    , mParent(nullptr)
#endif
    , mInvalidateAllLayers(false)
  {
    MOZ_COUNT_CTOR(LayerManagerData);
  }
  ~LayerManagerData() {
    MOZ_COUNT_DTOR(LayerManagerData);
  }

#ifdef DEBUG_DISPLAY_ITEM_DATA
  void Dump(const char *aPrefix = "") {
    printf_stderr("%sLayerManagerData %p\n", aPrefix, this);
    nsAutoCString prefix;
    prefix += aPrefix;
    prefix += "  ";
    mDisplayItems.EnumerateEntries(
        FrameLayerBuilder::DumpDisplayItemDataForFrame, (void*)prefix.get());
  }
#endif

  /**
   * Tracks which frames have layers associated with them.
   */
  LayerManager *mLayerManager;
#ifdef DEBUG_DISPLAY_ITEM_DATA
  LayerManagerData *mParent;
#endif
  nsTHashtable<nsRefPtrHashKey<FrameLayerBuilder::DisplayItemData> > mDisplayItems;
  bool mInvalidateAllLayers;
};

/* static */ void
FrameLayerBuilder::DestroyDisplayItemDataFor(nsIFrame* aFrame)
{
  FrameProperties props = aFrame->Properties();
  props.Delete(LayerManagerDataProperty());
}

// a global cache of image containers used for mask layers
static MaskLayerImageCache* gMaskLayerImageCache = nullptr;

static inline MaskLayerImageCache* GetMaskLayerImageCache()
{
  if (!gMaskLayerImageCache) {
    gMaskLayerImageCache = new MaskLayerImageCache();
  }

  return gMaskLayerImageCache;
}

/**
 * We keep a stack of these to represent the ThebesLayers that are
 * currently available to have display items added to.
 * We use a stack here because as much as possible we want to
 * assign display items to existing ThebesLayers, and to the lowest
 * ThebesLayer in z-order. This reduces the number of layers and
 * makes it more likely a display item will be rendered to an opaque
 * layer, giving us the best chance of getting subpixel AA.
 */
class ThebesLayerData {
public:
  ThebesLayerData() :
    mAnimatedGeometryRoot(nullptr),
    mFixedPosFrameForLayerData(nullptr),
    mReferenceFrame(nullptr),
    mLayer(nullptr),
    mIsSolidColorInVisibleRegion(false),
    mSingleItemFixedToViewport(false),
    mNeedComponentAlpha(false),
    mForceTransparentSurface(false),
    mImage(nullptr),
    mCommonClipCount(-1),
    mAllDrawingAbove(false)
  {}
  /**
   * Record that an item has been added to the ThebesLayer, so we
   * need to update our regions.
   * @param aVisibleRect the area of the item that's visible
   * @param aDrawRect the area of the item that would be drawn if it
   * was completely visible
   * @param aOpaqueRect if non-null, the area of the item that's opaque.
   * We pass in a separate opaque rect because the opaque rect can be
   * bigger than the visible rect, and we want to have the biggest
   * opaque rect that we can.
   * @param aSolidColor if non-null, the visible area of the item is
   * a constant color given by *aSolidColor
   */
  void Accumulate(ContainerState* aState,
                  nsDisplayItem* aItem,
                  const nsIntRect& aVisibleRect,
                  const nsIntRect& aDrawRect,
                  const DisplayItemClip& aClip);
  const nsIFrame* GetAnimatedGeometryRoot() { return mAnimatedGeometryRoot; }

  /**
   * Add aHitRegion, aMaybeHitRegion, and aDispatchToContentHitRegion to the
   * hit regions for this ThebesLayer.
   */
  void AccumulateEventRegions(const nsRegion& aHitRegion,
                              const nsRegion& aMaybeHitRegion,
                              const nsRegion& aDispatchToContentHitRegion)
  {
    mHitRegion.Or(mHitRegion, aHitRegion);
    mMaybeHitRegion.Or(mMaybeHitRegion, aMaybeHitRegion);
    mDispatchToContentHitRegion.Or(mDispatchToContentHitRegion, aDispatchToContentHitRegion);
  }

  /**
   * If this represents only a nsDisplayImage, and the image type
   * supports being optimized to an ImageLayer (TYPE_RASTER only) returns
   * an ImageContainer for the image.
   */
  already_AddRefed<ImageContainer> CanOptimizeImageLayer(nsDisplayListBuilder* aBuilder);

  void AddDrawAboveRegion(const nsIntRegion& aAbove)
  {
    if (!mAllDrawingAbove) {
      mDrawAboveRegion.Or(mDrawAboveRegion, aAbove);
      mDrawAboveRegion.SimplifyOutward(4);
    }
  }

  void AddVisibleAboveRegion(const nsIntRegion& aAbove)
  {
    if (!mAllDrawingAbove) {
      mVisibleAboveRegion.Or(mVisibleAboveRegion, aAbove);
      mVisibleAboveRegion.SimplifyOutward(4);
    }
  }

  void CopyAboveRegion(ThebesLayerData* aOther)
  {
    if (aOther->mAllDrawingAbove || mAllDrawingAbove) {
      SetAllDrawingAbove();
    } else {
      mVisibleAboveRegion.Or(mVisibleAboveRegion, aOther->mVisibleAboveRegion);
      mVisibleAboveRegion.Or(mVisibleAboveRegion, aOther->mVisibleRegion);
      mVisibleAboveRegion.SimplifyOutward(4);
      mDrawAboveRegion.Or(mDrawAboveRegion, aOther->mDrawAboveRegion);
      mDrawAboveRegion.Or(mDrawAboveRegion, aOther->mDrawRegion);
      mDrawAboveRegion.SimplifyOutward(4);
   }
  }

  void SetAllDrawingAbove()
  {
    mAllDrawingAbove = true;
    mDrawAboveRegion.SetEmpty();
    mVisibleAboveRegion.SetEmpty();
  }

  bool DrawAboveRegionIntersects(const nsIntRect& aRect)
  {
    return mAllDrawingAbove || mDrawAboveRegion.Intersects(aRect);
  }

  bool DrawRegionIntersects(const nsIntRect& aRect)
  {
    return IsSubjectToAsyncTransforms() || mDrawRegion.Intersects(aRect);
  }

  bool IntersectsVisibleAboveRegion(const nsIntRegion& aVisibleRegion)
  {
    if (mAllDrawingAbove) {
      return true;
    }
    nsIntRegion visibleAboveIntersection;
    visibleAboveIntersection.And(mVisibleAboveRegion, aVisibleRegion);
    if (visibleAboveIntersection.IsEmpty()) {
      return false;
    }
    return true;
  }

  bool IsSubjectToAsyncTransforms()
  {
    return mFixedPosFrameForLayerData != nullptr;
  }

  /**
   * The region of visible content in the layer, relative to the
   * container layer (which is at the snapped top-left of the display
   * list reference frame).
   */
  nsIntRegion  mVisibleRegion;
  /**
   * The region containing the bounds of all display items in the layer,
   * regardless of visbility.
   * Same coordinate system as mVisibleRegion.
   * This is a conservative approximation: it contains the true region.
   */
  nsIntRegion  mDrawRegion;
  /**
   * The region of visible content in the layer that is opaque.
   * Same coordinate system as mVisibleRegion.
   */
  nsIntRegion  mOpaqueRegion;
  /**
   * The definitely-hit region for this ThebesLayer.
   */
  nsRegion  mHitRegion;
  /**
   * The maybe-hit region for this ThebesLayer.
   */
  nsRegion  mMaybeHitRegion;
  /**
   * The dispatch-to-content hit region for this ThebesLayer.
   */
  nsRegion  mDispatchToContentHitRegion;
  /**
   * The "active scrolled root" for all content in the layer. Must
   * be non-null; all content in a ThebesLayer must have the same
   * active scrolled root.
   */
  const nsIFrame* mAnimatedGeometryRoot;
  /**
   * If non-null, the frame from which we'll extract "fixed positioning"
   * metadata for this layer. This can be a position:fixed frame or a viewport
   * frame; the latter case is used for background-attachment:fixed content.
   */
  const nsIFrame* mFixedPosFrameForLayerData;
  const nsIFrame* mReferenceFrame;
  ThebesLayer* mLayer;
  /**
   * If mIsSolidColorInVisibleRegion is true, this is the color of the visible
   * region.
   */
  nscolor      mSolidColor;
  /**
   * True if every pixel in mVisibleRegion will have color mSolidColor.
   */
  bool mIsSolidColorInVisibleRegion;
  /**
   * True if the layer contains exactly one item that returned true for
   * ShouldFixToViewport.
   */
  bool mSingleItemFixedToViewport;
  /**
   * True if there is any text visible in the layer that's over
   * transparent pixels in the layer.
   */
  bool mNeedComponentAlpha;
  /**
   * Set if the layer should be treated as transparent, even if its entire
   * area is covered by opaque display items. For example, this needs to
   * be set if something is going to "punch holes" in the layer by clearing
   * part of its surface.
   */
  bool mForceTransparentSurface;

  /**
   * Stores the pointer to the nsDisplayImage if we want to
   * convert this to an ImageLayer.
   */
  nsDisplayImageContainer* mImage;
  /**
   * Stores the clip that we need to apply to the image or, if there is no
   * image, a clip for SOME item in the layer. There is no guarantee which
   * item's clip will be stored here and mItemClip should not be used to clip
   * the whole layer - only some part of the clip should be used, as determined
   * by ThebesDisplayItemLayerUserData::GetCommonClipCount() - which may even be
   * no part at all.
   */
  DisplayItemClip mItemClip;
  /**
   * The first mCommonClipCount rounded rectangle clips are identical for
   * all items in the layer.
   * -1 if there are no items in the layer; must be >=0 by the time that this
   * data is popped from the stack.
   */
  int32_t mCommonClipCount;
  /*
   * Updates mCommonClipCount by checking for rounded rect clips in common
   * between the clip on a new item (aCurrentClip) and the common clips
   * on items already in the layer (the first mCommonClipCount rounded rects
   * in mItemClip).
   */
  void UpdateCommonClipCount(const DisplayItemClip& aCurrentClip);

private:
  /**
   * The region of visible content above the layer and below the
   * next ThebesLayerData currently in the stack, if any. Note that not
   * all ThebesLayers for the container are in the ThebesLayerData stack.
   * Same coordinate system as mVisibleRegion.
   * This is a conservative approximation: it contains the true region.
   */
  nsIntRegion  mVisibleAboveRegion;
  /**
   * The region containing the bounds of all display items (regardless
   * of visibility) in the layer and below the next ThebesLayerData
   * currently in the stack, if any.
   * Note that not all ThebesLayers for the container are in the
   * ThebesLayerData stack.
   * Same coordinate system as mVisibleRegion.
   */
  nsIntRegion  mDrawAboveRegion;
  /**
   * True if mDrawAboveRegion and mVisibleAboveRegion should be treated
   * as infinite, and all display items should be considered 'above' this layer.
   */
  bool mAllDrawingAbove;
};

/**
 * This is a helper object used to build up the layer children for
 * a ContainerLayer.
 */
class ContainerState {
public:
  ContainerState(nsDisplayListBuilder* aBuilder,
                 LayerManager* aManager,
                 FrameLayerBuilder* aLayerBuilder,
                 nsIFrame* aContainerFrame,
                 nsDisplayItem* aContainerItem,
                 ContainerLayer* aContainerLayer,
                 const ContainerLayerParameters& aParameters) :
    mBuilder(aBuilder), mManager(aManager),
    mLayerBuilder(aLayerBuilder),
    mContainerFrame(aContainerFrame),
    mContainerLayer(aContainerLayer),
    mParameters(aParameters),
    mNextFreeRecycledThebesLayer(0)
  {
    nsPresContext* presContext = aContainerFrame->PresContext();
    mAppUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
    mContainerReferenceFrame =
      const_cast<nsIFrame*>(aContainerItem ? aContainerItem->ReferenceFrameForChildren() :
                                             mBuilder->FindReferenceFrameFor(mContainerFrame));
    mContainerAnimatedGeometryRoot = aContainerItem
      ? nsLayoutUtils::GetAnimatedGeometryRootFor(aContainerItem, aBuilder)
      : mContainerReferenceFrame;
    // When AllowResidualTranslation is false, display items will be drawn
    // scaled with a translation by integer pixels, so we know how the snapping
    // will work.
    mSnappingEnabled = aManager->IsSnappingEffectiveTransforms() &&
      !mParameters.AllowResidualTranslation();
    CollectOldLayers();
  }

  enum ProcessDisplayItemsFlags {
    NO_COMPONENT_ALPHA = 0x01,
  };

  /**
   * This is the method that actually walks a display list and builds
   * the child layers.
   */
  void ProcessDisplayItems(nsDisplayList* aList, uint32_t aFlags);
  /**
   * This finalizes all the open ThebesLayers by popping every element off
   * mThebesLayerDataStack, then sets the children of the container layer
   * to be all the layers in mNewChildLayers in that order and removes any
   * layers as children of the container that aren't in mNewChildLayers.
   * @param aTextContentFlags if any child layer has CONTENT_COMPONENT_ALPHA,
   * set *aTextContentFlags to CONTENT_COMPONENT_ALPHA
   */
  void Finish(uint32_t *aTextContentFlags, LayerManagerData* aData);

  nsRect GetChildrenBounds() { return mBounds; }

  nscoord GetAppUnitsPerDevPixel() { return mAppUnitsPerDevPixel; }

  nsIntRect ScaleToNearestPixels(const nsRect& aRect)
  {
    return aRect.ScaleToNearestPixels(mParameters.mXScale, mParameters.mYScale,
                                      mAppUnitsPerDevPixel);
  }
  nsIntRegion ScaleRegionToNearestPixels(const nsRegion& aRegion)
  {
    return aRegion.ScaleToNearestPixels(mParameters.mXScale, mParameters.mYScale,
                                        mAppUnitsPerDevPixel);
  }
  nsIntRect ScaleToOutsidePixels(const nsRect& aRect, bool aSnap = false)
  {
    if (aSnap && mSnappingEnabled) {
      return ScaleToNearestPixels(aRect);
    }
    return aRect.ScaleToOutsidePixels(mParameters.mXScale, mParameters.mYScale,
                                      mAppUnitsPerDevPixel);
  }
  nsIntRect ScaleToInsidePixels(const nsRect& aRect, bool aSnap = false)
  {
    if (aSnap && mSnappingEnabled) {
      return ScaleToNearestPixels(aRect);
    }
    return aRect.ScaleToInsidePixels(mParameters.mXScale, mParameters.mYScale,
                                     mAppUnitsPerDevPixel);
  }

  nsIntRegion ScaleRegionToInsidePixels(const nsRegion& aRegion, bool aSnap = false)
  {
    if (aSnap && mSnappingEnabled) {
      return ScaleRegionToNearestPixels(aRegion);
    }
    return aRegion.ScaleToInsidePixels(mParameters.mXScale, mParameters.mYScale,
                                        mAppUnitsPerDevPixel);
  }

  nsIntRegion ScaleRegionToOutsidePixels(const nsRegion& aRegion, bool aSnap = false)
  {
    if (aSnap && mSnappingEnabled) {
      return ScaleRegionToNearestPixels(aRegion);
    }
    return aRegion.ScaleToOutsidePixels(mParameters.mXScale, mParameters.mYScale,
                                        mAppUnitsPerDevPixel);
  }

protected:
  friend class ThebesLayerData;

  /**
   * Grab the next recyclable ThebesLayer, or create one if there are no
   * more recyclable ThebesLayers. Does any necessary invalidation of
   * a recycled ThebesLayer, and sets up the transform on the ThebesLayer
   * to account for scrolling.
   */
  already_AddRefed<ThebesLayer> CreateOrRecycleThebesLayer(const nsIFrame* aAnimatedGeometryRoot,
                                                           const nsIFrame *aReferenceFrame,
                                                           const nsPoint& aTopLeft);
  /**
   * Grab the next recyclable ColorLayer, or create one if there are no
   * more recyclable ColorLayers.
   */
  already_AddRefed<ColorLayer> CreateOrRecycleColorLayer(ThebesLayer* aThebes);
  /**
   * Grab the next recyclable ImageLayer, or create one if there are no
   * more recyclable ImageLayers.
   */
  already_AddRefed<ImageLayer> CreateOrRecycleImageLayer(ThebesLayer* aThebes);
  /**
   * Grab a recyclable ImageLayer for use as a mask layer for aLayer (that is a
   * mask layer which has been used for aLayer before), or create one if such
   * a layer doesn't exist.
   */
  already_AddRefed<ImageLayer> CreateOrRecycleMaskImageLayerFor(Layer* aLayer);
  /**
   * Grabs all ThebesLayers and ColorLayers from the ContainerLayer and makes them
   * available for recycling.
   */
  void CollectOldLayers();
  /**
   * If aItem used to belong to a ThebesLayer, invalidates the area of
   * aItem in that layer. If aNewLayer is a ThebesLayer, invalidates the area of
   * aItem in that layer.
   */
  void InvalidateForLayerChange(nsDisplayItem* aItem,
                                Layer* aNewLayer,
                                const DisplayItemClip& aClip,
                                const nsPoint& aTopLeft,
                                nsDisplayItemGeometry *aGeometry);
  /**
   * Try to determine whether the ThebesLayer at aThebesLayerIndex
   * has a single opaque color behind it, over the entire bounds of its visible
   * region.
   * If successful, return that color, otherwise return NS_RGBA(0,0,0,0).
   */
  nscolor FindOpaqueBackgroundColorFor(int32_t aThebesLayerIndex);
  /**
   * Find the fixed-pos frame, if any, containing (or equal to)
   * aAnimatedGeometryRoot. Only return a fixed-pos frame if its viewport
   * has a displayport. Updates *aVisibleRegion to be the intersection of
   * aDrawRegion and the displayport, and updates *aIsSolidColorInVisibleRegion
   * (if non-null) to false if the visible region grows.
   * aDisplayItemFixedToViewport is true if the layer contains a single display
   * item which returned true for ShouldFixToViewport.
   * This can return the actual viewport frame for layers whose display items
   * are directly on the viewport (e.g. background-attachment:fixed backgrounds).
   */
  const nsIFrame* FindFixedPosFrameForLayerData(const nsIFrame* aAnimatedGeometryRoot,
                                                bool aDisplayItemFixedToViewport);
  void AdjustLayerDataForFixedPositioning(const nsIFrame* aFixedPosFrame,
                                          const nsIntRegion& aDrawRegion,
                                          nsIntRegion* aVisibleRegion,
                                          bool* aIsSolidColorInVisibleRegion = nullptr);
  /**
   * Set fixed-pos layer metadata on aLayer according to the data for aFixedPosFrame.
   */
  void SetFixedPositionLayerData(Layer* aLayer,
                                 const nsIFrame* aFixedPosFrame);
  /**
   * Indicate that we are done adding items to the ThebesLayer at the top of
   * mThebesLayerDataStack. Set the final visible region and opaque-content
   * flag, and pop it off the stack.
   */
  void PopThebesLayerData();
  /**
   * Find the ThebesLayer to which we should assign the next display item.
   * We scan the ThebesLayerData stack to find the topmost ThebesLayer
   * that is compatible with the display item (i.e., has the same
   * active scrolled root), and that has no content from other layers above
   * it and intersecting the aVisibleRect.
   * Returns the layer, and also updates the ThebesLayerData. Will
   * push a new ThebesLayerData onto the stack if no suitable existing
   * layer is found. If we choose a ThebesLayer that's already on the
   * ThebesLayerData stack, later elements on the stack will be popped off.
   * @param aVisibleRect the area of the next display item that's visible
   * @param aAnimatedGeometryRoot the active scrolled root for the next
   * display item
   * @param aOpaqueRect if non-null, a region of the display item that is opaque
   * @param aSolidColor if non-null, indicates that every pixel in aVisibleRect
   * will be painted with aSolidColor by the item
   * @param aShouldFixToViewport if true, aAnimatedGeometryRoot is the viewport
   * and we will be adding fixed-pos metadata for this layer because the
   * display item returned true from ShouldFixToViewport.
   */
  ThebesLayerData* FindThebesLayerFor(nsDisplayItem* aItem,
                                      const nsIntRect& aVisibleRect,
                                      const nsIFrame* aAnimatedGeometryRoot,
                                      const nsPoint& aTopLeft,
                                      bool aShouldFixToViewport);
  ThebesLayerData* GetTopThebesLayerData()
  {
    return mThebesLayerDataStack.IsEmpty() ? nullptr
        : mThebesLayerDataStack[mThebesLayerDataStack.Length() - 1].get();
  }

  /* Build a mask layer to represent the clipping region. Will return null if
   * there is no clipping specified or a mask layer cannot be built.
   * Builds an ImageLayer for the appropriate backend; the mask is relative to
   * aLayer's visible region.
   * aLayer is the layer to be clipped.
   * aRoundedRectClipCount is used when building mask layers for ThebesLayers,
   * SetupMaskLayer will build a mask layer for only the first
   * aRoundedRectClipCount rounded rects in aClip
   */
  void SetupMaskLayer(Layer *aLayer, const DisplayItemClip& aClip,
                      uint32_t aRoundedRectClipCount = UINT32_MAX);

  bool ChooseAnimatedGeometryRoot(const nsDisplayList& aList,
                                  const nsIFrame **aAnimatedGeometryRoot);

  nsDisplayListBuilder*            mBuilder;
  LayerManager*                    mManager;
  FrameLayerBuilder*               mLayerBuilder;
  nsIFrame*                        mContainerFrame;
  nsIFrame*                        mContainerReferenceFrame;
  const nsIFrame*                  mContainerAnimatedGeometryRoot;
  ContainerLayer*                  mContainerLayer;
  ContainerLayerParameters         mParameters;
  /**
   * The region of ThebesLayers that should be invalidated every time
   * we recycle one.
   */
  nsIntRegion                      mInvalidThebesContent;
  nsRect                           mBounds;
  nsAutoTArray<nsAutoPtr<ThebesLayerData>,1>  mThebesLayerDataStack;
  /**
   * We collect the list of children in here. During ProcessDisplayItems,
   * the layers in this array either have mContainerLayer as their parent,
   * or no parent.
   */
  typedef nsAutoTArray<nsRefPtr<Layer>,1> AutoLayersArray;
  AutoLayersArray                  mNewChildLayers;
  nsTArray<nsRefPtr<ThebesLayer> > mRecycledThebesLayers;
  nsDataHashtable<nsPtrHashKey<Layer>, nsRefPtr<ImageLayer> >
    mRecycledMaskImageLayers;
  uint32_t                         mNextFreeRecycledThebesLayer;
  nscoord                          mAppUnitsPerDevPixel;
  bool                             mSnappingEnabled;
};

class ThebesDisplayItemLayerUserData : public LayerUserData
{
public:
  ThebesDisplayItemLayerUserData() :
    mMaskClipCount(0),
    mForcedBackgroundColor(NS_RGBA(0,0,0,0)),
    mXScale(1.f), mYScale(1.f),
    mAppUnitsPerDevPixel(0),
    mTranslation(0, 0),
    mAnimatedGeometryRootPosition(0, 0) {}

  /**
   * Record the number of clips in the Thebes layer's mask layer.
   * Should not be reset when the layer is recycled since it is used to track
   * changes in the use of mask layers.
   */
  uint32_t mMaskClipCount;

  /**
   * A color that should be painted over the bounds of the layer's visible
   * region before any other content is painted.
   */
  nscolor mForcedBackgroundColor;

  /**
   * The resolution scale used.
   */
  float mXScale, mYScale;

  /**
   * The appunits per dev pixel for the items in this layer.
   */
  nscoord mAppUnitsPerDevPixel;

  /**
   * The offset from the ThebesLayer's 0,0 to the
   * reference frame. This isn't necessarily the same as the transform
   * set on the ThebesLayer since we might also be applying an extra
   * offset specified by the parent ContainerLayer/
   */
  nsIntPoint mTranslation;

  /**
   * We try to make 0,0 of the ThebesLayer be the top-left of the
   * border-box of the "active scrolled root" frame (i.e. the nearest ancestor
   * frame for the display items that is being actively scrolled). But
   * we force the ThebesLayer transform to be an integer translation, and we may
   * have a resolution scale, so we have to snap the ThebesLayer transform, so
   * 0,0 may not be exactly the top-left of the active scrolled root. Here we
   * store the coordinates in ThebesLayer space of the top-left of the
   * active scrolled root.
   */
  gfxPoint mAnimatedGeometryRootPosition;

  nsIntRegion mRegionToInvalidate;

  // The offset between the active scrolled root of this layer
  // and the root of the container for the previous and current
  // paints respectively.
  nsPoint mLastAnimatedGeometryRootOrigin;
  nsPoint mAnimatedGeometryRootOrigin;

  nsRefPtr<ColorLayer> mColorLayer;
  nsRefPtr<ImageLayer> mImageLayer;
};

/*
 * User data for layers which will be used as masks.
 */
struct MaskLayerUserData : public LayerUserData
{
  MaskLayerUserData()
    : mScaleX(-1.0f)
    , mScaleY(-1.0f)
    , mAppUnitsPerDevPixel(-1)
  { }

  bool
  operator== (const MaskLayerUserData& aOther) const
  {
    return mRoundedClipRects == aOther.mRoundedClipRects &&
           mScaleX == aOther.mScaleX &&
           mScaleY == aOther.mScaleY &&
           mOffset == aOther.mOffset &&
           mAppUnitsPerDevPixel == aOther.mAppUnitsPerDevPixel;
  }

  nsRefPtr<const MaskLayerImageCache::MaskLayerImageKey> mImageKey;
  // properties of the mask layer; the mask layer may be re-used if these
  // remain unchanged.
  nsTArray<DisplayItemClip::RoundedRect> mRoundedClipRects;
  // scale from the masked layer which is applied to the mask
  float mScaleX, mScaleY;
  // The ContainerLayerParameters offset which is applied to the mask's transform.
  nsIntPoint mOffset;
  int32_t mAppUnitsPerDevPixel;
};

/**
 * The address of gThebesDisplayItemLayerUserData is used as the user
 * data key for ThebesLayers created by FrameLayerBuilder.
 * It identifies ThebesLayers used to draw non-layer content, which are
 * therefore eligible for recycling. We want display items to be able to
 * create their own dedicated ThebesLayers in BuildLayer, if necessary,
 * and we wouldn't want to accidentally recycle those.
 * The user data is a ThebesDisplayItemLayerUserData.
 */
uint8_t gThebesDisplayItemLayerUserData;
/**
 * The address of gColorLayerUserData is used as the user
 * data key for ColorLayers created by FrameLayerBuilder.
 * The user data is null.
 */
uint8_t gColorLayerUserData;
/**
 * The address of gImageLayerUserData is used as the user
 * data key for ImageLayers created by FrameLayerBuilder.
 * The user data is null.
 */
uint8_t gImageLayerUserData;
/**
 * The address of gLayerManagerUserData is used as the user
 * data key for retained LayerManagers managed by FrameLayerBuilder.
 * The user data is a LayerManagerData.
 */
uint8_t gLayerManagerUserData;
/**
 * The address of gMaskLayerUserData is used as the user
 * data key for mask layers managed by FrameLayerBuilder.
 * The user data is a MaskLayerUserData.
 */
uint8_t gMaskLayerUserData;

/**
  * Helper functions for getting user data and casting it to the correct type.
  * aLayer is the layer where the user data is stored.
  */
MaskLayerUserData* GetMaskLayerUserData(Layer* aLayer)
{
  return static_cast<MaskLayerUserData*>(aLayer->GetUserData(&gMaskLayerUserData));
}

ThebesDisplayItemLayerUserData* GetThebesDisplayItemLayerUserData(Layer* aLayer)
{
  return static_cast<ThebesDisplayItemLayerUserData*>(
    aLayer->GetUserData(&gThebesDisplayItemLayerUserData));
}

/* static */ void
FrameLayerBuilder::Shutdown()
{
  if (gMaskLayerImageCache) {
    delete gMaskLayerImageCache;
    gMaskLayerImageCache = nullptr;
  }
}

void
FrameLayerBuilder::Init(nsDisplayListBuilder* aBuilder, LayerManager* aManager,
                        ThebesLayerData* aLayerData)
{
  mDisplayListBuilder = aBuilder;
  mRootPresContext = aBuilder->RootReferenceFrame()->PresContext()->GetRootPresContext();
  if (mRootPresContext) {
    mInitialDOMGeneration = mRootPresContext->GetDOMGeneration();
  }
  mContainingThebesLayer = aLayerData;
  aManager->SetUserData(&gLayerManagerLayerBuilder, this);
}

void
FrameLayerBuilder::FlashPaint(gfxContext *aContext)
{
  float r = float(rand()) / RAND_MAX;
  float g = float(rand()) / RAND_MAX;
  float b = float(rand()) / RAND_MAX;
  aContext->SetColor(gfxRGBA(r, g, b, 0.4));
  aContext->Paint();
}

FrameLayerBuilder::DisplayItemData*
FrameLayerBuilder::GetDisplayItemData(nsIFrame* aFrame, uint32_t aKey)
{
  nsTArray<DisplayItemData*> *array =
    reinterpret_cast<nsTArray<DisplayItemData*>*>(aFrame->Properties().Get(LayerManagerDataProperty()));
  if (array) {
    for (uint32_t i = 0; i < array->Length(); i++) {
      DisplayItemData* item = array->ElementAt(i);
      if (item->mDisplayItemKey == aKey &&
          item->mLayer->Manager() == mRetainingManager) {
        return item;
      }
    }
  }
  return nullptr;
}

nsACString&
AppendToString(nsACString& s, const nsIntRect& r,
               const char* pfx="", const char* sfx="")
{
  s += pfx;
  s += nsPrintfCString(
    "(x=%d, y=%d, w=%d, h=%d)",
    r.x, r.y, r.width, r.height);
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const nsIntRegion& r,
               const char* pfx="", const char* sfx="")
{
  s += pfx;

  nsIntRegionRectIterator it(r);
  s += "< ";
  while (const nsIntRect* sr = it.Next()) {
    AppendToString(s, *sr) += "; ";
  }
  s += ">";

  return s += sfx;
}

/**
 * Invalidate aRegion in aLayer. aLayer is in the coordinate system
 * *after* aTranslation has been applied, so we need to
 * apply the inverse of that transform before calling InvalidateRegion.
 */
static void
InvalidatePostTransformRegion(ThebesLayer* aLayer, const nsIntRegion& aRegion,
                              const nsIntPoint& aTranslation)
{
  // Convert the region from the coordinates of the container layer
  // (relative to the snapped top-left of the display list reference frame)
  // to the ThebesLayer's own coordinates
  nsIntRegion rgn = aRegion;
  rgn.MoveBy(-aTranslation);
  aLayer->InvalidateRegion(rgn);
#ifdef MOZ_DUMP_PAINTING
  if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
    nsAutoCString str;
    AppendToString(str, rgn);
    printf_stderr("Invalidating layer %p: %s\n", aLayer, str.get());
  }
#endif
}

static void
InvalidatePostTransformRegion(ThebesLayer* aLayer, const nsRect& aRect,
                              const DisplayItemClip& aClip,
                              const nsIntPoint& aTranslation)
{
  ThebesDisplayItemLayerUserData* data =
      static_cast<ThebesDisplayItemLayerUserData*>(aLayer->GetUserData(&gThebesDisplayItemLayerUserData));

  nsRect rect = aClip.ApplyNonRoundedIntersection(aRect);

  nsIntRect pixelRect = rect.ScaleToOutsidePixels(data->mXScale, data->mYScale, data->mAppUnitsPerDevPixel);
  InvalidatePostTransformRegion(aLayer, pixelRect, aTranslation);
}


static nsIntPoint
GetTranslationForThebesLayer(ThebesLayer* aLayer)
{
  ThebesDisplayItemLayerUserData* data =
    static_cast<ThebesDisplayItemLayerUserData*>
      (aLayer->GetUserData(&gThebesDisplayItemLayerUserData));
  NS_ASSERTION(data, "Must be a tracked thebes layer!");

  return data->mTranslation;
}

/**
 * Some frames can have multiple, nested, retaining layer managers
 * associated with them (normal manager, inactive managers, SVG effects).
 * In these cases we store the 'outermost' LayerManager data property
 * on the frame since we can walk down the chain from there.
 *
 * If one of these frames has just been destroyed, we will free the inner
 * layer manager when removing the entry from mFramesWithLayers. Destroying
 * the layer manager destroys the LayerManagerData and calls into
 * the DisplayItemData destructor. If the inner layer manager had any
 * items with the same frame, then we attempt to retrieve properties
 * from the deleted frame.
 *
 * Cache the destroyed frame pointer here so we can avoid crashing in this case.
 */

/* static */ void
FrameLayerBuilder::RemoveFrameFromLayerManager(nsIFrame* aFrame,
                                               void* aPropertyValue)
{
  sDestroyedFrame = aFrame;
  nsTArray<DisplayItemData*> *array =
    reinterpret_cast<nsTArray<DisplayItemData*>*>(aPropertyValue);

  // Hold a reference to all the items so that they don't get
  // deleted from under us.
  nsTArray<nsRefPtr<DisplayItemData> > arrayCopy;
  for (uint32_t i = 0; i < array->Length(); ++i) {
    arrayCopy.AppendElement(array->ElementAt(i));
  }

#ifdef DEBUG_DISPLAY_ITEM_DATA
  if (array->Length()) {
    LayerManagerData *rootData = array->ElementAt(0)->mParent;
    while (rootData->mParent) {
      rootData = rootData->mParent;
    }
    printf_stderr("Removing frame %p - dumping display data\n", aFrame);
    rootData->Dump();
  }
#endif

  for (uint32_t i = 0; i < array->Length(); ++i) {
    DisplayItemData* data = array->ElementAt(i);

    ThebesLayer* t = data->mLayer->AsThebesLayer();
    if (t) {
      ThebesDisplayItemLayerUserData* thebesData =
          static_cast<ThebesDisplayItemLayerUserData*>(t->GetUserData(&gThebesDisplayItemLayerUserData));
      if (thebesData) {
        nsRegion old = data->mGeometry->ComputeInvalidationRegion();
        nsIntRegion rgn = old.ScaleToOutsidePixels(thebesData->mXScale, thebesData->mYScale, thebesData->mAppUnitsPerDevPixel);
        rgn.MoveBy(-GetTranslationForThebesLayer(t));
        thebesData->mRegionToInvalidate.Or(thebesData->mRegionToInvalidate, rgn);
        thebesData->mRegionToInvalidate.SimplifyOutward(8);
      }
    }

    data->mParent->mDisplayItems.RemoveEntry(data);
  }

  arrayCopy.Clear();
  delete array;
  sDestroyedFrame = nullptr;
}

void
FrameLayerBuilder::DidBeginRetainedLayerTransaction(LayerManager* aManager)
{
  mRetainingManager = aManager;
  LayerManagerData* data = static_cast<LayerManagerData*>
    (aManager->GetUserData(&gLayerManagerUserData));
  if (data) {
    mInvalidateAllLayers = data->mInvalidateAllLayers;
  } else {
    data = new LayerManagerData(aManager);
    aManager->SetUserData(&gLayerManagerUserData, data);
  }
}

void
FrameLayerBuilder::StoreOptimizedLayerForFrame(nsDisplayItem* aItem, Layer* aLayer)
{
  if (!mRetainingManager) {
    return;
  }

  DisplayItemData* data = GetDisplayItemDataForManager(aItem, aLayer->Manager());
  NS_ASSERTION(data, "Must have already stored data for this item!");
  data->mOptLayer = aLayer;
}

void
FrameLayerBuilder::DidEndTransaction()
{
  GetMaskLayerImageCache()->Sweep();
}

void
FrameLayerBuilder::WillEndTransaction()
{
  if (!mRetainingManager) {
    return;
  }

  // We need to save the data we'll need to support retaining.
  LayerManagerData* data = static_cast<LayerManagerData*>
    (mRetainingManager->GetUserData(&gLayerManagerUserData));
  NS_ASSERTION(data, "Must have data!");
  // Update all the frames that used to have layers.
  data->mDisplayItems.EnumerateEntries(ProcessRemovedDisplayItems, this);
  data->mInvalidateAllLayers = false;
}

/* static */ PLDHashOperator
FrameLayerBuilder::ProcessRemovedDisplayItems(nsRefPtrHashKey<DisplayItemData>* aEntry,
                                              void* aUserArg)
{
  DisplayItemData* data = aEntry->GetKey();
  if (!data->mUsed) {
    // This item was visible, but isn't anymore.
    FrameLayerBuilder* layerBuilder = static_cast<FrameLayerBuilder*>(aUserArg);

    ThebesLayer* t = data->mLayer->AsThebesLayer();
    if (t) {
#ifdef MOZ_DUMP_PAINTING
      if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
        printf_stderr("Invalidating unused display item (%i) belonging to frame %p from layer %p\n", data->mDisplayItemKey, data->mFrameList[0], t);
      }
#endif
      InvalidatePostTransformRegion(t,
                                    data->mGeometry->ComputeInvalidationRegion(),
                                    data->mClip,
                                    layerBuilder->GetLastPaintOffset(t));
    }
    return PL_DHASH_REMOVE;
  }

  data->mUsed = false;
  data->mIsInvalid = false;
  return PL_DHASH_NEXT;
}

/* static */ PLDHashOperator
FrameLayerBuilder::DumpDisplayItemDataForFrame(nsRefPtrHashKey<DisplayItemData>* aEntry,
                                               void* aClosure)
{
#ifdef DEBUG_DISPLAY_ITEM_DATA
  DisplayItemData *data = aEntry->GetKey();

  nsAutoCString prefix;
  prefix += static_cast<const char*>(aClosure);

  const char *layerState;
  switch (data->mLayerState) {
    case LAYER_NONE:
      layerState = "LAYER_NONE"; break;
    case LAYER_INACTIVE:
      layerState = "LAYER_INACTIVE"; break;
    case LAYER_ACTIVE:
      layerState = "LAYER_ACTIVE"; break;
    case LAYER_ACTIVE_FORCE:
      layerState = "LAYER_ACTIVE_FORCE"; break;
    case LAYER_ACTIVE_EMPTY:
      layerState = "LAYER_ACTIVE_EMPTY"; break;
    case LAYER_SVG_EFFECTS:
      layerState = "LAYER_SVG_EFFECTS"; break;
  }
  uint32_t mask = (1 << nsDisplayItem::TYPE_BITS) - 1;

  nsAutoCString str;
  str += prefix;
  str += nsPrintfCString("Frame %p ", data->mFrameList[0]);
  str += nsDisplayItem::DisplayItemTypeName(static_cast<nsDisplayItem::Type>(data->mDisplayItemKey & mask));
  if ((data->mDisplayItemKey >> nsDisplayItem::TYPE_BITS)) {
    str += nsPrintfCString("(%i)", data->mDisplayItemKey >> nsDisplayItem::TYPE_BITS);
  }
  str += nsPrintfCString(", %s, Layer %p", layerState, data->mLayer.get());
  if (data->mOptLayer) {
    str += nsPrintfCString(", OptLayer %p", data->mOptLayer.get());
  }
  if (data->mInactiveManager) {
    str += nsPrintfCString(", InactiveLayerManager %p", data->mInactiveManager.get());
  }
  str += "\n";

  printf_stderr("%s", str.get());

  if (data->mInactiveManager) {
    prefix += "  ";
    printf_stderr("%sDumping inactive layer info:\n", prefix.get());
    LayerManagerData* lmd = static_cast<LayerManagerData*>
      (data->mInactiveManager->GetUserData(&gLayerManagerUserData));
    lmd->Dump(prefix.get());
  }
#endif
  return PL_DHASH_NEXT;
}

/* static */ FrameLayerBuilder::DisplayItemData*
FrameLayerBuilder::GetDisplayItemDataForManager(nsDisplayItem* aItem,
                                                LayerManager* aManager)
{
  nsTArray<DisplayItemData*> *array =
    reinterpret_cast<nsTArray<DisplayItemData*>*>(aItem->Frame()->Properties().Get(LayerManagerDataProperty()));
  if (array) {
    for (uint32_t i = 0; i < array->Length(); i++) {
      DisplayItemData* item = array->ElementAt(i);
      if (item->mDisplayItemKey == aItem->GetPerFrameKey() &&
          item->mLayer->Manager() == aManager) {
        return item;
      }
    }
  }
  return nullptr;
}

bool
FrameLayerBuilder::HasRetainedDataFor(nsIFrame* aFrame, uint32_t aDisplayItemKey)
{
  nsTArray<DisplayItemData*> *array =
    reinterpret_cast<nsTArray<DisplayItemData*>*>(aFrame->Properties().Get(LayerManagerDataProperty()));
  if (array) {
    for (uint32_t i = 0; i < array->Length(); i++) {
      if (array->ElementAt(i)->mDisplayItemKey == aDisplayItemKey) {
        return true;
      }
    }
  }
  return false;
}

void
FrameLayerBuilder::IterateRetainedDataFor(nsIFrame* aFrame, DisplayItemDataCallback aCallback)
{
  nsTArray<DisplayItemData*> *array =
    reinterpret_cast<nsTArray<DisplayItemData*>*>(aFrame->Properties().Get(LayerManagerDataProperty()));
  if (!array) {
    return;
  }

  for (uint32_t i = 0; i < array->Length(); i++) {
    DisplayItemData* data = array->ElementAt(i);
    if (data->mDisplayItemKey != nsDisplayItem::TYPE_ZERO) {
      aCallback(aFrame, data);
    }
  }
}

FrameLayerBuilder::DisplayItemData*
FrameLayerBuilder::GetOldLayerForFrame(nsIFrame* aFrame, uint32_t aDisplayItemKey)
{
  // If we need to build a new layer tree, then just refuse to recycle
  // anything.
  if (!mRetainingManager || mInvalidateAllLayers)
    return nullptr;

  DisplayItemData *data = GetDisplayItemData(aFrame, aDisplayItemKey);

  if (data && data->mLayer->Manager() == mRetainingManager) {
    return data;
  }
  return nullptr;
}

Layer*
FrameLayerBuilder::GetOldLayerFor(nsDisplayItem* aItem,
                                  nsDisplayItemGeometry** aOldGeometry,
                                  DisplayItemClip** aOldClip,
                                  nsTArray<nsIFrame*>* aChangedFrames,
                                  bool *aIsInvalid)
{
  uint32_t key = aItem->GetPerFrameKey();
  nsIFrame* frame = aItem->Frame();

  DisplayItemData* oldData = GetOldLayerForFrame(frame, key);
  if (oldData) {
    if (aOldGeometry) {
      *aOldGeometry = oldData->mGeometry.get();
    }
    if (aOldClip) {
      *aOldClip = &oldData->mClip;
    }
    if (aChangedFrames) {
      oldData->GetFrameListChanges(aItem, *aChangedFrames);
    }
    if (aIsInvalid) {
      *aIsInvalid = oldData->mIsInvalid;
    }
    return oldData->mLayer;
  }

  return nullptr;
}

/* static */ Layer*
FrameLayerBuilder::GetDebugOldLayerFor(nsIFrame* aFrame, uint32_t aDisplayItemKey)
{
  nsTArray<DisplayItemData*> *array =
    reinterpret_cast<nsTArray<DisplayItemData*>*>(aFrame->Properties().Get(LayerManagerDataProperty()));

  if (!array) {
    return nullptr;
  }

  for (uint32_t i = 0; i < array->Length(); i++) {
    DisplayItemData *data = array->ElementAt(i);

    if (data->mDisplayItemKey == aDisplayItemKey) {
      return data->mLayer;
    }
  }
  return nullptr;
}

already_AddRefed<ColorLayer>
ContainerState::CreateOrRecycleColorLayer(ThebesLayer *aThebes)
{
  ThebesDisplayItemLayerUserData* data =
      static_cast<ThebesDisplayItemLayerUserData*>(aThebes->GetUserData(&gThebesDisplayItemLayerUserData));
  nsRefPtr<ColorLayer> layer = data->mColorLayer;
  if (layer) {
    layer->SetMaskLayer(nullptr);
  } else {
    // Create a new layer
    layer = mManager->CreateColorLayer();
    if (!layer)
      return nullptr;
    // Mark this layer as being used for Thebes-painting display items
    data->mColorLayer = layer;
    layer->SetUserData(&gColorLayerUserData, nullptr);

    // Remove other layer types we might have stored for this ThebesLayer
    data->mImageLayer = nullptr;
  }
  return layer.forget();
}

already_AddRefed<ImageLayer>
ContainerState::CreateOrRecycleImageLayer(ThebesLayer *aThebes)
{
  ThebesDisplayItemLayerUserData* data =
      static_cast<ThebesDisplayItemLayerUserData*>(aThebes->GetUserData(&gThebesDisplayItemLayerUserData));
  nsRefPtr<ImageLayer> layer = data->mImageLayer;
  if (layer) {
    layer->SetMaskLayer(nullptr);
  } else {
    // Create a new layer
    layer = mManager->CreateImageLayer();
    if (!layer)
      return nullptr;
    // Mark this layer as being used for Thebes-painting display items
    data->mImageLayer = layer;
    layer->SetUserData(&gImageLayerUserData, nullptr);

    // Remove other layer types we might have stored for this ThebesLayer
    data->mColorLayer = nullptr;
  }
  return layer.forget();
}

already_AddRefed<ImageLayer>
ContainerState::CreateOrRecycleMaskImageLayerFor(Layer* aLayer)
{
  nsRefPtr<ImageLayer> result = mRecycledMaskImageLayers.Get(aLayer);
  if (result) {
    mRecycledMaskImageLayers.Remove(aLayer);
    // XXX if we use clip on mask layers, null it out here
  } else {
    // Create a new layer
    result = mManager->CreateImageLayer();
    if (!result)
      return nullptr;
    result->SetUserData(&gMaskLayerUserData, new MaskLayerUserData());
    result->SetDisallowBigImage(true);
  }

  return result.forget();
}

static const double SUBPIXEL_OFFSET_EPSILON = 0.02;

/**
 * This normally computes NSToIntRoundUp(aValue). However, if that would
 * give a residual near 0.5 while aOldResidual is near -0.5, or
 * it would give a residual near -0.5 while aOldResidual is near 0.5, then
 * instead we return the integer in the other direction so that the residual
 * is close to aOldResidual.
 */
static int32_t
RoundToMatchResidual(double aValue, double aOldResidual)
{
  int32_t v = NSToIntRoundUp(aValue);
  double residual = aValue - v;
  if (aOldResidual < 0) {
    if (residual > 0 && fabs(residual - 1.0 - aOldResidual) < SUBPIXEL_OFFSET_EPSILON) {
      // Round up instead
      return int32_t(ceil(aValue));
    }
  } else if (aOldResidual > 0) {
    if (residual < 0 && fabs(residual + 1.0 - aOldResidual) < SUBPIXEL_OFFSET_EPSILON) {
      // Round down instead
      return int32_t(floor(aValue));
    }
  }
  return v;
}

static void
ResetScrollPositionForLayerPixelAlignment(const nsIFrame* aAnimatedGeometryRoot)
{
  nsIScrollableFrame* sf = nsLayoutUtils::GetScrollableFrameFor(aAnimatedGeometryRoot);
  if (sf) {
    sf->ResetScrollPositionForLayerPixelAlignment();
  }
}

static void
InvalidateEntireThebesLayer(ThebesLayer* aLayer, const nsIFrame* aAnimatedGeometryRoot)
{
#ifdef MOZ_DUMP_PAINTING
  if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
    printf_stderr("Invalidating entire layer %p\n", aLayer);
  }
#endif
  nsIntRect invalidate = aLayer->GetValidRegion().GetBounds();
  aLayer->InvalidateRegion(invalidate);
  aLayer->SetInvalidRectToVisibleRegion();
  ResetScrollPositionForLayerPixelAlignment(aAnimatedGeometryRoot);
}

already_AddRefed<ThebesLayer>
ContainerState::CreateOrRecycleThebesLayer(const nsIFrame* aAnimatedGeometryRoot,
                                           const nsIFrame* aReferenceFrame,
                                           const nsPoint& aTopLeft)
{
  // We need a new thebes layer
  nsRefPtr<ThebesLayer> layer;
  ThebesDisplayItemLayerUserData* data;
  bool layerRecycled = false;
#ifndef MOZ_ANDROID_OMTC
  bool didResetScrollPositionForLayerPixelAlignment = false;
#endif

  // Check whether the layer will be scrollable. This is used as a hint to
  // influence whether tiled layers are used or not.
  LayerManager::ThebesLayerCreationHint creationHint = LayerManager::NONE;
  nsIFrame* animatedGeometryRootParent = aAnimatedGeometryRoot->GetParent();
  if (animatedGeometryRootParent &&
      animatedGeometryRootParent->GetType() == nsGkAtoms::scrollFrame) {
    creationHint = LayerManager::SCROLLABLE;
  }

  if (mNextFreeRecycledThebesLayer < mRecycledThebesLayers.Length()) {
    // Try to recycle a layer
    layer = mRecycledThebesLayers[mNextFreeRecycledThebesLayer];
    ++mNextFreeRecycledThebesLayer;

    // Check if the layer hint has changed and whether or not the layer should
    // be recreated because of it.
    if (mManager->IsOptimizedFor(layer->AsThebesLayer(), creationHint)) {
      layerRecycled = true;

      // Clear clip rect and mask layer so we don't accidentally stay clipped.
      // We will reapply any necessary clipping.
      layer->SetMaskLayer(nullptr);

      data = static_cast<ThebesDisplayItemLayerUserData*>
          (layer->GetUserData(&gThebesDisplayItemLayerUserData));
      NS_ASSERTION(data, "Recycled ThebesLayers must have user data");

      // This gets called on recycled ThebesLayers that are going to be in the
      // final layer tree, so it's a convenient time to invalidate the
      // content that changed where we don't know what ThebesLayer it belonged
      // to, or if we need to invalidate the entire layer, we can do that.
      // This needs to be done before we update the ThebesLayer to its new
      // transform. See nsGfxScrollFrame::InvalidateInternal, where
      // we ensure that mInvalidThebesContent is updated according to the
      // scroll position as of the most recent paint.
      if (!FuzzyEqual(data->mXScale, mParameters.mXScale, 0.00001f) ||
          !FuzzyEqual(data->mYScale, mParameters.mYScale, 0.00001f) ||
          data->mAppUnitsPerDevPixel != mAppUnitsPerDevPixel) {
#ifdef MOZ_DUMP_PAINTING
      if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
        printf_stderr("Recycled layer %p changed scale\n", layer.get());
      }
#endif
        InvalidateEntireThebesLayer(layer, aAnimatedGeometryRoot);
#ifndef MOZ_ANDROID_OMTC
        didResetScrollPositionForLayerPixelAlignment = true;
#endif
      }
      if (!data->mRegionToInvalidate.IsEmpty()) {
#ifdef MOZ_DUMP_PAINTING
        if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
          printf_stderr("Invalidating deleted frame content from layer %p\n", layer.get());
        }
#endif
        layer->InvalidateRegion(data->mRegionToInvalidate);
#ifdef MOZ_DUMP_PAINTING
        if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
          nsAutoCString str;
          AppendToString(str, data->mRegionToInvalidate);
          printf_stderr("Invalidating layer %p: %s\n", layer.get(), str.get());
        }
#endif
        data->mRegionToInvalidate.SetEmpty();
      }

      // We do not need to Invalidate these areas in the widget because we
      // assume the caller of InvalidateThebesLayerContents has ensured
      // the area is invalidated in the widget.
    }
  }

  if (!layerRecycled) {
    // Create a new thebes layer
    layer = mManager->CreateThebesLayerWithHint(creationHint);
    if (!layer)
      return nullptr;
    // Mark this layer as being used for Thebes-painting display items
    data = new ThebesDisplayItemLayerUserData();
    layer->SetUserData(&gThebesDisplayItemLayerUserData, data);
    ResetScrollPositionForLayerPixelAlignment(aAnimatedGeometryRoot);
#ifndef MOZ_ANDROID_OMTC
    didResetScrollPositionForLayerPixelAlignment = true;
#endif
  }
  data->mXScale = mParameters.mXScale;
  data->mYScale = mParameters.mYScale;
  data->mLastAnimatedGeometryRootOrigin = data->mAnimatedGeometryRootOrigin;
  data->mAnimatedGeometryRootOrigin = aTopLeft;
  data->mAppUnitsPerDevPixel = mAppUnitsPerDevPixel;
  layer->SetAllowResidualTranslation(mParameters.AllowResidualTranslation());

  mLayerBuilder->SaveLastPaintOffset(layer);

  // Set up transform so that 0,0 in the Thebes layer corresponds to the
  // (pixel-snapped) top-left of the aAnimatedGeometryRoot.
  nsPoint offset = aAnimatedGeometryRoot->GetOffsetToCrossDoc(aReferenceFrame);
  nscoord appUnitsPerDevPixel = aAnimatedGeometryRoot->PresContext()->AppUnitsPerDevPixel();
  gfxPoint scaledOffset(
      NSAppUnitsToDoublePixels(offset.x, appUnitsPerDevPixel)*mParameters.mXScale,
      NSAppUnitsToDoublePixels(offset.y, appUnitsPerDevPixel)*mParameters.mYScale);
  // We call RoundToMatchResidual here so that the residual after rounding
  // is close to data->mAnimatedGeometryRootPosition if possible.
  nsIntPoint pixOffset(RoundToMatchResidual(scaledOffset.x, data->mAnimatedGeometryRootPosition.x),
                       RoundToMatchResidual(scaledOffset.y, data->mAnimatedGeometryRootPosition.y));
  data->mTranslation = pixOffset;
  pixOffset += mParameters.mOffset;
  Matrix matrix;
  matrix.Translate(pixOffset.x, pixOffset.y);
  layer->SetBaseTransform(Matrix4x4::From2D(matrix));

  // FIXME: Temporary workaround for bug 681192 and bug 724786.
#ifndef MOZ_ANDROID_OMTC
  // Calculate exact position of the top-left of the active scrolled root.
  // This might not be 0,0 due to the snapping in ScaleToNearestPixels.
  gfxPoint animatedGeometryRootTopLeft = scaledOffset - ThebesPoint(matrix.GetTranslation()) + mParameters.mOffset;
  // If it has changed, then we need to invalidate the entire layer since the
  // pixels in the layer buffer have the content at a (subpixel) offset
  // from what we need.
  if (!animatedGeometryRootTopLeft.WithinEpsilonOf(data->mAnimatedGeometryRootPosition, SUBPIXEL_OFFSET_EPSILON)) {
    data->mAnimatedGeometryRootPosition = animatedGeometryRootTopLeft;
    InvalidateEntireThebesLayer(layer, aAnimatedGeometryRoot);
  } else if (didResetScrollPositionForLayerPixelAlignment) {
    data->mAnimatedGeometryRootPosition = animatedGeometryRootTopLeft;
  }
#endif

  return layer.forget();
}

#if defined(DEBUG) || defined(MOZ_DUMP_PAINTING)
/**
 * Returns the appunits per dev pixel for the item's frame
 */
static int32_t
AppUnitsPerDevPixel(nsDisplayItem* aItem)
{
  // The underlying frame for zoom items is the root frame of the subdocument.
  // But zoom display items report their bounds etc using the parent document's
  // APD because zoom items act as a conversion layer between the two different
  // APDs.
  if (aItem->GetType() == nsDisplayItem::TYPE_ZOOM) {
    return static_cast<nsDisplayZoom*>(aItem)->GetParentAppUnitsPerDevPixel();
  }
  return aItem->Frame()->PresContext()->AppUnitsPerDevPixel();
}
#endif

/**
 * Restrict the visible region of aLayer to the region that is actually visible.
 * Because we only reduce the visible region here, we don't need to worry
 * about whether CONTENT_OPAQUE is set; if layer was opaque in the old
 * visible region, it will still be opaque in the new one.
 * @param aLayerVisibleRegion the visible region of the layer, in the layer's
 * coordinate space
 * @param aRestrictToRect the rect to restrict the visible region to, in the
 * parent's coordinate system
 */
static void
SetVisibleRegionForLayer(Layer* aLayer, const nsIntRegion& aLayerVisibleRegion,
                         const nsIntRect& aRestrictToRect)
{
  gfx3DMatrix transform;
  To3DMatrix(aLayer->GetTransform(), transform);

  // if 'transform' is not invertible, then nothing will be displayed
  // for the layer, so it doesn't really matter what we do here
  gfxRect itemVisible(aRestrictToRect.x, aRestrictToRect.y,
                      aRestrictToRect.width, aRestrictToRect.height);
  nsIntRect childBounds = aLayerVisibleRegion.GetBounds();
  gfxRect childGfxBounds(childBounds.x, childBounds.y,
                         childBounds.width, childBounds.height);
  gfxRect layerVisible = transform.Inverse().ProjectRectBounds(itemVisible);
  layerVisible = layerVisible.Intersect(childGfxBounds);
  layerVisible.RoundOut();

  nsIntRect visibleRect;
  if (!gfxUtils::GfxRectToIntRect(layerVisible, &visibleRect)) {
    aLayer->SetVisibleRegion(nsIntRegion());
  } else {
    nsIntRegion rgn;
    rgn.And(aLayerVisibleRegion, visibleRect);
    aLayer->SetVisibleRegion(rgn);
  }
}

nscolor
ContainerState::FindOpaqueBackgroundColorFor(int32_t aThebesLayerIndex)
{
  ThebesLayerData* target = mThebesLayerDataStack[aThebesLayerIndex];
  for (int32_t i = aThebesLayerIndex - 1; i >= 0; --i) {
    ThebesLayerData* candidate = mThebesLayerDataStack[i];
    if (candidate->IntersectsVisibleAboveRegion(target->mVisibleRegion)) {
      // Some non-Thebes content between target and candidate; this is
      // hopeless
      break;
    }

    nsIntRegion intersection;
    intersection.And(candidate->mVisibleRegion, target->mVisibleRegion);
    if (intersection.IsEmpty()) {
      // The layer doesn't intersect our target, ignore it and move on
      continue;
    }

    // The candidate intersects our target. If any layer has a solid-color
    // area behind our target, this must be it. Scan its display items.
    nsIntRect deviceRect = target->mVisibleRegion.GetBounds();
    nsRect appUnitRect = deviceRect.ToAppUnits(mAppUnitsPerDevPixel);
    appUnitRect.ScaleInverseRoundOut(mParameters.mXScale, mParameters.mYScale);

    FrameLayerBuilder::ThebesLayerItemsEntry* entry =
      mLayerBuilder->GetThebesLayerItemsEntry(candidate->mLayer);
    NS_ASSERTION(entry, "Must know about this layer!");
    for (int32_t j = entry->mItems.Length() - 1; j >= 0; --j) {
      nsDisplayItem* item = entry->mItems[j].mItem;
      bool snap;
      nsRect bounds = item->GetBounds(mBuilder, &snap);
      if (snap && mSnappingEnabled) {
        nsIntRect snappedBounds = ScaleToNearestPixels(bounds);
        if (!snappedBounds.Intersects(deviceRect))
          continue;

        if (!snappedBounds.Contains(deviceRect))
          break;

      } else {
        // The layer's visible rect is already (close enough to) pixel
        // aligned, so no need to round out and in here.
        if (!bounds.Intersects(appUnitRect))
          continue;

        if (!bounds.Contains(appUnitRect))
          break;
      }

      if (item->IsInvisibleInRect(appUnitRect)) {
        continue;
      }

      nscolor color;
      if (item->IsUniform(mBuilder, &color) && NS_GET_A(color) == 255)
        return color;

      break;
    }
    break;
  }
  return NS_RGBA(0,0,0,0);
}

void
ThebesLayerData::UpdateCommonClipCount(
    const DisplayItemClip& aCurrentClip)
{
  if (mCommonClipCount >= 0) {
    mCommonClipCount = mItemClip.GetCommonRoundedRectCount(aCurrentClip, mCommonClipCount);
  } else {
    // first item in the layer
    mCommonClipCount = aCurrentClip.GetRoundedRectCount();
  }
}

already_AddRefed<ImageContainer>
ThebesLayerData::CanOptimizeImageLayer(nsDisplayListBuilder* aBuilder)
{
  if (!mImage) {
    return nullptr;
  }

  return mImage->GetContainer(mLayer->Manager(), aBuilder);
}

const nsIFrame*
ContainerState::FindFixedPosFrameForLayerData(const nsIFrame* aAnimatedGeometryRoot,
                                              bool aDisplayItemFixedToViewport)
{
  if (!mManager->IsWidgetLayerManager()) {
    // Never attach any fixed-pos metadata to inactive layers, it's pointless!
    return nullptr;
  }

  nsPresContext* presContext = mContainerFrame->PresContext();
  nsIFrame* viewport = presContext->PresShell()->GetRootFrame();

  if (viewport == aAnimatedGeometryRoot && aDisplayItemFixedToViewport &&
      nsLayoutUtils::ViewportHasDisplayPort(presContext)) {
    // Probably a background-attachment:fixed item
    return viewport;
  }
  // Viewports with no fixed-pos frames are not relevant.
  if (!viewport->GetFirstChild(nsIFrame::kFixedList)) {
    return nullptr;
  }
  for (const nsIFrame* f = aAnimatedGeometryRoot; f; f = f->GetParent()) {
    if (nsLayoutUtils::IsFixedPosFrameInDisplayPort(f)) {
      return f;
    }
    if (f == mContainerReferenceFrame) {
      // The metadata will go on an ancestor layer if necessary.
      return nullptr;
    }
  }
  return nullptr;
}

void
ContainerState::AdjustLayerDataForFixedPositioning(const nsIFrame* aFixedPosFrame,
                                                   const nsIntRegion& aDrawRegion,
                                                   nsIntRegion* aVisibleRegion,
                                                   bool* aIsSolidColorInVisibleRegion)
{
  if (!aFixedPosFrame) {
    return;
  }

  nsRect fixedVisibleRect;
  nsPresContext* presContext = aFixedPosFrame->PresContext();
  nsIPresShell* presShell = presContext->PresShell();
  DebugOnly<bool> hasDisplayPort =
    nsLayoutUtils::ViewportHasDisplayPort(presContext, &fixedVisibleRect);
  NS_ASSERTION(hasDisplayPort, "No fixed-pos layer data if there's no displayport");
  // Display ports are relative to the viewport, convert it to be relative
  // to our reference frame.
  nsIFrame* viewport = presShell->GetRootFrame();
  if (aFixedPosFrame != viewport) {
    // position: fixed items are reflowed into and only drawn inside the
    // viewport, or the scroll position clamping scrollport size, if one is
    // set. We differentiate background-attachment: fixed items from
    // position: fixed items by the fact that background-attachment: fixed
    // items use the viewport as their aFixedPosFrame.
    NS_ASSERTION(aFixedPosFrame->StyleDisplay()->mPosition == NS_STYLE_POSITION_FIXED,
      "should be position fixed items only");
    fixedVisibleRect.MoveTo(0, 0);
    if (presShell->IsScrollPositionClampingScrollPortSizeSet()) {
      fixedVisibleRect.SizeTo(presShell->GetScrollPositionClampingScrollPortSize());
    } else {
      fixedVisibleRect.SizeTo(viewport->GetSize());
    }
  }
  fixedVisibleRect += viewport->GetOffsetToCrossDoc(mContainerReferenceFrame);
  nsIntRegion newVisibleRegion;
  newVisibleRegion.And(ScaleToOutsidePixels(fixedVisibleRect, false),
                       aDrawRegion);
  if (!aVisibleRegion->Contains(newVisibleRegion)) {
    if (aIsSolidColorInVisibleRegion) {
      *aIsSolidColorInVisibleRegion = false;
    }
    *aVisibleRegion = newVisibleRegion;
  }
}

void
ContainerState::SetFixedPositionLayerData(Layer* aLayer,
                                          const nsIFrame* aFixedPosFrame)
{
  aLayer->SetIsFixedPosition(aFixedPosFrame != nullptr);
  if (!aFixedPosFrame) {
    return;
  }

  nsPresContext* presContext = aFixedPosFrame->PresContext();

  const nsIFrame* viewportFrame = aFixedPosFrame->GetParent();
  // anchorRect will be in the container's coordinate system (aLayer's parent layer).
  // This is the same as the display items' reference frame.
  nsRect anchorRect;
  if (viewportFrame) {
    // Fixed position frames are reflowed into the scroll-port size if one has
    // been set.
    if (presContext->PresShell()->IsScrollPositionClampingScrollPortSizeSet()) {
      anchorRect.SizeTo(presContext->PresShell()->GetScrollPositionClampingScrollPortSize());
    } else {
      anchorRect.SizeTo(viewportFrame->GetSize());
    }
  } else {
    // A display item directly attached to the viewport.
    // For background-attachment:fixed items, the anchor point is always the
    // top-left of the viewport currently.
    viewportFrame = aFixedPosFrame;
  }
  // The anchorRect top-left is always the viewport top-left.
  anchorRect.MoveTo(viewportFrame->GetOffsetToCrossDoc(mContainerReferenceFrame));

  nsLayoutUtils::SetFixedPositionLayerData(aLayer,
      viewportFrame, anchorRect, aFixedPosFrame, presContext, mParameters);
}

static bool
CanOptimizeAwayThebesLayer(ThebesLayerData* aData,
                           FrameLayerBuilder* aLayerBuilder)
{
  bool isRetained = aData->mLayer->Manager()->IsWidgetLayerManager();
  if (!isRetained) {
    return false;
  }

  // If there's no thebes layer with valid content in it that we can reuse,
  // always create a color or image layer (and potentially throw away an
  // existing completely invalid thebes layer).
  if (aData->mLayer->GetValidRegion().IsEmpty()) {
    return true;
  }

  // There is an existing thebes layer we can reuse. Throwing it away can make
  // compositing cheaper (see bug 946952), but it might cause us to re-allocate
  // the thebes layer frequently due to an animation. So we only discard it if
  // we're in tree compression mode, which is triggered at a low frequency.
  return aLayerBuilder->CheckInLayerTreeCompressionMode();
}

void
ContainerState::PopThebesLayerData()
{
  NS_ASSERTION(!mThebesLayerDataStack.IsEmpty(), "Can't pop");

  int32_t lastIndex = mThebesLayerDataStack.Length() - 1;
  ThebesLayerData* data = mThebesLayerDataStack[lastIndex];

  AdjustLayerDataForFixedPositioning(data->mFixedPosFrameForLayerData,
                                     data->mDrawRegion,
                                     &data->mVisibleRegion,
                                     &data->mIsSolidColorInVisibleRegion);
  nsRefPtr<Layer> layer;
  nsRefPtr<ImageContainer> imageContainer = data->CanOptimizeImageLayer(mBuilder);

  if ((data->mIsSolidColorInVisibleRegion || imageContainer) &&
      CanOptimizeAwayThebesLayer(data, mLayerBuilder)) {
    NS_ASSERTION(!(data->mIsSolidColorInVisibleRegion && imageContainer),
                 "Can't be a solid color as well as an image!");
    if (imageContainer) {
      nsRefPtr<ImageLayer> imageLayer = CreateOrRecycleImageLayer(data->mLayer);
      imageLayer->SetContainer(imageContainer);
      data->mImage->ConfigureLayer(imageLayer, mParameters.mOffset);
      imageLayer->SetPostScale(mParameters.mXScale,
                               mParameters.mYScale);
      if (data->mItemClip.HasClip()) {
        nsIntRect clip = ScaleToNearestPixels(data->mItemClip.GetClipRect());
        clip.MoveBy(mParameters.mOffset);
        imageLayer->SetClipRect(&clip);
      } else {
        imageLayer->SetClipRect(nullptr);
      }
      layer = imageLayer;
      mLayerBuilder->StoreOptimizedLayerForFrame(data->mImage,
                                                 imageLayer);
    } else {
      nsRefPtr<ColorLayer> colorLayer = CreateOrRecycleColorLayer(data->mLayer);
      colorLayer->SetColor(data->mSolidColor);

      // Copy transform
      colorLayer->SetBaseTransform(data->mLayer->GetBaseTransform());
      colorLayer->SetPostScale(data->mLayer->GetPostXScale(), data->mLayer->GetPostYScale());

      nsIntRect visibleRect = data->mVisibleRegion.GetBounds();
      visibleRect.MoveBy(-GetTranslationForThebesLayer(data->mLayer));
      colorLayer->SetBounds(visibleRect);

      layer = colorLayer;
    }

    NS_ASSERTION(!mNewChildLayers.Contains(layer), "Layer already in list???");
    AutoLayersArray::index_type index = mNewChildLayers.IndexOf(data->mLayer);
    NS_ASSERTION(index != AutoLayersArray::NoIndex, "Thebes layer not found?");
    mNewChildLayers.InsertElementAt(index + 1, layer);

    // Hide the ThebesLayer. We leave it in the layer tree so that we
    // can find and recycle it later.
    nsIntRect emptyRect;
    data->mLayer->SetClipRect(&emptyRect);
    data->mLayer->SetVisibleRegion(nsIntRegion());
    data->mLayer->SetEventRegions(EventRegions());
  } else {
    layer = data->mLayer;
    imageContainer = nullptr;
    layer->SetClipRect(nullptr);
  }

  Matrix transform;
  if (!layer->GetTransform().Is2D(&transform)) {
    NS_ERROR("Only 2D transformations currently supported");
  }

  // ImageLayers are already configured with a visible region
  if (!imageContainer) {
    NS_ASSERTION(!transform.HasNonIntegerTranslation(),
                 "Matrix not just an integer translation?");
    // Convert from relative to the container to relative to the
    // ThebesLayer itself.
    nsIntRegion rgn = data->mVisibleRegion;
    rgn.MoveBy(-GetTranslationForThebesLayer(data->mLayer));
    layer->SetVisibleRegion(rgn);
  }

  nsIntRegion transparentRegion;
  transparentRegion.Sub(data->mVisibleRegion, data->mOpaqueRegion);
  bool isOpaque = transparentRegion.IsEmpty();
  // For translucent ThebesLayers, try to find an opaque background
  // color that covers the entire area beneath it so we can pull that
  // color into this layer to make it opaque.
  if (layer == data->mLayer) {
    nscolor backgroundColor = NS_RGBA(0,0,0,0);
    if (!isOpaque) {
      backgroundColor = FindOpaqueBackgroundColorFor(lastIndex);
      if (NS_GET_A(backgroundColor) == 255) {
        isOpaque = true;
      }
    }

    // Store the background color
    ThebesDisplayItemLayerUserData* userData =
      GetThebesDisplayItemLayerUserData(data->mLayer);
    NS_ASSERTION(userData, "where did our user data go?");
    if (userData->mForcedBackgroundColor != backgroundColor) {
      // Invalidate the entire target ThebesLayer since we're changing
      // the background color
#ifdef MOZ_DUMP_PAINTING
      if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
        printf_stderr("Forced background color has changed from #%08X to #%08X on layer %p\n",
                      userData->mForcedBackgroundColor, backgroundColor, data->mLayer);
        nsAutoCString str;
        AppendToString(str, data->mLayer->GetValidRegion());
        printf_stderr("Invalidating layer %p: %s\n", data->mLayer, str.get());
      }
#endif
      data->mLayer->InvalidateRegion(data->mLayer->GetValidRegion());
    }
    userData->mForcedBackgroundColor = backgroundColor;

    // use a mask layer for rounded rect clipping.
    // data->mCommonClipCount may be -1 if we haven't put any actual
    // drawable items in this layer (i.e. it's only catching events).
    int32_t commonClipCount = std::max(0, data->mCommonClipCount);
    SetupMaskLayer(layer, data->mItemClip, commonClipCount);
    // copy commonClipCount to the entry
    FrameLayerBuilder::ThebesLayerItemsEntry* entry = mLayerBuilder->
      GetThebesLayerItemsEntry(static_cast<ThebesLayer*>(layer.get()));
    entry->mCommonClipCount = commonClipCount;
  } else {
    // mask layer for image and color layers
    SetupMaskLayer(layer, data->mItemClip);
  }

  uint32_t flags = 0;
  nsIWidget* widget = mContainerReferenceFrame->PresContext()->GetRootWidget();
  // See bug 941095. Not quite ready to disable this.
  bool hidpi = false && widget && widget->GetDefaultScale().scale >= 2;
  if (hidpi) {
    flags |= Layer::CONTENT_DISABLE_SUBPIXEL_AA;
  }
  if (isOpaque && !data->mForceTransparentSurface) {
    flags |= Layer::CONTENT_OPAQUE;
  } else if (data->mNeedComponentAlpha && !hidpi) {
    flags |= Layer::CONTENT_COMPONENT_ALPHA;
  }
  layer->SetContentFlags(flags);

  SetFixedPositionLayerData(layer, data->mFixedPosFrameForLayerData);

  ThebesLayerData* containingThebesLayerData =
     mLayerBuilder->GetContainingThebesLayerData();
  if (containingThebesLayerData) {
    if (!data->mDispatchToContentHitRegion.GetBounds().IsEmpty()) {
      nsRect rect = nsLayoutUtils::TransformFrameRectToAncestor(
        mContainerReferenceFrame,
        data->mDispatchToContentHitRegion.GetBounds(),
        containingThebesLayerData->mReferenceFrame);
      containingThebesLayerData->mDispatchToContentHitRegion.Or(
        containingThebesLayerData->mDispatchToContentHitRegion, rect);
    }
    if (!data->mMaybeHitRegion.GetBounds().IsEmpty()) {
      nsRect rect = nsLayoutUtils::TransformFrameRectToAncestor(
        mContainerReferenceFrame,
        data->mMaybeHitRegion.GetBounds(),
        containingThebesLayerData->mReferenceFrame);
      containingThebesLayerData->mMaybeHitRegion.Or(
        containingThebesLayerData->mMaybeHitRegion, rect);
    }
    if (!data->mHitRegion.GetBounds().IsEmpty()) {
      // Our definitely-hit region must go to the maybe-hit-region since
      // this function is an approximation.
      gfx3DMatrix matrix = nsLayoutUtils::GetTransformToAncestor(
        mContainerReferenceFrame, containingThebesLayerData->mReferenceFrame);
      gfxMatrix matrix2D;
      bool isPrecise = matrix.Is2D(&matrix2D) && !matrix2D.HasNonAxisAlignedTransform();
      nsRect rect = nsLayoutUtils::TransformFrameRectToAncestor(
        mContainerReferenceFrame,
        data->mHitRegion.GetBounds(),
        containingThebesLayerData->mReferenceFrame);
      nsRegion* dest = isPrecise ? &containingThebesLayerData->mHitRegion
                                 : &containingThebesLayerData->mMaybeHitRegion;
      dest->Or(*dest, rect);
    }
  } else {
    EventRegions regions;
    regions.mHitRegion = ScaleRegionToOutsidePixels(data->mHitRegion);
    // Points whose hit-region status we're not sure about need to be dispatched
    // to the content thread.
    nsIntRegion maybeHitRegion = ScaleRegionToOutsidePixels(data->mMaybeHitRegion);
    regions.mDispatchToContentHitRegion.Sub(maybeHitRegion, regions.mHitRegion);
    regions.mDispatchToContentHitRegion.Or(regions.mDispatchToContentHitRegion,
                                           ScaleRegionToOutsidePixels(data->mDispatchToContentHitRegion));

    nsIntPoint translation = -GetTranslationForThebesLayer(data->mLayer);
    regions.mHitRegion.MoveBy(translation);
    regions.mDispatchToContentHitRegion.MoveBy(translation);

    layer->SetEventRegions(regions);
  }

  if (lastIndex > 0) {
    // Since we're going to pop off the last ThebesLayerData, the
    // mVisibleAboveRegion of the second-to-last item will need to include
    // the regions of the last item.
    ThebesLayerData* nextData = mThebesLayerDataStack[lastIndex - 1];
    nextData->CopyAboveRegion(data);
  }

  mThebesLayerDataStack.RemoveElementAt(lastIndex);
}

static bool
SuppressComponentAlpha(nsDisplayListBuilder* aBuilder,
                       nsDisplayItem* aItem,
                       const nsRect& aComponentAlphaBounds)
{
  const nsRegion* windowTransparentRegion = aBuilder->GetFinalTransparentRegion();
  if (!windowTransparentRegion || windowTransparentRegion->IsEmpty())
    return false;

  // Suppress component alpha for items in the toplevel window that are over
  // the window translucent area
  nsIFrame* f = aItem->Frame();
  nsIFrame* ref = aBuilder->RootReferenceFrame();
  if (f->PresContext() != ref->PresContext())
    return false;

  for (nsIFrame* t = f; t; t = t->GetParent()) {
    if (t->IsTransformed())
      return false;
  }

  return windowTransparentRegion->Intersects(aComponentAlphaBounds);
}

static bool
WindowHasTransparency(nsDisplayListBuilder* aBuilder)
{
  const nsRegion* windowTransparentRegion = aBuilder->GetFinalTransparentRegion();
  return windowTransparentRegion && !windowTransparentRegion->IsEmpty();
}

void
ThebesLayerData::Accumulate(ContainerState* aState,
                            nsDisplayItem* aItem,
                            const nsIntRect& aVisibleRect,
                            const nsIntRect& aDrawRect,
                            const DisplayItemClip& aClip)
{
  if (aState->mBuilder->NeedToForceTransparentSurfaceForItem(aItem)) {
    mForceTransparentSurface = true;
  }
  if (aState->mParameters.mDisableSubpixelAntialiasingInDescendants) {
    // Disable component alpha.
    // Note that the transform (if any) on the ThebesLayer is always an integer translation so
    // we don't have to factor that in here.
    aItem->DisableComponentAlpha();
  }

  /* Mark as available for conversion to image layer if this is a nsDisplayImage and
   * we are the first visible item in the ThebesLayerData object.
   */
  if (mVisibleRegion.IsEmpty() &&
      aItem->SupportsOptimizingToImage()) {
    mImage = static_cast<nsDisplayImageContainer*>(aItem);
  } else {
    mImage = nullptr;
  }
  bool clipMatches = mItemClip == aClip;
  mItemClip = aClip;

  if (!mIsSolidColorInVisibleRegion && mOpaqueRegion.Contains(aDrawRect) &&
      mVisibleRegion.Contains(aVisibleRect)) {
    // A very common case! Most pages have a ThebesLayer with the page
    // background (opaque) visible and most or all of the page content over the
    // top of that background.
    // The rest of this method won't do anything. mVisibleRegion, mOpaqueRegion
    // and mDrawRegion don't need updating. mVisibleRegion contains aVisibleRect
    // already, mOpaqueRegion contains aDrawRect and therefore whatever
    // the opaque region of the item is. mDrawRegion must contain mOpaqueRegion
    // and therefore aDrawRect.
    NS_ASSERTION(mDrawRegion.Contains(aDrawRect), "Draw region not covered");
    return;
  }

  nscolor uniformColor;
  bool isUniform = aItem->IsUniform(aState->mBuilder, &uniformColor);

  // Some display items have to exist (so they can set forceTransparentSurface
  // below) but don't draw anything. They'll return true for isUniform but
  // a color with opacity 0.
  if (!isUniform || NS_GET_A(uniformColor) > 0) {
    // Make sure that the visible area is covered by uniform pixels. In
    // particular this excludes cases where the edges of the item are not
    // pixel-aligned (thus the item will not be truly uniform).
    if (isUniform) {
      bool snap;
      nsRect bounds = aItem->GetBounds(aState->mBuilder, &snap);
      if (!aState->ScaleToInsidePixels(bounds, snap).Contains(aVisibleRect)) {
        isUniform = false;
      }
    }
    if (isUniform) {
      if (mVisibleRegion.IsEmpty()) {
        // This color is all we have
        mSolidColor = uniformColor;
        mIsSolidColorInVisibleRegion = true;
      } else if (mIsSolidColorInVisibleRegion &&
                 mVisibleRegion.IsEqual(nsIntRegion(aVisibleRect)) &&
                 clipMatches) {
        // we can just blend the colors together
        mSolidColor = NS_ComposeColors(mSolidColor, uniformColor);
      } else {
        mIsSolidColorInVisibleRegion = false;
      }
    } else {
      mIsSolidColorInVisibleRegion = false;
    }

    mVisibleRegion.Or(mVisibleRegion, aVisibleRect);
    mVisibleRegion.SimplifyOutward(4);
    mDrawRegion.Or(mDrawRegion, aDrawRect);
    mDrawRegion.SimplifyOutward(4);
  }

  bool snap;
  nsRegion opaque = aItem->GetOpaqueRegion(aState->mBuilder, &snap);
  if (!opaque.IsEmpty()) {
    nsRegion opaqueClipped;
    nsRegionRectIterator iter(opaque);
    for (const nsRect* r = iter.Next(); r; r = iter.Next()) {
      opaqueClipped.Or(opaqueClipped, aClip.ApproximateIntersectInward(*r));
    }

    nsIntRegion opaquePixels = aState->ScaleRegionToInsidePixels(opaqueClipped, snap);

    nsIntRegionRectIterator iter2(opaquePixels);
    for (const nsIntRect* r = iter2.Next(); r; r = iter2.Next()) {
      // We don't use SimplifyInward here since it's not defined exactly
      // what it will discard. For our purposes the most important case
      // is a large opaque background at the bottom of z-order (e.g.,
      // a canvas background), so we need to make sure that the first rect
      // we see doesn't get discarded.
      nsIntRegion tmp;
      tmp.Or(mOpaqueRegion, *r);
       // Opaque display items in chrome documents whose window is partially
       // transparent are always added to the opaque region. This helps ensure
       // that we get as much subpixel-AA as possible in the chrome.
       if (tmp.GetNumRects() <= 4 ||
           (WindowHasTransparency(aState->mBuilder) &&
            aItem->Frame()->PresContext()->IsChrome())) {
        mOpaqueRegion = tmp;
      }
    }
  }

  if (!aState->mParameters.mDisableSubpixelAntialiasingInDescendants) {
    nsRect componentAlpha = aItem->GetComponentAlphaBounds(aState->mBuilder);
    if (!componentAlpha.IsEmpty()) {
      nsIntRect componentAlphaRect =
        aState->ScaleToOutsidePixels(componentAlpha, false).Intersect(aVisibleRect);
      if (!mOpaqueRegion.Contains(componentAlphaRect)) {
        if (SuppressComponentAlpha(aState->mBuilder, aItem, componentAlpha)) {
          aItem->DisableComponentAlpha();
        } else {
          mNeedComponentAlpha = true;
        }
      }
    }
  }
}

ThebesLayerData*
ContainerState::FindThebesLayerFor(nsDisplayItem* aItem,
                                   const nsIntRect& aVisibleRect,
                                   const nsIFrame* aAnimatedGeometryRoot,
                                   const nsPoint& aTopLeft,
                                   bool aShouldFixToViewport)
{
  int32_t i;
  int32_t lowestUsableLayerWithScrolledRoot = -1;
  int32_t topmostLayerWithScrolledRoot = -1;
  for (i = mThebesLayerDataStack.Length() - 1; i >= 0; --i) {
    // Don't let should-fix-to-viewport items share a layer with any other items.
    if (aShouldFixToViewport) {
      ++i;
      break;
    }
    ThebesLayerData* data = mThebesLayerDataStack[i];
    // Give up if there is content drawn above (in z-order) this layer that
    // intersects aItem's visible region; aItem must be placed in a
    // layer above this layer.
    if (data->DrawAboveRegionIntersects(aVisibleRect)) {
      ++i;
      break;
    }
    // If the animated scrolled roots are the same and we can share this layer
    // with the item, note this as a usable layer.
    if (data->mAnimatedGeometryRoot == aAnimatedGeometryRoot &&
        !data->mSingleItemFixedToViewport) {
      lowestUsableLayerWithScrolledRoot = i;
      if (topmostLayerWithScrolledRoot < 0) {
        topmostLayerWithScrolledRoot = i;
      }
    }
    // If the layer's drawn region intersects the item, stop now since no
    // lower layer will be usable. Do the same if the layer is subject to
    // async transforms, since we don't know where it will really be drawn.
    if (data->DrawRegionIntersects(aVisibleRect))
      break;
  }
  if (topmostLayerWithScrolledRoot < 0) {
    --i;
    for (; i >= 0; --i) {
      ThebesLayerData* data = mThebesLayerDataStack[i];
      if (data->mAnimatedGeometryRoot == aAnimatedGeometryRoot) {
        topmostLayerWithScrolledRoot = i;
        break;
      }
    }
  }

  if (topmostLayerWithScrolledRoot >= 0) {
    while (uint32_t(topmostLayerWithScrolledRoot + 1) < mThebesLayerDataStack.Length()) {
      PopThebesLayerData();
    }
  }

  ThebesLayerData* thebesLayerData = nullptr;
  if (lowestUsableLayerWithScrolledRoot < 0) {
    nsRefPtr<ThebesLayer> layer =
      CreateOrRecycleThebesLayer(aAnimatedGeometryRoot, aItem->ReferenceFrame(), aTopLeft);

    thebesLayerData = new ThebesLayerData();
    mThebesLayerDataStack.AppendElement(thebesLayerData);
    thebesLayerData->mLayer = layer;
    thebesLayerData->mAnimatedGeometryRoot = aAnimatedGeometryRoot;
    thebesLayerData->mFixedPosFrameForLayerData =
      FindFixedPosFrameForLayerData(aAnimatedGeometryRoot, aShouldFixToViewport);
    thebesLayerData->mReferenceFrame = aItem->ReferenceFrame();
    thebesLayerData->mSingleItemFixedToViewport = aShouldFixToViewport;

    NS_ASSERTION(!mNewChildLayers.Contains(layer), "Layer already in list???");
    *mNewChildLayers.AppendElement() = layer.forget();
  } else {
    thebesLayerData = mThebesLayerDataStack[lowestUsableLayerWithScrolledRoot];
  }

  return thebesLayerData;
}

#ifdef MOZ_DUMP_PAINTING
static void
DumpPaintedImage(nsDisplayItem* aItem, SourceSurface* aSurface)
{
  nsCString string(aItem->Name());
  string.Append('-');
  string.AppendInt((uint64_t)aItem);
  fprintf_stderr(gfxUtils::sDumpPaintFile, "array[\"%s\"]=\"", string.BeginReading());
  gfxUtils::DumpAsDataURI(aSurface, gfxUtils::sDumpPaintFile);
  fprintf_stderr(gfxUtils::sDumpPaintFile, "\";");
}
#endif

static void
PaintInactiveLayer(nsDisplayListBuilder* aBuilder,
                   LayerManager* aManager,
                   nsDisplayItem* aItem,
                   gfxContext* aContext,
                   nsRenderingContext* aCtx)
{
  // This item has an inactive layer. Render it to a ThebesLayer
  // using a temporary BasicLayerManager.
  BasicLayerManager* basic = static_cast<BasicLayerManager*>(aManager);
  nsRefPtr<gfxContext> context = aContext;
#ifdef MOZ_DUMP_PAINTING
  int32_t appUnitsPerDevPixel = AppUnitsPerDevPixel(aItem);
  nsIntRect itemVisibleRect =
    aItem->GetVisibleRect().ToOutsidePixels(appUnitsPerDevPixel);

  RefPtr<DrawTarget> tempDT;
  if (gfxUtils::sDumpPainting) {
    tempDT = gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
                                      itemVisibleRect.Size().ToIntSize(),
                                      SurfaceFormat::B8G8R8A8);
    context = new gfxContext(tempDT);
    context->SetMatrix(gfxMatrix().Translate(-gfxPoint(itemVisibleRect.x,
                                                       itemVisibleRect.y)));
  }
#endif
  basic->BeginTransaction();
  basic->SetTarget(context);

  if (aItem->GetType() == nsDisplayItem::TYPE_SVG_EFFECTS) {
    static_cast<nsDisplaySVGEffects*>(aItem)->PaintAsLayer(aBuilder, aCtx, basic);
    if (basic->InTransaction()) {
      basic->AbortTransaction();
    }
  } else {
    basic->EndTransaction(FrameLayerBuilder::DrawThebesLayer, aBuilder);
  }
  FrameLayerBuilder *builder = static_cast<FrameLayerBuilder*>(basic->GetUserData(&gLayerManagerLayerBuilder));
  if (builder) {
    builder->DidEndTransaction();
  }

  basic->SetTarget(nullptr);

#ifdef MOZ_DUMP_PAINTING
  if (gfxUtils::sDumpPainting) {
    RefPtr<SourceSurface> surface = tempDT->Snapshot();
    DumpPaintedImage(aItem, surface);

    DrawTarget* drawTarget = aContext->GetDrawTarget();
    Rect rect(itemVisibleRect.x, itemVisibleRect.y,
              itemVisibleRect.width, itemVisibleRect.height);
    drawTarget->DrawSurface(surface, rect, Rect(Point(0,0), rect.Size()));

    aItem->SetPainted();
  }
#endif
}

/**
 * Chooses a single active scrolled root for the entire display list, used
 * when we are flattening layers.
 */
bool
ContainerState::ChooseAnimatedGeometryRoot(const nsDisplayList& aList,
                                           const nsIFrame **aAnimatedGeometryRoot)
{
  for (nsDisplayItem* item = aList.GetBottom(); item; item = item->GetAbove()) {
    LayerState layerState = item->GetLayerState(mBuilder, mManager, mParameters);
    // Don't use an item that won't be part of any ThebesLayers to pick the
    // active scrolled root.
    if (layerState == LAYER_ACTIVE_FORCE) {
      continue;
    }

    // Try using the actual active scrolled root of the backmost item, as that
    // should result in the least invalidation when scrolling.
    *aAnimatedGeometryRoot =
      nsLayoutUtils::GetAnimatedGeometryRootFor(item, mBuilder);
    return true;
  }
  return false;
}

/* Checks if aPotentialScrollItem is a scroll layer item and aPotentialScrollbarItem
 * is an overlay scrollbar item for the same scroll frame.
 */
static bool
IsScrollLayerItemAndOverlayScrollbarForScrollFrame(
  nsDisplayItem* aPotentialScrollItem, nsDisplayItem* aPotentialScrollbarItem)
{
  if (aPotentialScrollItem->GetType() == nsDisplayItem::TYPE_SCROLL_LAYER &&
      aPotentialScrollbarItem &&
      aPotentialScrollbarItem->GetType() == nsDisplayItem::TYPE_OWN_LAYER &&
      LookAndFeel::GetInt(LookAndFeel::eIntID_UseOverlayScrollbars)) {
    nsDisplayScrollLayer* scrollItem =
      static_cast<nsDisplayScrollLayer*>(aPotentialScrollItem);
    nsDisplayOwnLayer* layerItem =
      static_cast<nsDisplayOwnLayer*>(aPotentialScrollbarItem);
    if ((layerItem->GetFlags() &
         (nsDisplayOwnLayer::VERTICAL_SCROLLBAR |
          nsDisplayOwnLayer::HORIZONTAL_SCROLLBAR)) &&
        layerItem->Frame()->GetParent() == scrollItem->GetScrollFrame()) {
      return true;
    }
  }
  return false;
}

/*
 * Iterate through the non-clip items in aList and its descendants.
 * For each item we compute the effective clip rect. Each item is assigned
 * to a layer. We invalidate the areas in ThebesLayers where an item
 * has moved from one ThebesLayer to another. Also,
 * aState->mInvalidThebesContent is invalidated in every ThebesLayer.
 * We set the clip rect for items that generated their own layer, and
 * create a mask layer to do any rounded rect clipping.
 * (ThebesLayers don't need a clip rect on the layer, we clip the items
 * individually when we draw them.)
 * We set the visible rect for all layers, although the actual setting
 * of visible rects for some ThebesLayers is deferred until the calling
 * of ContainerState::Finish.
 */
void
ContainerState::ProcessDisplayItems(nsDisplayList* aList,
                                    uint32_t aFlags)
{
  PROFILER_LABEL("ContainerState", "ProcessDisplayItems",
    js::ProfileEntry::Category::GRAPHICS);

  const nsIFrame* lastAnimatedGeometryRoot = mContainerReferenceFrame;
  nsPoint topLeft(0,0);

  // When NO_COMPONENT_ALPHA is set, items will be flattened into a single
  // layer, so we need to choose which active scrolled root to use for all
  // items.
  if (aFlags & NO_COMPONENT_ALPHA) {
    if (ChooseAnimatedGeometryRoot(*aList, &lastAnimatedGeometryRoot)) {
      topLeft = lastAnimatedGeometryRoot->GetOffsetToCrossDoc(mContainerReferenceFrame);
    }
  }

  int32_t maxLayers = nsDisplayItem::MaxActiveLayers();
  int layerCount = 0;

  nsDisplayList savedItems;
  nsDisplayItem* item;
  while ((item = aList->RemoveBottom()) != nullptr) {
    // Peek ahead to the next item and try merging with it or swapping with it
    // if necessary.
    nsDisplayItem* aboveItem;
    while ((aboveItem = aList->GetBottom()) != nullptr) {
      if (aboveItem->TryMerge(mBuilder, item)) {
        aList->RemoveBottom();
        item->~nsDisplayItem();
        item = aboveItem;
      } else if (IsScrollLayerItemAndOverlayScrollbarForScrollFrame(aboveItem, item)) {
        // If an overlay scrollbar item is between a scroll layer item and the
        // other scroll layer items that we need to merge with just move the
        // scrollbar item up, that way it will be on top of the scrolled content
        // and we can try to merge all the scroll layer items.
        aList->RemoveBottom();
        aList->AppendToBottom(item);
        item = aboveItem;
      } else {
        break;
      }
    }

    nsDisplayList* itemSameCoordinateSystemChildren
      = item->GetSameCoordinateSystemChildren();
    if (itemSameCoordinateSystemChildren) {
      if (item->ShouldFlattenAway(mBuilder)) {
        aList->AppendToBottom(itemSameCoordinateSystemChildren);
        item->~nsDisplayItem();
        continue;
      }
    }

    savedItems.AppendToTop(item);

    NS_ASSERTION(mAppUnitsPerDevPixel == AppUnitsPerDevPixel(item),
      "items in a container layer should all have the same app units per dev pixel");

    nsIntRect itemVisibleRect =
      ScaleToOutsidePixels(item->GetVisibleRect(), false);
    bool snap;
    nsRect itemContent = item->GetBounds(mBuilder, &snap);
    nsIntRect itemDrawRect = ScaleToOutsidePixels(itemContent, snap);
    nsDisplayItem::Type itemType = item->GetType();
    nsIntRect clipRect;
    const DisplayItemClip& itemClip = item->GetClip();
    if (itemClip.HasClip()) {
      itemContent.IntersectRect(itemContent, itemClip.GetClipRect());
      clipRect = ScaleToNearestPixels(itemClip.GetClipRect());
      itemDrawRect.IntersectRect(itemDrawRect, clipRect);
      clipRect.MoveBy(mParameters.mOffset);
    }
    mBounds.UnionRect(mBounds, itemContent);
    itemVisibleRect.IntersectRect(itemVisibleRect, itemDrawRect);

    LayerState layerState = item->GetLayerState(mBuilder, mManager, mParameters);
    if (layerState == LAYER_INACTIVE &&
        nsDisplayItem::ForceActiveLayers()) {
      layerState = LAYER_ACTIVE;
    }

    bool forceInactive;
    const nsIFrame* animatedGeometryRoot;
    if (aFlags & NO_COMPONENT_ALPHA) {
      forceInactive = true;
      animatedGeometryRoot = lastAnimatedGeometryRoot;
    } else {
      forceInactive = false;
      if (mManager->IsWidgetLayerManager()) {
        animatedGeometryRoot = nsLayoutUtils::GetAnimatedGeometryRootFor(item, mBuilder);
      } else {
        // For inactive layer subtrees, splitting content into ThebesLayers
        // based on animated geometry roots is pointless. It's more efficient
        // to build the minimum number of layers.
        animatedGeometryRoot = mContainerAnimatedGeometryRoot;
      }
      if (animatedGeometryRoot != lastAnimatedGeometryRoot) {
        lastAnimatedGeometryRoot = animatedGeometryRoot;
        topLeft = animatedGeometryRoot->GetOffsetToCrossDoc(mContainerReferenceFrame);
      }
    }
    bool shouldFixToViewport = !animatedGeometryRoot->GetParent() &&
      item->ShouldFixToViewport(mBuilder);

    if (maxLayers != -1 && layerCount >= maxLayers) {
      forceInactive = true;
    }

    // Assign the item to a layer
    if (layerState == LAYER_ACTIVE_FORCE ||
        (layerState == LAYER_INACTIVE && !mManager->IsWidgetLayerManager()) ||
        (!forceInactive &&
         (layerState == LAYER_ACTIVE_EMPTY ||
          layerState == LAYER_ACTIVE))) {

      layerCount++;

      // LAYER_ACTIVE_EMPTY means the layer is created just for its metadata.
      // We should never see an empty layer with any visible content!
      NS_ASSERTION(layerState != LAYER_ACTIVE_EMPTY ||
                   itemVisibleRect.IsEmpty(),
                   "State is LAYER_ACTIVE_EMPTY but visible rect is not.");

      // As long as the new layer isn't going to be a ThebesLayer,
      // InvalidateForLayerChange doesn't need the new layer pointer.
      // We also need to check the old data now, because BuildLayer
      // can overwrite it.
      InvalidateForLayerChange(item, nullptr, itemClip, topLeft, nullptr);

      // If the item would have its own layer but is invisible, just hide it.
      // Note that items without their own layers can't be skipped this
      // way, since their ThebesLayer may decide it wants to draw them
      // into its buffer even if they're currently covered.
      if (itemVisibleRect.IsEmpty() &&
          !item->ShouldBuildLayerEvenIfInvisible(mBuilder)) {
        continue;
      }

      if (itemType == nsDisplayItem::TYPE_TRANSFORM) {
        mParameters.mAncestorClipRect = itemClip.HasClip() ? &clipRect : nullptr;
      } else {
        mParameters.mAncestorClipRect = nullptr;
      }

      // Just use its layer.
      nsRefPtr<Layer> ownLayer = item->BuildLayer(mBuilder, mManager, mParameters);
      if (!ownLayer) {
        continue;
      }

      NS_ASSERTION(!ownLayer->AsThebesLayer(),
                   "Should never have created a dedicated Thebes layer!");

      const nsIFrame* fixedPosFrame =
        FindFixedPosFrameForLayerData(animatedGeometryRoot, shouldFixToViewport);
      if (fixedPosFrame) {
        nsIntRegion visibleRegion(itemVisibleRect);
        AdjustLayerDataForFixedPositioning(fixedPosFrame,
                                           nsIntRegion(itemDrawRect), &visibleRegion);
        itemVisibleRect = visibleRegion.GetBounds();
      }
      SetFixedPositionLayerData(ownLayer, fixedPosFrame);

      nsRect invalid;
      if (item->IsInvalid(invalid)) {
        ownLayer->SetInvalidRectToVisibleRegion();
      }

      // If it's not a ContainerLayer, we need to apply the scale transform
      // ourselves.
      if (!ownLayer->AsContainerLayer()) {
        ownLayer->SetPostScale(mParameters.mXScale,
                               mParameters.mYScale);
      }

      // Update that layer's clip and visible rects.
      NS_ASSERTION(ownLayer->Manager() == mManager, "Wrong manager");
      NS_ASSERTION(!ownLayer->HasUserData(&gLayerManagerUserData),
                   "We shouldn't have a FrameLayerBuilder-managed layer here!");
      NS_ASSERTION(itemClip.HasClip() ||
                   itemClip.GetRoundedRectCount() == 0,
                   "If we have rounded rects, we must have a clip rect");
      // It has its own layer. Update that layer's clip and visible rects.
      if (itemClip.HasClip()) {
        ownLayer->SetClipRect(&clipRect);
      } else {
        ownLayer->SetClipRect(nullptr);
      }
      ThebesLayerData* data = GetTopThebesLayerData();
      if (data) {
        // Prerendered transform items can be updated without layer building
        // (async animations or an empty transaction), so we need to put items
        // that the transform item can potentially move under into a layer
        // above this item.
        if (item->GetType() == nsDisplayItem::TYPE_TRANSFORM &&
            nsDisplayTransform::ShouldPrerenderTransformedContent(mBuilder,
                                                                  item->Frame(),
                                                                  false)) {
          if (!itemClip.HasClip()) {
            // The transform item can move anywhere, treat all other content
            // as being above this item.
            data->SetAllDrawingAbove();
          } else {
            // The transform can't escape from the clip rect, and the clip
            // rect can't change without new layer building. Treat all content
            // that intersects the clip rect as being above this item.
            data->AddVisibleAboveRegion(clipRect);
            data->AddDrawAboveRegion(clipRect);
          }
        } else {
          data->AddVisibleAboveRegion(itemVisibleRect);

          // Add the entire bounds rect to the mDrawAboveRegion.
          // The visible region may be excluding opaque content above the
          // item, and we need to ensure that that content is not placed
          // in a ThebesLayer below the item!
          data->AddDrawAboveRegion(itemDrawRect);
        }
      }
      itemVisibleRect.MoveBy(mParameters.mOffset);
      if (item->SetVisibleRegionOnLayer()) {
        SetVisibleRegionForLayer(ownLayer, ownLayer->GetVisibleRegion(), itemVisibleRect);
      }

      // rounded rectangle clipping using mask layers
      // (must be done after visible rect is set on layer)
      if (itemClip.IsRectClippedByRoundedCorner(itemContent)) {
        SetupMaskLayer(ownLayer, itemClip);
      }

      ContainerLayer* oldContainer = ownLayer->GetParent();
      if (oldContainer && oldContainer != mContainerLayer) {
        oldContainer->RemoveChild(ownLayer);
      }
      NS_ASSERTION(!mNewChildLayers.Contains(ownLayer),
                   "Layer already in list???");

      mNewChildLayers.AppendElement(ownLayer);

      /**
       * No need to allocate geometry for items that aren't
       * part of a ThebesLayer.
       */
      nsAutoPtr<nsDisplayItemGeometry> dummy;
      mLayerBuilder->AddLayerDisplayItem(ownLayer, item,
                                         itemClip, layerState,
                                         topLeft, nullptr,
                                         dummy);
    } else {
      ThebesLayerData* data =
        FindThebesLayerFor(item, itemVisibleRect, animatedGeometryRoot, topLeft,
                           shouldFixToViewport);

      if (itemType == nsDisplayItem::TYPE_LAYER_EVENT_REGIONS) {
        nsDisplayLayerEventRegions* eventRegions =
            static_cast<nsDisplayLayerEventRegions*>(item);
        data->AccumulateEventRegions(eventRegions->HitRegion(),
                                     eventRegions->MaybeHitRegion(),
                                     eventRegions->DispatchToContentHitRegion());
      } else {
        // check to see if the new item has rounded rect clips in common with
        // other items in the layer
        data->UpdateCommonClipCount(itemClip);
        data->Accumulate(this, item, itemVisibleRect, itemDrawRect, itemClip);

        nsAutoPtr<nsDisplayItemGeometry> geometry(item->AllocateGeometry(mBuilder));
        InvalidateForLayerChange(item, data->mLayer, itemClip, topLeft, geometry);

        mLayerBuilder->AddThebesDisplayItem(data, item, itemClip,
                                            mContainerFrame,
                                            layerState, topLeft,
                                            geometry);
      }
    }
  }

  aList->AppendToTop(&savedItems);
}

void
ContainerState::InvalidateForLayerChange(nsDisplayItem* aItem,
                                         Layer* aNewLayer,
                                         const DisplayItemClip& aClip,
                                         const nsPoint& aTopLeft,
                                         nsDisplayItemGeometry *aGeometry)
{
  NS_ASSERTION(aItem->GetPerFrameKey(),
               "Display items that render using Thebes must have a key");
  nsDisplayItemGeometry *oldGeometry = nullptr;
  DisplayItemClip* oldClip = nullptr;
  nsAutoTArray<nsIFrame*,4> changedFrames;
  bool isInvalid = false;
  Layer* oldLayer = mLayerBuilder->GetOldLayerFor(aItem, &oldGeometry, &oldClip, &changedFrames, &isInvalid);
  if (aNewLayer != oldLayer && oldLayer) {
    // The item has changed layers.
    // Invalidate the old bounds in the old layer and new bounds in the new layer.
    ThebesLayer* t = oldLayer->AsThebesLayer();
    if (t) {
      // Note that whenever the layer's scale changes, we invalidate the whole thing,
      // so it doesn't matter whether we are using the old scale at last paint
      // or a new scale here
#ifdef MOZ_DUMP_PAINTING
      if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
        printf_stderr("Display item type %s(%p) changed layers %p to %p!\n", aItem->Name(), aItem->Frame(), t, aNewLayer);
      }
#endif
      InvalidatePostTransformRegion(t,
          oldGeometry->ComputeInvalidationRegion(),
          *oldClip,
          mLayerBuilder->GetLastPaintOffset(t));
    }
    if (aNewLayer) {
      ThebesLayer* newThebesLayer = aNewLayer->AsThebesLayer();
      if (newThebesLayer) {
        InvalidatePostTransformRegion(newThebesLayer,
            aGeometry->ComputeInvalidationRegion(),
            aClip,
            GetTranslationForThebesLayer(newThebesLayer));
      }
    }
    aItem->NotifyRenderingChanged();
    return;
  }
  if (!aNewLayer) {
    return;
  }

  ThebesLayer* newThebesLayer = aNewLayer->AsThebesLayer();
  if (!newThebesLayer) {
    return;
  }

  ThebesDisplayItemLayerUserData* data =
    static_cast<ThebesDisplayItemLayerUserData*>(newThebesLayer->GetUserData(&gThebesDisplayItemLayerUserData));
  // If the frame is marked as invalidated, and didn't specify a rect to invalidate  then we want to
  // invalidate both the old and new bounds, otherwise we only want to invalidate the changed areas.
  // If we do get an invalid rect, then we want to add this on top of the change areas.
  nsRect invalid;
  nsRegion combined;
  nsPoint shift = aTopLeft - data->mLastAnimatedGeometryRootOrigin;
  bool notifyRenderingChanged = true;
  if (!oldLayer) {
    // This item is being added for the first time, invalidate its entire area.
    //TODO: We call GetGeometry again in AddThebesDisplayItem, we should reuse this.
    combined = aClip.ApplyNonRoundedIntersection(aGeometry->ComputeInvalidationRegion());
#ifdef MOZ_DUMP_PAINTING
    if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
      printf_stderr("Display item type %s(%p) added to layer %p!\n", aItem->Name(), aItem->Frame(), aNewLayer);
    }
#endif
  } else if (isInvalid || (aItem->IsInvalid(invalid) && invalid.IsEmpty())) {
    // Either layout marked item as needing repainting, invalidate the entire old and new areas.
    combined = oldClip->ApplyNonRoundedIntersection(oldGeometry->ComputeInvalidationRegion());
    combined.MoveBy(shift);
    combined.Or(combined, aClip.ApplyNonRoundedIntersection(aGeometry->ComputeInvalidationRegion()));
#ifdef MOZ_DUMP_PAINTING
    if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
      printf_stderr("Display item type %s(%p) (in layer %p) belongs to an invalidated frame!\n", aItem->Name(), aItem->Frame(), aNewLayer);
    }
#endif
  } else {
    // Let the display item check for geometry changes and decide what needs to be
    // repainted.

    // We have an optimization to cache the drawing background-attachment: fixed canvas
    // background images so we can scroll and just blit them when they are flattened into
    // the same layer as scrolling content. NotifyRenderingChanged is only used to tell
    // the canvas bg image item to purge this cache. We want to be careful not to accidentally
    // purge the cache if we are just invalidating due to scrolling (ie the background image
    // moves on the scrolling layer but it's rendering stays the same) so if
    // AddOffsetAndComputeDifference is the only thing that will invalidate we skip the
    // NotifyRenderingChanged call (ComputeInvalidationRegion for background images also calls
    // NotifyRenderingChanged if anything changes).
    if (oldGeometry->ComputeInvalidationRegion() == aGeometry->ComputeInvalidationRegion() &&
        *oldClip == aClip && invalid.IsEmpty() && changedFrames.Length() == 0) {
      notifyRenderingChanged = false;
    }

    oldGeometry->MoveBy(shift);
    aItem->ComputeInvalidationRegion(mBuilder, oldGeometry, &combined);
    oldClip->AddOffsetAndComputeDifference(shift, oldGeometry->ComputeInvalidationRegion(),
                                           aClip, aGeometry->ComputeInvalidationRegion(),
                                           &combined);

    // Add in any rect that the frame specified
    combined.Or(combined, invalid);

    for (uint32_t i = 0; i < changedFrames.Length(); i++) {
      combined.Or(combined, changedFrames[i]->GetVisualOverflowRect());
    }

    // Restrict invalidation to the clipped region
    nsRegion clip;
    if (aClip.ComputeRegionInClips(oldClip, shift, &clip)) {
      combined.And(combined, clip);
    }
#ifdef MOZ_DUMP_PAINTING
    if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
      if (!combined.IsEmpty()) {
        printf_stderr("Display item type %s(%p) (in layer %p) changed geometry!\n", aItem->Name(), aItem->Frame(), aNewLayer);
      }
    }
#endif
  }
  if (!combined.IsEmpty()) {
    if (notifyRenderingChanged) {
      aItem->NotifyRenderingChanged();
    }
    InvalidatePostTransformRegion(newThebesLayer,
        combined.ScaleToOutsidePixels(data->mXScale, data->mYScale, mAppUnitsPerDevPixel),
        GetTranslationForThebesLayer(newThebesLayer));
  }
}

void
FrameLayerBuilder::AddThebesDisplayItem(ThebesLayerData* aLayerData,
                                        nsDisplayItem* aItem,
                                        const DisplayItemClip& aClip,
                                        nsIFrame* aContainerLayerFrame,
                                        LayerState aLayerState,
                                        const nsPoint& aTopLeft,
                                        nsAutoPtr<nsDisplayItemGeometry> aGeometry)
{
  ThebesLayer* layer = aLayerData->mLayer;
  ThebesDisplayItemLayerUserData* thebesData =
    static_cast<ThebesDisplayItemLayerUserData*>
      (layer->GetUserData(&gThebesDisplayItemLayerUserData));
  nsRefPtr<BasicLayerManager> tempManager;
  nsIntRect intClip;
  bool hasClip = false;
  if (aLayerState != LAYER_NONE) {
    DisplayItemData *data = GetDisplayItemDataForManager(aItem, layer->Manager());
    if (data) {
      tempManager = data->mInactiveManager;
    }
    if (!tempManager) {
      tempManager = new BasicLayerManager(BasicLayerManager::BLM_INACTIVE);
    }

    // We need to grab these before calling AddLayerDisplayItem because it will overwrite them.
    nsRegion clip;
    DisplayItemClip* oldClip = nullptr;
    GetOldLayerFor(aItem, nullptr, &oldClip);
    hasClip = aClip.ComputeRegionInClips(oldClip,
                                         aTopLeft - thebesData->mLastAnimatedGeometryRootOrigin,
                                         &clip);

    if (hasClip) {
      intClip = clip.GetBounds().ScaleToOutsidePixels(thebesData->mXScale,
                                                      thebesData->mYScale,
                                                      thebesData->mAppUnitsPerDevPixel);
    }
  }

  AddLayerDisplayItem(layer, aItem, aClip, aLayerState, aTopLeft, tempManager, aGeometry);

  ThebesLayerItemsEntry* entry = mThebesLayerItems.PutEntry(layer);
  if (entry) {
    entry->mContainerLayerFrame = aContainerLayerFrame;
    if (entry->mContainerLayerGeneration == 0) {
      entry->mContainerLayerGeneration = mContainerLayerGeneration;
    }
    if (tempManager) {
      FrameLayerBuilder* layerBuilder = new FrameLayerBuilder();
      layerBuilder->Init(mDisplayListBuilder, tempManager, aLayerData);

      tempManager->BeginTransaction();
      if (mRetainingManager) {
        layerBuilder->DidBeginRetainedLayerTransaction(tempManager);
      }

      nsAutoPtr<LayerProperties> props(LayerProperties::CloneFrom(tempManager->GetRoot()));
      nsRefPtr<Layer> tmpLayer =
        aItem->BuildLayer(mDisplayListBuilder, tempManager, ContainerLayerParameters());
      // We have no easy way of detecting if this transaction will ever actually get finished.
      // For now, I've just silenced the warning with nested transactions in BasicLayers.cpp
      if (!tmpLayer) {
        tempManager->EndTransaction(nullptr, nullptr);
        tempManager->SetUserData(&gLayerManagerLayerBuilder, nullptr);
        return;
      }

      // If BuildLayer didn't call BuildContainerLayerFor, then our new layer won't have been
      // stored in layerBuilder. Manually add it now.
      if (mRetainingManager) {
#ifdef DEBUG_DISPLAY_ITEM_DATA
        LayerManagerData* parentLmd = static_cast<LayerManagerData*>
          (layer->Manager()->GetUserData(&gLayerManagerUserData));
        LayerManagerData* lmd = static_cast<LayerManagerData*>
          (tempManager->GetUserData(&gLayerManagerUserData));
        lmd->mParent = parentLmd;
#endif
        layerBuilder->StoreDataForFrame(aItem, tmpLayer, LAYER_ACTIVE);
      }

      tempManager->SetRoot(tmpLayer);
      layerBuilder->WillEndTransaction();
      tempManager->AbortTransaction();

      nsIntPoint offset = GetLastPaintOffset(layer) - GetTranslationForThebesLayer(layer);
      props->MoveBy(-offset);
      nsIntRegion invalid = props->ComputeDifferences(tmpLayer, nullptr);
      if (aLayerState == LAYER_SVG_EFFECTS) {
        invalid = nsSVGIntegrationUtils::AdjustInvalidAreaForSVGEffects(aItem->Frame(),
                                                                        aItem->ToReferenceFrame(),
                                                                        invalid);
      }
      if (!invalid.IsEmpty()) {
#ifdef MOZ_DUMP_PAINTING
        if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
          printf_stderr("Inactive LayerManager(%p) for display item %s(%p) has an invalid region - invalidating layer %p\n", tempManager.get(), aItem->Name(), aItem->Frame(), layer);
        }
#endif
        if (hasClip) {
          invalid.And(invalid, intClip);
        }

        invalid.ScaleRoundOut(thebesData->mXScale, thebesData->mYScale);
        InvalidatePostTransformRegion(layer, invalid,
                                      GetTranslationForThebesLayer(layer));
      }
    }
    ClippedDisplayItem* cdi =
      entry->mItems.AppendElement(ClippedDisplayItem(aItem,
                                                     mContainerLayerGeneration));
    cdi->mInactiveLayerManager = tempManager;
  }
}

FrameLayerBuilder::DisplayItemData*
FrameLayerBuilder::StoreDataForFrame(nsDisplayItem* aItem, Layer* aLayer, LayerState aState)
{
  DisplayItemData* oldData = GetDisplayItemDataForManager(aItem, mRetainingManager);
  if (oldData) {
    if (!oldData->mUsed) {
      oldData->UpdateContents(aLayer, aState, mContainerLayerGeneration, aItem);
    }
    return oldData;
  }

  LayerManagerData* lmd = static_cast<LayerManagerData*>
    (mRetainingManager->GetUserData(&gLayerManagerUserData));

  nsRefPtr<DisplayItemData> data =
    new DisplayItemData(lmd, aItem->GetPerFrameKey(),
                        aLayer, aState, mContainerLayerGeneration);

  data->AddFrame(aItem->Frame());

  nsAutoTArray<nsIFrame*,4> mergedFrames;
  aItem->GetMergedFrames(&mergedFrames);

  for (uint32_t i = 0; i < mergedFrames.Length(); ++i) {
    data->AddFrame(mergedFrames[i]);
  }

  lmd->mDisplayItems.PutEntry(data);
  return data;
}

void
FrameLayerBuilder::StoreDataForFrame(nsIFrame* aFrame,
                                     uint32_t aDisplayItemKey,
                                     Layer* aLayer,
                                     LayerState aState)
{
  DisplayItemData* oldData = GetDisplayItemData(aFrame, aDisplayItemKey);
  if (oldData && oldData->mFrameList.Length() == 1) {
    oldData->UpdateContents(aLayer, aState, mContainerLayerGeneration);
    return;
  }

  LayerManagerData* lmd = static_cast<LayerManagerData*>
    (mRetainingManager->GetUserData(&gLayerManagerUserData));

  nsRefPtr<DisplayItemData> data =
    new DisplayItemData(lmd, aDisplayItemKey, aLayer,
                        aState, mContainerLayerGeneration);

  data->AddFrame(aFrame);

  lmd->mDisplayItems.PutEntry(data);
}

FrameLayerBuilder::ClippedDisplayItem::~ClippedDisplayItem()
{
  if (mInactiveLayerManager) {
    BasicLayerManager* basic = static_cast<BasicLayerManager*>(mInactiveLayerManager.get());
    basic->SetUserData(&gLayerManagerLayerBuilder, nullptr);
  }
}

void
FrameLayerBuilder::AddLayerDisplayItem(Layer* aLayer,
                                       nsDisplayItem* aItem,
                                       const DisplayItemClip& aClip,
                                       LayerState aLayerState,
                                       const nsPoint& aTopLeft,
                                       BasicLayerManager* aManager,
                                       nsAutoPtr<nsDisplayItemGeometry> aGeometry)
{
  if (aLayer->Manager() != mRetainingManager)
    return;

  DisplayItemData *data = StoreDataForFrame(aItem, aLayer, aLayerState);
  ThebesLayer *t = aLayer->AsThebesLayer();
  if (t) {
    data->mGeometry = aGeometry;
    data->mClip = aClip;
  }
  data->mInactiveManager = aManager;
}

nsIntPoint
FrameLayerBuilder::GetLastPaintOffset(ThebesLayer* aLayer)
{
  ThebesLayerItemsEntry* entry = mThebesLayerItems.PutEntry(aLayer);
  if (entry) {
    if (entry->mContainerLayerGeneration == 0) {
      entry->mContainerLayerGeneration = mContainerLayerGeneration;
    }
    if (entry->mHasExplicitLastPaintOffset)
      return entry->mLastPaintOffset;
  }
  return GetTranslationForThebesLayer(aLayer);
}

void
FrameLayerBuilder::SaveLastPaintOffset(ThebesLayer* aLayer)
{
  ThebesLayerItemsEntry* entry = mThebesLayerItems.PutEntry(aLayer);
  if (entry) {
    if (entry->mContainerLayerGeneration == 0) {
      entry->mContainerLayerGeneration = mContainerLayerGeneration;
    }
    entry->mLastPaintOffset = GetTranslationForThebesLayer(aLayer);
    entry->mHasExplicitLastPaintOffset = true;
  }
}

bool
FrameLayerBuilder::CheckInLayerTreeCompressionMode()
{
  if (mInLayerTreeCompressionMode) {
    return true;
  }

  // If we wanted to be in layer tree compression mode, but weren't, then scheduled
  // a delayed repaint where we will be.
  mRootPresContext->PresShell()->GetRootFrame()->SchedulePaint(nsIFrame::PAINT_DELAYED_COMPRESS);

  return false;
}

void
ContainerState::CollectOldLayers()
{
  for (Layer* layer = mContainerLayer->GetFirstChild(); layer;
       layer = layer->GetNextSibling()) {
    NS_ASSERTION(!layer->HasUserData(&gMaskLayerUserData),
                 "Mask layer in layer tree; could not be recycled.");
    if (layer->HasUserData(&gThebesDisplayItemLayerUserData)) {
      NS_ASSERTION(layer->AsThebesLayer(), "Wrong layer type");
      mRecycledThebesLayers.AppendElement(static_cast<ThebesLayer*>(layer));
    }

    if (Layer* maskLayer = layer->GetMaskLayer()) {
      NS_ASSERTION(maskLayer->GetType() == Layer::TYPE_IMAGE,
                   "Could not recycle mask layer, unsupported layer type.");
      mRecycledMaskImageLayers.Put(layer, static_cast<ImageLayer*>(maskLayer));
    }
  }
}

void
ContainerState::Finish(uint32_t* aTextContentFlags, LayerManagerData* aData)
{
  while (!mThebesLayerDataStack.IsEmpty()) {
    PopThebesLayerData();
  }

  uint32_t textContentFlags = 0;

  // Make sure that current/existing layers are added to the parent and are
  // in the correct order.
  Layer* layer = nullptr;
  for (uint32_t i = 0; i < mNewChildLayers.Length(); ++i) {
    Layer* prevChild = i == 0 ? nullptr : mNewChildLayers[i - 1].get();
    layer = mNewChildLayers[i];

    if (!layer->GetVisibleRegion().IsEmpty()) {
      textContentFlags |= layer->GetContentFlags() & Layer::CONTENT_COMPONENT_ALPHA;
    }

    if (!layer->GetParent()) {
      // This is not currently a child of the container, so just add it
      // now.
      mContainerLayer->InsertAfter(layer, prevChild);
      continue;
    }

    NS_ASSERTION(layer->GetParent() == mContainerLayer,
                 "Layer shouldn't be the child of some other container");
    if (layer->GetPrevSibling() != prevChild) {
      mContainerLayer->RepositionChild(layer, prevChild);
    }
  }

  // Remove old layers that have become unused.
  if (!layer) {
    layer = mContainerLayer->GetFirstChild();
  } else {
    layer = layer->GetNextSibling();
  }
  while (layer) {
    Layer *layerToRemove = layer;
    layer = layer->GetNextSibling();
    mContainerLayer->RemoveChild(layerToRemove);
  }

  *aTextContentFlags = textContentFlags;
}

static inline gfxSize RoundToFloatPrecision(const gfxSize& aSize)
{
  return gfxSize(float(aSize.width), float(aSize.height));
}

static void RestrictScaleToMaxLayerSize(gfxSize& aScale,
                                        const nsRect& aVisibleRect,
                                        nsIFrame* aContainerFrame,
                                        Layer* aContainerLayer)
{
  if (!aContainerLayer->Manager()->IsWidgetLayerManager()) {
    return;
  }

  nsIntRect pixelSize =
    aVisibleRect.ScaleToOutsidePixels(aScale.width, aScale.height,
                                      aContainerFrame->PresContext()->AppUnitsPerDevPixel());

  int32_t maxLayerSize = aContainerLayer->GetMaxLayerSize();

  if (pixelSize.width > maxLayerSize) {
    float scale = (float)pixelSize.width / maxLayerSize;
    scale = gfxUtils::ClampToScaleFactor(scale);
    aScale.width /= scale;
  }
  if (pixelSize.height > maxLayerSize) {
    float scale = (float)pixelSize.height / maxLayerSize;
    scale = gfxUtils::ClampToScaleFactor(scale);
    aScale.height /= scale;
  }
}
static bool
ChooseScaleAndSetTransform(FrameLayerBuilder* aLayerBuilder,
                           nsDisplayListBuilder* aDisplayListBuilder,
                           nsIFrame* aContainerFrame,
                           const nsRect& aVisibleRect,
                           const gfx3DMatrix* aTransform,
                           const ContainerLayerParameters& aIncomingScale,
                           ContainerLayer* aLayer,
                           LayerState aState,
                           ContainerLayerParameters& aOutgoingScale)
{
  nsIntPoint offset;

  gfx3DMatrix transform =
    gfx3DMatrix::ScalingMatrix(aIncomingScale.mXScale, aIncomingScale.mYScale, 1.0);
  if (aTransform) {
    // aTransform is applied first, then the scale is applied to the result
    transform = (*aTransform)*transform;
    // Set any matrix entries close to integers to be those exact integers.
    // This protects against floating-point inaccuracies causing problems
    // in the checks below.
    // We use the fixed epsilon version here because we don't want the nudging
    // to depend on the scroll position.
    transform.NudgeToIntegersFixedEpsilon();
  }
  gfxMatrix transform2d;
  if (aContainerFrame &&
      (aState == LAYER_INACTIVE || aState == LAYER_SVG_EFFECTS) &&
      (!aTransform || (aTransform->Is2D(&transform2d) &&
                       !transform2d.HasNonTranslation()))) {
    // When we have an inactive ContainerLayer, translate the container by the offset to the
    // reference frame (and offset all child layers by the reverse) so that the coordinate
    // space of the child layers isn't affected by scrolling.
    // This gets confusing for complicated transform (since we'd have to compute the scale
    // factors for the matrix), so we don't bother. Any frames that are building an nsDisplayTransform
    // for a css transform would have 0,0 as their offset to the reference frame, so this doesn't
    // matter.
    nsPoint appUnitOffset = aDisplayListBuilder->ToReferenceFrame(aContainerFrame);
    nscoord appUnitsPerDevPixel = aContainerFrame->PresContext()->AppUnitsPerDevPixel();
    offset = nsIntPoint(
        NS_lround(NSAppUnitsToDoublePixels(appUnitOffset.x, appUnitsPerDevPixel)*aIncomingScale.mXScale),
        NS_lround(NSAppUnitsToDoublePixels(appUnitOffset.y, appUnitsPerDevPixel)*aIncomingScale.mYScale));
  }
  transform = transform * gfx3DMatrix::Translation(offset.x + aIncomingScale.mOffset.x, offset.y + aIncomingScale.mOffset.y, 0);

  if (transform.IsSingular()) {
    return false;
  }

  bool canDraw2D = transform.CanDraw2D(&transform2d);
  gfxSize scale;
  // XXX Should we do something for 3D transforms?
  if (canDraw2D) {
    // If the container's transform is animated off main thread, fix a suitable scale size
    // for animation
    if (aContainerFrame->GetContent() &&
        nsLayoutUtils::HasAnimationsForCompositor(
          aContainerFrame->GetContent(), eCSSProperty_transform)) {
      scale = nsLayoutUtils::ComputeSuitableScaleForAnimation(aContainerFrame->GetContent());
    } else {
      // Scale factors are normalized to a power of 2 to reduce the number of resolution changes
      scale = RoundToFloatPrecision(transform2d.ScaleFactors(true));
      // For frames with a changing transform that's not just a translation,
      // round scale factors up to nearest power-of-2 boundary so that we don't
      // keep having to redraw the content as it scales up and down. Rounding up to nearest
      // power-of-2 boundary ensures we never scale up, only down --- avoiding
      // jaggies. It also ensures we never scale down by more than a factor of 2,
      // avoiding bad downscaling quality.
      gfxMatrix frameTransform;
      if (ActiveLayerTracker::IsStyleAnimated(aContainerFrame, eCSSProperty_transform) &&
          aTransform &&
          (!aTransform->Is2D(&frameTransform) || frameTransform.HasNonTranslationOrFlip())) {
        // Don't clamp the scale factor when the new desired scale factor matches the old one
        // or it was previously unscaled.
        bool clamp = true;
        Matrix oldFrameTransform2d;
        if (aLayer->GetBaseTransform().Is2D(&oldFrameTransform2d)) {
          gfxSize oldScale = RoundToFloatPrecision(ThebesMatrix(oldFrameTransform2d).ScaleFactors(true));
          if (oldScale == scale || oldScale == gfxSize(1.0, 1.0)) {
            clamp = false;
          }
        }
        if (clamp) {
          scale.width = gfxUtils::ClampToScaleFactor(scale.width);
          scale.height = gfxUtils::ClampToScaleFactor(scale.height);
        }
      } else {
        // XXX Do we need to move nearly-integer values to integers here?
      }
    }
    // If the scale factors are too small, just use 1.0. The content is being
    // scaled out of sight anyway.
    if (fabs(scale.width) < 1e-8 || fabs(scale.height) < 1e-8) {
      scale = gfxSize(1.0, 1.0);
    }
    // If this is a transform container layer, then pre-rendering might
    // mean we try render a layer bigger than the max texture size. Apply
    // clmaping to prevent this.
    if (aTransform) {
      RestrictScaleToMaxLayerSize(scale, aVisibleRect, aContainerFrame, aLayer);
    }
  } else {
    scale = gfxSize(1.0, 1.0);
  }

  // Store the inverse of our resolution-scale on the layer
  Matrix4x4 baseTransform;
  ToMatrix4x4(transform, baseTransform);
  aLayer->SetBaseTransform(baseTransform);
  aLayer->SetPreScale(1.0f/float(scale.width),
                      1.0f/float(scale.height));
  aLayer->SetInheritedScale(aIncomingScale.mXScale,
                            aIncomingScale.mYScale);

  aOutgoingScale =
    ContainerLayerParameters(scale.width, scale.height, -offset, aIncomingScale);
  if (aTransform) {
    aOutgoingScale.mInTransformedSubtree = true;
    if (ActiveLayerTracker::IsStyleAnimated(aContainerFrame, eCSSProperty_transform)) {
      aOutgoingScale.mInActiveTransformedSubtree = true;
    }
  }
  bool isRetained = aLayer->Manager()->IsWidgetLayerManager();
  if (isRetained && (!canDraw2D || transform2d.HasNonIntegerTranslation())) {
    aOutgoingScale.mDisableSubpixelAntialiasingInDescendants = true;
  }
  return true;
}

/* static */ PLDHashOperator
FrameLayerBuilder::RestoreDisplayItemData(nsRefPtrHashKey<DisplayItemData>* aEntry, void* aUserArg)
{
  DisplayItemData* data = aEntry->GetKey();
  uint32_t *generation = static_cast<uint32_t*>(aUserArg);

  if (data->mUsed && data->mContainerLayerGeneration >= *generation) {
    return PL_DHASH_REMOVE;
  }

  return PL_DHASH_NEXT;
}

/* static */ PLDHashOperator
FrameLayerBuilder::RestoreThebesLayerItemEntries(ThebesLayerItemsEntry* aEntry, void* aUserArg)
{
  uint32_t *generation = static_cast<uint32_t*>(aUserArg);

  if (aEntry->mContainerLayerGeneration >= *generation) {
    // We can just remove these items rather than attempting to revert them
    // because we're going to want to invalidate everything when transitioning
    // to component alpha flattening.
    return PL_DHASH_REMOVE;
  }

  for (uint32_t i = 0; i < aEntry->mItems.Length(); i++) {
    if (aEntry->mItems[i].mContainerLayerGeneration >= *generation) {
      aEntry->mItems.TruncateLength(i);
      return PL_DHASH_NEXT;
    }
  }

  return PL_DHASH_NEXT;
}

already_AddRefed<ContainerLayer>
FrameLayerBuilder::BuildContainerLayerFor(nsDisplayListBuilder* aBuilder,
                                          LayerManager* aManager,
                                          nsIFrame* aContainerFrame,
                                          nsDisplayItem* aContainerItem,
                                          nsDisplayList* aChildren,
                                          const ContainerLayerParameters& aParameters,
                                          const gfx3DMatrix* aTransform,
                                          uint32_t aFlags)
{
  uint32_t containerDisplayItemKey =
    aContainerItem ? aContainerItem->GetPerFrameKey() : nsDisplayItem::TYPE_ZERO;
  NS_ASSERTION(aContainerFrame, "Container display items here should have a frame");
  NS_ASSERTION(!aContainerItem ||
               aContainerItem->Frame() == aContainerFrame,
               "Container display item must match given frame");

  if (!aParameters.mXScale || !aParameters.mYScale) {
    return nullptr;
  }

  nsRefPtr<ContainerLayer> containerLayer;
  if (aManager == mRetainingManager) {
    // Using GetOldLayerFor will search merged frames, as well as the underlying
    // frame. The underlying frame can change when a page scrolls, so this
    // avoids layer recreation in the situation that a new underlying frame is
    // picked for a layer.
    Layer* oldLayer = nullptr;
    if (aContainerItem) {
      oldLayer = GetOldLayerFor(aContainerItem);
    } else {
      DisplayItemData *data = GetOldLayerForFrame(aContainerFrame, containerDisplayItemKey);
      if (data) {
        oldLayer = data->mLayer;
      }
    }

    if (oldLayer) {
      NS_ASSERTION(oldLayer->Manager() == aManager, "Wrong manager");
      if (oldLayer->HasUserData(&gThebesDisplayItemLayerUserData)) {
        // The old layer for this item is actually our ThebesLayer
        // because we rendered its layer into that ThebesLayer. So we
        // don't actually have a retained container layer.
      } else {
        NS_ASSERTION(oldLayer->GetType() == Layer::TYPE_CONTAINER,
                     "Wrong layer type");
        containerLayer = static_cast<ContainerLayer*>(oldLayer);
        containerLayer->SetMaskLayer(nullptr);
      }
    }
  }
  if (!containerLayer) {
    // No suitable existing layer was found.
    containerLayer = aManager->CreateContainerLayer();
    if (!containerLayer)
      return nullptr;
  }

  LayerState state = aContainerItem ? aContainerItem->GetLayerState(aBuilder, aManager, aParameters) : LAYER_ACTIVE;
  if (state == LAYER_INACTIVE &&
      nsDisplayItem::ForceActiveLayers()) {
    state = LAYER_ACTIVE;
  }

  if (aContainerItem && state == LAYER_ACTIVE_EMPTY) {
    // Empty layers only have metadata and should never have display items. We
    // early exit because later, invalidation will walk up the frame tree to
    // determine which thebes layer gets invalidated. Since an empty layer
    // should never have anything to paint, it should never be invalidated.
    NS_ASSERTION(aChildren->IsEmpty(), "Should have no children");
    return containerLayer.forget();
  }

  ContainerLayerParameters scaleParameters;
  if (!ChooseScaleAndSetTransform(this, aBuilder, aContainerFrame, aChildren.GetVisibleRect(),
                                  aTransform, aParameters, containerLayer, state, scaleParameters)) {
    return nullptr;
  }

  uint32_t oldGeneration = mContainerLayerGeneration;
  mContainerLayerGeneration = ++mMaxContainerLayerGeneration;

  nsRefPtr<RefCountedRegion> thebesLayerInvalidRegion = nullptr;
  if (mRetainingManager) {
    if (aContainerItem) {
      StoreDataForFrame(aContainerItem, containerLayer, LAYER_ACTIVE);
    } else {
      StoreDataForFrame(aContainerFrame, containerDisplayItemKey, containerLayer, LAYER_ACTIVE);
    }
  }

  LayerManagerData* data = static_cast<LayerManagerData*>
    (aManager->GetUserData(&gLayerManagerUserData));

  nsRect bounds;
  nsIntRect pixBounds;
  int32_t appUnitsPerDevPixel;
  uint32_t stateFlags = 0;
  if ((aContainerFrame->GetStateBits() & NS_FRAME_NO_COMPONENT_ALPHA) &&
      mRetainingManager && !mRetainingManager->AreComponentAlphaLayersEnabled()) {
    stateFlags = ContainerState::NO_COMPONENT_ALPHA;
  }
  uint32_t flags;
  while (true) {
    ContainerState state(aBuilder, aManager, aManager->GetLayerBuilder(),
                         aContainerFrame, aContainerItem,
                         containerLayer, scaleParameters);

    state.ProcessDisplayItems(aChildren, stateFlags);

    // Set CONTENT_COMPONENT_ALPHA if any of our children have it.
    // This is suboptimal ... a child could have text that's over transparent
    // pixels in its own layer, but over opaque parts of previous siblings.
    state.Finish(&flags, data);
    bounds = state.GetChildrenBounds();
    pixBounds = state.ScaleToOutsidePixels(bounds, false);
    appUnitsPerDevPixel = state.GetAppUnitsPerDevPixel();

    if ((flags & Layer::CONTENT_COMPONENT_ALPHA) &&
        mRetainingManager &&
        !mRetainingManager->AreComponentAlphaLayersEnabled() &&
        !stateFlags) {
      // Since we don't want any component alpha layers on BasicLayers, we repeat
      // the layer building process with this explicitely forced off.
      // We restore the previous FrameLayerBuilder state since the first set
      // of layer building will have changed it.
      stateFlags = ContainerState::NO_COMPONENT_ALPHA;
      data->mDisplayItems.EnumerateEntries(RestoreDisplayItemData,
                                           &mContainerLayerGeneration);
      mThebesLayerItems.EnumerateEntries(RestoreThebesLayerItemEntries,
                                         &mContainerLayerGeneration);
      aContainerFrame->AddStateBits(NS_FRAME_NO_COMPONENT_ALPHA);
      continue;
    }
    break;
  }

  NS_ASSERTION(bounds.IsEqualInterior(aChildren->GetBounds(aBuilder)), "Wrong bounds");
  pixBounds.MoveBy(nsIntPoint(scaleParameters.mOffset.x, scaleParameters.mOffset.y));
  if (aParameters.mAncestorClipRect && !(aFlags & CONTAINER_NOT_CLIPPED_BY_ANCESTORS)) {
    SetVisibleRegionForLayer(containerLayer, nsIntRegion(pixBounds),
                             *aParameters.mAncestorClipRect);
  } else {
    containerLayer->SetVisibleRegion(pixBounds);
  }
  // Make sure that rounding the visible region out didn't add any area
  // we won't paint
  if (aChildren->IsOpaque() && !aChildren->NeedsTransparentSurface()) {
    bounds.ScaleRoundIn(scaleParameters.mXScale, scaleParameters.mYScale);
    if (bounds.Contains(pixBounds.ToAppUnits(appUnitsPerDevPixel))) {
      // Clear CONTENT_COMPONENT_ALPHA
      flags = Layer::CONTENT_OPAQUE;
    }
  }
  containerLayer->SetContentFlags(flags);

  mContainerLayerGeneration = oldGeneration;
  nsPresContext::ClearNotifySubDocInvalidationData(containerLayer);

  return containerLayer.forget();
}

Layer*
FrameLayerBuilder::GetLeafLayerFor(nsDisplayListBuilder* aBuilder,
                                   nsDisplayItem* aItem)
{
  Layer* layer = GetOldLayerFor(aItem);
  if (!layer)
    return nullptr;
  if (layer->HasUserData(&gThebesDisplayItemLayerUserData)) {
    // This layer was created to render Thebes-rendered content for this
    // display item. The display item should not use it for its own
    // layer rendering.
    return nullptr;
  }
  layer->SetMaskLayer(nullptr);
  return layer;
}

/* static */ void
FrameLayerBuilder::InvalidateAllLayers(LayerManager* aManager)
{
  LayerManagerData* data = static_cast<LayerManagerData*>
    (aManager->GetUserData(&gLayerManagerUserData));
  if (data) {
    data->mInvalidateAllLayers = true;
  }
}

/* static */ void
FrameLayerBuilder::InvalidateAllLayersForFrame(nsIFrame *aFrame)
{
  nsTArray<DisplayItemData*> *array =
    reinterpret_cast<nsTArray<DisplayItemData*>*>(aFrame->Properties().Get(LayerManagerDataProperty()));
  if (array) {
    for (uint32_t i = 0; i < array->Length(); i++) {
      array->ElementAt(i)->mParent->mInvalidateAllLayers = true;
    }
  }
}

/* static */
Layer*
FrameLayerBuilder::GetDedicatedLayer(nsIFrame* aFrame, uint32_t aDisplayItemKey)
{
  //TODO: This isn't completely correct, since a frame could exist as a layer
  // in the normal widget manager, and as a different layer (or no layer)
  // in the secondary manager

  nsTArray<DisplayItemData*> *array =
    reinterpret_cast<nsTArray<DisplayItemData*>*>(aFrame->Properties().Get(LayerManagerDataProperty()));
  if (array) {
    for (uint32_t i = 0; i < array->Length(); i++) {
      DisplayItemData *element = array->ElementAt(i);
      if (!element->mParent->mLayerManager->IsWidgetLayerManager()) {
        continue;
      }
      if (element->mDisplayItemKey == aDisplayItemKey) {
        if (element->mOptLayer) {
          return element->mOptLayer;
        }

        Layer* layer = element->mLayer;
        if (!layer->HasUserData(&gColorLayerUserData) &&
            !layer->HasUserData(&gImageLayerUserData) &&
            !layer->HasUserData(&gThebesDisplayItemLayerUserData)) {
          return layer;
        }
      }
    }
  }
  return nullptr;
}

static gfxSize
PredictScaleForContent(nsIFrame* aFrame, nsIFrame* aAncestorWithScale,
                       const gfxSize& aScale)
{
  gfx3DMatrix transform =
    gfx3DMatrix::ScalingMatrix(aScale.width, aScale.height, 1.0);
  if (aFrame != aAncestorWithScale) {
    // aTransform is applied first, then the scale is applied to the result
    transform = nsLayoutUtils::GetTransformToAncestor(aFrame, aAncestorWithScale)*transform;
  }
  gfxMatrix transform2d;
  if (transform.CanDraw2D(&transform2d)) {
     return transform2d.ScaleFactors(true);
  }
  return gfxSize(1.0, 1.0);
}

gfxSize
FrameLayerBuilder::GetThebesLayerScaleForFrame(nsIFrame* aFrame)
{
  MOZ_ASSERT(aFrame, "need a frame");
  nsIFrame* last = nullptr;
  for (nsIFrame* f = aFrame; f; f = nsLayoutUtils::GetCrossDocParentFrame(f)) {
    last = f;

    if (nsLayoutUtils::IsPopup(f)) {
      // Don't examine ancestors of a popup. It won't make sense to check
      // the transform from some content inside the popup to some content
      // which is an ancestor of the popup.
      break;
    }

    nsTArray<DisplayItemData*> *array =
      reinterpret_cast<nsTArray<DisplayItemData*>*>(f->Properties().Get(LayerManagerDataProperty()));
    if (!array) {
      continue;
    }

    for (uint32_t i = 0; i < array->Length(); i++) {
      Layer* layer = array->ElementAt(i)->mLayer;
      ContainerLayer* container = layer->AsContainerLayer();
      if (!container ||
          !layer->Manager()->IsWidgetLayerManager()) {
        continue;
      }
      for (Layer* l = container->GetFirstChild(); l; l = l->GetNextSibling()) {
        ThebesDisplayItemLayerUserData* data =
            static_cast<ThebesDisplayItemLayerUserData*>
              (l->GetUserData(&gThebesDisplayItemLayerUserData));
        if (data) {
          return PredictScaleForContent(aFrame, f, gfxSize(data->mXScale, data->mYScale));
        }
      }
    }
  }

  return PredictScaleForContent(aFrame, last,
      last->PresContext()->PresShell()->GetResolution());
}

#ifdef MOZ_DUMP_PAINTING
static void DebugPaintItem(nsRenderingContext* aDest,
                           nsPresContext* aPresContext,
                           nsDisplayItem *aItem,
                           nsDisplayListBuilder* aBuilder)
{
  bool snap;
  nsRect appUnitBounds = aItem->GetBounds(aBuilder, &snap);
  gfxRect bounds(appUnitBounds.x, appUnitBounds.y, appUnitBounds.width, appUnitBounds.height);
  bounds.ScaleInverse(aPresContext->AppUnitsPerDevPixel());

  RefPtr<DrawTarget> tempDT =
    gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
                                          IntSize(bounds.width, bounds.height),
                                          SurfaceFormat::B8G8R8A8);
  nsRefPtr<gfxContext> context = new gfxContext(tempDT);
  context->SetMatrix(gfxMatrix().Translate(-gfxPoint(bounds.x, bounds.y)));
  nsRefPtr<nsRenderingContext> ctx = new nsRenderingContext();
  ctx->Init(aDest->DeviceContext(), context);

  aItem->Paint(aBuilder, ctx);
  RefPtr<SourceSurface> surface = tempDT->Snapshot();
  DumpPaintedImage(aItem, surface);

  DrawTarget* drawTarget = aDest->ThebesContext()->GetDrawTarget();
  Rect rect = ToRect(bounds);
  drawTarget->DrawSurface(surface, rect, Rect(Point(0,0), rect.Size()));

  aItem->SetPainted();
}
#endif

/* static */ void
FrameLayerBuilder::RecomputeVisibilityForItems(nsTArray<ClippedDisplayItem>& aItems,
                                               nsDisplayListBuilder *aBuilder,
                                               const nsIntRegion& aRegionToDraw,
                                               const nsIntPoint& aOffset,
                                               int32_t aAppUnitsPerDevPixel,
                                               float aXScale,
                                               float aYScale)
{
  uint32_t i;
  // Update visible regions. We need perform visibility analysis again
  // because we may be asked to draw into part of a ThebesLayer that
  // isn't actually visible in the window (e.g., because a ThebesLayer
  // expanded its visible region to a rectangle internally), in which
  // case the mVisibleRect stored in the display item may be wrong.
  nsRegion visible = aRegionToDraw.ToAppUnits(aAppUnitsPerDevPixel);
  visible.MoveBy(NSIntPixelsToAppUnits(aOffset.x, aAppUnitsPerDevPixel),
                 NSIntPixelsToAppUnits(aOffset.y, aAppUnitsPerDevPixel));
  visible.ScaleInverseRoundOut(aXScale, aYScale);

  for (i = aItems.Length(); i > 0; --i) {
    ClippedDisplayItem* cdi = &aItems[i - 1];
    const DisplayItemClip& clip = cdi->mItem->GetClip();

    NS_ASSERTION(AppUnitsPerDevPixel(cdi->mItem) == aAppUnitsPerDevPixel,
                 "a thebes layer should contain items only at the same zoom");

    NS_ABORT_IF_FALSE(clip.HasClip() ||
                      clip.GetRoundedRectCount() == 0,
                      "If we have rounded rects, we must have a clip rect");

    if (!clip.IsRectAffectedByClip(visible.GetBounds())) {
      cdi->mItem->RecomputeVisibility(aBuilder, &visible);
      continue;
    }

    // Do a little dance to account for the fact that we're clipping
    // to cdi->mClipRect
    nsRegion clipped;
    clipped.And(visible, clip.NonRoundedIntersection());
    nsRegion finalClipped = clipped;
    cdi->mItem->RecomputeVisibility(aBuilder, &finalClipped);
    // If we have rounded clip rects, don't subtract from the visible
    // region since we aren't displaying everything inside the rect.
    if (clip.GetRoundedRectCount() == 0) {
      nsRegion removed;
      removed.Sub(clipped, finalClipped);
      nsRegion newVisible;
      newVisible.Sub(visible, removed);
      // Don't let the visible region get too complex.
      if (newVisible.GetNumRects() <= 15) {
        visible = newVisible;
      }
    }
  }
}

void
FrameLayerBuilder::PaintItems(nsTArray<ClippedDisplayItem>& aItems,
                              const nsIntRect& aRect,
                              gfxContext *aContext,
                              nsRenderingContext *aRC,
                              nsDisplayListBuilder* aBuilder,
                              nsPresContext* aPresContext,
                              const nsIntPoint& aOffset,
                              float aXScale, float aYScale,
                              int32_t aCommonClipCount)
{
  int32_t appUnitsPerDevPixel = aPresContext->AppUnitsPerDevPixel();
  nsRect boundRect = aRect.ToAppUnits(appUnitsPerDevPixel);
  boundRect.MoveBy(NSIntPixelsToAppUnits(aOffset.x, appUnitsPerDevPixel),
                 NSIntPixelsToAppUnits(aOffset.y, appUnitsPerDevPixel));
  boundRect.ScaleInverseRoundOut(aXScale, aYScale);

  DisplayItemClip currentClip;
  bool currentClipIsSetInContext = false;
  DisplayItemClip tmpClip;

  for (uint32_t i = 0; i < aItems.Length(); ++i) {
    ClippedDisplayItem* cdi = &aItems[i];

    nsRect paintRect = cdi->mItem->GetVisibleRect().Intersect(boundRect);
    if (paintRect.IsEmpty())
      continue;

#ifdef MOZ_DUMP_PAINTING
    PROFILER_LABEL_PRINTF("DisplayList", "Draw", js::ProfileEntry::Category::GRAPHICS, "%s %p", cdi->mItem->Name(), cdi->mItem);
#else
    PROFILER_LABEL_PRINTF("DisplayList", "Draw", js::ProfileEntry::Category::GRAPHICS, "%p", cdi->mItem);
#endif

    // If the new desired clip state is different from the current state,
    // update the clip.
    const DisplayItemClip* clip = &cdi->mItem->GetClip();
    if (clip->GetRoundedRectCount() > 0 &&
        !clip->IsRectClippedByRoundedCorner(cdi->mItem->GetVisibleRect())) {
      tmpClip = *clip;
      tmpClip.RemoveRoundedCorners();
      clip = &tmpClip;
    }
    if (currentClipIsSetInContext != clip->HasClip() ||
        (clip->HasClip() && *clip != currentClip)) {
      if (currentClipIsSetInContext) {
        aContext->Restore();
      }
      currentClipIsSetInContext = clip->HasClip();
      if (currentClipIsSetInContext) {
        currentClip = *clip;
        aContext->Save();
        NS_ASSERTION(aCommonClipCount < 100,
          "Maybe you really do have more than a hundred clipping rounded rects, or maybe something has gone wrong.");
        currentClip.ApplyTo(aContext, aPresContext, aCommonClipCount);
        aContext->NewPath();
      }
    }

    if (cdi->mInactiveLayerManager) {
      PaintInactiveLayer(aBuilder, cdi->mInactiveLayerManager, cdi->mItem, aContext, aRC);
    } else {
      nsIFrame* frame = cdi->mItem->Frame();
      frame->AddStateBits(NS_FRAME_PAINTED_THEBES);
#ifdef MOZ_DUMP_PAINTING

      if (gfxUtils::sDumpPainting) {
        DebugPaintItem(aRC, aPresContext, cdi->mItem, aBuilder);
      } else {
#else
      {
#endif
        cdi->mItem->Paint(aBuilder, aRC);
      }
    }

    if (CheckDOMModified())
      break;
  }

  if (currentClipIsSetInContext) {
    aContext->Restore();
  }
}

/**
 * Returns true if it is preferred to draw the list of display
 * items separately for each rect in the visible region rather
 * than clipping to a complex region.
 */
static bool ShouldDrawRectsSeparately(gfxContext* aContext, DrawRegionClip aClip)
{
  if (!gfxPrefs::LayoutPaintRectsSeparately() ||
      aClip == DrawRegionClip::CLIP_NONE) {
    return false;
  }

  DrawTarget *dt = aContext->GetDrawTarget();
  return dt->GetBackendType() == BackendType::DIRECT2D;
}

static void DrawForcedBackgroundColor(gfxContext* aContext, Layer* aLayer, nscolor aBackgroundColor)
{
  if (NS_GET_A(aBackgroundColor) > 0) {
    nsIntRect r = aLayer->GetVisibleRegion().GetBounds();
    aContext->NewPath();
    aContext->Rectangle(gfxRect(r.x, r.y, r.width, r.height));
    aContext->SetColor(gfxRGBA(aBackgroundColor));
    aContext->Fill();
  }
}

/*
 * A note on residual transforms:
 *
 * In a transformed subtree we sometimes apply the ThebesLayer's
 * "residual transform" when drawing content into the ThebesLayer.
 * This is a translation by components in the range [-0.5,0.5) provided
 * by the layer system; applying the residual transform followed by the
 * transforms used by layer compositing ensures that the subpixel alignment
 * of the content of the ThebesLayer exactly matches what it would be if
 * we used cairo/Thebes to draw directly to the screen without going through
 * retained layer buffers.
 *
 * The visible and valid regions of the ThebesLayer are computed without
 * knowing the residual transform (because we don't know what the residual
 * transform is going to be until we've built the layer tree!). So we have to
 * consider whether content painted in the range [x, xmost) might be painted
 * outside the visible region we computed for that content. The visible region
 * would be [floor(x), ceil(xmost)). The content would be rendered at
 * [x + r, xmost + r), where -0.5 <= r < 0.5. So some half-rendered pixels could
 * indeed fall outside the computed visible region, which is not a big deal;
 * similar issues already arise when we snap cliprects to nearest pixels.
 * Note that if the rendering of the content is snapped to nearest pixels ---
 * which it often is --- then the content is actually rendered at
 * [snap(x + r), snap(xmost + r)). It turns out that floor(x) <= snap(x + r)
 * and ceil(xmost) >= snap(xmost + r), so the rendering of snapped content
 * always falls within the visible region we computed.
 */

/* static */ void
FrameLayerBuilder::DrawThebesLayer(ThebesLayer* aLayer,
                                   gfxContext* aContext,
                                   const nsIntRegion& aRegionToDraw,
                                   DrawRegionClip aClip,
                                   const nsIntRegion& aRegionToInvalidate,
                                   void* aCallbackData)
{
  PROFILER_LABEL("FrameLayerBuilder", "DrawThebesLayer",
    js::ProfileEntry::Category::GRAPHICS);

  nsDisplayListBuilder* builder = static_cast<nsDisplayListBuilder*>
    (aCallbackData);

  FrameLayerBuilder *layerBuilder = aLayer->Manager()->GetLayerBuilder();
  NS_ASSERTION(layerBuilder, "Unexpectedly null layer builder!");

  if (layerBuilder->CheckDOMModified())
    return;

  ThebesLayerItemsEntry* entry = layerBuilder->mThebesLayerItems.GetEntry(aLayer);
  NS_ASSERTION(entry, "We shouldn't be drawing into a layer with no items!");
  if (!entry->mContainerLayerFrame) {
    return;
  }


  ThebesDisplayItemLayerUserData* userData =
    static_cast<ThebesDisplayItemLayerUserData*>
      (aLayer->GetUserData(&gThebesDisplayItemLayerUserData));
  NS_ASSERTION(userData, "where did our user data go?");

  bool shouldDrawRectsSeparately = ShouldDrawRectsSeparately(aContext, aClip);

  if (!shouldDrawRectsSeparately) {
    if (aClip == DrawRegionClip::DRAW_SNAPPED) {
      gfxUtils::ClipToRegionSnapped(aContext, aRegionToDraw);
    } else if (aClip == DrawRegionClip::DRAW) {
      gfxUtils::ClipToRegion(aContext, aRegionToDraw);
    }

    DrawForcedBackgroundColor(aContext, aLayer, userData->mForcedBackgroundColor);
  }

  // make the origin of the context coincide with the origin of the
  // ThebesLayer
  gfxContextMatrixAutoSaveRestore saveMatrix(aContext);
  nsIntPoint offset = GetTranslationForThebesLayer(aLayer);

  nsPresContext* presContext = entry->mContainerLayerFrame->PresContext();
  int32_t appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();

  RecomputeVisibilityForItems(entry->mItems, builder, aRegionToDraw,
                              offset, appUnitsPerDevPixel,
                              userData->mXScale, userData->mYScale);

  nsRefPtr<nsRenderingContext> rc = new nsRenderingContext();
  rc->Init(presContext->DeviceContext(), aContext);

  if (shouldDrawRectsSeparately) {
    nsIntRegionRectIterator it(aRegionToDraw);
    while (const nsIntRect* iterRect = it.Next()) {
      gfxContextAutoSaveRestore save(aContext);
      aContext->NewPath();
      aContext->Rectangle(*iterRect, aClip == DrawRegionClip::DRAW_SNAPPED);
      aContext->Clip();

      DrawForcedBackgroundColor(aContext, aLayer, userData->mForcedBackgroundColor);

      // Apply the residual transform if it has been enabled, to ensure that
      // snapping when we draw into aContext exactly matches the ideal transform.
      // See above for why this is OK.
      aContext->Translate(aLayer->GetResidualTranslation() - gfxPoint(offset.x, offset.y));
      aContext->Scale(userData->mXScale, userData->mYScale);

      layerBuilder->PaintItems(entry->mItems, *iterRect, aContext, rc,
                               builder, presContext,
                               offset, userData->mXScale, userData->mYScale,
                               entry->mCommonClipCount);
    }
  } else {
    // Apply the residual transform if it has been enabled, to ensure that
    // snapping when we draw into aContext exactly matches the ideal transform.
    // See above for why this is OK.
    aContext->Translate(aLayer->GetResidualTranslation() - gfxPoint(offset.x, offset.y));
    aContext->Scale(userData->mXScale, userData->mYScale);

    layerBuilder->PaintItems(entry->mItems, aRegionToDraw.GetBounds(), aContext, rc,
                             builder, presContext,
                             offset, userData->mXScale, userData->mYScale,
                             entry->mCommonClipCount);
  }

  if (presContext->GetPaintFlashing() &&
      !aLayer->Manager()->IsInactiveLayerManager()) {
    gfxContextAutoSaveRestore save(aContext);
    if (shouldDrawRectsSeparately) {
      if (aClip == DrawRegionClip::DRAW_SNAPPED) {
        gfxUtils::ClipToRegionSnapped(aContext, aRegionToDraw);
      } else if (aClip == DrawRegionClip::DRAW) {
        gfxUtils::ClipToRegion(aContext, aRegionToDraw);
      }
    }
    FlashPaint(aContext);
  }

  if (!aRegionToInvalidate.IsEmpty()) {
    aLayer->AddInvalidRect(aRegionToInvalidate.GetBounds());
  }
}

bool
FrameLayerBuilder::CheckDOMModified()
{
  if (!mRootPresContext ||
      mInitialDOMGeneration == mRootPresContext->GetDOMGeneration())
    return false;
  if (mDetectedDOMModification) {
    // Don't spam the console with extra warnings
    return true;
  }
  mDetectedDOMModification = true;
  // Painting is not going to complete properly. There's not much
  // we can do here though. Invalidating the window to get another repaint
  // is likely to lead to an infinite repaint loop.
  NS_WARNING("Detected DOM modification during paint, bailing out!");
  return true;
}

#ifdef MOZ_DUMP_PAINTING
/* static */ void
FrameLayerBuilder::DumpRetainedLayerTree(LayerManager* aManager, std::stringstream& aStream, bool aDumpHtml)
{
  aManager->Dump(aStream, "", aDumpHtml);
}
#endif

gfx::Rect
CalculateBounds(const nsTArray<DisplayItemClip::RoundedRect>& aRects, int32_t A2D)
{
  nsRect bounds = aRects[0].mRect;
  for (uint32_t i = 1; i < aRects.Length(); ++i) {
    bounds.UnionRect(bounds, aRects[i].mRect);
   }

  return gfx::ToRect(nsLayoutUtils::RectToGfxRect(bounds, A2D));
}

static void
SetClipCount(ThebesDisplayItemLayerUserData* aThebesData,
             uint32_t aClipCount)
{
  if (aThebesData) {
    aThebesData->mMaskClipCount = aClipCount;
  }
}

void
ContainerState::SetupMaskLayer(Layer *aLayer, const DisplayItemClip& aClip,
                               uint32_t aRoundedRectClipCount)
{
  // if the number of clips we are going to mask has decreased, then aLayer might have
  // cached graphics which assume the existence of a soon-to-be non-existent mask layer
  // in that case, invalidate the whole layer.
  ThebesDisplayItemLayerUserData* thebesData = GetThebesDisplayItemLayerUserData(aLayer);
  if (thebesData &&
      aRoundedRectClipCount < thebesData->mMaskClipCount) {
    ThebesLayer* thebes = aLayer->AsThebesLayer();
    thebes->InvalidateRegion(thebes->GetValidRegion().GetBounds());
  }

  // don't build an unnecessary mask
  nsIntRect layerBounds = aLayer->GetVisibleRegion().GetBounds();
  if (aClip.GetRoundedRectCount() == 0 ||
      aRoundedRectClipCount == 0 ||
      layerBounds.IsEmpty()) {
    SetClipCount(thebesData, 0);
    return;
  }

  // check if we can re-use the mask layer
  nsRefPtr<ImageLayer> maskLayer =  CreateOrRecycleMaskImageLayerFor(aLayer);
  MaskLayerUserData* userData = GetMaskLayerUserData(maskLayer);

  MaskLayerUserData newData;
  aClip.AppendRoundedRects(&newData.mRoundedClipRects, aRoundedRectClipCount);
  newData.mScaleX = mParameters.mXScale;
  newData.mScaleY = mParameters.mYScale;
  newData.mOffset = mParameters.mOffset;
  newData.mAppUnitsPerDevPixel = mContainerFrame->PresContext()->AppUnitsPerDevPixel();

  if (*userData == newData) {
    aLayer->SetMaskLayer(maskLayer);
    SetClipCount(thebesData, aRoundedRectClipCount);
    return;
  }

  // calculate a more precise bounding rect
  gfx::Rect boundingRect = CalculateBounds(newData.mRoundedClipRects,
                                           newData.mAppUnitsPerDevPixel);
  boundingRect.Scale(mParameters.mXScale, mParameters.mYScale);

  uint32_t maxSize = mManager->GetMaxTextureSize();
  NS_ASSERTION(maxSize > 0, "Invalid max texture size");
  gfx::Size surfaceSize(std::min<gfx::Float>(boundingRect.Width(), maxSize),
                        std::min<gfx::Float>(boundingRect.Height(), maxSize));

  // maskTransform is applied to the clip when it is painted into the mask (as a
  // component of imageTransform), and its inverse used when the mask is used for
  // masking.
  // It is the transform from the masked layer's space to mask space
  gfx::Matrix maskTransform;
  maskTransform.Scale(surfaceSize.width/boundingRect.Width(),
                      surfaceSize.height/boundingRect.Height());
  gfx::Point p = boundingRect.TopLeft();
  maskTransform.Translate(-p.x, -p.y);
  // imageTransform is only used when the clip is painted to the mask
  gfx::Matrix imageTransform = maskTransform;
  imageTransform.Scale(mParameters.mXScale, mParameters.mYScale);

  nsAutoPtr<MaskLayerImageCache::MaskLayerImageKey> newKey(
    new MaskLayerImageCache::MaskLayerImageKey());

  // copy and transform the rounded rects
  for (uint32_t i = 0; i < newData.mRoundedClipRects.Length(); ++i) {
    newKey->mRoundedClipRects.AppendElement(
      MaskLayerImageCache::PixelRoundedRect(newData.mRoundedClipRects[i],
                                            mContainerFrame->PresContext()));
    newKey->mRoundedClipRects[i].ScaleAndTranslate(imageTransform);
  }

  const MaskLayerImageCache::MaskLayerImageKey* lookupKey = newKey;

  // check to see if we can reuse a mask image
  nsRefPtr<ImageContainer> container =
    GetMaskLayerImageCache()->FindImageFor(&lookupKey);

  if (!container) {
    IntSize surfaceSizeInt(NSToIntCeil(surfaceSize.width),
                           NSToIntCeil(surfaceSize.height));
    // no existing mask image, so build a new one
    RefPtr<DrawTarget> dt =
      aLayer->Manager()->CreateOptimalMaskDrawTarget(surfaceSizeInt);

    // fail if we can't get the right surface
    if (!dt) {
      NS_WARNING("Could not create DrawTarget for mask layer.");
      SetClipCount(thebesData, 0);
      return;
    }

    nsRefPtr<gfxContext> context = new gfxContext(dt);
    context->Multiply(ThebesMatrix(imageTransform));

    // paint the clipping rects with alpha to create the mask
    context->SetColor(gfxRGBA(1, 1, 1, 1));
    aClip.DrawRoundedRectsTo(context,
                             newData.mAppUnitsPerDevPixel,
                             0,
                             aRoundedRectClipCount);

    RefPtr<SourceSurface> surface = dt->Snapshot();

    // build the image and container
    container = aLayer->Manager()->CreateImageContainer();
    NS_ASSERTION(container, "Could not create image container for mask layer.");
    nsRefPtr<Image> image = container->CreateImage(ImageFormat::CAIRO_SURFACE);
    NS_ASSERTION(image, "Could not create image container for mask layer.");
    CairoImage::Data data;
    data.mSize = surfaceSizeInt;
    data.mSourceSurface = surface;

    static_cast<CairoImage*>(image.get())->SetData(data);
    container->SetCurrentImageInTransaction(image);

    GetMaskLayerImageCache()->PutImage(newKey.forget(), container);
  }

  maskLayer->SetContainer(container);

  maskTransform.Invert();
  Matrix4x4 matrix = Matrix4x4::From2D(maskTransform);
  matrix.Translate(mParameters.mOffset.x, mParameters.mOffset.y, 0);
  maskLayer->SetBaseTransform(matrix);

  // save the details of the clip in user data
  userData->mScaleX = newData.mScaleX;
  userData->mScaleY = newData.mScaleY;
  userData->mOffset = newData.mOffset;
  userData->mAppUnitsPerDevPixel = newData.mAppUnitsPerDevPixel;
  userData->mRoundedClipRects.SwapElements(newData.mRoundedClipRects);
  userData->mImageKey = lookupKey;

  aLayer->SetMaskLayer(maskLayer);
  SetClipCount(thebesData, aRoundedRectClipCount);
  return;
}

} // namespace mozilla
