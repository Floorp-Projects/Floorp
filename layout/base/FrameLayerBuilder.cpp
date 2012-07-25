/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FrameLayerBuilder.h"

#include "nsDisplayList.h"
#include "nsPresContext.h"
#include "nsLayoutUtils.h"
#include "Layers.h"
#include "BasicLayers.h"
#include "nsSubDocumentFrame.h"
#include "nsCSSRendering.h"
#include "nsCSSFrameConstructor.h"
#include "gfxUtils.h"
#include "nsImageFrame.h"
#include "nsRenderingContext.h"
#include "MaskLayerImageCache.h"

#include "mozilla/Preferences.h"
#include "sampler.h"

#include "nsAnimationManager.h"
#include "nsTransitionManager.h"

#ifdef DEBUG
#include <stdio.h>
#endif

using namespace mozilla::layers;

namespace mozilla {

/**
 * This is the userdata we associate with a layer manager.
 */
class LayerManagerData : public LayerUserData {
public:
  LayerManagerData(LayerManager *aManager) :
    mInvalidateAllLayers(false),
    mLayerManager(aManager)
  {
    MOZ_COUNT_CTOR(LayerManagerData);
    mFramesWithLayers.Init();
  }
  ~LayerManagerData() {
    // Remove display item data properties now, since we won't be able
    // to find these frames again without mFramesWithLayers.
    mFramesWithLayers.EnumerateEntries(
        FrameLayerBuilder::RemoveDisplayItemDataForFrame, nsnull);
    MOZ_COUNT_DTOR(LayerManagerData);
  }

  /**
   * Tracks which frames have layers associated with them.
   */
  nsTHashtable<FrameLayerBuilder::DisplayItemDataEntry> mFramesWithLayers;
  bool mInvalidateAllLayers;
  /** Layer manager we belong to, we hold a reference to this object. */
  nsRefPtr<LayerManager> mLayerManager;
};

LayerManagerLayerBuilder::~LayerManagerLayerBuilder()
{
  MOZ_COUNT_DTOR(LayerManagerLayerBuilder);
  if (mDelete) {
    delete mLayerBuilder;
  }
}

namespace {

// a global cache of image containers used for mask layers
static MaskLayerImageCache* gMaskLayerImageCache = nsnull;

static inline MaskLayerImageCache* GetMaskLayerImageCache()
{
  if (!gMaskLayerImageCache) {
    gMaskLayerImageCache = new MaskLayerImageCache();
  }

  return gMaskLayerImageCache;
}

static void DestroyRefCountedRegion(void* aPropertyValue)
{
  static_cast<RefCountedRegion*>(aPropertyValue)->Release();
}

/**
 * This property represents a region that should be invalidated in every
 * ThebesLayer child whose parent ContainerLayer is associated with the
 * frame. This is an nsRegion*; the coordinates of the region are
 * relative to the top-left of the border-box of the frame the property
 * is attached to (which is the frame for the ContainerLayer).
 * 
 * We add to this region in InvalidateThebesLayerContents. The region
 * is propagated to ContainerState in BuildContainerLayerFor, and then
 * the region(s) are actually invalidated in CreateOrRecycleThebesLayer.
 *
 * When the property value is null, the region is infinite --- i.e. all
 * areas of the ThebesLayers should be invalidated.
 */
NS_DECLARE_FRAME_PROPERTY(ThebesLayerInvalidRegionProperty, DestroyRefCountedRegion)

static void DestroyPoint(void* aPropertyValue)
{
  delete static_cast<nsPoint*>(aPropertyValue);
}

/**
 * The valid content in our child ThebesLayers is defined relative to
 * the offset from this frame to its active scroll root, mapped back
 * by the ThebesLayer's inverse transform.  Since we accumulate the
 * region invalidated between last-paint and next-paint, and because
 * the offset of this frame to its active root may change during that
 * period, we save the offset at last-paint in this property and use
 * it to invalidate at next-paint.
 */
NS_DECLARE_FRAME_PROPERTY(ThebesLayerLastPaintOffsetProperty, DestroyPoint)

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
                 ContainerLayer* aContainerLayer,
                 const FrameLayerBuilder::ContainerParameters& aParameters) :
    mBuilder(aBuilder), mManager(aManager),
    mLayerBuilder(aLayerBuilder),
    mContainerFrame(aContainerFrame), mContainerLayer(aContainerLayer),
    mParameters(aParameters),
    mNextFreeRecycledThebesLayer(0), mNextFreeRecycledColorLayer(0),
    mNextFreeRecycledImageLayer(0), mInvalidateAllThebesContent(false)
  {
    nsPresContext* presContext = aContainerFrame->PresContext();
    mAppUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
    // When AllowResidualTranslation is false, display items will be drawn
    // scaled with a translation by integer pixels, so we know how the snapping
    // will work.
    mSnappingEnabled = aManager->IsSnappingEffectiveTransforms() &&
      !mParameters.AllowResidualTranslation();
    mRecycledMaskImageLayers.Init();
    CollectOldLayers();
  }

  enum ProcessDisplayItemsFlags {
    NO_COMPONENT_ALPHA = 0x01,
  };

  void AddInvalidThebesContent(const nsIntRegion& aRegion)
  {
    mInvalidThebesContent.Or(mInvalidThebesContent, aRegion);
  }
  void SetInvalidateAllThebesContent()
  {
    mInvalidateAllThebesContent = true;
  }
  /**
   * This is the method that actually walks a display list and builds
   * the child layers. We invoke it recursively to process clipped sublists.
   * @param aClipRect the clip rect to apply to the list items, or null
   * if no clipping is required
   */
  void ProcessDisplayItems(const nsDisplayList& aList,
                           FrameLayerBuilder::Clip& aClip,
                           PRUint32 aFlags);
  /**
   * This finalizes all the open ThebesLayers by popping every element off
   * mThebesLayerDataStack, then sets the children of the container layer
   * to be all the layers in mNewChildLayers in that order and removes any
   * layers as children of the container that aren't in mNewChildLayers.
   * @param aTextContentFlags if any child layer has CONTENT_COMPONENT_ALPHA,
   * set *aTextContentFlags to CONTENT_COMPONENT_ALPHA
   */
  void Finish(PRUint32 *aTextContentFlags);

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
  nsIntRect ScaleToOutsidePixels(const nsRect& aRect, bool aSnap)
  {
    if (aSnap && mSnappingEnabled) {
      return ScaleToNearestPixels(aRect);
    }
    return aRect.ScaleToOutsidePixels(mParameters.mXScale, mParameters.mYScale,
                                      mAppUnitsPerDevPixel);
  }
  nsIntRect ScaleToInsidePixels(const nsRect& aRect, bool aSnap)
  {
    if (aSnap && mSnappingEnabled) {
      return ScaleToNearestPixels(aRect);
    }
    return aRect.ScaleToInsidePixels(mParameters.mXScale, mParameters.mYScale,
                                     mAppUnitsPerDevPixel);
  }

  nsIntRegion ScaleRegionToInsidePixels(const nsRegion& aRegion, bool aSnap)
  {
    if (aSnap && mSnappingEnabled) {
      return ScaleRegionToNearestPixels(aRegion);
    }
    return aRegion.ScaleToInsidePixels(mParameters.mXScale, mParameters.mYScale,
                                        mAppUnitsPerDevPixel);
  }

  const FrameLayerBuilder::ContainerParameters& ScaleParameters() { return mParameters; };

protected:
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
      mActiveScrolledRoot(nsnull), mLayer(nsnull),
      mIsSolidColorInVisibleRegion(false),
      mNeedComponentAlpha(false),
      mForceTransparentSurface(false),
      mImage(nsnull),
      mCommonClipCount(-1) {}
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
                    const FrameLayerBuilder::Clip& aClip);
    nsIFrame* GetActiveScrolledRoot() { return mActiveScrolledRoot; }

    /**
     * If this represents only a nsDisplayImage, and the image type
     * supports being optimized to an ImageLayer (TYPE_RASTER only) returns
     * an ImageContainer for the image.
     */
    already_AddRefed<ImageContainer> CanOptimizeImageLayer();

    /**
     * The region of visible content in the layer, relative to the
     * container layer (which is at the snapped top-left of the display
     * list reference frame).
     */
    nsIntRegion  mVisibleRegion;
    /**
     * The region of visible content above the layer and below the
     * next ThebesLayerData currently in the stack, if any. Note that not
     * all ThebesLayers for the container are in the ThebesLayerData stack.
     * Same coordinate system as mVisibleRegion.
     * This is a conservative approximation: it contains the true region.
     */
    nsIntRegion  mVisibleAboveRegion;
    /**
     * The region containing the bounds of all display items in the layer,
     * regardless of visbility.
     * Same coordinate system as mVisibleRegion.
     * This is a conservative approximation: it contains the true region.
     */
    nsIntRegion  mDrawRegion;
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
     * The region of visible content in the layer that is opaque.
     * Same coordinate system as mVisibleRegion.
     */
    nsIntRegion  mOpaqueRegion;
    /**
     * The "active scrolled root" for all content in the layer. Must
     * be non-null; all content in a ThebesLayer must have the same
     * active scrolled root.
     */
    nsIFrame*    mActiveScrolledRoot;
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
    nsDisplayImage* mImage;
    /**
     * Stores the clip that we need to apply to the image or, if there is no
     * image, a clip for SOME item in the layer. There is no guarantee which
     * item's clip will be stored here and mItemClip should not be used to clip
     * the whole layer - only some part of the clip should be used, as determined
     * by ThebesDisplayItemLayerUserData::GetCommonClipCount() - which may even be
     * no part at all.
     */
    FrameLayerBuilder::Clip mItemClip;
    /**
     * The first mCommonClipCount rounded rectangle clips are identical for
     * all items in the layer.
     * -1 if there are no items in the layer; must be >=0 by the time that this
     * data is popped from the stack.
     */
    PRInt32 mCommonClipCount;
    /*
     * Updates mCommonClipCount by checking for rounded rect clips in common
     * between the clip on a new item (aCurrentClip) and the common clips
     * on items already in the layer (the first mCommonClipCount rounded rects
     * in mItemClip).
     */
    void UpdateCommonClipCount(const FrameLayerBuilder::Clip& aCurrentClip);
  };
  friend class ThebesLayerData;

  /**
   * Grab the next recyclable ThebesLayer, or create one if there are no
   * more recyclable ThebesLayers. Does any necessary invalidation of
   * a recycled ThebesLayer, and sets up the transform on the ThebesLayer
   * to account for scrolling.
   */
  already_AddRefed<ThebesLayer> CreateOrRecycleThebesLayer(nsIFrame* aActiveScrolledRoot);
  /**
   * Grab the next recyclable ColorLayer, or create one if there are no
   * more recyclable ColorLayers.
   */
  already_AddRefed<ColorLayer> CreateOrRecycleColorLayer();
  /**
   * Grab the next recyclable ImageLayer, or create one if there are no
   * more recyclable ImageLayers.
   */
  already_AddRefed<ImageLayer> CreateOrRecycleImageLayer();
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
  void InvalidateForLayerChange(nsDisplayItem* aItem, Layer* aNewLayer);
  /**
   * Try to determine whether the ThebesLayer at aThebesLayerIndex
   * has a single opaque color behind it, over the entire bounds of its visible
   * region.
   * If successful, return that color, otherwise return NS_RGBA(0,0,0,0).
   */
  nscolor FindOpaqueBackgroundColorFor(PRInt32 aThebesLayerIndex);
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
   * @param aActiveScrolledRoot the active scrolled root for the next
   * display item
   * @param aOpaqueRect if non-null, a region of the display item that is opaque
   * @param aSolidColor if non-null, indicates that every pixel in aVisibleRect
   * will be painted with aSolidColor by the item
   */
  ThebesLayerData* FindThebesLayerFor(nsDisplayItem* aItem,
                                                   const nsIntRect& aVisibleRect,
                                                   const nsIntRect& aDrawRect,
                                                   const FrameLayerBuilder::Clip& aClip,
                                                   nsIFrame* aActiveScrolledRoot);
  ThebesLayerData* GetTopThebesLayerData()
  {
    return mThebesLayerDataStack.IsEmpty() ? nsnull
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
  void SetupMaskLayer(Layer *aLayer, const FrameLayerBuilder::Clip& aClip,
                      PRUint32 aRoundedRectClipCount = PR_UINT32_MAX);

  nsDisplayListBuilder*            mBuilder;
  LayerManager*                    mManager;
  FrameLayerBuilder*               mLayerBuilder;
  nsIFrame*                        mContainerFrame;
  ContainerLayer*                  mContainerLayer;
  FrameLayerBuilder::ContainerParameters mParameters;
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
  nsTArray<nsRefPtr<ColorLayer> >  mRecycledColorLayers;
  nsTArray<nsRefPtr<ImageLayer> >  mRecycledImageLayers;
  nsDataHashtable<nsPtrHashKey<Layer>, nsRefPtr<ImageLayer> >
    mRecycledMaskImageLayers;
  PRUint32                         mNextFreeRecycledThebesLayer;
  PRUint32                         mNextFreeRecycledColorLayer;
  PRUint32                         mNextFreeRecycledImageLayer;
  nscoord                          mAppUnitsPerDevPixel;
  bool                             mInvalidateAllThebesContent;
  bool                             mSnappingEnabled;
};

class ThebesDisplayItemLayerUserData : public LayerUserData
{
public:
  ThebesDisplayItemLayerUserData() :
    mForcedBackgroundColor(NS_RGBA(0,0,0,0)),
    mXScale(1.f), mYScale(1.f),
    mActiveScrolledRootPosition(0, 0) {}

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
   * We try to make 0,0 of the ThebesLayer be the top-left of the
   * border-box of the "active scrolled root" frame (i.e. the nearest ancestor
   * frame for the display items that is being actively scrolled). But
   * we force the ThebesLayer transform to be an integer translation, and we may
   * have a resolution scale, so we have to snap the ThebesLayer transform, so
   * 0,0 may not be exactly the top-left of the active scrolled root. Here we
   * store the coordinates in ThebesLayer space of the top-left of the
   * active scrolled root.
   */
  gfxPoint mActiveScrolledRootPosition;
};

/*
 * User data for layers which will be used as masks.
 */
struct MaskLayerUserData : public LayerUserData
{
  MaskLayerUserData() : mImageKey(nsnull) {}

  bool
  operator== (const MaskLayerUserData& aOther) const
  {
    return mRoundedClipRects == aOther.mRoundedClipRects &&
           mScaleX == aOther.mScaleX &&
           mScaleY == aOther.mScaleY;
  }

  nsRefPtr<const MaskLayerImageCache::MaskLayerImageKey> mImageKey;
  // properties of the mask layer; the mask layer may be re-used if these
  // remain unchanged.
  nsTArray<FrameLayerBuilder::Clip::RoundedRect> mRoundedClipRects;
  // scale from the masked layer which is applied to the mask
  float mScaleX, mScaleY;
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
PRUint8 gThebesDisplayItemLayerUserData;
/**
 * The address of gColorLayerUserData is used as the user
 * data key for ColorLayers created by FrameLayerBuilder.
 * The user data is null.
 */
PRUint8 gColorLayerUserData;
/**
 * The address of gImageLayerUserData is used as the user
 * data key for ImageLayers created by FrameLayerBuilder.
 * The user data is null.
 */
PRUint8 gImageLayerUserData;
/**
 * The address of gLayerManagerUserData is used as the user
 * data key for retained LayerManagers managed by FrameLayerBuilder.
 * The user data is a LayerManagerData.
 */
PRUint8 gLayerManagerUserData;
/**
 * The address of gMaskLayerUserData is used as the user
 * data key for mask layers managed by FrameLayerBuilder.
 * The user data is a MaskLayerUserData.
 */
PRUint8 gMaskLayerUserData;

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

} // anonymous namespace

PRUint8 gLayerManagerLayerBuilder;

/* static */ void
FrameLayerBuilder::Shutdown()
{
  if (gMaskLayerImageCache) {
    delete gMaskLayerImageCache;
    gMaskLayerImageCache = nsnull;
  }
}

void
FrameLayerBuilder::Init(nsDisplayListBuilder* aBuilder)
{
  mRootPresContext = aBuilder->ReferenceFrame()->PresContext()->GetRootPresContext();
  if (mRootPresContext) {
    mInitialDOMGeneration = mRootPresContext->GetDOMGeneration();
  }
}

bool
FrameLayerBuilder::DisplayItemDataEntry::HasNonEmptyContainerLayer()
{
  if (mIsSharingContainerLayer)
    return true;
  for (PRUint32 i = 0; i < mData.Length(); ++i) {
    if (mData[i].mLayer->GetType() == Layer::TYPE_CONTAINER &&
        mData[i].mLayerState != LAYER_ACTIVE_EMPTY)
      return true;
  }
  return false;
}

void
FrameLayerBuilder::FlashPaint(gfxContext *aContext)
{
  static bool sPaintFlashingEnabled;
  static bool sPaintFlashingPrefCached = false;

  if (!sPaintFlashingPrefCached) {
    sPaintFlashingPrefCached = true;
    mozilla::Preferences::AddBoolVarCache(&sPaintFlashingEnabled,
                                          "nglayout.debug.paint_flashing");
  }

  if (sPaintFlashingEnabled) {
    float r = float(rand()) / RAND_MAX;
    float g = float(rand()) / RAND_MAX;
    float b = float(rand()) / RAND_MAX;
    aContext->SetColor(gfxRGBA(r, g, b, 0.2));
    aContext->Paint();
  }
}

/* static */ nsTArray<FrameLayerBuilder::DisplayItemData>*
FrameLayerBuilder::GetDisplayItemDataArrayForFrame(nsIFrame* aFrame)
{
  FrameProperties props = aFrame->Properties();
  LayerManagerData *data =
    reinterpret_cast<LayerManagerData*>(props.Get(LayerManagerDataProperty()));
  if (!data)
    return nsnull;

  DisplayItemDataEntry *entry = data->mFramesWithLayers.GetEntry(aFrame);
  NS_ASSERTION(entry, "out of sync?");
  if (!entry)
    return nsnull;

  return &entry->mData;
}

/* static */ void
FrameLayerBuilder::RemoveFrameFromLayerManager(nsIFrame* aFrame,
                                               void* aPropertyValue)
{
  LayerManagerData *data = reinterpret_cast<LayerManagerData*>(aPropertyValue);
  data->mFramesWithLayers.RemoveEntry(aFrame);
  if (data->mFramesWithLayers.Count() == 0) {
    data->mLayerManager->RemoveUserData(&gLayerManagerUserData);
  }
}

void
FrameLayerBuilder::DidBeginRetainedLayerTransaction(LayerManager* aManager)
{
  mRetainingManager = aManager;
  LayerManagerData* data = static_cast<LayerManagerData*>
    (aManager->GetUserData(&gLayerManagerUserData));
  if (data) {
    mInvalidateAllLayers = data->mInvalidateAllLayers;
  }
}

/**
 * A helper function to remove the mThebesLayerItems entries for every
 * layer in aLayer's subtree.
 */
void
FrameLayerBuilder::RemoveThebesItemsForLayerSubtree(Layer* aLayer)
{
  ThebesLayer* thebes = aLayer->AsThebesLayer();
  if (thebes) {
    mThebesLayerItems.RemoveEntry(thebes);
    return;
  }

  for (Layer* child = aLayer->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    RemoveThebesItemsForLayerSubtree(child);
  }
}

void
FrameLayerBuilder::DidEndTransaction(LayerManager* aManager)
{
  if (aManager != mRetainingManager) {
    Layer* root = aManager->GetRoot();
    if (root) {
      RemoveThebesItemsForLayerSubtree(root);
    }
  }

  GetMaskLayerImageCache()->Sweep();
}

void
FrameLayerBuilder::WillEndTransaction(LayerManager* aManager)
{
  if (aManager != mRetainingManager)
    return;

  // We need to save the data we'll need to support retaining. We do this
  // before we paint so that invalidation triggered by painting will
  // be able to update the ThebesLayerInvalidRegionProperty values
  // correctly and the NS_FRAME_HAS_CONTAINER_LAYER bits will be set
  // correctly.
  LayerManagerData* data = static_cast<LayerManagerData*>
    (mRetainingManager->GetUserData(&gLayerManagerUserData));
  if (data) {
    // Update all the frames that used to have layers.
    data->mFramesWithLayers.EnumerateEntries(UpdateDisplayItemDataForFrame, this);
  } else {
    data = new LayerManagerData(mRetainingManager);
    mRetainingManager->SetUserData(&gLayerManagerUserData, data);
  }
  // Now go through all the frames that didn't have any retained
  // display items before, and record those retained display items.
  // This also empties mNewDisplayItemData.
  mNewDisplayItemData.EnumerateEntries(StoreNewDisplayItemData, data);
  data->mInvalidateAllLayers = false;

  NS_ASSERTION(data->mFramesWithLayers.Count() > 0,
               "Some frame must have a layer!");
}

/**
 * If *aThebesLayerInvalidRegion is non-null, use it as this frame's
 * region property. Otherwise set it to the frame's region property.
 */
static void
SetHasContainerLayer(nsIFrame* aFrame, nsPoint aOffsetToRoot)
{
  aFrame->AddStateBits(NS_FRAME_HAS_CONTAINER_LAYER);
  for (nsIFrame* f = aFrame;
       f && !(f->GetStateBits() & NS_FRAME_HAS_CONTAINER_LAYER_DESCENDANT);
       f = nsLayoutUtils::GetCrossDocParentFrame(f)) {
    f->AddStateBits(NS_FRAME_HAS_CONTAINER_LAYER_DESCENDANT);
  }

  FrameProperties props = aFrame->Properties();
  nsPoint* lastPaintOffset = static_cast<nsPoint*>
    (props.Get(ThebesLayerLastPaintOffsetProperty()));
  if (lastPaintOffset) {
    *lastPaintOffset = aOffsetToRoot;
  } else {
    props.Set(ThebesLayerLastPaintOffsetProperty(), new nsPoint(aOffsetToRoot));
  }
}

static void
SetNoContainerLayer(nsIFrame* aFrame)
{
  FrameProperties props = aFrame->Properties();
  props.Delete(ThebesLayerInvalidRegionProperty());
  props.Delete(ThebesLayerLastPaintOffsetProperty());
  aFrame->RemoveStateBits(NS_FRAME_HAS_CONTAINER_LAYER);
}

/* static */ void
FrameLayerBuilder::SetAndClearInvalidRegion(DisplayItemDataEntry* aEntry)
{
  if (aEntry->mInvalidRegion) {
    nsIFrame* f = aEntry->GetKey();
    FrameProperties props = f->Properties();

    RefCountedRegion* invalidRegion;
    aEntry->mInvalidRegion.forget(&invalidRegion);

    invalidRegion->mRegion.SetEmpty();
    props.Set(ThebesLayerInvalidRegionProperty(), invalidRegion);
  }
}

/* static */ PLDHashOperator
FrameLayerBuilder::UpdateDisplayItemDataForFrame(DisplayItemDataEntry* aEntry,
                                                 void* aUserArg)
{
  FrameLayerBuilder* builder = static_cast<FrameLayerBuilder*>(aUserArg);
  nsIFrame* f = aEntry->GetKey();
  FrameProperties props = f->Properties();
  DisplayItemDataEntry* newDisplayItems =
    builder ? builder->mNewDisplayItemData.GetEntry(f) : nsnull;
  if (!newDisplayItems || newDisplayItems->mData.IsEmpty()) {
    // This frame was visible, but isn't anymore.
    if (newDisplayItems) {
      builder->mNewDisplayItemData.RawRemoveEntry(newDisplayItems);
    }
    bool found;
    props.Remove(LayerManagerDataProperty(), &found);
    NS_ASSERTION(found, "How can the frame property be missing?");
    SetNoContainerLayer(f);
    return PL_DHASH_REMOVE;
  }

  if (!newDisplayItems->HasNonEmptyContainerLayer()) {
    SetNoContainerLayer(f);
  }

  // Steal the list of display item layers and invalid region
  aEntry->mData.SwapElements(newDisplayItems->mData);
  aEntry->mInvalidRegion.swap(newDisplayItems->mInvalidRegion);
  // Clear and reset the invalid region now so we can start collecting new
  // dirty areas.
  SetAndClearInvalidRegion(aEntry);
  // Don't need to process this frame again
  builder->mNewDisplayItemData.RawRemoveEntry(newDisplayItems);
  return PL_DHASH_NEXT;
}

/* static */ PLDHashOperator
FrameLayerBuilder::StoreNewDisplayItemData(DisplayItemDataEntry* aEntry,
                                           void* aUserArg)
{
  LayerManagerData* data = static_cast<LayerManagerData*>(aUserArg);
  nsIFrame* f = aEntry->GetKey();
  FrameProperties props = f->Properties();

  // Clear and reset the invalid region now so we can start collecting new
  // dirty areas.
  SetAndClearInvalidRegion(aEntry);

  // Remember that this frame has display items in retained layers
  NS_ASSERTION(!data->mFramesWithLayers.GetEntry(f),
               "We shouldn't get here if we're already in mFramesWithLayers");
  DisplayItemDataEntry *newEntry = data->mFramesWithLayers.PutEntry(f);
  NS_ASSERTION(!props.Get(LayerManagerDataProperty()),
               "mFramesWithLayers out of sync");

  newEntry->mData.SwapElements(aEntry->mData);
  props.Set(LayerManagerDataProperty(), data);
  return PL_DHASH_REMOVE;
}

bool
FrameLayerBuilder::HasRetainedLayerFor(nsIFrame* aFrame, PRUint32 aDisplayItemKey)
{
  nsTArray<DisplayItemData> *array = GetDisplayItemDataArrayForFrame(aFrame);
  if (!array)
    return false;

  for (PRUint32 i = 0; i < array->Length(); ++i) {
    if (array->ElementAt(i).mDisplayItemKey == aDisplayItemKey) {
      Layer* layer = array->ElementAt(i).mLayer;
      if (layer->Manager()->GetUserData(&gLayerManagerUserData)) {
        // All layer managers with our user data are retained layer managers
        return true;
      }
    }
  }
  return false;
}

Layer*
FrameLayerBuilder::GetOldLayerFor(nsIFrame* aFrame, PRUint32 aDisplayItemKey)
{
  // If we need to build a new layer tree, then just refuse to recycle
  // anything.
  if (!mRetainingManager || mInvalidateAllLayers)
    return nsnull;

  nsTArray<DisplayItemData> *array = GetDisplayItemDataArrayForFrame(aFrame);
  if (!array)
    return nsnull;

  for (PRUint32 i = 0; i < array->Length(); ++i) {
    if (array->ElementAt(i).mDisplayItemKey == aDisplayItemKey) {
      Layer* layer = array->ElementAt(i).mLayer;
      if (layer->Manager() == mRetainingManager)
        return layer;
    }
  }
  return nsnull;
}

/* static */ Layer*
FrameLayerBuilder::GetDebugOldLayerFor(nsIFrame* aFrame, PRUint32 aDisplayItemKey)
{
  FrameProperties props = aFrame->Properties();
  LayerManagerData* data = static_cast<LayerManagerData*>(props.Get(LayerManagerDataProperty()));
  if (!data) {
    return nsnull;
  }
  DisplayItemDataEntry *entry = data->mFramesWithLayers.GetEntry(aFrame);
  if (!entry)
    return nsnull;

  nsTArray<DisplayItemData> *array = &entry->mData;
  if (!array)
    return nsnull;

  for (PRUint32 i = 0; i < array->Length(); ++i) {
    if (array->ElementAt(i).mDisplayItemKey == aDisplayItemKey) {
      return array->ElementAt(i).mLayer;
    }
  }
  return nsnull;
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
}

already_AddRefed<ColorLayer>
ContainerState::CreateOrRecycleColorLayer()
{
  nsRefPtr<ColorLayer> layer;
  if (mNextFreeRecycledColorLayer < mRecycledColorLayers.Length()) {
    // Recycle a layer
    layer = mRecycledColorLayers[mNextFreeRecycledColorLayer];
    ++mNextFreeRecycledColorLayer;
    // Clear clip rect and mask layer so we don't accidentally stay clipped.
    // We will reapply any necessary clipping.
    layer->SetClipRect(nsnull);
    layer->SetMaskLayer(nsnull);
  } else {
    // Create a new layer
    layer = mManager->CreateColorLayer();
    if (!layer)
      return nsnull;
    // Mark this layer as being used for Thebes-painting display items
    layer->SetUserData(&gColorLayerUserData, nsnull);
  }
  return layer.forget();
}

already_AddRefed<ImageLayer>
ContainerState::CreateOrRecycleImageLayer()
{
  nsRefPtr<ImageLayer> layer;
  if (mNextFreeRecycledImageLayer < mRecycledImageLayers.Length()) {
    // Recycle a layer
    layer = mRecycledImageLayers[mNextFreeRecycledImageLayer];
    ++mNextFreeRecycledImageLayer;
    // Clear clip rect and mask layer so we don't accidentally stay clipped.
    // We will reapply any necessary clipping.
    layer->SetClipRect(nsnull);
    layer->SetMaskLayer(nsnull);
  } else {
    // Create a new layer
    layer = mManager->CreateImageLayer();
    if (!layer)
      return nsnull;
    // Mark this layer as being used for Thebes-painting display items
    layer->SetUserData(&gImageLayerUserData, nsnull);
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
      return nsnull;
    result->SetUserData(&gMaskLayerUserData, new MaskLayerUserData());
    result->SetForceSingleTile(true);
  }

  return result.forget();
}

static nsIntPoint
GetTranslationForThebesLayer(ThebesLayer* aLayer)
{
  gfxMatrix transform;
  if (!aLayer->GetTransform().Is2D(&transform) ||
      transform.HasNonIntegerTranslation()) {
    NS_ERROR("ThebesLayers should have integer translations only");
    return nsIntPoint(0, 0);
  }
  return nsIntPoint(PRInt32(transform.x0), PRInt32(transform.y0));
}

static const double SUBPIXEL_OFFSET_EPSILON = 0.02;

static PRBool
SubpixelOffsetFuzzyEqual(gfxPoint aV1, gfxPoint aV2)
{
  return fabs(aV2.x - aV1.x) < SUBPIXEL_OFFSET_EPSILON &&
         fabs(aV2.y - aV1.y) < SUBPIXEL_OFFSET_EPSILON;
}

/**
 * This normally computes NSToIntRoundUp(aValue). However, if that would
 * give a residual near 0.5 while aOldResidual is near -0.5, or
 * it would give a residual near -0.5 while aOldResidual is near 0.5, then
 * instead we return the integer in the other direction so that the residual
 * is close to aOldResidual.
 */
static PRInt32
RoundToMatchResidual(double aValue, double aOldResidual)
{
  PRInt32 v = NSToIntRoundUp(aValue);
  double residual = aValue - v;
  if (aOldResidual < 0) {
    if (residual > 0 && fabs(residual - 1.0 - aOldResidual) < SUBPIXEL_OFFSET_EPSILON) {
      // Round up instead
      return PRInt32(ceil(aValue));
    }
  } else if (aOldResidual > 0) {
    if (residual < 0 && fabs(residual + 1.0 - aOldResidual) < SUBPIXEL_OFFSET_EPSILON) {
      // Round down instead
      return PRInt32(floor(aValue));
    }
  }
  return v;
}

already_AddRefed<ThebesLayer>
ContainerState::CreateOrRecycleThebesLayer(nsIFrame* aActiveScrolledRoot)
{
  // We need a new thebes layer
  nsRefPtr<ThebesLayer> layer;
  ThebesDisplayItemLayerUserData* data;
  if (mNextFreeRecycledThebesLayer < mRecycledThebesLayers.Length()) {
    // Recycle a layer
    layer = mRecycledThebesLayers[mNextFreeRecycledThebesLayer];
    ++mNextFreeRecycledThebesLayer;
    // Clear clip rect and mask layer so we don't accidentally stay clipped.
    // We will reapply any necessary clipping.
    layer->SetClipRect(nsnull);
    layer->SetMaskLayer(nsnull);

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
    if (mInvalidateAllThebesContent ||
        data->mXScale != mParameters.mXScale ||
        data->mYScale != mParameters.mYScale) {
      nsIntRect invalidate = layer->GetValidRegion().GetBounds();
      layer->InvalidateRegion(invalidate);
    } else {
      InvalidatePostTransformRegion(layer, mInvalidThebesContent,
                                    GetTranslationForThebesLayer(layer));
    }
    // We do not need to Invalidate these areas in the widget because we
    // assume the caller of InvalidateThebesLayerContents has ensured
    // the area is invalidated in the widget.
  } else {
    // Create a new thebes layer
    layer = mManager->CreateThebesLayer();
    if (!layer)
      return nsnull;
    // Mark this layer as being used for Thebes-painting display items
    data = new ThebesDisplayItemLayerUserData();
    layer->SetUserData(&gThebesDisplayItemLayerUserData, data);
  }
  data->mXScale = mParameters.mXScale;
  data->mYScale = mParameters.mYScale;
  layer->SetAllowResidualTranslation(mParameters.AllowResidualTranslation());

  mLayerBuilder->SaveLastPaintOffset(layer);

  // Set up transform so that 0,0 in the Thebes layer corresponds to the
  // (pixel-snapped) top-left of the aActiveScrolledRoot.
  nsPoint offset = mBuilder->ToReferenceFrame(aActiveScrolledRoot);
  nscoord appUnitsPerDevPixel = aActiveScrolledRoot->PresContext()->AppUnitsPerDevPixel();
  gfxPoint scaledOffset(
      NSAppUnitsToDoublePixels(offset.x, appUnitsPerDevPixel)*mParameters.mXScale,
      NSAppUnitsToDoublePixels(offset.y, appUnitsPerDevPixel)*mParameters.mYScale);
  // We call RoundToMatchResidual here so that the residual after rounding
  // is close to data->mActiveScrolledRootPosition if possible.
  nsIntPoint pixOffset(RoundToMatchResidual(scaledOffset.x, data->mActiveScrolledRootPosition.x),
                       RoundToMatchResidual(scaledOffset.y, data->mActiveScrolledRootPosition.y));
  gfxMatrix matrix;
  matrix.Translate(gfxPoint(pixOffset.x, pixOffset.y));
  layer->SetBaseTransform(gfx3DMatrix::From2D(matrix));

  // FIXME: Temporary workaround for bug 681192 and bug 724786.
#ifndef MOZ_JAVA_COMPOSITOR
  // Calculate exact position of the top-left of the active scrolled root.
  // This might not be 0,0 due to the snapping in ScaleToNearestPixels.
  gfxPoint activeScrolledRootTopLeft = scaledOffset - matrix.GetTranslation();
  // If it has changed, then we need to invalidate the entire layer since the
  // pixels in the layer buffer have the content at a (subpixel) offset
  // from what we need.
  if (!SubpixelOffsetFuzzyEqual(activeScrolledRootTopLeft, data->mActiveScrolledRootPosition)) {
    data->mActiveScrolledRootPosition = activeScrolledRootTopLeft;
    nsIntRect invalidate = layer->GetValidRegion().GetBounds();
    layer->InvalidateRegion(invalidate);
  }
#endif

  return layer.forget();
}

/**
 * Returns the appunits per dev pixel for the item's frame. The item must
 * have a frame because only nsDisplayClip items don't have a frame,
 * and those items are flattened away by ProcessDisplayItems.
 */
static PRInt32
AppUnitsPerDevPixel(nsDisplayItem* aItem)
{
  // The underlying frame for zoom items is the root frame of the subdocument.
  // But zoom display items report their bounds etc using the parent document's
  // APD because zoom items act as a conversion layer between the two different
  // APDs.
  if (aItem->GetType() == nsDisplayItem::TYPE_ZOOM) {
    return static_cast<nsDisplayZoom*>(aItem)->GetParentAppUnitsPerDevPixel();
  }
  return aItem->GetUnderlyingFrame()->PresContext()->AppUnitsPerDevPixel();
}

/**
 * Restrict the visible region of aLayer to the region that is actually visible.
 * Because we only reduce the visible region here, we don't need to worry
 * about whether CONTENT_OPAQUE is set; if layer was opauqe in the old
 * visible region, it will still be opaque in the new one.
 * @param aItemVisible the visible region of the display item (that is,
 * after any layer transform has been applied)
 */
static void
RestrictVisibleRegionForLayer(Layer* aLayer, const nsIntRect& aItemVisible)
{
  gfx3DMatrix transform = aLayer->GetTransform();

  // if 'transform' is not invertible, then nothing will be displayed
  // for the layer, so it doesn't really matter what we do here
  gfxRect itemVisible(aItemVisible.x, aItemVisible.y, aItemVisible.width, aItemVisible.height);
  gfxRect layerVisible = transform.Inverse().ProjectRectBounds(itemVisible);
  layerVisible.RoundOut();

  nsIntRect visibleRect;
  if (!gfxUtils::GfxRectToIntRect(layerVisible, &visibleRect))
    return;

  nsIntRegion rgn = aLayer->GetVisibleRegion();
  if (!visibleRect.Contains(rgn.GetBounds())) {
    rgn.And(rgn, visibleRect);
    aLayer->SetVisibleRegion(rgn);
  }
}

nscolor
ContainerState::FindOpaqueBackgroundColorFor(PRInt32 aThebesLayerIndex)
{
  ThebesLayerData* target = mThebesLayerDataStack[aThebesLayerIndex];
  for (PRInt32 i = aThebesLayerIndex - 1; i >= 0; --i) {
    ThebesLayerData* candidate = mThebesLayerDataStack[i];
    nsIntRegion visibleAboveIntersection;
    visibleAboveIntersection.And(candidate->mVisibleAboveRegion, target->mVisibleRegion);
    if (!visibleAboveIntersection.IsEmpty()) {
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
    nsRect rect =
      target->mVisibleRegion.GetBounds().ToAppUnits(mAppUnitsPerDevPixel);
    rect.ScaleInverseRoundOut(mParameters.mXScale, mParameters.mYScale);
    return mLayerBuilder->
      FindOpaqueColorCovering(mBuilder, candidate->mLayer, rect);
  }
  return NS_RGBA(0,0,0,0);
}

void
ContainerState::ThebesLayerData::UpdateCommonClipCount(
    const FrameLayerBuilder::Clip& aCurrentClip)
{
  if (mCommonClipCount >= 0) {
    PRInt32 end = NS_MIN<PRInt32>(aCurrentClip.mRoundedClipRects.Length(),
                                  mCommonClipCount);
    PRInt32 clipCount = 0;
    for (; clipCount < end; ++clipCount) {
      if (mItemClip.mRoundedClipRects[clipCount] !=
          aCurrentClip.mRoundedClipRects[clipCount]) {
        break;
      }
    }
    mCommonClipCount = clipCount;
    NS_ASSERTION(mItemClip.mRoundedClipRects.Length() >= PRUint32(mCommonClipCount),
                 "Inconsistent common clip count.");
  } else {
    // first item in the layer
    mCommonClipCount = aCurrentClip.mRoundedClipRects.Length();
  }
}

already_AddRefed<ImageContainer>
ContainerState::ThebesLayerData::CanOptimizeImageLayer()
{
  if (!mImage) {
    return nsnull;
  }

  return mImage->GetContainer();
}

void
ContainerState::PopThebesLayerData()
{
  NS_ASSERTION(!mThebesLayerDataStack.IsEmpty(), "Can't pop");

  PRInt32 lastIndex = mThebesLayerDataStack.Length() - 1;
  ThebesLayerData* data = mThebesLayerDataStack[lastIndex];

  nsRefPtr<Layer> layer;
  nsRefPtr<ImageContainer> imageContainer = data->CanOptimizeImageLayer();

  if ((data->mIsSolidColorInVisibleRegion || imageContainer) &&
      data->mLayer->GetValidRegion().IsEmpty()) {
    NS_ASSERTION(!(data->mIsSolidColorInVisibleRegion && imageContainer),
                 "Can't be a solid color as well as an image!");
    if (imageContainer) {
      nsRefPtr<ImageLayer> imageLayer = CreateOrRecycleImageLayer();
      imageLayer->SetContainer(imageContainer);
      data->mImage->ConfigureLayer(imageLayer);
      // The layer's current transform is applied first, then the result is scaled.
      imageLayer->SetScale(mParameters.mXScale, mParameters.mYScale);
      if (data->mItemClip.mHaveClipRect) {
        nsIntRect clip = ScaleToNearestPixels(data->mItemClip.mClipRect);
        imageLayer->IntersectClipRect(clip);
      }
      layer = imageLayer;
    } else {
      nsRefPtr<ColorLayer> colorLayer = CreateOrRecycleColorLayer();
      colorLayer->SetIsFixedPosition(data->mLayer->GetIsFixedPosition());
      colorLayer->SetColor(data->mSolidColor);

      // Copy transform
      colorLayer->SetBaseTransform(data->mLayer->GetTransform());
      colorLayer->SetScale(data->mLayer->GetXScale(), data->mLayer->GetYScale());

      // Clip colorLayer to its visible region, since ColorLayers are
      // allowed to paint outside the visible region. Here we rely on the
      // fact that uniform display items fill rectangles; obviously the
      // area to fill must contain the visible region, and because it's
      // a rectangle, it must therefore contain the visible region's GetBounds.
      // Note that the visible region is already clipped appropriately.
      nsIntRect visibleRect = data->mVisibleRegion.GetBounds();
      colorLayer->SetClipRect(&visibleRect);

      layer = colorLayer;
    }

    NS_ASSERTION(!mNewChildLayers.Contains(layer), "Layer already in list???");
    AutoLayersArray::index_type index = mNewChildLayers.IndexOf(data->mLayer);
    NS_ASSERTION(index != AutoLayersArray::NoIndex, "Thebes layer not found?");
    mNewChildLayers.InsertElementAt(index + 1, layer);

    // Hide the ThebesLayer. We leave it in the layer tree so that we
    // can find and recycle it later.
    data->mLayer->IntersectClipRect(nsIntRect());
    data->mLayer->SetVisibleRegion(nsIntRegion());
  } else {
    layer = data->mLayer;
    imageContainer = nsnull;
  }

  gfxMatrix transform;
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
    rgn.MoveBy(-nsIntPoint(PRInt32(transform.x0), PRInt32(transform.y0)));
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
      data->mLayer->InvalidateRegion(data->mLayer->GetValidRegion());
    }
    userData->mForcedBackgroundColor = backgroundColor;

    // use a mask layer for rounded rect clipping
    PRInt32 commonClipCount = data->mCommonClipCount;
    NS_ASSERTION(commonClipCount >= 0, "Inconsistent clip count.");
    SetupMaskLayer(layer, data->mItemClip, commonClipCount);
    // copy commonClipCount to the entry
    FrameLayerBuilder::ThebesLayerItemsEntry* entry = mLayerBuilder->
      GetThebesLayerItemsEntry(static_cast<ThebesLayer*>(layer.get()));
    entry->mCommonClipCount = commonClipCount;
  } else {
    // mask layer for image and color layers
    SetupMaskLayer(layer, data->mItemClip);
  }
  PRUint32 flags;
  if (isOpaque && !data->mForceTransparentSurface) {
    flags = Layer::CONTENT_OPAQUE;
  } else if (data->mNeedComponentAlpha) {
    flags = Layer::CONTENT_COMPONENT_ALPHA;
  } else {
    flags = 0;
  }
  layer->SetContentFlags(flags);

  if (lastIndex > 0) {
    // Since we're going to pop off the last ThebesLayerData, the
    // mVisibleAboveRegion of the second-to-last item will need to include
    // the regions of the last item.
    ThebesLayerData* nextData = mThebesLayerDataStack[lastIndex - 1];
    nextData->mVisibleAboveRegion.Or(nextData->mVisibleAboveRegion,
                                     data->mVisibleAboveRegion);
    nextData->mVisibleAboveRegion.Or(nextData->mVisibleAboveRegion,
                                     data->mVisibleRegion);
    nextData->mVisibleAboveRegion.SimplifyOutward(4);
    nextData->mDrawAboveRegion.Or(nextData->mDrawAboveRegion,
                                     data->mDrawAboveRegion);
    nextData->mDrawAboveRegion.Or(nextData->mDrawAboveRegion,
                                     data->mDrawRegion);
    nextData->mDrawAboveRegion.SimplifyOutward(4);
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
  nsIFrame* f = aItem->GetUnderlyingFrame();
  nsIFrame* ref = aBuilder->ReferenceFrame();
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
ContainerState::ThebesLayerData::Accumulate(ContainerState* aState,
                                            nsDisplayItem* aItem,
                                            const nsIntRect& aVisibleRect,
                                            const nsIntRect& aDrawRect,
                                            const FrameLayerBuilder::Clip& aClip)
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
  if (mVisibleRegion.IsEmpty() && aItem->GetType() == nsDisplayItem::TYPE_IMAGE) {
    mImage = static_cast<nsDisplayImage*>(aItem);
  } else {
    mImage = nsnull;
  }
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
    if (isUniform && aClip.mRoundedClipRects.IsEmpty()) {
      if (mVisibleRegion.IsEmpty()) {
        // This color is all we have
        mSolidColor = uniformColor;
        mIsSolidColorInVisibleRegion = true;
      } else if (mIsSolidColorInVisibleRegion &&
                 mVisibleRegion.IsEqual(nsIntRegion(aVisibleRect))) {
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
      opaqueClipped.Or(opaqueClipped, aClip.ApproximateIntersect(*r));
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
            aItem->GetUnderlyingFrame()->PresContext()->IsChrome())) {
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

ContainerState::ThebesLayerData*
ContainerState::FindThebesLayerFor(nsDisplayItem* aItem,
                                   const nsIntRect& aVisibleRect,
                                   const nsIntRect& aDrawRect,
                                   const FrameLayerBuilder::Clip& aClip,
                                   nsIFrame* aActiveScrolledRoot)
{
  PRInt32 i;
  PRInt32 lowestUsableLayerWithScrolledRoot = -1;
  PRInt32 topmostLayerWithScrolledRoot = -1;
  for (i = mThebesLayerDataStack.Length() - 1; i >= 0; --i) {
    ThebesLayerData* data = mThebesLayerDataStack[i];
    if (data->mDrawAboveRegion.Intersects(aVisibleRect)) {
      ++i;
      break;
    }
    if (data->mActiveScrolledRoot == aActiveScrolledRoot) {
      lowestUsableLayerWithScrolledRoot = i;
      if (topmostLayerWithScrolledRoot < 0) {
        topmostLayerWithScrolledRoot = i;
      }
    }
    if (data->mDrawRegion.Intersects(aVisibleRect))
      break;
  }
  if (topmostLayerWithScrolledRoot < 0) {
    --i;
    for (; i >= 0; --i) {
      ThebesLayerData* data = mThebesLayerDataStack[i];
      if (data->mActiveScrolledRoot == aActiveScrolledRoot) {
        topmostLayerWithScrolledRoot = i;
        break;
      }
    }
  }

  if (topmostLayerWithScrolledRoot >= 0) {
    while (PRUint32(topmostLayerWithScrolledRoot + 1) < mThebesLayerDataStack.Length()) {
      PopThebesLayerData();
    }
  }

  nsRefPtr<ThebesLayer> layer;
  ThebesLayerData* thebesLayerData = nsnull;
  if (lowestUsableLayerWithScrolledRoot < 0) {
    layer = CreateOrRecycleThebesLayer(aActiveScrolledRoot);

    NS_ASSERTION(!mNewChildLayers.Contains(layer), "Layer already in list???");
    mNewChildLayers.AppendElement(layer);

    thebesLayerData = new ThebesLayerData();
    mThebesLayerDataStack.AppendElement(thebesLayerData);
    thebesLayerData->mLayer = layer;
    thebesLayerData->mActiveScrolledRoot = aActiveScrolledRoot;
  } else {
    thebesLayerData = mThebesLayerDataStack[lowestUsableLayerWithScrolledRoot];
    layer = thebesLayerData->mLayer;
  }

  thebesLayerData->Accumulate(this, aItem, aVisibleRect, aDrawRect, aClip);

  return thebesLayerData;
}

#ifdef MOZ_DUMP_PAINTING
static void
DumpPaintedImage(nsDisplayItem* aItem, gfxASurface* aSurf)
{
  nsCString string(aItem->Name());
  string.Append("-");
  string.AppendInt((PRUint64)aItem);
  fprintf(gfxUtils::sDumpPaintFile, "array[\"%s\"]=\"", string.BeginReading());
  aSurf->DumpAsDataURL(gfxUtils::sDumpPaintFile);
  fprintf(gfxUtils::sDumpPaintFile, "\";");
}
#endif

static void
PaintInactiveLayer(nsDisplayListBuilder* aBuilder,
                   nsDisplayItem* aItem,
                   gfxContext* aContext,
                   nsRenderingContext* aCtx,
                   FrameLayerBuilder *aLayerBuilder)
{
  // This item has an inactive layer. Render it to a ThebesLayer
  // using a temporary BasicLayerManager.
  PRInt32 appUnitsPerDevPixel = AppUnitsPerDevPixel(aItem);
  nsIntRect itemVisibleRect =
    aItem->GetVisibleRect().ToOutsidePixels(appUnitsPerDevPixel);

  nsRefPtr<gfxContext> context = aContext;
#ifdef MOZ_DUMP_PAINTING
  nsRefPtr<gfxASurface> surf;
  if (gfxUtils::sDumpPainting) {
    surf = gfxPlatform::GetPlatform()->CreateOffscreenSurface(itemVisibleRect.Size(),
                                                              gfxASurface::CONTENT_COLOR_ALPHA);
    surf->SetDeviceOffset(-itemVisibleRect.TopLeft());
    context = new gfxContext(surf);
  }
#endif

  nsRefPtr<BasicLayerManager> tempManager = new BasicLayerManager();
  tempManager->SetUserData(&gLayerManagerLayerBuilder, new LayerManagerLayerBuilder(aLayerBuilder, false));
  tempManager->BeginTransactionWithTarget(context);
  nsRefPtr<Layer> layer =
    aItem->BuildLayer(aBuilder, tempManager, FrameLayerBuilder::ContainerParameters());
  if (!layer) {
    tempManager->EndTransaction(nsnull, nsnull);
    return;
  }
  RestrictVisibleRegionForLayer(layer, itemVisibleRect);
  
  tempManager->SetRoot(layer);
  aLayerBuilder->WillEndTransaction(tempManager);
  if (aItem->GetType() == nsDisplayItem::TYPE_SVG_EFFECTS) {
    static_cast<nsDisplaySVGEffects*>(aItem)->PaintAsLayer(aBuilder, aCtx, tempManager);
    if (tempManager->InTransaction()) {
      tempManager->AbortTransaction();
    }
  } else {
    tempManager->EndTransaction(FrameLayerBuilder::DrawThebesLayer, aBuilder);
  }
  aLayerBuilder->DidEndTransaction(tempManager);
 
#ifdef MOZ_DUMP_PAINTING
  if (gfxUtils::sDumpPainting) {
    DumpPaintedImage(aItem, surf);

    surf->SetDeviceOffset(gfxPoint(0, 0));
    aContext->SetSource(surf, itemVisibleRect.TopLeft());
    aContext->Rectangle(itemVisibleRect);
    aContext->Fill();
    aItem->SetPainted();
  }
#endif
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
ContainerState::ProcessDisplayItems(const nsDisplayList& aList,
                                    FrameLayerBuilder::Clip& aClip,
                                    PRUint32 aFlags)
{
  SAMPLE_LABEL("ContainerState", "ProcessDisplayItems");
  for (nsDisplayItem* item = aList.GetBottom(); item; item = item->GetAbove()) {
    nsDisplayItem::Type type = item->GetType();
    if (type == nsDisplayItem::TYPE_CLIP ||
        type == nsDisplayItem::TYPE_CLIP_ROUNDED_RECT) {
      FrameLayerBuilder::Clip childClip(aClip, item);
      ProcessDisplayItems(*item->GetList(), childClip, aFlags);
      continue;
    }

    NS_ASSERTION(mAppUnitsPerDevPixel == AppUnitsPerDevPixel(item),
      "items in a container layer should all have the same app units per dev pixel");

    nsIntRect itemVisibleRect =
      ScaleToOutsidePixels(item->GetVisibleRect(), false);
    bool snap;
    nsRect itemContent = item->GetBounds(mBuilder, &snap);
    nsIntRect itemDrawRect = ScaleToOutsidePixels(itemContent, snap);
    if (aClip.mHaveClipRect) {
      itemContent.IntersectRect(itemContent, aClip.mClipRect);
      nsIntRect clipRect = ScaleToNearestPixels(aClip.mClipRect);
      itemDrawRect.IntersectRect(itemDrawRect, clipRect);
    }
    mBounds.UnionRect(mBounds, itemContent);
    itemVisibleRect.IntersectRect(itemVisibleRect, itemDrawRect);

    LayerState layerState = item->GetLayerState(mBuilder, mManager, mParameters);

    nsIFrame* activeScrolledRoot;
    bool forceInactive = false;
    if (aFlags & NO_COMPONENT_ALPHA) {
      activeScrolledRoot =
        nsLayoutUtils::GetActiveScrolledRootFor(mContainerFrame,
                                                mBuilder->ReferenceFrame());
      forceInactive = true;
    } else {
      activeScrolledRoot = nsLayoutUtils::GetActiveScrolledRootFor(item, mBuilder);
    }

    // Assign the item to a layer
    if (layerState == LAYER_ACTIVE_FORCE ||
        (!forceInactive &&
         (layerState == LAYER_ACTIVE_EMPTY ||
          layerState == LAYER_ACTIVE))) {

      // LAYER_ACTIVE_EMPTY means the layer is created just for its metadata.
      // We should never see an empty layer with any visible content!
      NS_ASSERTION(layerState != LAYER_ACTIVE_EMPTY ||
                   itemVisibleRect.IsEmpty(),
                   "State is LAYER_ACTIVE_EMPTY but visible rect is not.");

      // If the item would have its own layer but is invisible, just hide it.
      // Note that items without their own layers can't be skipped this
      // way, since their ThebesLayer may decide it wants to draw them
      // into its buffer even if they're currently covered.
      if (itemVisibleRect.IsEmpty() && layerState != LAYER_ACTIVE_EMPTY) {
        InvalidateForLayerChange(item, nsnull);
        continue;
      }

      // Just use its layer.
      nsRefPtr<Layer> ownLayer = item->BuildLayer(mBuilder, mManager, mParameters);
      if (!ownLayer) {
        InvalidateForLayerChange(item, ownLayer);
        continue;
      }

      // If it's not a ContainerLayer, we need to apply the scale transform
      // ourselves.
      if (!ownLayer->AsContainerLayer()) {
        // The layer's current transform is applied first, then the result is scaled.
        gfx3DMatrix transform = ownLayer->GetTransform()*
            gfx3DMatrix::ScalingMatrix(mParameters.mXScale, mParameters.mYScale, 1.0f);
        ownLayer->SetBaseTransform(transform);
      }

      ownLayer->SetIsFixedPosition(
        !nsLayoutUtils::IsScrolledByRootContentDocumentDisplayportScrolling(
                                      activeScrolledRoot, mBuilder));

      // Update that layer's clip and visible rects.
      NS_ASSERTION(ownLayer->Manager() == mManager, "Wrong manager");
      NS_ASSERTION(!ownLayer->HasUserData(&gLayerManagerUserData),
                   "We shouldn't have a FrameLayerBuilder-managed layer here!");
      NS_ASSERTION(aClip.mHaveClipRect ||
                     aClip.mRoundedClipRects.IsEmpty(),
                   "If we have rounded rects, we must have a clip rect");
      // It has its own layer. Update that layer's clip and visible rects.
      if (aClip.mHaveClipRect) {
        ownLayer->IntersectClipRect(
          ScaleToNearestPixels(aClip.NonRoundedIntersection()));
      }
      ThebesLayerData* data = GetTopThebesLayerData();
      if (data) {
        data->mVisibleAboveRegion.Or(data->mVisibleAboveRegion, itemVisibleRect);
        data->mVisibleAboveRegion.SimplifyOutward(4);
        // Add the entire bounds rect to the mDrawAboveRegion.
        // The visible region may be excluding opaque content above the
        // item, and we need to ensure that that content is not placed
        // in a ThebesLayer below the item!
        data->mDrawAboveRegion.Or(data->mDrawAboveRegion, itemDrawRect);
        data->mDrawAboveRegion.SimplifyOutward(4);
      }
      RestrictVisibleRegionForLayer(ownLayer, itemVisibleRect);

      // rounded rectangle clipping using mask layers
      // (must be done after visible rect is set on layer)
      if (aClip.IsRectClippedByRoundedCorner(itemContent)) {
          SetupMaskLayer(ownLayer, aClip);
      }

      ContainerLayer* oldContainer = ownLayer->GetParent();
      if (oldContainer && oldContainer != mContainerLayer) {
        oldContainer->RemoveChild(ownLayer);
      }
      NS_ASSERTION(!mNewChildLayers.Contains(ownLayer),
                   "Layer already in list???");

      InvalidateForLayerChange(item, ownLayer);

      mNewChildLayers.AppendElement(ownLayer);
      mLayerBuilder->AddLayerDisplayItem(ownLayer, item, layerState);
    } else {
      ThebesLayerData* data =
        FindThebesLayerFor(item, itemVisibleRect, itemDrawRect, aClip,
                           activeScrolledRoot);

      data->mLayer->SetIsFixedPosition(
        !nsLayoutUtils::IsScrolledByRootContentDocumentDisplayportScrolling(
                                       activeScrolledRoot, mBuilder));

      InvalidateForLayerChange(item, data->mLayer);

      mLayerBuilder->AddThebesDisplayItem(data->mLayer, item, aClip,
                                          mContainerFrame,
                                          layerState);

      // check to see if the new item has rounded rect clips in common with
      // other items in the layer
      data->UpdateCommonClipCount(aClip);
    }
  }
}

void
ContainerState::InvalidateForLayerChange(nsDisplayItem* aItem, Layer* aNewLayer)
{
  nsIFrame* f = aItem->GetUnderlyingFrame();
  NS_ASSERTION(f, "Display items that render using Thebes must have a frame");
  PRUint32 key = aItem->GetPerFrameKey();
  NS_ASSERTION(key, "Display items that render using Thebes must have a key");
  Layer* oldLayer = mLayerBuilder->GetOldLayerFor(f, key);
  if (!oldLayer) {
    // Nothing to do here, this item didn't have a layer before
    return;
  }
  if (aNewLayer != oldLayer) {
    // The item has changed layers.
    // Invalidate the bounds in the old layer and new layer.
    // The bounds might have changed, but we assume that any difference
    // in the bounds will have been invalidated for all Thebes layers
    // in the container via regular frame invalidation.
    bool snap;
    nsRect bounds = aItem->GetBounds(mBuilder, &snap);

    ThebesLayer* t = oldLayer->AsThebesLayer();
    if (t) {
      ThebesDisplayItemLayerUserData* data =
          static_cast<ThebesDisplayItemLayerUserData*>(t->GetUserData(&gThebesDisplayItemLayerUserData));
      // Note that whenever the layer's scale changes, we invalidate the whole thing,
      // so it doesn't matter whether we are using the old scale at last paint
      // or a new scale here
      InvalidatePostTransformRegion(t,
          bounds.ScaleToOutsidePixels(data->mXScale, data->mYScale, mAppUnitsPerDevPixel),
          mLayerBuilder->GetLastPaintOffset(t));
    }
    if (aNewLayer) {
      ThebesLayer* newLayer = aNewLayer->AsThebesLayer();
      if (newLayer) {
        ThebesDisplayItemLayerUserData* data =
            static_cast<ThebesDisplayItemLayerUserData*>(newLayer->GetUserData(&gThebesDisplayItemLayerUserData));
        InvalidatePostTransformRegion(newLayer,
            bounds.ScaleToOutsidePixels(data->mXScale, data->mYScale, mAppUnitsPerDevPixel),
            GetTranslationForThebesLayer(newLayer));
      }
    }

    mContainerFrame->InvalidateWithFlags(
        bounds - mBuilder->ToReferenceFrame(mContainerFrame),
        nsIFrame::INVALIDATE_NO_THEBES_LAYERS |
        nsIFrame::INVALIDATE_EXCLUDE_CURRENT_PAINT);
  }
}

bool
FrameLayerBuilder::NeedToInvalidateFixedDisplayItem(nsDisplayListBuilder* aBuilder,
                                                    nsDisplayItem* aItem)
{
  return !aItem->ShouldFixToViewport(aBuilder) ||
      !HasRetainedLayerFor(aItem->GetUnderlyingFrame(), aItem->GetPerFrameKey());
}

void
FrameLayerBuilder::AddThebesDisplayItem(ThebesLayer* aLayer,
                                        nsDisplayItem* aItem,
                                        const Clip& aClip,
                                        nsIFrame* aContainerLayerFrame,
                                        LayerState aLayerState)
{
  AddLayerDisplayItem(aLayer, aItem, aLayerState);

  ThebesLayerItemsEntry* entry = mThebesLayerItems.PutEntry(aLayer);
  if (entry) {
    entry->mContainerLayerFrame = aContainerLayerFrame;
    entry->mContainerLayerGeneration = mContainerLayerGeneration;
    NS_ASSERTION(aItem->GetUnderlyingFrame(), "Must have frame");
    ClippedDisplayItem* cdi =
      entry->mItems.AppendElement(ClippedDisplayItem(aItem, aClip,
                                                     mContainerLayerGeneration));
    cdi->mInactiveLayer = aLayerState != LAYER_NONE;
  }
}

void
FrameLayerBuilder::AddLayerDisplayItem(Layer* aLayer,
                                       nsDisplayItem* aItem,
                                       LayerState aLayerState)
{
  if (aLayer->Manager() != mRetainingManager)
    return;

  nsIFrame* f = aItem->GetUnderlyingFrame();
  DisplayItemDataEntry* entry = mNewDisplayItemData.PutEntry(f);
  entry->mContainerLayerGeneration = mContainerLayerGeneration;
  if (entry) {
    entry->mData.AppendElement(DisplayItemData(aLayer, aItem->GetPerFrameKey(), aLayerState, mContainerLayerGeneration));
  }
}

nsIntPoint
FrameLayerBuilder::GetLastPaintOffset(ThebesLayer* aLayer)
{
  ThebesLayerItemsEntry* entry = mThebesLayerItems.PutEntry(aLayer);
  if (entry) {
    entry->mContainerLayerGeneration = mContainerLayerGeneration;
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
    entry->mContainerLayerGeneration = mContainerLayerGeneration;
    entry->mLastPaintOffset = GetTranslationForThebesLayer(aLayer);
    entry->mHasExplicitLastPaintOffset = true;
  }
}

nscolor
FrameLayerBuilder::FindOpaqueColorCovering(nsDisplayListBuilder* aBuilder,
                                           ThebesLayer* aLayer,
                                           const nsRect& aRect)
{
  ThebesLayerItemsEntry* entry = mThebesLayerItems.GetEntry(aLayer);
  NS_ASSERTION(entry, "Must know about this layer!");
  for (PRInt32 i = entry->mItems.Length() - 1; i >= 0; --i) {
    nsDisplayItem* item = entry->mItems[i].mItem;
    const nsRect& visible = item->GetVisibleRect();
    if (!visible.Intersects(aRect))
      continue;

    nscolor color;
    if (visible.Contains(aRect) && item->IsUniform(aBuilder, &color) &&
        NS_GET_A(color) == 255)
      return color;
    break;
  }
  return NS_RGBA(0,0,0,0);
}

void
ContainerState::CollectOldLayers()
{
  for (Layer* layer = mContainerLayer->GetFirstChild(); layer;
       layer = layer->GetNextSibling()) {
    NS_ASSERTION(!layer->HasUserData(&gMaskLayerUserData),
                 "Mask layer in layer tree; could not be recycled.");
    if (layer->HasUserData(&gColorLayerUserData)) {
      mRecycledColorLayers.AppendElement(static_cast<ColorLayer*>(layer));
    } else if (layer->HasUserData(&gImageLayerUserData)) {
      mRecycledImageLayers.AppendElement(static_cast<ImageLayer*>(layer));
    } else if (layer->HasUserData(&gThebesDisplayItemLayerUserData)) {
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
ContainerState::Finish(PRUint32* aTextContentFlags)
{
  while (!mThebesLayerDataStack.IsEmpty()) {
    PopThebesLayerData();
  }

  PRUint32 textContentFlags = 0;

  for (PRUint32 i = 0; i <= mNewChildLayers.Length(); ++i) {
    // An invariant of this loop is that the layers in mNewChildLayers
    // with index < i are the first i child layers of mContainerLayer.
    Layer* layer;
    if (i < mNewChildLayers.Length()) {
      layer = mNewChildLayers[i];
      if (!layer->GetVisibleRegion().IsEmpty()) {
        textContentFlags |= layer->GetContentFlags() & Layer::CONTENT_COMPONENT_ALPHA;
      }
      if (!layer->GetParent()) {
        // This is not currently a child of the container, so just add it
        // now.
        Layer* prevChild = i == 0 ? nsnull : mNewChildLayers[i - 1].get();
        mContainerLayer->InsertAfter(layer, prevChild);
        continue;
      }
      NS_ASSERTION(layer->GetParent() == mContainerLayer,
                   "Layer shouldn't be the child of some other container");
    } else {
      layer = nsnull;
    }

    // If layer is non-null, then it's already a child of the container,
    // so scan forward until we find it, removing the other layers we
    // don't want here.
    // If it's null, scan forward until we've removed all the leftover
    // children.
    Layer* nextOldChild = i == 0 ? mContainerLayer->GetFirstChild() :
      mNewChildLayers[i - 1]->GetNextSibling();
    while (nextOldChild != layer) {
      Layer* tmp = nextOldChild;
      nextOldChild = nextOldChild->GetNextSibling();
      mContainerLayer->RemoveChild(tmp);
    }
    // If non-null, 'layer' is now in the right place in the list, so we
    // can just move on to the next one.
  }

  *aTextContentFlags = textContentFlags;
}

static FrameLayerBuilder::ContainerParameters
ChooseScaleAndSetTransform(FrameLayerBuilder* aLayerBuilder,
                           nsIFrame* aContainerFrame,
                           const gfx3DMatrix* aTransform,
                           const FrameLayerBuilder::ContainerParameters& aIncomingScale,
                           ContainerLayer* aLayer)
{
  gfx3DMatrix transform =
    gfx3DMatrix::ScalingMatrix(aIncomingScale.mXScale, aIncomingScale.mYScale, 1.0);
  if (aTransform) {
    // aTransform is applied first, then the scale is applied to the result
    transform = (*aTransform)*transform;
  }

  gfxMatrix transform2d;
  bool canDraw2D = transform.CanDraw2D(&transform2d);
  gfxSize scale;
  bool isRetained = aLayerBuilder->GetRetainingLayerManager() == aLayer->Manager();
  // Only fiddle with scale factors for the retaining layer manager, since
  // it only matters for retained layers
  // XXX Should we do something for 3D transforms?
  if (canDraw2D && isRetained) {
    //Scale factors are normalized to a power of 2 to reduce the number of resolution changes
    scale = transform2d.ScaleFactors(true);
    // For frames with a changing transform that's not just a translation,
    // round scale factors up to nearest power-of-2 boundary so that we don't
    // keep having to redraw the content as it scales up and down. Rounding up to nearest
    // power-of-2 boundary ensures we never scale up, only down --- avoiding
    // jaggies. It also ensures we never scale down by more than a factor of 2,
    // avoiding bad downscaling quality.
    gfxMatrix frameTransform;
    if (aContainerFrame->AreLayersMarkedActive(nsChangeHint_UpdateTransformLayer) &&
        aTransform &&
        (!aTransform->Is2D(&frameTransform) || frameTransform.HasNonTranslationOrFlip())) {
      // Don't clamp the scale factor when the new desired scale factor matches the old one
      // or it was previously unscaled.
      bool clamp = true;
      gfxMatrix oldFrameTransform2d;
      if (aLayer->GetTransform().Is2D(&oldFrameTransform2d)) {
        gfxSize oldScale = oldFrameTransform2d.ScaleFactors(true);
        if (oldScale == scale || oldScale == gfxSize(1.0, 1.0))
          clamp = false;
      }
      if (clamp) {
        scale.width = gfxUtils::ClampToScaleFactor(scale.width);
        scale.height = gfxUtils::ClampToScaleFactor(scale.height);
      }
    } else {
      // XXX Do we need to move nearly-integer values to integers here?
    }
    // If the scale factors are too small, just use 1.0. The content is being
    // scaled out of sight anyway.
    if (fabs(scale.width) < 1e-8 || fabs(scale.height) < 1e-8) {
      scale = gfxSize(1.0, 1.0);
    }
  } else {
    scale = gfxSize(1.0, 1.0);
  }

  aLayer->SetBaseTransform(transform);
  // Store the inverse of our resolution-scale on the layer
  aLayer->SetScale(1.0f/float(scale.width), 1.0f/float(scale.height));

  FrameLayerBuilder::ContainerParameters
    result(scale.width, scale.height, aIncomingScale);
  if (aTransform) {
    result.mInTransformedSubtree = true;
    if (aContainerFrame->AreLayersMarkedActive(nsChangeHint_UpdateTransformLayer)) {
      result.mInActiveTransformedSubtree = true;
    }
  }
  if (isRetained && (!canDraw2D || transform2d.HasNonIntegerTranslation())) {
    result.mDisableSubpixelAntialiasingInDescendants = true;
  }
  return result;
}

static void
ApplyThebesLayerInvalidation(nsDisplayListBuilder* aBuilder,
                             nsIFrame* aContainerFrame,
                             nsDisplayItem* aContainerItem,
                             ContainerState& aState,
                             nsPoint* aCurrentOffset)
{
  *aCurrentOffset = aContainerItem ? aContainerItem->ToReferenceFrame()
    : aBuilder->ToReferenceFrame(aContainerFrame);

  FrameProperties props = aContainerFrame->Properties();
  RefCountedRegion* invalidThebesContent = static_cast<RefCountedRegion*>
    (props.Get(ThebesLayerInvalidRegionProperty()));
  if (invalidThebesContent) {
    const FrameLayerBuilder::ContainerParameters& scaleParameters = aState.ScaleParameters();
    aState.AddInvalidThebesContent(invalidThebesContent->mRegion.
      ScaleToOutsidePixels(scaleParameters.mXScale, scaleParameters.mYScale,
                           aState.GetAppUnitsPerDevPixel()));
    // We have to preserve the current contents of invalidThebesContent
    // because there might be multiple container layers for the same
    // frame and we need to invalidate the ThebesLayer children of all
    // of them. Also, multiple calls to ApplyThebesLayerInvalidation for the
    // same layer can share the same region.
  } else {
    // The region was deleted to indicate that everything should be
    // invalidated.
    aState.SetInvalidateAllThebesContent();
  }
}

/* static */ PLDHashOperator
FrameLayerBuilder::RestoreDisplayItemData(DisplayItemDataEntry* aEntry, void* aUserArg)
{
  PRUint32 *generation = static_cast<PRUint32*>(aUserArg);

  if (aEntry->mContainerLayerGeneration >= *generation) {
    return PL_DHASH_REMOVE;
  }

  for (PRUint32 i = 0; i < aEntry->mData.Length(); i++) {
    if (aEntry->mData[i].mContainerLayerGeneration >= *generation) {
      aEntry->mData.TruncateLength(i);
      return PL_DHASH_NEXT;
    }
  }

  return PL_DHASH_NEXT;
}

/* static */ PLDHashOperator
FrameLayerBuilder::RestoreThebesLayerItemEntries(ThebesLayerItemsEntry* aEntry, void* aUserArg)
{
  PRUint32 *generation = static_cast<PRUint32*>(aUserArg);

  if (aEntry->mContainerLayerGeneration >= *generation) {
    return PL_DHASH_REMOVE;
  }

  for (PRUint32 i = 0; i < aEntry->mItems.Length(); i++) {
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
                                          const nsDisplayList& aChildren,
                                          const ContainerParameters& aParameters,
                                          const gfx3DMatrix* aTransform)
{
  PRUint32 containerDisplayItemKey =
    aContainerItem ? aContainerItem->GetPerFrameKey() : 0;
  NS_ASSERTION(aContainerFrame, "Container display items here should have a frame");
  NS_ASSERTION(!aContainerItem ||
               aContainerItem->GetUnderlyingFrame() == aContainerFrame,
               "Container display item must match given frame");

  nsRefPtr<ContainerLayer> containerLayer;
  if (aManager == mRetainingManager) {
    Layer* oldLayer = GetOldLayerFor(aContainerFrame, containerDisplayItemKey);
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
        // Clear clip rect; the caller will set it if necessary.
        containerLayer->SetClipRect(nsnull);
        containerLayer->SetMaskLayer(nsnull);
      }
    }
  }
  if (!containerLayer) {
    // No suitable existing layer was found.
    containerLayer = aManager->CreateContainerLayer();
    if (!containerLayer)
      return nsnull;
  }

  if (aContainerItem &&
      aContainerItem->GetLayerState(aBuilder, aManager, aParameters) == LAYER_ACTIVE_EMPTY) {
    // Empty layers only have metadata and should never have display items. We
    // early exit because later, invalidation will walk up the frame tree to
    // determine which thebes layer gets invalidated. Since an empty layer
    // should never have anything to paint, it should never be invalidated.
    NS_ASSERTION(aChildren.IsEmpty(), "Should have no children");
    return containerLayer.forget();
  }

  ContainerParameters scaleParameters =
    ChooseScaleAndSetTransform(this, aContainerFrame, aTransform, aParameters,
                               containerLayer);

  PRUint32 oldGeneration = mContainerLayerGeneration;
  mContainerLayerGeneration = ++mMaxContainerLayerGeneration;

  nsRefPtr<RefCountedRegion> thebesLayerInvalidRegion = nsnull;
  if (aManager == mRetainingManager) {
    FrameProperties props = aContainerFrame->Properties();
    DisplayItemDataEntry* entry = mNewDisplayItemData.PutEntry(aContainerFrame);
    if (entry) {
      entry->mData.AppendElement(
          DisplayItemData(containerLayer, containerDisplayItemKey,
                          LAYER_ACTIVE, mContainerLayerGeneration));
      thebesLayerInvalidRegion = static_cast<RefCountedRegion*>
        (props.Get(ThebesLayerInvalidRegionProperty()));
      if (!thebesLayerInvalidRegion) {
        thebesLayerInvalidRegion = new RefCountedRegion();
      }
      entry->mInvalidRegion = thebesLayerInvalidRegion;
      entry->mContainerLayerGeneration = mContainerLayerGeneration;
    }
  }

  nsRect bounds;
  nsIntRect pixBounds;
  PRInt32 appUnitsPerDevPixel;
  PRUint32 stateFlags =
    (aContainerFrame->GetStateBits() & NS_FRAME_NO_COMPONENT_ALPHA) ?
      ContainerState::NO_COMPONENT_ALPHA : 0;
  PRUint32 flags;
  bool flattenedLayers = false;
  while (true) {
    ContainerState state(aBuilder, aManager, GetLayerBuilderForManager(aManager),
                         aContainerFrame, containerLayer, scaleParameters);
    if (flattenedLayers) {
      state.SetInvalidateAllThebesContent();
    }

    if (aManager == mRetainingManager) {
      nsPoint currentOffset;
      ApplyThebesLayerInvalidation(aBuilder, aContainerFrame, aContainerItem, state,
                                   &currentOffset);
      SetHasContainerLayer(aContainerFrame, currentOffset);

      nsAutoTArray<nsIFrame*,4> mergedFrames;
      if (aContainerItem) {
        aContainerItem->GetMergedFrames(&mergedFrames);
      }
      for (PRUint32 i = 0; i < mergedFrames.Length(); ++i) {
        nsIFrame* mergedFrame = mergedFrames[i];
        DisplayItemDataEntry* entry = mNewDisplayItemData.PutEntry(mergedFrame);
        if (entry) {
          // Ensure that UpdateDisplayItemDataForFrame recognizes that we
          // still have a container layer associated with this frame.
          entry->mIsSharingContainerLayer = true;

          // Store the invalid region property in case this frame is represented
          // by multiple container layers. This is cleared and set when iterating
          // over the DisplayItemDataEntry's in WillEndTransaction.
          entry->mInvalidRegion = thebesLayerInvalidRegion;
        }
        ApplyThebesLayerInvalidation(aBuilder, mergedFrame, nsnull, state,
                                     &currentOffset);
        SetHasContainerLayer(mergedFrame, currentOffset);
      }
    }

    Clip clip;
    state.ProcessDisplayItems(aChildren, clip, stateFlags);
 
    // Set CONTENT_COMPONENT_ALPHA if any of our children have it.
    // This is suboptimal ... a child could have text that's over transparent
    // pixels in its own layer, but over opaque parts of previous siblings.
    state.Finish(&flags);
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
      mNewDisplayItemData.EnumerateEntries(RestoreDisplayItemData,
                                           &mContainerLayerGeneration);
      mThebesLayerItems.EnumerateEntries(RestoreThebesLayerItemEntries,
                                         &mContainerLayerGeneration);
      aContainerFrame->AddStateBits(NS_FRAME_NO_COMPONENT_ALPHA);
      flattenedLayers = true;
      continue;
    }
    break;
  }

  NS_ASSERTION(bounds.IsEqualInterior(aChildren.GetBounds(aBuilder)), "Wrong bounds");
  containerLayer->SetVisibleRegion(pixBounds);
  // Make sure that rounding the visible region out didn't add any area
  // we won't paint
  if (aChildren.IsOpaque() && !aChildren.NeedsTransparentSurface()) {
    bounds.ScaleRoundIn(scaleParameters.mXScale, scaleParameters.mYScale);
    if (bounds.Contains(pixBounds.ToAppUnits(appUnitsPerDevPixel))) {
      // Clear CONTENT_COMPONENT_ALPHA
      flags = Layer::CONTENT_OPAQUE;
    }
  }
  containerLayer->SetContentFlags(flags);

  mContainerLayerGeneration = oldGeneration;
  return containerLayer.forget();
}

Layer*
FrameLayerBuilder::GetLeafLayerFor(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   nsDisplayItem* aItem)
{
  if (aManager != mRetainingManager)
    return nsnull;

  nsIFrame* f = aItem->GetUnderlyingFrame();
  NS_ASSERTION(f, "Can only call GetLeafLayerFor on items that have a frame");
  Layer* layer = GetOldLayerFor(f, aItem->GetPerFrameKey());
  if (!layer)
    return nsnull;
  if (layer->HasUserData(&gThebesDisplayItemLayerUserData)) {
    // This layer was created to render Thebes-rendered content for this
    // display item. The display item should not use it for its own
    // layer rendering.
    return nsnull;
  }
  // Clear clip rect; the caller is responsible for setting it.
  layer->SetClipRect(nsnull);
  layer->SetMaskLayer(nsnull);
  return layer;
}

/* static */ void
FrameLayerBuilder::InvalidateThebesLayerContents(nsIFrame* aFrame,
                                                 const nsRect& aRect)
{
  FrameProperties props = aFrame->Properties();
  RefCountedRegion* invalidThebesContent = static_cast<RefCountedRegion*>
    (props.Get(ThebesLayerInvalidRegionProperty()));
  if (!invalidThebesContent)
    return;

  nsPoint* offsetAtLastPaint = static_cast<nsPoint*>
    (props.Get(ThebesLayerLastPaintOffsetProperty()));
  NS_ASSERTION(offsetAtLastPaint,
               "This must have been set up along with ThebesLayerInvalidRegionProperty");
  invalidThebesContent->mRegion.Or(invalidThebesContent->mRegion,
          aRect + *offsetAtLastPaint);
  invalidThebesContent->mRegion.SimplifyOutward(20);
}

/**
 * Returns true if we find a descendant with a container layer
 */
static bool
InternalInvalidateThebesLayersInSubtree(nsIFrame* aFrame)
{
  if (!(aFrame->GetStateBits() & NS_FRAME_HAS_CONTAINER_LAYER_DESCENDANT))
    return false;

  bool foundContainerLayer = false;
  if (aFrame->GetStateBits() & NS_FRAME_HAS_CONTAINER_LAYER) {
    // Delete the invalid region to indicate that all Thebes contents
    // need to be invalidated
    FrameLayerBuilder::InvalidateThebesLayerContents(aFrame, aFrame->GetVisualOverflowRectRelativeToSelf());
    foundContainerLayer = true;
  }

  nsAutoTArray<nsIFrame::ChildList,4> childListArray;
  if (!aFrame->GetFirstPrincipalChild()) {
    nsSubDocumentFrame* subdocumentFrame = do_QueryFrame(aFrame);
    if (subdocumentFrame) {
      // Descend into the subdocument
      nsIFrame* root = subdocumentFrame->GetSubdocumentRootFrame();
      if (root) {
        childListArray.AppendElement(nsIFrame::ChildList(
          nsFrameList(root, nsLayoutUtils::GetLastSibling(root)),
          nsIFrame::kPrincipalList));
      }
    }
  }

  aFrame->GetChildLists(&childListArray);
  nsIFrame::ChildListArrayIterator lists(childListArray);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      if (InternalInvalidateThebesLayersInSubtree(childFrames.get())) {
        foundContainerLayer = true;
      }
    }
  }

  if (!foundContainerLayer) {
    aFrame->RemoveStateBits(NS_FRAME_HAS_CONTAINER_LAYER_DESCENDANT);
  }
  return foundContainerLayer;
}

/* static */ void
FrameLayerBuilder::InvalidateThebesLayersInSubtree(nsIFrame* aFrame)
{
  InternalInvalidateThebesLayersInSubtree(aFrame);
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

/* static */
Layer*
FrameLayerBuilder::GetDedicatedLayer(nsIFrame* aFrame, PRUint32 aDisplayItemKey)
{
  nsTArray<DisplayItemData>* array = GetDisplayItemDataArrayForFrame(aFrame);
  if (!array)
    return nsnull;

  for (PRUint32 i = 0; i < array->Length(); ++i) {
    if (array->ElementAt(i).mDisplayItemKey == aDisplayItemKey) {
      Layer* layer = array->ElementAt(i).mLayer;
      if (!layer->HasUserData(&gColorLayerUserData) &&
          !layer->HasUserData(&gImageLayerUserData) &&
          !layer->HasUserData(&gThebesDisplayItemLayerUserData))
        return layer;
    }
  }
  return nsnull;
}

bool
FrameLayerBuilder::GetThebesLayerResolutionForFrame(nsIFrame* aFrame,
                                                    double* aXres, double* aYres,
                                                    gfxPoint* aPoint)
{
  nsTArray<DisplayItemData> *array = GetDisplayItemDataArrayForFrame(aFrame);
  if (array) {
    for (PRUint32 i = 0; i < array->Length(); ++i) {
      Layer* layer = array->ElementAt(i).mLayer;
      if (layer->HasUserData(&gThebesDisplayItemLayerUserData)) {
        ThebesDisplayItemLayerUserData* data =
          static_cast<ThebesDisplayItemLayerUserData*>
            (layer->GetUserData(&gThebesDisplayItemLayerUserData));
        *aXres = data->mXScale;
        *aYres = data->mYScale;
        *aPoint = data->mActiveScrolledRootPosition;
        return true;
      }
    }
  }

  nsIFrame::ChildListIterator lists(aFrame);
  for (; !lists.IsDone(); lists.Next()) {
    if (lists.CurrentID() == nsIFrame::kPopupList ||
        lists.CurrentID() == nsIFrame::kSelectPopupList) {
      continue;
    }

    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      if (GetThebesLayerResolutionForFrame(childFrames.get(),
                                           aXres, aYres, aPoint)) {
        return true;
      }
    }
  }

  return false;
}

#ifdef MOZ_DUMP_PAINTING
static void DebugPaintItem(nsRenderingContext* aDest, nsDisplayItem *aItem, nsDisplayListBuilder* aBuilder)
{
  bool snap;
  nsRect appUnitBounds = aItem->GetBounds(aBuilder, &snap);
  gfxRect bounds(appUnitBounds.x, appUnitBounds.y, appUnitBounds.width, appUnitBounds.height);
  bounds.ScaleInverse(aDest->AppUnitsPerDevPixel());

  nsRefPtr<gfxASurface> surf =
    gfxPlatform::GetPlatform()->CreateOffscreenSurface(gfxIntSize(bounds.width, bounds.height),
                                                       gfxASurface::CONTENT_COLOR_ALPHA);
  surf->SetDeviceOffset(-bounds.TopLeft());
  nsRefPtr<gfxContext> context = new gfxContext(surf);
  nsRefPtr<nsRenderingContext> ctx = new nsRenderingContext();
  ctx->Init(aDest->DeviceContext(), context);

  aItem->Paint(aBuilder, ctx);
  DumpPaintedImage(aItem, surf);
  aItem->SetPainted();

  surf->SetDeviceOffset(gfxPoint(0, 0));
  aDest->ThebesContext()->SetSource(surf, bounds.TopLeft());
  aDest->ThebesContext()->Rectangle(bounds);
  aDest->ThebesContext()->Fill();
}
#endif

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
                                   const nsIntRegion& aRegionToInvalidate,
                                   void* aCallbackData)
{
  SAMPLE_LABEL("gfx", "DrawThebesLayer");

  nsDisplayListBuilder* builder = static_cast<nsDisplayListBuilder*>
    (aCallbackData);

  FrameLayerBuilder *layerBuilder = GetLayerBuilderForManager(aLayer->Manager());

  if (layerBuilder->CheckDOMModified())
    return;

  nsTArray<ClippedDisplayItem> items;
  PRUint32 commonClipCount;
  nsIFrame* containerLayerFrame;
  {
    ThebesLayerItemsEntry* entry = layerBuilder->mThebesLayerItems.GetEntry(aLayer);
    NS_ASSERTION(entry, "We shouldn't be drawing into a layer with no items!");
    items.SwapElements(entry->mItems);
    commonClipCount = entry->mCommonClipCount;
    containerLayerFrame = entry->mContainerLayerFrame;
    // Later after this point, due to calls to DidEndTransaction
    // for temporary layer managers, mThebesLayerItems can change,
    // so 'entry' could become invalid.
  }

  if (!containerLayerFrame) {
    return;
  }

  ThebesDisplayItemLayerUserData* userData =
    static_cast<ThebesDisplayItemLayerUserData*>
      (aLayer->GetUserData(&gThebesDisplayItemLayerUserData));
  NS_ASSERTION(userData, "where did our user data go?");
  if (NS_GET_A(userData->mForcedBackgroundColor) > 0) {
    nsIntRect r = aLayer->GetVisibleRegion().GetBounds();
    aContext->NewPath();
    aContext->Rectangle(gfxRect(r.x, r.y, r.width, r.height));
    aContext->SetColor(gfxRGBA(userData->mForcedBackgroundColor));
    aContext->Fill();
  }

  // make the origin of the context coincide with the origin of the
  // ThebesLayer
  gfxContextMatrixAutoSaveRestore saveMatrix(aContext);
  nsIntPoint offset = GetTranslationForThebesLayer(aLayer);
  // Apply the residual transform if it has been enabled, to ensure that
  // snapping when we draw into aContext exactly matches the ideal transform.
  // See above for why this is OK.
  aContext->Translate(aLayer->GetResidualTranslation() - gfxPoint(offset.x, offset.y));
  aContext->Scale(userData->mXScale, userData->mYScale);

  nsPresContext* presContext = containerLayerFrame->PresContext();
  PRInt32 appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
  if (!aRegionToInvalidate.IsEmpty()) {
    nsRect r = (aRegionToInvalidate.GetBounds() + offset).
      ToAppUnits(appUnitsPerDevPixel);
    r.ScaleInverseRoundOut(userData->mXScale, userData->mYScale);
    containerLayerFrame->InvalidateWithFlags(r,
        nsIFrame::INVALIDATE_NO_THEBES_LAYERS |
        nsIFrame::INVALIDATE_EXCLUDE_CURRENT_PAINT);
  }

  PRUint32 i;
  // Update visible regions. We need perform visibility analysis again
  // because we may be asked to draw into part of a ThebesLayer that
  // isn't actually visible in the window (e.g., because a ThebesLayer
  // expanded its visible region to a rectangle internally), in which
  // case the mVisibleRect stored in the display item may be wrong.
  nsRegion visible = aRegionToDraw.ToAppUnits(appUnitsPerDevPixel);
  visible.MoveBy(NSIntPixelsToAppUnits(offset.x, appUnitsPerDevPixel),
                 NSIntPixelsToAppUnits(offset.y, appUnitsPerDevPixel));
  visible.ScaleInverseRoundOut(userData->mXScale, userData->mYScale);

  for (i = items.Length(); i > 0; --i) {
    ClippedDisplayItem* cdi = &items[i - 1];

    NS_ASSERTION(AppUnitsPerDevPixel(cdi->mItem) == appUnitsPerDevPixel,
                 "a thebes layer should contain items only at the same zoom");

    NS_ABORT_IF_FALSE(cdi->mClip.mHaveClipRect ||
                      cdi->mClip.mRoundedClipRects.IsEmpty(),
                      "If we have rounded rects, we must have a clip rect");

    if (!cdi->mClip.mHaveClipRect ||
        (cdi->mClip.mRoundedClipRects.IsEmpty() &&
         cdi->mClip.mClipRect.Contains(visible.GetBounds()))) {
      cdi->mItem->RecomputeVisibility(builder, &visible);
      continue;
    }

    // Do a little dance to account for the fact that we're clipping
    // to cdi->mClipRect
    nsRegion clipped;
    clipped.And(visible, cdi->mClip.mClipRect);
    nsRegion finalClipped = clipped;
    cdi->mItem->RecomputeVisibility(builder, &finalClipped);
    // If we have rounded clip rects, don't subtract from the visible
    // region since we aren't displaying everything inside the rect.
    if (cdi->mClip.mRoundedClipRects.IsEmpty()) {
      nsRegion removed;
      removed.Sub(clipped, finalClipped);
      nsRegion newVisible;
      newVisible.Sub(visible, removed);
      // Don't let the visible region get too complex.
      if (newVisible.GetNumRects() <= 15) {
        visible = newVisible;
      }
    }
    if (!cdi->mClip.IsRectClippedByRoundedCorner(cdi->mItem->GetVisibleRect())) {
      cdi->mClip.RemoveRoundedCorners();
    }
  }

  nsRefPtr<nsRenderingContext> rc = new nsRenderingContext();
  rc->Init(presContext->DeviceContext(), aContext);

  Clip currentClip;
  bool setClipRect = false;

  for (i = 0; i < items.Length(); ++i) {
    ClippedDisplayItem* cdi = &items[i];

    if (cdi->mItem->GetVisibleRect().IsEmpty())
      continue;

    // If the new desired clip state is different from the current state,
    // update the clip.
    if (setClipRect != cdi->mClip.mHaveClipRect ||
        (cdi->mClip.mHaveClipRect && cdi->mClip != currentClip)) {
      if (setClipRect) {
        aContext->Restore();
      }
      setClipRect = cdi->mClip.mHaveClipRect;
      if (setClipRect) {
        currentClip = cdi->mClip;
        aContext->Save();
        NS_ASSERTION(commonClipCount < 100,
          "Maybe you really do have more than a hundred clipping rounded rects, or maybe something has gone wrong.");
        currentClip.ApplyTo(aContext, presContext, commonClipCount);
      }
    }

    if (cdi->mInactiveLayer) {
      PaintInactiveLayer(builder, cdi->mItem, aContext, rc, layerBuilder);
    } else {
      nsIFrame* frame = cdi->mItem->GetUnderlyingFrame();
      if (frame) {
        frame->AddStateBits(NS_FRAME_PAINTED_THEBES);
      }
#ifdef MOZ_DUMP_PAINTING

      if (gfxUtils::sDumpPainting) {
        DebugPaintItem(rc, cdi->mItem, builder);
      } else {
#else
      {
#endif
        cdi->mItem->Paint(builder, rc);
      }
    }

    if (layerBuilder->CheckDOMModified())
      break;
  }

  {
    ThebesLayerItemsEntry* entry =
      layerBuilder->mThebesLayerItems.GetEntry(aLayer);
    items.SwapElements(entry->mItems);
  }

  if (setClipRect) {
    aContext->Restore();
  }

  FlashPaint(aContext);
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
FrameLayerBuilder::DumpRetainedLayerTree(LayerManager* aManager, FILE* aFile)
{
  aManager->Dump(aFile);
}
#endif

FrameLayerBuilder::Clip::Clip(const Clip& aOther, nsDisplayItem* aClipItem)
  : mRoundedClipRects(aOther.mRoundedClipRects),
    mHaveClipRect(true)
{
  nsDisplayItem::Type type = aClipItem->GetType();
  NS_ABORT_IF_FALSE(type == nsDisplayItem::TYPE_CLIP ||
                    type == nsDisplayItem::TYPE_CLIP_ROUNDED_RECT,
                    "unexpected display item type");
  nsDisplayClip* item = static_cast<nsDisplayClip*>(aClipItem);
  // Always intersect with mClipRect, even if we're going to add a
  // rounded rect.
  if (aOther.mHaveClipRect) {
    mClipRect.IntersectRect(aOther.mClipRect, item->GetClipRect());
  } else {
    mClipRect = item->GetClipRect();
  }

  if (type == nsDisplayItem::TYPE_CLIP_ROUNDED_RECT) {
    RoundedRect *rr = mRoundedClipRects.AppendElement();
    if (rr) {
      rr->mRect = item->GetClipRect();
      static_cast<nsDisplayClipRoundedRect*>(item)->GetRadii(rr->mRadii);
    }
  }

  // FIXME: Optimize away excess rounded rectangles due to the new addition.
}

void
FrameLayerBuilder::Clip::ApplyTo(gfxContext* aContext,
                                 nsPresContext* aPresContext,
                                 PRUint32 aBegin, PRUint32 aEnd)
{
  PRInt32 A2D = aPresContext->AppUnitsPerDevPixel();
  ApplyRectTo(aContext, A2D);
  ApplyRoundedRectsTo(aContext, A2D, aBegin, aEnd);
}

void
FrameLayerBuilder::Clip::ApplyRectTo(gfxContext* aContext, PRInt32 A2D) const
{
  aContext->NewPath();
  gfxRect clip = nsLayoutUtils::RectToGfxRect(mClipRect, A2D);
  aContext->Rectangle(clip, true);
  aContext->Clip();
}

void
FrameLayerBuilder::Clip::ApplyRoundedRectsTo(gfxContext* aContext,
                                             PRInt32 A2D,
                                             PRUint32 aBegin, PRUint32 aEnd) const
{
  aEnd = NS_MIN<PRUint32>(aEnd, mRoundedClipRects.Length());

  for (PRUint32 i = aBegin; i < aEnd; ++i) {
    AddRoundedRectPathTo(aContext, A2D, mRoundedClipRects[i]);
    aContext->Clip();
  }
}

void
FrameLayerBuilder::Clip::DrawRoundedRectsTo(gfxContext* aContext,
                                            PRInt32 A2D,
                                            PRUint32 aBegin, PRUint32 aEnd) const
{
  aEnd = NS_MIN<PRUint32>(aEnd, mRoundedClipRects.Length());

  if (aEnd - aBegin == 0)
    return;

  // If there is just one rounded rect we can just fill it, if there are more then we
  // must clip the rest to get the intersection of clips
  ApplyRoundedRectsTo(aContext, A2D, aBegin, aEnd - 1);
  AddRoundedRectPathTo(aContext, A2D, mRoundedClipRects[aEnd - 1]);
  aContext->Fill();
}

void
FrameLayerBuilder::Clip::AddRoundedRectPathTo(gfxContext* aContext,
                                              PRInt32 A2D,
                                              const RoundedRect &aRoundRect) const
{
  gfxCornerSizes pixelRadii;
  nsCSSRendering::ComputePixelRadii(aRoundRect.mRadii, A2D, &pixelRadii);

  gfxRect clip = nsLayoutUtils::RectToGfxRect(aRoundRect.mRect, A2D);
  clip.Round();
  clip.Condition();

  aContext->NewPath();
  aContext->RoundedRectangle(clip, pixelRadii);
}

nsRect
FrameLayerBuilder::Clip::ApproximateIntersect(const nsRect& aRect) const
{
  nsRect r = aRect;
  if (mHaveClipRect) {
    r.IntersectRect(r, mClipRect);
  }
  for (PRUint32 i = 0, iEnd = mRoundedClipRects.Length();
       i < iEnd; ++i) {
    const Clip::RoundedRect &rr = mRoundedClipRects[i];
    nsRegion rgn = nsLayoutUtils::RoundedRectIntersectRect(rr.mRect, rr.mRadii, r);
    r = rgn.GetLargestRectangle();
  }
  return r;
}

// Test if (aXPoint, aYPoint) is in the ellipse with center (aXCenter, aYCenter)
// and radii aXRadius, aYRadius.
bool IsInsideEllipse(nscoord aXRadius, nscoord aXCenter, nscoord aXPoint,
                     nscoord aYRadius, nscoord aYCenter, nscoord aYPoint)
{
  float scaledX = float(aXPoint - aXCenter) / float(aXRadius);
  float scaledY = float(aYPoint - aYCenter) / float(aYRadius);
  return scaledX * scaledX + scaledY * scaledY < 1.0f;
}

bool
FrameLayerBuilder::Clip::IsRectClippedByRoundedCorner(const nsRect& aRect) const
{
  if (mRoundedClipRects.IsEmpty())
    return false;

  nsRect rect;
  rect.IntersectRect(aRect, NonRoundedIntersection());
  for (PRUint32 i = 0, iEnd = mRoundedClipRects.Length();
       i < iEnd; ++i) {
    const Clip::RoundedRect &rr = mRoundedClipRects[i];
    // top left
    if (rect.x < rr.mRect.x + rr.mRadii[NS_CORNER_TOP_LEFT_X] &&
        rect.y < rr.mRect.y + rr.mRadii[NS_CORNER_TOP_LEFT_Y]) {
      if (!IsInsideEllipse(rr.mRadii[NS_CORNER_TOP_LEFT_X],
                           rr.mRect.x + rr.mRadii[NS_CORNER_TOP_LEFT_X],
                           rect.x,
                           rr.mRadii[NS_CORNER_TOP_LEFT_Y],
                           rr.mRect.y + rr.mRadii[NS_CORNER_TOP_LEFT_Y],
                           rect.y)) {
        return true;
      }
    }
    // top right
    if (rect.XMost() > rr.mRect.XMost() - rr.mRadii[NS_CORNER_TOP_RIGHT_X] &&
        rect.y < rr.mRect.y + rr.mRadii[NS_CORNER_TOP_RIGHT_Y]) {
      if (!IsInsideEllipse(rr.mRadii[NS_CORNER_TOP_RIGHT_X],
                           rr.mRect.XMost() - rr.mRadii[NS_CORNER_TOP_RIGHT_X],
                           rect.XMost(),
                           rr.mRadii[NS_CORNER_TOP_RIGHT_Y],
                           rr.mRect.y + rr.mRadii[NS_CORNER_TOP_RIGHT_Y],
                           rect.y)) {
        return true;
      }
    }
    // bottom left
    if (rect.x < rr.mRect.x + rr.mRadii[NS_CORNER_BOTTOM_LEFT_X] &&
        rect.YMost() > rr.mRect.YMost() - rr.mRadii[NS_CORNER_BOTTOM_LEFT_Y]) {
      if (!IsInsideEllipse(rr.mRadii[NS_CORNER_BOTTOM_LEFT_X],
                           rr.mRect.x + rr.mRadii[NS_CORNER_BOTTOM_LEFT_X],
                           rect.x,
                           rr.mRadii[NS_CORNER_BOTTOM_LEFT_Y],
                           rr.mRect.YMost() - rr.mRadii[NS_CORNER_BOTTOM_LEFT_Y],
                           rect.YMost())) {
        return true;
      }
    }
    // bottom right
    if (rect.XMost() > rr.mRect.XMost() - rr.mRadii[NS_CORNER_BOTTOM_RIGHT_X] &&
        rect.YMost() > rr.mRect.YMost() - rr.mRadii[NS_CORNER_BOTTOM_RIGHT_Y]) {
      if (!IsInsideEllipse(rr.mRadii[NS_CORNER_BOTTOM_RIGHT_X],
                           rr.mRect.XMost() - rr.mRadii[NS_CORNER_BOTTOM_RIGHT_X],
                           rect.XMost(),
                           rr.mRadii[NS_CORNER_BOTTOM_RIGHT_Y],
                           rr.mRect.YMost() - rr.mRadii[NS_CORNER_BOTTOM_RIGHT_Y],
                           rect.YMost())) {
        return true;
      }
    }
  }
  return false;
}

nsRect
FrameLayerBuilder::Clip::NonRoundedIntersection() const
{
  nsRect result = mClipRect;
  for (PRUint32 i = 0, iEnd = mRoundedClipRects.Length();
       i < iEnd; ++i) {
    result.IntersectRect(result, mRoundedClipRects[i].mRect);
  }
  return result;
}

void
FrameLayerBuilder::Clip::RemoveRoundedCorners()
{
  if (mRoundedClipRects.IsEmpty())
    return;

  mClipRect = NonRoundedIntersection();
  mRoundedClipRects.Clear();
}

gfxRect
CalculateBounds(nsTArray<FrameLayerBuilder::Clip::RoundedRect> aRects, PRInt32 A2D)
{
  nsRect bounds = aRects[0].mRect;
  for (PRUint32 i = 1; i < aRects.Length(); ++i) {
    bounds.UnionRect(bounds, aRects[i].mRect);
   }
 
  return nsLayoutUtils::RectToGfxRect(bounds, A2D);
}
 
void
ContainerState::SetupMaskLayer(Layer *aLayer, const FrameLayerBuilder::Clip& aClip,
                               PRUint32 aRoundedRectClipCount)
{
  // don't build an unnecessary mask
  nsIntRect layerBounds = aLayer->GetVisibleRegion().GetBounds();
  if (aClip.mRoundedClipRects.IsEmpty() ||
      aRoundedRectClipCount <= 0 ||
      layerBounds.IsEmpty()) {
    return;
  }

  // check if we can re-use the mask layer
  nsRefPtr<ImageLayer> maskLayer =  CreateOrRecycleMaskImageLayerFor(aLayer);
  MaskLayerUserData* userData = GetMaskLayerUserData(maskLayer);

  MaskLayerUserData newData;
  newData.mRoundedClipRects.AppendElements(aClip.mRoundedClipRects);
  if (aRoundedRectClipCount < newData.mRoundedClipRects.Length()) {
    newData.mRoundedClipRects.TruncateLength(aRoundedRectClipCount);
  }
  newData.mScaleX = mParameters.mXScale;
  newData.mScaleY = mParameters.mYScale;

  if (*userData == newData) {
    aLayer->SetMaskLayer(maskLayer);
    return;
  }

  // calculate a more precise bounding rect
  const PRInt32 A2D = mContainerFrame->PresContext()->AppUnitsPerDevPixel();
  gfxRect boundingRect = CalculateBounds(newData.mRoundedClipRects, A2D);
  boundingRect.Scale(mParameters.mXScale, mParameters.mYScale);

  PRUint32 maxSize = mManager->GetMaxTextureSize();
  NS_ASSERTION(maxSize > 0, "Invalid max texture size");
  nsIntSize surfaceSize(NS_MIN<PRInt32>(boundingRect.Width(), maxSize),
                        NS_MIN<PRInt32>(boundingRect.Height(), maxSize));

  // maskTransform is applied to the clip when it is painted into the mask (as a
  // component of imageTransform), and its inverse used when the mask is used for
  // masking.
  // It is the transform from the masked layer's space to mask space
  gfxMatrix maskTransform;
  maskTransform.Scale(float(surfaceSize.width)/float(boundingRect.Width()),
                      float(surfaceSize.height)/float(boundingRect.Height()));
  maskTransform.Translate(-boundingRect.TopLeft());
  // imageTransform is only used when the clip is painted to the mask
  gfxMatrix imageTransform = maskTransform;
  imageTransform.Scale(mParameters.mXScale, mParameters.mYScale);

  // copy and transform the rounded rects
  nsTArray<MaskLayerImageCache::PixelRoundedRect> roundedRects;
  for (PRUint32 i = 0; i < newData.mRoundedClipRects.Length(); ++i) {
    roundedRects.AppendElement(
      MaskLayerImageCache::PixelRoundedRect(newData.mRoundedClipRects[i],
                                            mContainerFrame->PresContext()));
    roundedRects[i].ScaleAndTranslate(imageTransform);
  }
 
  // check to see if we can reuse a mask image
  const MaskLayerImageCache::MaskLayerImageKey* key =
    new MaskLayerImageCache::MaskLayerImageKey(roundedRects);
  const MaskLayerImageCache::MaskLayerImageKey* lookupKey = key;

  nsRefPtr<ImageContainer> container =
    GetMaskLayerImageCache()->FindImageFor(&lookupKey);

  if (container) {
    // track the returned key for the mask image
    delete key;
    key = lookupKey;
  } else {
    // no existing mask image, so build a new one
    nsRefPtr<gfxASurface> surface =
      aLayer->Manager()->CreateOptimalMaskSurface(surfaceSize);

    // fail if we can't get the right surface
    if (!surface || surface->CairoStatus()) {
      NS_WARNING("Could not create surface for mask layer.");
      return;
    }

    nsRefPtr<gfxContext> context = new gfxContext(surface);
    context->Multiply(imageTransform);

    // paint the clipping rects with alpha to create the mask
    context->SetColor(gfxRGBA(1, 1, 1, 1));
    aClip.DrawRoundedRectsTo(context, A2D, 0, aRoundedRectClipCount);

    // build the image and container
    container = aLayer->Manager()->CreateImageContainer();
    NS_ASSERTION(container, "Could not create image container for mask layer.");
    static const Image::Format format = Image::CAIRO_SURFACE;
    nsRefPtr<Image> image = container->CreateImage(&format, 1);
    NS_ASSERTION(image, "Could not create image container for mask layer.");
    CairoImage::Data data;
    data.mSurface = surface;
    data.mSize = surfaceSize;
    static_cast<CairoImage*>(image.get())->SetData(data);
    container->SetCurrentImage(image);

    GetMaskLayerImageCache()->PutImage(key, container);
  }

  maskLayer->SetContainer(container);
  maskLayer->SetBaseTransform(gfx3DMatrix::From2D(maskTransform.Invert()));

  // save the details of the clip in user data
  userData->mScaleX = newData.mScaleX;
  userData->mScaleY = newData.mScaleY;
  userData->mRoundedClipRects.SwapElements(newData.mRoundedClipRects);
  userData->mImageKey = key;

  aLayer->SetMaskLayer(maskLayer);
  return;
}

} // namespace mozilla
