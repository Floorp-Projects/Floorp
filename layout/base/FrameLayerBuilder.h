/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Portions created by the Initial Developer are Copyright (C) 2010
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

#ifndef FRAMELAYERBUILDER_H_
#define FRAMELAYERBUILDER_H_

#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"
#include "nsRegion.h"
#include "nsIFrame.h"

class nsDisplayListBuilder;
class nsDisplayList;
class nsDisplayItem;
class gfxContext;

namespace mozilla {

namespace layers {
class Layer;
class ThebesLayer;
class LayerManager;
}

/**
 * The FrameLayerBuilder belongs to an nsDisplayListBuilder and is
 * responsible for converting display lists into layer trees.
 * 
 * The most important API in this class is BuildContainerLayerFor. This
 * method takes a display list as input and constructs a ContainerLayer
 * with child layers that render the contents of the display list. It
 * also updates userdata for the retained layer manager, and
 * DisplayItemDataProperty data for frames, to record the relationship
 * between frames and layers.
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
 * corresponds to the (pixel-snapped) top-left of the aActiveScrolledRoot.
 * It sets up ContainerLayers so that 0,0 in the container layer
 * corresponds to the snapped top-left of the display list reference frame.
 */
class FrameLayerBuilder {
public:
  typedef layers::Layer Layer; 
  typedef layers::ThebesLayer ThebesLayer;
  typedef layers::LayerManager LayerManager;

  FrameLayerBuilder() :
    mRetainingManager(nsnull),
    mInvalidateAllThebesContent(PR_FALSE),
    mInvalidateAllLayers(PR_FALSE)
  {
    mNewDisplayItemData.Init();
    mThebesLayerItems.Init();
  }

  /**
   * Call this to register a layer tree which was retained since the last
   * paint.
   */
  void BeginUpdatingRetainedLayers(LayerManager* aManager);

  /**
   * Call this whenever we end a transaction on aManager. If aManager
   * is not the retained layer manager then it must be a temporary layer
   * manager that will not be used again.
   */
  void DidEndTransaction(LayerManager* aManager);

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
   * caller's responsibility to add any clip rect and set the visible
   * region.
   */
  already_AddRefed<Layer> BuildContainerLayerFor(nsDisplayListBuilder* aBuilder,
                                                 LayerManager* aManager,
                                                 nsIFrame* aContainerFrame,
                                                 nsDisplayItem* aContainerItem,
                                                 const nsDisplayList& aChildren);

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
                         LayerManager* aManager,
                         nsDisplayItem* aItem);

  /**
   * Call this during invalidation if aFrame has
   * the NS_FRAME_HAS_CONTAINER_LAYER state bit. Only the nearest
   * ancestor frame of the damaged frame that has
   * NS_FRAME_HAS_CONTAINER_LAYER needs to be invalidated this way.
   */
  static void InvalidateThebesLayerContents(nsIFrame* aFrame,
                                            const nsRect& aRect);

  /**
   * Call this to force *all* retained layer contents to be discarded at
   * the next paint.
   */
  static void InvalidateAllThebesLayerContents(LayerManager* aManager);

  /**
   * Call this to force all retained layers to be discarded and recreated at
   * the next paint.
   */
  static void InvalidateAllLayers(LayerManager* aManager);
  
  /**
   * This callback must be provided to EndTransaction. The callback data
   * must be the nsDisplayListBuilder containing this FrameLayerBuilder.
   */
  static void DrawThebesLayer(ThebesLayer* aLayer,
                              gfxContext* aContext,
                              const nsIntRegion& aRegionToDraw,
                              const nsIntRegion& aRegionToInvalidate,
                              void* aCallbackData);

#ifdef DEBUG
  /**
   * Dumps aManager's layer tree to stderr.
   */
  static void DumpLayerTree(LayerManager* aManager);

  /**
   * Dumps this FrameLayerBuilder's retained layer manager's retained
   * layer tree to stderr.
   */
  void DumpRetainedLayerTree();
#endif

  /******* PRIVATE METHODS to FrameLayerBuilder.cpp ********/
  /* These are only in the public section because they need
   * to be called by file-scope helper functions in FrameLayerBuilder.cpp.
   */
  
  /**
   * Record aItem as a display item that is rendered by aLayer.
   */
  void AddLayerDisplayItem(Layer* aLayer, nsDisplayItem* aItem);

  /**
   * Record aItem as a display item that is rendered by the ThebesLayer
   * aLayer, with aClipRect, where aContainerLayerFrame is the frame
   * for the container layer this ThebesItem belongs to.
   * aItem must have an underlying frame.
   */
  void AddThebesDisplayItem(ThebesLayer* aLayer, nsDisplayItem* aItem,
                            const nsRect* aClipRect,
                            nsIFrame* aContainerLayerFrame);

  /**
   * Given a frame and a display item key that uniquely identifies a
   * display item for the frame, find the layer that was last used to
   * render that display item. Returns null if there is no such layer.
   * This could be a dedicated layer for the display item, or a ThebesLayer
   * that renders many display items.
   */
  Layer* GetOldLayerFor(nsIFrame* aFrame, PRUint32 aDisplayItemKey);

protected:
  /**
   * We store an array of these for each frame that is associated with
   * one or more retained layers. Each DisplayItemData records the layer
   * used to render one of the frame's display items.
   */
  class DisplayItemData {
  public:
    DisplayItemData(Layer* aLayer, PRUint32 aKey)
      : mLayer(aLayer), mDisplayItemKey(aKey) {}

    nsRefPtr<Layer> mLayer;
    PRUint32        mDisplayItemKey;
  };

  static void InternalDestroyDisplayItemData(nsIFrame* aFrame,
                                             void* aPropertyValue,
                                             PRBool aRemoveFromFramesWithLayers);
  static void DestroyDisplayItemData(nsIFrame* aFrame, void* aPropertyValue);

  /**
   * For DisplayItemDataProperty, the property value *is* an
   * nsTArray<DisplayItemData>, not a pointer to an array. This works
   * because sizeof(nsTArray<T>) == sizeof(void*).
   */
  NS_DECLARE_FRAME_PROPERTY_WITH_FRAME_IN_DTOR(DisplayItemDataProperty,
                                               DestroyDisplayItemData)

  /**
   * We accumulate DisplayItemData elements in a hashtable during
   * the paint process, and store them in the frame property only when
   * paint is complete. This is the hashentry for that hashtable.
   */
  class DisplayItemDataEntry : public nsPtrHashKey<nsIFrame> {
  public:
    DisplayItemDataEntry(const nsIFrame *key) : nsPtrHashKey<nsIFrame>(key) {}
    DisplayItemDataEntry(const DisplayItemDataEntry &toCopy) :
      nsPtrHashKey<nsIFrame>(toCopy.mKey), mData(toCopy.mData)
    {
      NS_ERROR("Should never be called, since we ALLOW_MEMMOVE");
    }

    PRBool HasContainerLayer();

    nsTArray<DisplayItemData> mData;

    enum { ALLOW_MEMMOVE = PR_TRUE };
  };

  /**
   * We store one of these for each display item associated with a
   * ThebesLayer, in a hashtable that maps each ThebesLayer to an array
   * of ClippedDisplayItems. (ThebesLayerItemsEntry is the hash entry
   * for that hashtable.)
   * These are only stored during the paint process, so that the
   * DrawThebesLayer callback can figure out which items to draw for the
   * ThebesLayer.
   * mItem always has an underlying frame.
   */
  struct ClippedDisplayItem {
    ClippedDisplayItem(nsDisplayItem* aItem, const nsRect* aClipRect)
      : mItem(aItem), mHasClipRect(aClipRect != nsnull)
    {
      if (mHasClipRect) {
        mClipRect = *aClipRect;
      }
    }

    nsDisplayItem* mItem;
    nsRect         mClipRect;
    PRPackedBool   mHasClipRect;
  };

  /**
   * We accumulate ClippedDisplayItem elements in a hashtable during
   * the paint process. This is the hashentry for that hashtable.
   */
  class ThebesLayerItemsEntry : public nsPtrHashKey<ThebesLayer> {
  public:
    ThebesLayerItemsEntry(const ThebesLayer *key) : nsPtrHashKey<ThebesLayer>(key) {}
    ThebesLayerItemsEntry(const ThebesLayerItemsEntry &toCopy) :
      nsPtrHashKey<ThebesLayer>(toCopy.mKey), mItems(toCopy.mItems)
    {
      NS_ERROR("Should never be called, since we ALLOW_MEMMOVE");
    }

    nsTArray<ClippedDisplayItem> mItems;
    nsIFrame* mContainerLayerFrame;

    enum { ALLOW_MEMMOVE = PR_TRUE };
  };

  void RemoveThebesItemsForLayerSubtree(Layer* aLayer);

  static PLDHashOperator UpdateDisplayItemDataForFrame(nsPtrHashKey<nsIFrame>* aEntry,
                                                       void* aUserArg);
  static PLDHashOperator StoreNewDisplayItemData(DisplayItemDataEntry* aEntry,
                                                 void* aUserArg);

  /**
   * The layer manager belonging to the widget that is being retained
   * across paints.
   */
  LayerManager*                       mRetainingManager;
  /**
   * A map from frames to a list of (display item key, layer) pairs that
   * describes what layers various parts of the frame are assigned to.
   */
  nsTHashtable<DisplayItemDataEntry>  mNewDisplayItemData;
  /**
   * A map from ThebesLayers to the list of display items (plus
   * clipping data) to be rendered in the layer.
   */
  nsTHashtable<ThebesLayerItemsEntry> mThebesLayerItems;
  /**
   * Indicates that the contents of all ThebesLayers should be rerendered
   * during this paint.
   */
  PRPackedBool                        mInvalidateAllThebesContent;
  /**
   * Indicates that the entire layer tree should be rerendered
   * during this paint.
   */
  PRPackedBool                        mInvalidateAllLayers;
};

}

#endif /* FRAMELAYERBUILDER_H_ */
