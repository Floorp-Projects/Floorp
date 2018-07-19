/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FRAMELAYERBUILDER_H_
#define FRAMELAYERBUILDER_H_

#include "nsAutoPtr.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"
#include "nsRegion.h"
#include "nsIFrame.h"
#include "DisplayItemClip.h"
#include "mozilla/gfx/MatrixFwd.h"
#include "mozilla/layers/LayersTypes.h"
#include "LayerState.h"
#include "Layers.h"
#include "LayerUserData.h"
#include "nsDisplayItemTypes.h"
#include "MatrixStack.h"

class nsDisplayListBuilder;
class nsDisplayList;
class nsDisplayItem;
class gfxContext;
class nsDisplayItemGeometry;
class nsDisplayMask;

namespace mozilla {
struct ActiveScrolledRoot;
struct DisplayItemClipChain;
namespace layers {
class ContainerLayer;
class LayerManager;
class BasicLayerManager;
class PaintedLayer;
class ImageLayer;
} // namespace layers

class FrameLayerBuilder;
class LayerManagerData;
class PaintedLayerData;
class ContainerState;
class PaintedDisplayItemLayerUserData;

enum class DisplayItemEntryType {
  ITEM,
  PUSH_OPACITY,
  PUSH_OPACITY_WITH_BG,
  POP_OPACITY,
  PUSH_TRANSFORM,
  POP_TRANSFORM
};

/**
  * Retained data storage:
  *
  * Each layer manager (widget, and inactive) stores a LayerManagerData object
  * that keeps a hash-set of DisplayItemData items that were drawn into it.
  * Each frame also keeps a list of DisplayItemData pointers that were
  * created for that frame. DisplayItemData objects manage these lists automatically.
  *
  * During layer construction we update the data in the LayerManagerData object, marking
  * items that are modified. At the end we sweep the LayerManagerData hash-set and remove
  * all items that haven't been modified.
  */

/**
  * Retained data for a display item.
  */
class DisplayItemData final {
public:
  friend class FrameLayerBuilder;
  friend class ContainerState;

  uint32_t GetDisplayItemKey() { return mDisplayItemKey; }
  layers::Layer* GetLayer() const { return mLayer; }
  nsDisplayItemGeometry* GetGeometry() const { return mGeometry.get(); }
  const DisplayItemClip& GetClip() const { return mClip; }
  void Invalidate() { mIsInvalid = true; }
  void ClearAnimationCompositorState();
  void SetItem(nsDisplayItem* aItem) { mItem = aItem; }
  nsDisplayItem* GetItem() const { return mItem; }

  bool HasMergedFrames() const { return mFrameList.Length() > 1; }

  static DisplayItemData* AssertDisplayItemData(DisplayItemData* aData);

  void* operator new(size_t sz, nsPresContext* aPresContext)
  {
    // Check the recycle list first.
    return aPresContext->PresShell()->
      AllocateByObjectID(eArenaObjectID_DisplayItemData, sz);
  }

  nsrefcnt AddRef() {
    if (mRefCnt == UINT32_MAX) {
      NS_WARNING("refcount overflow, leaking object");
      return mRefCnt;
    }
    ++mRefCnt;
    NS_LOG_ADDREF(this, mRefCnt, "ComputedStyle", sizeof(ComputedStyle));
    return mRefCnt;
  }

  nsrefcnt Release() {
    if (mRefCnt == UINT32_MAX) {
      NS_WARNING("refcount overflow, leaking object");
      return mRefCnt;
    }
    --mRefCnt;
    NS_LOG_RELEASE(this, mRefCnt, "ComputedStyle");
    if (mRefCnt == 0) {
      Destroy();
      return 0;
    }
    return mRefCnt;
  }

  RefPtr<TransformClipNode> mTransform;

private:
  DisplayItemData(LayerManagerData* aParent,
                  uint32_t aKey,
                  layers::Layer* aLayer,
                  nsIFrame* aFrame = nullptr);

  /**
    * Removes any references to this object from frames
    * in mFrameList.
    */
  ~DisplayItemData();

  void Destroy()
  {
    // Get the pres context.
    RefPtr<nsPresContext> presContext = mFrameList[0]->PresContext();

    // Call our destructor.
    this->~DisplayItemData();

    // Don't let the memory be freed, since it will be recycled
    // instead. Don't call the global operator delete.
    presContext->PresShell()->
      FreeByObjectID(eArenaObjectID_DisplayItemData, this);
  }

  /**
    * Associates this DisplayItemData with a frame, and adds it
    * to the LayerManagerDataProperty list on the frame.
    */
  void AddFrame(nsIFrame* aFrame);
  void RemoveFrame(nsIFrame* aFrame);
  const nsRegion& GetChangedFrameInvalidations();

  /**
    * Updates the contents of this item to a new set of data, instead of allocating a new
    * object.
    * Set the passed in parameters, and clears the opt layer and inactive manager.
    * Parent, and display item key are assumed to be the same.
    *
    * EndUpdate must be called before the end of the transaction to complete the update.
    */
  void BeginUpdate(layers::Layer* aLayer, LayerState aState,
                   bool aFirstUpdate,
                   nsDisplayItem* aItem = nullptr);
  void BeginUpdate(layers::Layer* aLayer, LayerState aState,
                   nsDisplayItem* aItem, bool aIsReused, bool aIsMerged);

  /**
    * Completes the update of this, and removes any references to data that won't live
    * longer than the transaction.
    *
    * Updates the geometry, frame list and clip.
    * For items within a PaintedLayer, a geometry object must be specified to retain
    * until the next transaction.
    *
    */
  void EndUpdate(nsAutoPtr<nsDisplayItemGeometry> aGeometry);
  void EndUpdate();

  uint32_t mRefCnt;
  LayerManagerData* mParent;
  RefPtr<layers::Layer> mLayer;
  RefPtr<layers::Layer> mOptLayer;
  RefPtr<layers::BasicLayerManager> mInactiveManager;
  AutoTArray<nsIFrame*, 1> mFrameList;
  nsAutoPtr<nsDisplayItemGeometry> mGeometry;
  DisplayItemClip mClip;
  uint32_t        mDisplayItemKey;
  LayerState      mLayerState;

  /**
    * Temporary stoarage of the display item being referenced, only valid between
    * BeginUpdate and EndUpdate.
    */
  nsDisplayItem* mItem;
  nsRegion mChangedFrameInvalidations;

  /**
    * Used to track if data currently stored in mFramesWithLayers (from an existing
    * paint) has been updated in the current paint.
    */
  bool            mUsed;
  bool            mIsInvalid;
  bool            mReusedItem;
};

class RefCountedRegion {
private:
  ~RefCountedRegion() {}
public:
  NS_INLINE_DECL_REFCOUNTING(RefCountedRegion)

  RefCountedRegion() : mIsInfinite(false) {}
  nsRegion mRegion;
  bool mIsInfinite;
};

struct AssignedDisplayItem
{
  AssignedDisplayItem(nsDisplayItem* aItem,
                      LayerState aLayerState,
                      DisplayItemData* aData,
                      const nsRect& aContentRect,
                      DisplayItemEntryType aType,
                      const bool aHasOpacity,
                      const RefPtr<TransformClipNode>& aTransform);
  ~AssignedDisplayItem();

  nsDisplayItem* mItem;
  LayerState mLayerState;
  DisplayItemData* mDisplayItemData;
  nsRect mContentRect;

  /**
   * If the display item is being rendered as an inactive
   * layer, then this stores the layer manager being
   * used for the inactive transaction.
   */
  RefPtr<layers::LayerManager> mInactiveLayerManager;
  RefPtr<TransformClipNode> mTransform;
  DisplayItemEntryType mType;

  bool mReused;
  bool mMerged;
  bool mHasOpacity;
  bool mHasTransform;
  bool mHasPaintRect;
};


struct ContainerLayerParameters {
  ContainerLayerParameters()
    : mXScale(1)
    , mYScale(1)
    , mLayerContentsVisibleRect(nullptr)
    , mBackgroundColor(NS_RGBA(0,0,0,0))
    , mScrollMetadataASR(nullptr)
    , mCompositorASR(nullptr)
    , mInTransformedSubtree(false)
    , mInActiveTransformedSubtree(false)
    , mDisableSubpixelAntialiasingInDescendants(false)
    , mForEventsAndPluginsOnly(false)
    , mLayerCreationHint(layers::LayerManager::NONE)
  {}
  ContainerLayerParameters(float aXScale, float aYScale)
    : mXScale(aXScale)
    , mYScale(aYScale)
    , mLayerContentsVisibleRect(nullptr)
    , mBackgroundColor(NS_RGBA(0,0,0,0))
    , mScrollMetadataASR(nullptr)
    , mCompositorASR(nullptr)
    , mInTransformedSubtree(false)
    , mInActiveTransformedSubtree(false)
    , mDisableSubpixelAntialiasingInDescendants(false)
    , mForEventsAndPluginsOnly(false)
    , mLayerCreationHint(layers::LayerManager::NONE)
  {}
  ContainerLayerParameters(float aXScale, float aYScale,
                           const nsIntPoint& aOffset,
                           const ContainerLayerParameters& aParent)
    : mXScale(aXScale)
    , mYScale(aYScale)
    , mLayerContentsVisibleRect(nullptr)
    , mOffset(aOffset)
    , mBackgroundColor(aParent.mBackgroundColor)
    , mScrollMetadataASR(aParent.mScrollMetadataASR)
    , mCompositorASR(aParent.mCompositorASR)
    , mInTransformedSubtree(aParent.mInTransformedSubtree)
    , mInActiveTransformedSubtree(aParent.mInActiveTransformedSubtree)
    , mDisableSubpixelAntialiasingInDescendants(aParent.mDisableSubpixelAntialiasingInDescendants)
    , mForEventsAndPluginsOnly(aParent.mForEventsAndPluginsOnly)
    , mLayerCreationHint(aParent.mLayerCreationHint)
  {}

  float mXScale, mYScale;

  LayoutDeviceToLayerScale2D Scale() const {
    return LayoutDeviceToLayerScale2D(mXScale, mYScale);
  }

  /**
   * If non-null, the rectangle in which BuildContainerLayerFor stores the
   * visible rect of the layer, in the coordinate system of the created layer.
   */
  nsIntRect* mLayerContentsVisibleRect;

  /**
   * An offset to apply to all child layers created.
   */
  nsIntPoint mOffset;

  LayerIntPoint Offset() const {
    return LayerIntPoint::FromUnknownPoint(mOffset);
  }

  nscolor mBackgroundColor;
  const ActiveScrolledRoot* mScrollMetadataASR;
  const ActiveScrolledRoot* mCompositorASR;

  bool mInTransformedSubtree;
  bool mInActiveTransformedSubtree;
  bool mDisableSubpixelAntialiasingInDescendants;
  bool mForEventsAndPluginsOnly;
  layers::LayerManager::PaintedLayerCreationHint mLayerCreationHint;

  /**
   * When this is false, PaintedLayer coordinates are drawn to with an integer
   * translation and the scale in mXScale/mYScale.
   */
  bool AllowResidualTranslation()
  {
    // If we're in a transformed subtree, but no ancestor transform is actively
    // changing, we'll use the residual translation when drawing into the
    // PaintedLayer to ensure that snapping exactly matches the ideal transform.
    return mInTransformedSubtree && !mInActiveTransformedSubtree;
  }
};

/**
 * The FrameLayerBuilder is responsible for converting display lists
 * into layer trees. Every LayerManager needs a unique FrameLayerBuilder
 * to build layers.
 *
 * The most important API in this class is BuildContainerLayerFor. This
 * method takes a display list as input and constructs a ContainerLayer
 * with child layers that render the contents of the display list. It
 * records the relationship between frames and layers.
 *
 * That data enables us to retain layer trees. When constructing a
 * ContainerLayer, we first check to see if there's an existing
 * ContainerLayer for the same frame that can be recycled. If we recycle
 * it, we also try to reuse its existing PaintedLayer children to render
 * the display items without layers of their own. The idea is that by
 * recycling layers deterministically, we can ensure that when nothing
 * changes in a display list, we will reuse the existing layers without
 * changes.
 *
 * We expose a GetLeafLayerFor method that can be called by display items
 * that make their own layers (e.g. canvas and video); this method
 * locates the last layer used to render the display item, if any, and
 * return it as a candidate for recycling.
 *
 * FrameLayerBuilder sets up PaintedLayers so that 0,0 in the Painted layer
 * corresponds to the (pixel-snapped) top-left of the aAnimatedGeometryRoot.
 * It sets up ContainerLayers so that 0,0 in the container layer
 * corresponds to the snapped top-left of the display item reference frame.
 *
 * When we construct a container layer, we know the transform that will be
 * applied to the layer. If the transform scales the content, we can get
 * better results when intermediate buffers are used by pushing some scale
 * from the container's transform down to the children. For PaintedLayer
 * children, the scaling can be achieved by changing the size of the layer
 * and drawing into it with increased or decreased resolution. By convention,
 * integer types (nsIntPoint/nsIntSize/nsIntRect/nsIntRegion) are all in layer
 * coordinates, post-scaling, whereas appunit types are all pre-scaling.
 */
class FrameLayerBuilder : public layers::LayerUserData {
public:
  typedef layers::ContainerLayer ContainerLayer;
  typedef layers::Layer Layer;
  typedef layers::PaintedLayer PaintedLayer;
  typedef layers::ImageLayer ImageLayer;
  typedef layers::LayerManager LayerManager;
  typedef layers::BasicLayerManager BasicLayerManager;
  typedef layers::EventRegions EventRegions;

  FrameLayerBuilder();
  ~FrameLayerBuilder();

  static void Shutdown();

  void Init(nsDisplayListBuilder* aBuilder, LayerManager* aManager,
            PaintedLayerData* aLayerData = nullptr,
            bool aIsInactiveLayerManager = false,
            const DisplayItemClip* aInactiveLayerClip = nullptr);

  /**
   * Call this to notify that we have just started a transaction on the
   * retained layer manager aManager.
   */
  void DidBeginRetainedLayerTransaction(LayerManager* aManager);

  /**
   * Call this just before we end a transaction.
   */
  void WillEndTransaction();

  /**
   * Call this after we end a transaction.
   */
  void DidEndTransaction();

  enum {
    /**
     * Set this when pulling an opaque background color from behind the
     * container layer into the container doesn't change the visual results,
     * given the effects you're going to apply to the container layer.
     * For example, this is compatible with opacity or clipping/masking, but
     * not with non-OVER blend modes or filters.
     */
    CONTAINER_ALLOW_PULL_BACKGROUND_COLOR = 0x01
  };
  /**
   * Build a container layer for a display item that contains a child
   * list, either reusing an existing one or creating a new one. It
   * sets the container layer children to layers which together render
   * the contents of the display list. It reuses existing layers from
   * the retained layer manager if possible.
   * aContainerItem may be null, in which case we construct a root layer.
   * This gets called by display list code. It calls BuildLayer on the
   * items in the display list, making items with their own layers
   * children of the new container, and assigning all other items to
   * PaintedLayer children created and managed by the FrameLayerBuilder.
   * Returns a layer with clip rect cleared; it is the
   * caller's responsibility to add any clip rect. The visible region
   * is set based on what's in the layer.
   * The container layer is transformed by aTransform (if non-null), and
   * the result is transformed by the scale factors in aContainerParameters.
   * aChildren is modified due to display item merging and flattening.
   * The visible region of the returned layer is set only if aContainerItem
   * is null.
   */
  already_AddRefed<ContainerLayer>
  BuildContainerLayerFor(nsDisplayListBuilder* aBuilder,
                         LayerManager* aManager,
                         nsIFrame* aContainerFrame,
                         nsDisplayItem* aContainerItem,
                         nsDisplayList* aChildren,
                         const ContainerLayerParameters& aContainerParameters,
                         const gfx::Matrix4x4* aTransform,
                         uint32_t aFlags = 0);

  /**
   * Get a retained layer for a display item that needs to create its own
   * layer for rendering (i.e. under nsDisplayItem::BuildLayer). Returns
   * null if no retained layer is available, which usually means that this
   * display item didn't have a layer before so the caller will
   * need to create one.
   * Returns a layer with clip rect cleared; it is the
   * caller's responsibility to add any clip rect and set the visible
   * region.
   */
  Layer* GetLeafLayerFor(nsDisplayListBuilder* aBuilder,
                         nsDisplayItem* aItem);

  /**
   * Call this to force all retained layers to be discarded and recreated at
   * the next paint.
   */
  static void InvalidateAllLayers(LayerManager* aManager);
  static void InvalidateAllLayersForFrame(nsIFrame *aFrame);

  /**
   * Call this to determine if a frame has a dedicated (non-Painted) layer
   * for the given display item key. If there isn't one, we return null,
   * otherwise we return the layer.
   */
  static Layer* GetDedicatedLayer(nsIFrame* aFrame, DisplayItemType aDisplayItemType);

  /**
   * This callback must be provided to EndTransaction. The callback data
   * must be the nsDisplayListBuilder containing this FrameLayerBuilder.
   * This function can be called multiple times in a row to draw
   * different regions. This will occur when, for example, progressive paint is
   * enabled. In these cases aDirtyRegion can be used to specify a larger region
   * than aRegionToDraw that will be drawn during the transaction, possibly
   * allowing the callback to make optimizations.
   */
  static void DrawPaintedLayer(PaintedLayer* aLayer,
                              gfxContext* aContext,
                              const nsIntRegion& aRegionToDraw,
                              const nsIntRegion& aDirtyRegion,
                              mozilla::layers::DrawRegionClip aClip,
                              const nsIntRegion& aRegionToInvalidate,
                              void* aCallbackData);

  /**
   * Dumps this FrameLayerBuilder's retained layer manager's retained
   * layer tree. Defaults to dumping to stdout in non-HTML format.
   */
  static void DumpRetainedLayerTree(LayerManager* aManager, std::stringstream& aStream, bool aDumpHtml = false);

  /**
   * Returns the most recently allocated geometry item for the given display
   * item.
   *
   * XXX(seth): The current implementation must iterate through all display
   * items allocated for this display item's frame. This may lead to O(n^2)
   * behavior in some situations.
   */
  static nsDisplayItemGeometry* GetMostRecentGeometry(nsDisplayItem* aItem);


  /******* PRIVATE METHODS to FrameLayerBuilder.cpp ********/
  /* These are only in the public section because they need
   * to be called by file-scope helper functions in FrameLayerBuilder.cpp.
   */

  /**
   * Record aItem as a display item that is rendered by the PaintedLayer
   * aLayer, with aClipRect, where aContainerLayerFrame is the frame
   * for the container layer this ThebesItem belongs to.
   * aItem must have an underlying frame.
   * @param aTopLeft offset from active scrolled root to reference frame
   */
  void AddPaintedDisplayItem(PaintedLayerData* aLayerData,
                             AssignedDisplayItem& aAssignedDisplayItem,
                             ContainerState& aContainerState,
                             Layer* aLayer);

  /**
   * Calls GetOldLayerForFrame on the underlying frame of the display item,
   * and each subsequent merged frame if no layer is found for the underlying
   * frame.
   */
  Layer* GetOldLayerFor(nsDisplayItem* aItem,
                        nsDisplayItemGeometry** aOldGeometry = nullptr,
                        DisplayItemClip** aOldClip = nullptr);

  static DisplayItemData* GetOldDataFor(nsDisplayItem* aItem);

  /**
   * Return the layer that all display items of aFrame were assigned to in the
   * last paint, or nullptr if there was no single layer assigned to all of the
   * frame's display items (i.e. zero, or more than one).
   * This function is for testing purposes and not performance sensitive.
   */
  template<class T>
  static T*
  GetDebugSingleOldLayerForFrame(nsIFrame* aFrame)
  {
    SmallPointerArray<DisplayItemData>& array = aFrame->DisplayItemData();

    Layer* layer = nullptr;
    for (DisplayItemData* data : array) {
      DisplayItemData::AssertDisplayItemData(data);
      if (data->mLayer->GetType() != T::Type()) {
        continue;
      }
      if (layer && layer != data->mLayer) {
        // More than one layer assigned, bail.
        return nullptr;
      }
      layer = data->mLayer;
    }

    if (!layer) {
      return nullptr;
    }

    return static_cast<T*>(layer);
  }

  /**
   * Destroy any stored LayerManagerDataProperty and the associated data for
   * aFrame.
   */
  static void DestroyDisplayItemDataFor(nsIFrame* aFrame);

  LayerManager* GetRetainingLayerManager() { return mRetainingManager; }

  /**
   * Returns true if the given display item was rendered during the previous
   * paint. Returns false otherwise.
   */
  static bool HasRetainedDataFor(nsIFrame* aFrame, uint32_t aDisplayItemKey);

  typedef void (*DisplayItemDataCallback)(nsIFrame *aFrame, DisplayItemData* aItem);

  /**
   * Get the translation transform that was in aLayer when we last painted. It's either
   * the transform saved by SaveLastPaintTransform, or else the transform
   * that's currently in the layer (which must be an integer translation).
   */
  nsIntPoint GetLastPaintOffset(PaintedLayer* aLayer);

  /**
   * Return the resolution at which we expect to render aFrame's contents,
   * assuming they are being painted to retained layers. This takes into account
   * the resolution the contents of the ContainerLayer containing aFrame are
   * being rendered at, as well as any currently-inactive transforms between
   * aFrame and that container layer.
   */
  static gfxSize GetPaintedLayerScaleForFrame(nsIFrame* aFrame);

  static void RemoveFrameFromLayerManager(const nsIFrame* aFrame,
                                          SmallPointerArray<DisplayItemData>& aArray);

  /**
   * Given a frame and a display item key that uniquely identifies a
   * display item for the frame, find the layer that was last used to
   * render that display item. Returns null if there is no such layer.
   * This could be a dedicated layer for the display item, or a PaintedLayer
   * that renders many display items.
   */
  DisplayItemData* GetOldLayerForFrame(nsIFrame* aFrame,
                                       uint32_t aDisplayItemKey,
                                       DisplayItemData* aOldData = nullptr,
                                       LayerManager* aOldLayerManager = nullptr);

  /**
   * Stores DisplayItemData associated with aFrame, stores the data in
   * mNewDisplayItemData.
   */
  DisplayItemData* StoreDataForFrame(nsDisplayItem* aItem, Layer* aLayer,
                                     LayerState aState, DisplayItemData* aData);
  void StoreDataForFrame(nsIFrame* aFrame,
                         uint32_t aDisplayItemKey,
                         Layer* aLayer,
                         LayerState aState);

protected:
  friend class LayerManagerData;

  // Flash the area within the context clip if paint flashing is enabled.
  static void FlashPaint(gfxContext *aContext);

  /*
   * Get the DisplayItemData array associated with this frame, or null if one
   * doesn't exist.
   *
   * Note that the pointer returned here is only valid so long as you don't
   * poke the LayerManagerData's mFramesWithLayers hashtable.
   */
  DisplayItemData* GetDisplayItemData(nsIFrame *aFrame, uint32_t aKey);

  /*
   * Get the DisplayItemData associated with this frame / display item pair,
   * using the LayerManager instead of FrameLayerBuilder.
   */
  static DisplayItemData* GetDisplayItemDataForManager(nsIFrame* aFrame,
                                                       uint32_t aDisplayItemKey,
                                                       LayerManager* aManager);
  static DisplayItemData* GetDisplayItemDataForManager(nsIFrame* aFrame,
                                                       uint32_t aDisplayItemKey);
  static DisplayItemData* GetDisplayItemDataForManager(nsDisplayItem* aItem, LayerManager* aManager);
  static DisplayItemData* GetDisplayItemDataForManager(nsIFrame* aFrame,
                                                       uint32_t aDisplayItemKey,
                                                       LayerManagerData* aData);

  /**
   * We store one of these for each display item associated with a
   * PaintedLayer, in a hashtable that maps each PaintedLayer to an array
   * of ClippedDisplayItems. (PaintedLayerItemsEntry is the hash entry
   * for that hashtable.)
   * These are only stored during the paint process, so that the
   * DrawPaintedLayer callback can figure out which items to draw for the
   * PaintedLayer.
   */

  static void RecomputeVisibilityForItems(std::vector<AssignedDisplayItem>& aItems,
                                          nsDisplayListBuilder* aBuilder,
                                          const nsIntRegion& aRegionToDraw,
                                          nsRect& aPreviousRectToDraw,
                                          const nsIntPoint& aOffset,
                                          int32_t aAppUnitsPerDevPixel,
                                          float aXScale,
                                          float aYScale);

  void PaintItems(std::vector<AssignedDisplayItem>& aItems,
                  const nsIntRect& aRect,
                  gfxContext* aContext,
                  nsDisplayListBuilder* aBuilder,
                  nsPresContext* aPresContext,
                  const nsIntPoint& aOffset,
                  float aXScale, float aYScale);

  /**
   * We accumulate ClippedDisplayItem elements in a hashtable during
   * the paint process. This is the hashentry for that hashtable.
   */
public:
  /**
   * Add the PaintedDisplayItemLayerUserData object as being used in this
   * transaction so that we clean it up afterwards.
   */
  void AddPaintedLayerItemsEntry(PaintedDisplayItemLayerUserData* aData);

  PaintedLayerData* GetContainingPaintedLayerData()
  {
    return mContainingPaintedLayer;
  }

  const DisplayItemClip* GetInactiveLayerClip() const
  {
    return mInactiveLayerClip;
  }

  /*
   * If we're building layers for an item with an inactive layer tree,
   * this function saves the item's clip, which will later be applied
   * to the event regions. The clip should be relative to
   * mContainingPaintedLayer->mReferenceFrame.
   */
  void SetInactiveLayerClip(const DisplayItemClip* aClip)
  {
    mInactiveLayerClip = aClip;
  }

  bool IsBuildingRetainedLayers()
  {
    return !mIsInactiveLayerManager && mRetainingManager;
  }

  /**
   * Attempt to build the most compressed layer tree possible, even if it means
   * throwing away existing retained buffers.
   */
  void SetLayerTreeCompressionMode() { mInLayerTreeCompressionMode = true; }
  bool CheckInLayerTreeCompressionMode();

  void ComputeGeometryChangeForItem(DisplayItemData* aData);

protected:
  /**
   * The layer manager belonging to the widget that is being retained
   * across paints.
   */
  LayerManager*                       mRetainingManager;
  /**
   * The root prescontext for the display list builder reference frame
   */
  RefPtr<nsRootPresContext>         mRootPresContext;

  /**
   * The display list builder being used.
   */
  nsDisplayListBuilder*               mDisplayListBuilder;
  /**
   * An array of PaintedLayer user data objects containing the
   * list of display items (plus clipping data) to be rendered in the
   * layer. We clean these up at the end of the transaction to
   * remove references to display items.
   */
  AutoTArray<RefPtr<PaintedDisplayItemLayerUserData>, 5> mPaintedLayerItems;

  /**
   * When building layers for an inactive layer, this is where the
   * inactive layer will be placed.
   */
  PaintedLayerData*                   mContainingPaintedLayer;

  /**
   * When building layers for an inactive layer, this stores the clip
   * of the display item that built the inactive layer.
   */
  const DisplayItemClip*              mInactiveLayerClip;

  /**
   * Indicates that the entire layer tree should be rerendered
   * during this paint.
   */
  bool                                mInvalidateAllLayers;

  bool                                mInLayerTreeCompressionMode;

  bool                                mIsInactiveLayerManager;

};

} // namespace mozilla

#endif /* FRAMELAYERBUILDER_H_ */
