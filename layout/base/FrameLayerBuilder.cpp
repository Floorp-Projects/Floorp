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

#include "FrameLayerBuilder.h"

#include "nsDisplayList.h"
#include "nsPresContext.h"
#include "nsLayoutUtils.h"

using namespace mozilla::layers;

namespace mozilla {

namespace {

/**
 * This class iterates through a display list tree, descending only into
 * nsDisplayClip items, and returns each display item encountered during
 * such iteration. Along with each item we also return the clip rect
 * accumulated for the item.
 */
class ClippedItemIterator {
public:
  ClippedItemIterator(const nsDisplayList* aList)
  {
    DescendIntoList(aList, nsnull, nsnull);
    AdvanceToItem();
  }
  PRBool IsDone()
  {
    return mStack.IsEmpty();
  }
  void Next()
  {
    State* top = StackTop();
    top->mItem = top->mItem->GetAbove();
    AdvanceToItem();
  }
  // Returns null if there is no clipping affecting the item. The
  // clip rect is in device pixels
  const gfxRect* GetEffectiveClipRect()
  {
    State* top = StackTop();
    return top->mHasClipRect ? &top->mEffectiveClipRect : nsnull;
  }
  nsDisplayItem* Item()
  {
    return StackTop()->mItem;
  }

private:
  // We maintain a stack of state objects. Each State object represents
  // where we're up to in the iteration of a list.
  struct State {
    // The current item we're at in the list
    nsDisplayItem* mItem;
    // The effective clip rect applying to all the items in this list
    gfxRect mEffectiveClipRect;
    PRPackedBool mHasClipRect;
  };

  State* StackTop()
  {
    return &mStack[mStack.Length() - 1];
  }
  void DescendIntoList(const nsDisplayList* aList,
                       nsPresContext* aPresContext,
                       const nsRect* aClipRect)
  {
    State* state = mStack.AppendElement();
    if (!state)
      return;
    if (mStack.Length() >= 2) {
      *state = mStack[mStack.Length() - 2];
    } else {
      state->mHasClipRect = PR_FALSE;
    }
    state->mItem = aList->GetBottom();
    if (aClipRect) {
      gfxRect r(aClipRect->x, aClipRect->y, aClipRect->width, aClipRect->height);
      r.ScaleInverse(aPresContext->AppUnitsPerDevPixel());
      if (state->mHasClipRect) {
        state->mEffectiveClipRect = state->mEffectiveClipRect.Intersect(r);
      } else {
        state->mEffectiveClipRect = r;
        state->mHasClipRect = PR_TRUE;
      }
    }
  }
  // Advances to an item that the iterator should return.
  void AdvanceToItem()
  {
    while (!mStack.IsEmpty()) {
      State* top = StackTop();
      if (!top->mItem) {
        mStack.SetLength(mStack.Length() - 1);
        if (!mStack.IsEmpty()) {
          top = StackTop();
          top->mItem = top->mItem->GetAbove();
        }
        continue;
      }
      if (top->mItem->GetType() != nsDisplayItem::TYPE_CLIP)
        return;
      nsDisplayClip* clipItem = static_cast<nsDisplayClip*>(top->mItem);
      nsRect clip = clipItem->GetClipRect();
      DescendIntoList(clipItem->GetList(),
                      clipItem->GetClippingFrame()->PresContext(),
                      &clip);
    }
  }

  nsAutoTArray<State,10> mStack;
};

/**
 * This class represents a sublist of consecutive items in an nsDisplayList.
 * The first item in the sublist is mStartItem and the last item
 * is the item before mEndItem.
 * 
 * These sublists are themselves organized into a linked list of all
 * the ItemGroups associated with a given layer, via mNextItemsForLayer.
 * This list will have more than one element if the display items in a layer
 * come from different nsDisplayLists, or if they come from the same
 * nsDisplayList but they aren't consecutive in that list.
 * 
 * These objects are allocated from the nsDisplayListBuilder arena.
 */
struct ItemGroup {
  // If null, then the item group is empty.
  nsDisplayItem* mStartItem;
  nsDisplayItem* mEndItem;
  ItemGroup* mNextItemsForLayer;
  // The clipping (if any) that needs to be applied to all these items.
  gfxRect mClipRect;
  PRPackedBool mHasClipRect;

  ItemGroup() : mStartItem(nsnull), mEndItem(nsnull),
    mNextItemsForLayer(nsnull), mHasClipRect(PR_FALSE) {}

  void* operator new(size_t aSize,
                     nsDisplayListBuilder* aBuilder) CPP_THROW_NEW {
    return aBuilder->Allocate(aSize);
  }
};

/**
 * This class represents a layer and the display item(s) it
 * will render. The items are stored in a linked list of ItemGroups.
 */
struct LayerItems {
  nsRefPtr<Layer> mLayer;
  // equal to mLayer, or null if mLayer is not a ThebesLayer
  ThebesLayer* mThebesLayer;
  ItemGroup* mItems;
  // The bounds of the visible region for this layer, in device pixels
  nsIntRect mVisibleRect;

  LayerItems(ItemGroup* aItems) :
    mThebesLayer(nsnull), mItems(aItems)
  {
  }

  void* operator new(size_t aSize,
                     nsDisplayListBuilder* aBuilder) CPP_THROW_NEW {
    return aBuilder->Allocate(aSize);
  }
};

/**
 * Given a (possibly clipped) display item in aItem, try to append it to
 * the items in aGroup. If aItem is the next item in the sublist in
 * aGroup, and the clipping matches, we can just update aGroup in-place,
 * otherwise we'll allocate a new ItemGroup, add it to the linked list,
 * and put aItem in the new ItemGroup. We return the ItemGroup into which
 * aItem was inserted.
 */
static ItemGroup*
AddToItemGroup(nsDisplayListBuilder* aBuilder,
               ItemGroup* aGroup, nsDisplayItem* aItem,
               const gfxRect* aClipRect)
{
  NS_ASSERTION(!aGroup->mNextItemsForLayer,
               "aGroup must be the last group in the chain");

  if (!aGroup->mStartItem) {
    aGroup->mStartItem = aItem;
    aGroup->mEndItem = aItem->GetAbove();
    aGroup->mHasClipRect = aClipRect != nsnull;
    if (aClipRect) {
      aGroup->mClipRect = *aClipRect;
    }
    return aGroup;
  }

  if (aGroup->mEndItem == aItem &&
      (aGroup->mHasClipRect
       ? (aClipRect && aGroup->mClipRect == *aClipRect)
       : !aClipRect))  {
    aGroup->mEndItem = aItem->GetAbove();
    return aGroup;
  }

  ItemGroup* itemGroup = new (aBuilder) ItemGroup();
  if (!itemGroup)
    return aGroup;
  aGroup->mNextItemsForLayer = itemGroup;
  return AddToItemGroup(aBuilder, itemGroup, aItem, aClipRect);
}

/**
 * Create an empty Thebes layer, with an empty ItemGroup associated with
 * it, and append it to aLayers.
 */
static ItemGroup*
CreateEmptyThebesLayer(nsDisplayListBuilder* aBuilder,
                       LayerManager* aManager,
                       nsTArray<LayerItems*>* aLayers)
{
  ItemGroup* itemGroup = new (aBuilder) ItemGroup();
  if (!itemGroup)
    return nsnull;
  nsRefPtr<ThebesLayer> thebesLayer = aManager->CreateThebesLayer();
  if (!thebesLayer)
    return nsnull;
  LayerItems* layerItems = new (aBuilder) LayerItems(itemGroup);
  aLayers->AppendElement(layerItems);
  thebesLayer->SetUserData(layerItems);
  layerItems->mThebesLayer = thebesLayer;
  layerItems->mLayer = thebesLayer.forget();
  return itemGroup;
}

static PRBool
IsAllUniform(nsDisplayListBuilder* aBuilder, ItemGroup* aGroup,
             nscolor* aColor)
{
  nsRect visibleRect = aGroup->mStartItem->GetVisibleRect();
  nscolor finalColor = NS_RGBA(0,0,0,0);
  for (ItemGroup* group = aGroup; group;
       group = group->mNextItemsForLayer) {
    for (nsDisplayItem* item = group->mStartItem; item != group->mEndItem;
         item = item->GetAbove()) {
      nscolor color;
      if (visibleRect != item->GetVisibleRect())
        return PR_FALSE;
      if (!item->IsUniform(aBuilder, &color))
        return PR_FALSE;
      finalColor = NS_ComposeColors(finalColor, color);
    }
  }
  *aColor = finalColor;
  return PR_TRUE;
}

/**
 * This is the heart of layout's integration with layers. We
 * use a ClippedItemIterator to iterate through descendant display
 * items. Each item either has its own layer or is assigned to a
 * ThebesLayer. We create ThebesLayers as necessary, although we try
 * to put items in the bottom-most ThebesLayer because that is most
 * likely to be able to render with an opaque background, which will often
 * be required for subpixel text antialiasing to work.
 */
static void BuildLayers(nsDisplayListBuilder* aBuilder,
                        const nsDisplayList& aList,
                        LayerManager* aManager,
                        nsTArray<LayerItems*>* aLayers)
{
  NS_ASSERTION(aLayers->IsEmpty(), "aLayers must be initially empty");

  // Create "bottom" Thebes layer. We'll try to put as much content
  // as possible in this layer because if the container is filled with
  // opaque content, this bottommost layer can also be treated as opaque,
  // which means content in this layer can have subpixel AA.
  // firstThebesLayerItems always points to the last ItemGroup for the
  // first Thebes layer.
  ItemGroup* firstThebesLayerItems =
    CreateEmptyThebesLayer(aBuilder, aManager, aLayers);
  if (!firstThebesLayerItems)
    return;
  // lastThebesLayerItems always points to the last ItemGroup for the
  // topmost layer, if it's a ThebesLayer. If the top layer is not a
  // Thebes layer, this is null.
  ItemGroup* lastThebesLayerItems = firstThebesLayerItems;
  // This region contains the bounds of all the content that is above
  // the first Thebes layer.
  nsRegion areaAboveFirstThebesLayer;

  for (ClippedItemIterator iter(&aList); !iter.IsDone(); iter.Next()) {
    nsDisplayItem* item = iter.Item();
    const gfxRect* clipRect = iter.GetEffectiveClipRect();
    // Ask the item if it manages its own layer
    nsRefPtr<Layer> layer = item->BuildLayer(aBuilder, aManager);
    nsRect bounds = item->GetBounds(aBuilder);
    // We set layerItems to point to the LayerItems object where the
    // item ends up.
    LayerItems* layerItems = nsnull;
    if (layer) {
      // item has a dedicated layer. Add it to the list, with an ItemGroup
      // covering this item only.
      ItemGroup* itemGroup = new (aBuilder) ItemGroup();
      if (itemGroup) {
        AddToItemGroup(aBuilder, itemGroup, item, clipRect);
        layerItems = new (aBuilder) LayerItems(itemGroup);
        aLayers->AppendElement(layerItems);
        if (layerItems) {
          if (itemGroup->mHasClipRect) {
            gfxRect r = itemGroup->mClipRect;
            r.Round();
            nsIntRect intRect(r.X(), r.Y(), r.Width(), r.Height());
            layer->IntersectClipRect(intRect);
          }
          layerItems->mLayer = layer.forget();
        }
      }
      // This item is above the first Thebes layer.
      areaAboveFirstThebesLayer.Or(areaAboveFirstThebesLayer, bounds);
      lastThebesLayerItems = nsnull;
    } else {
      // No dedicated layer. Add it to a Thebes layer. First try to add
      // it to the first Thebes layer, which we can do if there's no
      // content between the first Thebes layer and our display item that
      // overlaps our display item.
      if (!areaAboveFirstThebesLayer.Intersects(bounds)) {
        firstThebesLayerItems =
          AddToItemGroup(aBuilder, firstThebesLayerItems, item, clipRect);
        layerItems = aLayers->ElementAt(0);
      } else if (lastThebesLayerItems) {
        // Try to add to the last Thebes layer
        lastThebesLayerItems =
          AddToItemGroup(aBuilder, lastThebesLayerItems, item, clipRect);
        // This item is above the first Thebes layer.
        areaAboveFirstThebesLayer.Or(areaAboveFirstThebesLayer, bounds);
        layerItems = aLayers->ElementAt(aLayers->Length() - 1);
      } else {
        // Create a new Thebes layer
        ItemGroup* itemGroup =
          CreateEmptyThebesLayer(aBuilder, aManager, aLayers);
        if (itemGroup) {
          lastThebesLayerItems =
            AddToItemGroup(aBuilder, itemGroup, item, clipRect);
          NS_ASSERTION(lastThebesLayerItems == itemGroup,
                       "AddToItemGroup shouldn't create a new group if the "
                       "initial group is empty");
          // This item is above the first Thebes layer.
          areaAboveFirstThebesLayer.Or(areaAboveFirstThebesLayer, bounds);
          layerItems = aLayers->ElementAt(aLayers->Length() - 1);
        }
      }
    }

    if (layerItems) {
      // Update the visible region of the layer to account for the new
      // item
      nscoord appUnitsPerDevPixel =
        item->GetUnderlyingFrame()->PresContext()->AppUnitsPerDevPixel();
      layerItems->mVisibleRect.UnionRect(layerItems->mVisibleRect,
        item->GetVisibleRect().ToNearestPixels(appUnitsPerDevPixel));
    }
  }

  if (!firstThebesLayerItems->mStartItem) {
    // The first Thebes layer has nothing in it. Delete the layer.
    // Ensure layer is released.
    aLayers->ElementAt(0)->mLayer = nsnull;
    aLayers->RemoveElementAt(0);
  }

  for (PRUint32 i = 0; i < aLayers->Length(); ++i) {
    LayerItems* layerItems = aLayers->ElementAt(i);

    nscolor color;
    // Only convert layers with identity transform to ColorLayers, for now.
    // This simplifies the code to set the clip region.
    if (layerItems->mThebesLayer &&
        IsAllUniform(aBuilder, layerItems->mItems, &color) &&
        layerItems->mLayer->GetTransform().IsIdentity()) {
      nsRefPtr<ColorLayer> layer = aManager->CreateColorLayer();
      layer->SetClipRect(layerItems->mThebesLayer->GetClipRect());
      // Clip to mVisibleRect to ensure only the pixels we want are filled.
      layer->IntersectClipRect(layerItems->mVisibleRect);
      layer->SetColor(gfxRGBA(color));
      layerItems->mLayer = layer.forget();
      layerItems->mThebesLayer = nsnull;
    }

    gfxMatrix transform;
    nsIntRect visibleRect = layerItems->mVisibleRect;
    if (layerItems->mLayer->GetTransform().Is2D(&transform)) {
      // if 'transform' is not invertible, then nothing will be displayed
      // for the layer, so it doesn't really matter what we do here
      transform.Invert();
      gfxRect layerVisible = transform.TransformBounds(
          gfxRect(visibleRect.x, visibleRect.y, visibleRect.width, visibleRect.height));
      layerVisible.RoundOut();
      if (NS_FAILED(nsLayoutUtils::GfxRectToIntRect(layerVisible, &visibleRect))) {
        NS_ERROR("Visible rect transformed out of bounds");
      }
    } else {
      NS_ERROR("Only 2D transformations currently supported");
    }
    layerItems->mLayer->SetVisibleRegion(nsIntRegion(visibleRect));
  }
}

} // anonymous namespace

already_AddRefed<Layer>
FrameLayerBuilder::GetContainerLayerFor(nsDisplayListBuilder* aBuilder,
                                        LayerManager* aManager,
                                        nsDisplayItem* aContainer,
                                        const nsDisplayList& aChildren)
{
  // If there's only one layer, then in principle we can try to flatten
  // things by returning that layer here. But that adds complexity to
  // retained layer management so we don't do it. Layer backends can
  // flatten internally.
  nsRefPtr<ContainerLayer> container = aManager->CreateContainerLayer();
  if (!container)
    return nsnull;

  nsAutoTArray<LayerItems*,10> layerItems;
  BuildLayers(aBuilder, aChildren, aManager, &layerItems);

  Layer* lastChild = nsnull;
  for (PRUint32 i = 0; i < layerItems.Length(); ++i) {
    Layer* child = layerItems[i]->mLayer;
    container->InsertAfter(child, lastChild);
    lastChild = child;
    // release the layer now because the ItemGroup destructor doesn't run;
    // the container is still holding a reference to it
    layerItems[i]->mLayer = nsnull;
  }
  container->SetIsOpaqueContent(aChildren.IsOpaque());
  nsRefPtr<Layer> layer = container.forget();
  return layer.forget();
}

Layer*
FrameLayerBuilder::GetLeafLayerFor(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   nsDisplayItem* aItem)
{
  // Layers aren't retained yet
  return nsnull;
}

/* static */ void
FrameLayerBuilder::InvalidateThebesLayerContents(nsIFrame* aFrame,
                                                 const nsRect& aRect)
{
  // do nothing; layers aren't retained yet
}

/* static */ void
FrameLayerBuilder::DrawThebesLayer(ThebesLayer* aLayer,
                                   gfxContext* aContext,
                                   const nsIntRegion& aRegionToDraw,
                                   const nsIntRegion& aRegionToInvalidate,
                                   void* aCallbackData)
{
  // For now, we can ignore aRegionToInvalidate since we don't
  // use retained layers.

  LayerItems* layerItems = static_cast<LayerItems*>(aLayer->GetUserData());
  nsDisplayListBuilder* builder =
    static_cast<nsDisplayListBuilder*>(aCallbackData);

  // For now, we'll ignore toDraw and just draw the entire visible
  // area, because the "visible area" is already confined to just the
  // area that needs to be repainted. Later, when we start reusing layers
  // from paint to paint, we'll need to pay attention to toDraw and
  // actually try to avoid drawing stuff that's not in it.

  // Our list may contain content with different prescontexts at
  // different zoom levels. 'rc' contains the nsIRenderingContext
  // used for the previous display item, and lastPresContext is the
  // prescontext for that item. We also cache the clip state for that
  // item.
  nsRefPtr<nsIRenderingContext> rc;
  nsPresContext* lastPresContext = nsnull;
  gfxRect currentClip;
  PRBool setClipRect = PR_FALSE;
  NS_ASSERTION(layerItems->mItems, "No empty layers allowed");
  for (ItemGroup* group = layerItems->mItems; group;
       group = group->mNextItemsForLayer) {
    // If the new desired clip state is different from the current state,
    // update the clip.
    if (setClipRect != group->mHasClipRect ||
        (group->mHasClipRect && group->mClipRect != currentClip)) {
      if (setClipRect) {
        aContext->Restore();
      }
      setClipRect = group->mHasClipRect;
      if (setClipRect) {
        aContext->Save();
        aContext->NewPath();
        aContext->Rectangle(group->mClipRect, PR_TRUE);
        aContext->Clip();
        currentClip = group->mClipRect;
      }
    }
    NS_ASSERTION(group->mStartItem, "No empty groups allowed");
    for (nsDisplayItem* item = group->mStartItem; item != group->mEndItem;
         item = item->GetAbove()) {
      nsPresContext* presContext = item->GetUnderlyingFrame()->PresContext();
      if (presContext != lastPresContext) {
        // Create a new rendering context with the right
        // appunits-per-dev-pixel.
        nsresult rv =
          presContext->DeviceContext()->CreateRenderingContextInstance(*getter_AddRefs(rc));
        if (NS_FAILED(rv))
          break;
        rc->Init(presContext->DeviceContext(), aContext);
        lastPresContext = presContext;
      }
      item->Paint(builder, rc);
    }
  }
  if (setClipRect) {
    aContext->Restore();
  }
}

} // namespace mozilla
