/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FRAMELAYERBUILDER_H_
#define FRAMELAYERBUILDER_H_

#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"
#include "nsRegion.h"
#include "nsIFrame.h"
#include "ImageLayers.h"
#include "DisplayItemClip.h"
#include "mozilla/layers/LayersTypes.h"

class nsDisplayListBuilder;
class nsDisplayList;
class nsDisplayItem;
class gfxContext;
class nsDisplayItemGeometry;

namespace mozilla {
namespace layers {
class ContainerLayer;
class LayerManager;
class BasicLayerManager;
class ThebesLayer;
}

class FrameLayerBuilder;
class LayerManagerData;
class ThebesLayerData;

enum LayerState {
  LAYER_NONE,
  LAYER_INACTIVE,
  LAYER_ACTIVE,
  // Force an active layer even if it causes incorrect rendering, e.g.
  // when the layer has rounded rect clips.
  LAYER_ACTIVE_FORCE,
  // Special layer that is metadata only.
  LAYER_ACTIVE_EMPTY,
  // Inactive style layer for rendering SVG effects.
  LAYER_SVG_EFFECTS
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

struct ContainerLayerParameters {
  ContainerLayerParameters() :
    mXScale(1), mYScale(1), mAncestorClipRect(nullptr),
    mInTransformedSubtree(false), mInActiveTransformedSubtree(false),
    mDisableSubpixelAntialiasingInDescendants(false)
  {}
  ContainerLayerParameters(float aXScale, float aYScale) :
    mXScale(aXScale), mYScale(aYScale), mAncestorClipRect(nullptr),
    mInTransformedSubtree(false), mInActiveTransformedSubtree(false),
    mDisableSubpixelAntialiasingInDescendants(false)
  {}
  ContainerLayerParameters(float aXScale, float aYScale,
                           const nsIntPoint& aOffset,
                           const ContainerLayerParameters& aParent) :
    mXScale(aXScale), mYScale(aYScale), mAncestorClipRect(nullptr),
    mOffset(aOffset),
    mInTransformedSubtree(aParent.mInTransformedSubtree),
    mInActiveTransformedSubtree(aParent.mInActiveTransformedSubtree),
    mDisableSubpixelAntialiasingInDescendants(aParent.mDisableSubpixelAntialiasingInDescendants)
  {}
  float mXScale, mYScale;
  /**
   * An ancestor clip rect that can be applied to restrict the visibility
   * of this container. Null if none available.
   */
  const nsIntRect* mAncestorClipRect;
  /**
   * An offset to append to the transform set on all child layers created.
   */
  nsIntPoint mOffset;

  bool mInTransformedSubtree;
  bool mInActiveTransformedSubtree;
  bool mDisableSubpixelAntialiasingInDescendants;
  /**
   * When this is false, ThebesLayer coordinates are drawn to with an integer
   * translation and the scale in mXScale/mYScale.
   */
  bool AllowResidualTranslation()
  {
    // If we're in a transformed subtree, but no ancestor transform is actively
    // changing, we'll use the residual translation when drawing into the
    // ThebesLayer to ensure that snapping exactly matches the ideal transform.
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
 * it, we also try to reuse its existing ThebesLayer children to render
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
 * FrameLayerBuilder sets up ThebesLayers so that 0,0 in the Thebes layer
 * corresponds to the (pixel-snapped) top-left of the aAnimatedGeometryRoot.
 * It sets up ContainerLayers so that 0,0 in the container layer
 * corresponds to the snapped top-left of the display item reference frame.
 *
 * When we construct a container layer, we know the transform that will be
 * applied to the layer. If the transform scales the content, we can get
 * better results when intermediate buffers are used by pushing some scale
 * from the container's transform down to the children. For ThebesLayer
 * children, the scaling can be achieved by changing the size of the layer
 * and drawing into it with increased or decreased resolution. By convention,
 * integer types (nsIntPoint/nsIntSize/nsIntRect/nsIntRegion) are all in layer
 * coordinates, post-scaling, whereas appunit types are all pre-scaling.
 */
class FrameLayerBuilder : public layers::LayerUserData {
public:
  typedef layers::ContainerLayer ContainerLayer;
  typedef layers::Layer Layer;
  typedef layers::ThebesLayer ThebesLayer;
  typedef layers::ImageLayer ImageLayer;
  typedef layers::LayerManager LayerManager;
  typedef layers::BasicLayerManager BasicLayerManager;
  typedef layers::EventRegions EventRegions;

  FrameLayerBuilder() :
    mRetainingManager(nullptr),
    mDetectedDOMModification(false),
    mInvalidateAllLayers(false),
    mInLayerTreeCompressionMode(false),
    mContainerLayerGeneration(0),
    mMaxContainerLayerGeneration(0)
  {
    MOZ_COUNT_CTOR(FrameLayerBuilder);
  }
  ~FrameLayerBuilder()
  {
    MOZ_COUNT_DTOR(FrameLayerBuilder);
  }

  static void Shutdown();

  void Init(nsDisplayListBuilder* aBuilder, LayerManager* aManager,
            ThebesLayerData* aLayerData = nullptr);

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
    CONTAINER_NOT_CLIPPED_BY_ANCESTORS = 0x01
  };
  /**
   * Build a container layer for a display item that contains a child
   * list, either reusing an existing one or creating a new one. It
   * sets the container layer children to layers which together render
   * the contents of the display list. It reuses existing layers from
   * the retained layer manager if possible.
   * aContainer may be null, in which case we construct a root layer.
   * This gets called by display list code. It calls BuildLayer on the
   * items in the display list, making items with their own layers
   * children of the new container, and assigning all other items to
   * ThebesLayer children created and managed by the FrameLayerBuilder.
   * Returns a layer with clip rect cleared; it is the
   * caller's responsibility to add any clip rect. The visible region
   * is set based on what's in the layer.
   * The container layer is transformed by aTransform (if non-null), and
   * the result is transformed by the scale factors in aContainerParameters.
   */
  already_AddRefed<ContainerLayer>
  BuildContainerLayerFor(nsDisplayListBuilder* aBuilder,
                         LayerManager* aManager,
                         nsIFrame* aContainerFrame,
                         nsDisplayItem* aContainerItem,
                         const nsDisplayList& aChildren,
                         const ContainerLayerParameters& aContainerParameters,
                         const gfx3DMatrix* aTransform,
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
   * Call this to determine if a frame has a dedicated (non-Thebes) layer
   * for the given display item key. If there isn't one, we return null,
   * otherwise we return the layer.
   */
  static Layer* GetDedicatedLayer(nsIFrame* aFrame, uint32_t aDisplayItemKey);

  /**
   * This callback must be provided to EndTransaction. The callback data
   * must be the nsDisplayListBuilder containing this FrameLayerBuilder.
   * This function can be called multiple times in a row to draw
   * different regions.
   */
  static void DrawThebesLayer(ThebesLayer* aLayer,
                              gfxContext* aContext,
                              const nsIntRegion& aRegionToDraw,
                              mozilla::layers::DrawRegionClip aClip,
                              const nsIntRegion& aRegionToInvalidate,
                              void* aCallbackData);

#ifdef MOZ_DUMP_PAINTING
  /**
   * Dumps this FrameLayerBuilder's retained layer manager's retained
   * layer tree. Defaults to dumping to stdout in non-HTML format.
   */
  static void DumpRetainedLayerTree(LayerManager* aManager, std::stringstream& aStream, bool aDumpHtml = false);
#endif

  /******* PRIVATE METHODS to FrameLayerBuilder.cpp ********/
  /* These are only in the public section because they need
   * to be called by file-scope helper functions in FrameLayerBuilder.cpp.
   */

  /**
   * Record aItem as a display item that is rendered by aLayer.
   *
   * @param aLayer Layer that the display item will be rendered into
   * @param aItem Display item to be drawn.
   * @param aLayerState What LayerState the item is using.
   * @param aTopLeft offset from active scrolled root to reference frame
   * @param aManager If the layer is in the LAYER_INACTIVE state,
   * then this is the temporary layer manager to draw with.
   */
  void AddLayerDisplayItem(Layer* aLayer,
                           nsDisplayItem* aItem,
                           const DisplayItemClip& aClip,
                           LayerState aLayerState,
                           const nsPoint& aTopLeft,
                           BasicLayerManager* aManager,
                           nsAutoPtr<nsDisplayItemGeometry> aGeometry);

  /**
   * Record aItem as a display item that is rendered by the ThebesLayer
   * aLayer, with aClipRect, where aContainerLayerFrame is the frame
   * for the container layer this ThebesItem belongs to.
   * aItem must have an underlying frame.
   * @param aTopLeft offset from active scrolled root to reference frame
   */
  void AddThebesDisplayItem(ThebesLayerData* aLayer,
                            nsDisplayItem* aItem,
                            const DisplayItemClip& aClip,
                            nsIFrame* aContainerLayerFrame,
                            LayerState aLayerState,
                            const nsPoint& aTopLeft,
                            nsAutoPtr<nsDisplayItemGeometry> aGeometry);

  /**
   * Gets the frame property descriptor for the given manager, or for the current
   * widget layer manager if nullptr is passed.
   */
  static const FramePropertyDescriptor* GetDescriptorForManager(LayerManager* aManager);

  /**
   * Calls GetOldLayerForFrame on the underlying frame of the display item,
   * and each subsequent merged frame if no layer is found for the underlying
   * frame.
   */
  Layer* GetOldLayerFor(nsDisplayItem* aItem, 
                        nsDisplayItemGeometry** aOldGeometry = nullptr, 
                        DisplayItemClip** aOldClip = nullptr,
                        nsTArray<nsIFrame*>* aChangedFrames = nullptr,
                        bool *aIsInvalid = nullptr);

  static Layer* GetDebugOldLayerFor(nsIFrame* aFrame, uint32_t aDisplayItemKey);

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

  class DisplayItemData;
  typedef void (*DisplayItemDataCallback)(nsIFrame *aFrame, DisplayItemData* aItem);

  static void IterateRetainedDataFor(nsIFrame* aFrame, DisplayItemDataCallback aCallback);

  /**
   * Save transform that was in aLayer when we last painted, and the position
   * of the active scrolled root frame. It must be an integer
   * translation.
   */
  void SaveLastPaintOffset(ThebesLayer* aLayer);
  /**
   * Get the translation transform that was in aLayer when we last painted. It's either
   * the transform saved by SaveLastPaintTransform, or else the transform
   * that's currently in the layer (which must be an integer translation).
   */
  nsIntPoint GetLastPaintOffset(ThebesLayer* aLayer);

  /**
   * Return the resolution at which we expect to render aFrame's contents,
   * assuming they are being painted to retained layers. This takes into account
   * the resolution the contents of the ContainerLayer containing aFrame are
   * being rendered at, as well as any currently-inactive transforms between
   * aFrame and that container layer.
   */
  static gfxSize GetThebesLayerScaleForFrame(nsIFrame* aFrame);

  /**
   * Stores a Layer as the dedicated layer in the DisplayItemData for a given frame/key pair.
   *
   * Used when we optimize a ThebesLayer into an ImageLayer and want to retroactively update the 
   * DisplayItemData so we can retrieve the layer from within layout.
   */
  void StoreOptimizedLayerForFrame(nsDisplayItem* aItem, Layer* aLayer);
  
  NS_DECLARE_FRAME_PROPERTY_WITH_FRAME_IN_DTOR(LayerManagerDataProperty,
                                               RemoveFrameFromLayerManager)

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
  class DisplayItemData MOZ_FINAL {
  public:
    friend class FrameLayerBuilder;

    uint32_t GetDisplayItemKey() { return mDisplayItemKey; }
    Layer* GetLayer() { return mLayer; }
    void Invalidate() { mIsInvalid = true; }

  private:
    DisplayItemData(LayerManagerData* aParent, uint32_t aKey, Layer* aLayer, LayerState aLayerState, uint32_t aGeneration);
    DisplayItemData(DisplayItemData &toCopy);

    /**
     * Removes any references to this object from frames
     * in mFrameList.
     */
    ~DisplayItemData();

    NS_INLINE_DECL_REFCOUNTING(DisplayItemData)


    /**
     * Associates this DisplayItemData with a frame, and adds it
     * to the LayerManagerDataProperty list on the frame.
     */
    void AddFrame(nsIFrame* aFrame);
    void RemoveFrame(nsIFrame* aFrame);
    void GetFrameListChanges(nsDisplayItem* aOther, nsTArray<nsIFrame*>& aOut);

    /**
     * Updates the contents of this item to a new set of data, instead of allocating a new
     * object.
     * Set the passed in parameters, and clears the opt layer, inactive manager, geometry
     * and clip.
     * Parent, frame list and display item key are assumed to be the same.
     */
    void UpdateContents(Layer* aLayer, LayerState aState,
                        uint32_t aContainerLayerGeneration, nsDisplayItem* aItem = nullptr);

    LayerManagerData* mParent;
    nsRefPtr<Layer> mLayer;
    nsRefPtr<Layer> mOptLayer;
    nsRefPtr<BasicLayerManager> mInactiveManager;
    nsAutoTArray<nsIFrame*, 1> mFrameList;
    nsAutoPtr<nsDisplayItemGeometry> mGeometry;
    DisplayItemClip mClip;
    uint32_t        mDisplayItemKey;
    uint32_t        mContainerLayerGeneration;
    LayerState      mLayerState;

    /**
     * Used to track if data currently stored in mFramesWithLayers (from an existing
     * paint) has been updated in the current paint.
     */
    bool            mUsed;
    bool            mIsInvalid;
  };

protected:

  friend class LayerManagerData;

  static void RemoveFrameFromLayerManager(nsIFrame* aFrame, void* aPropertyValue);

  /**
   * Given a frame and a display item key that uniquely identifies a
   * display item for the frame, find the layer that was last used to
   * render that display item. Returns null if there is no such layer.
   * This could be a dedicated layer for the display item, or a ThebesLayer
   * that renders many display items.
   */
  DisplayItemData* GetOldLayerForFrame(nsIFrame* aFrame, uint32_t aDisplayItemKey);

  /**
   * Stores DisplayItemData associated with aFrame, stores the data in
   * mNewDisplayItemData.
   */
  DisplayItemData* StoreDataForFrame(nsDisplayItem* aItem, Layer* aLayer, LayerState aState);
  void StoreDataForFrame(nsIFrame* aFrame,
                         uint32_t aDisplayItemKey,
                         Layer* aLayer,
                         LayerState aState);

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

  static PLDHashOperator DumpDisplayItemDataForFrame(nsRefPtrHashKey<DisplayItemData>* aEntry,
                                                     void* aClosure);
  /**
   * We store one of these for each display item associated with a
   * ThebesLayer, in a hashtable that maps each ThebesLayer to an array
   * of ClippedDisplayItems. (ThebesLayerItemsEntry is the hash entry
   * for that hashtable.)
   * These are only stored during the paint process, so that the
   * DrawThebesLayer callback can figure out which items to draw for the
   * ThebesLayer.
   */
  struct ClippedDisplayItem {
    ClippedDisplayItem(nsDisplayItem* aItem, uint32_t aGeneration)
      : mItem(aItem), mContainerLayerGeneration(aGeneration)
    {
    }

    ~ClippedDisplayItem();

    nsDisplayItem* mItem;

    /**
     * If the display item is being rendered as an inactive
     * layer, then this stores the layer manager being
     * used for the inactive transaction.
     */
    nsRefPtr<LayerManager> mInactiveLayerManager;

    uint32_t mContainerLayerGeneration;
  };

  static void RecomputeVisibilityForItems(nsTArray<ClippedDisplayItem>& aItems,
                                          nsDisplayListBuilder* aBuilder,
                                          const nsIntRegion& aRegionToDraw,
                                          const nsIntPoint& aOffset,
                                          int32_t aAppUnitsPerDevPixel,
                                          float aXScale,
                                          float aYScale);

  void PaintItems(nsTArray<ClippedDisplayItem>& aItems,
                  const nsIntRect& aRect,
                  gfxContext* aContext,
                  nsRenderingContext* aRC,
                  nsDisplayListBuilder* aBuilder,
                  nsPresContext* aPresContext,
                  const nsIntPoint& aOffset,
                  float aXScale, float aYScale,
                  int32_t aCommonClipCount);

  /**
   * We accumulate ClippedDisplayItem elements in a hashtable during
   * the paint process. This is the hashentry for that hashtable.
   */
public:
  class ThebesLayerItemsEntry : public nsPtrHashKey<ThebesLayer> {
  public:
    ThebesLayerItemsEntry(const ThebesLayer *key) :
        nsPtrHashKey<ThebesLayer>(key), mContainerLayerFrame(nullptr),
        mContainerLayerGeneration(0),
        mHasExplicitLastPaintOffset(false), mCommonClipCount(0) {}
    ThebesLayerItemsEntry(const ThebesLayerItemsEntry &toCopy) :
      nsPtrHashKey<ThebesLayer>(toCopy.mKey), mItems(toCopy.mItems)
    {
      NS_ERROR("Should never be called, since we ALLOW_MEMMOVE");
    }

    nsTArray<ClippedDisplayItem> mItems;
    nsIFrame* mContainerLayerFrame;
    // The translation set on this ThebesLayer before we started updating the
    // layer tree.
    nsIntPoint mLastPaintOffset;
    uint32_t mContainerLayerGeneration;
    bool mHasExplicitLastPaintOffset;
    /**
      * The first mCommonClipCount rounded rectangle clips are identical for
      * all items in the layer. Computed in ThebesLayerData.
      */
    uint32_t mCommonClipCount;

    enum { ALLOW_MEMMOVE = true };
  };

  /**
   * Get the ThebesLayerItemsEntry object associated with aLayer in this
   * FrameLayerBuilder
   */
  ThebesLayerItemsEntry* GetThebesLayerItemsEntry(ThebesLayer* aLayer)
  {
    return mThebesLayerItems.GetEntry(aLayer);
  }

  ThebesLayerData* GetContainingThebesLayerData()
  {
    return mContainingThebesLayer;
  }

  /**
   * Attempt to build the most compressed layer tree possible, even if it means
   * throwing away existing retained buffers.
   */
  void SetLayerTreeCompressionMode() { mInLayerTreeCompressionMode = true; }
  bool CheckInLayerTreeCompressionMode();

protected:
  void RemoveThebesItemsAndOwnerDataForLayerSubtree(Layer* aLayer,
                                                    bool aRemoveThebesItems,
                                                    bool aRemoveOwnerData);

  static PLDHashOperator ProcessRemovedDisplayItems(nsRefPtrHashKey<DisplayItemData>* aEntry,
                                                    void* aUserArg);
  static PLDHashOperator RestoreDisplayItemData(nsRefPtrHashKey<DisplayItemData>* aEntry,
                                                void *aUserArg);

  static PLDHashOperator RestoreThebesLayerItemEntries(ThebesLayerItemsEntry* aEntry,
                                                       void *aUserArg);

  /**
   * Returns true if the DOM has been modified since we started painting,
   * in which case we should bail out and not paint anymore. This should
   * never happen, but plugins can trigger it in some cases.
   */
  bool CheckDOMModified();

  /**
   * The layer manager belonging to the widget that is being retained
   * across paints.
   */
  LayerManager*                       mRetainingManager;
  /**
   * The root prescontext for the display list builder reference frame
   */
  nsRefPtr<nsRootPresContext>         mRootPresContext;

  /**
   * The display list builder being used.
   */
  nsDisplayListBuilder*               mDisplayListBuilder;
  /**
   * A map from ThebesLayers to the list of display items (plus
   * clipping data) to be rendered in the layer.
   */
  nsTHashtable<ThebesLayerItemsEntry> mThebesLayerItems;

  /**
   * When building layers for an inactive layer, this is where the
   * inactive layer will be placed.
   */
  ThebesLayerData*                    mContainingThebesLayer;

  /**
   * Saved generation counter so we can detect DOM changes.
   */
  uint32_t                            mInitialDOMGeneration;
  /**
   * Set to true if we have detected and reported DOM modification during
   * the current paint.
   */
  bool                                mDetectedDOMModification;
  /**
   * Indicates that the entire layer tree should be rerendered
   * during this paint.
   */
  bool                                mInvalidateAllLayers;

  bool                                mInLayerTreeCompressionMode;

  uint32_t                            mContainerLayerGeneration;
  uint32_t                            mMaxContainerLayerGeneration;
};

}

#endif /* FRAMELAYERBUILDER_H_ */
