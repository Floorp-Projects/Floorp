/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFETileElement.h"
#include "mozilla/dom/SVGFETileElementBinding.h"
#include "nsSVGFilterInstance.h"
#include "gfxUtils.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FETile)

namespace mozilla {
namespace dom {

JSObject*
SVGFETileElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return SVGFETileElementBinding::Wrap(aCx, aScope, this);
}

nsSVGElement::StringInfo SVGFETileElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true },
  { &nsGkAtoms::in, kNameSpaceID_None, true }
};

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFETileElement)

already_AddRefed<nsIDOMSVGAnimatedString>
SVGFETileElement::In1()
{
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

void
SVGFETileElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

nsIntRect
SVGFETileElement::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
        const nsSVGFilterInstance& aInstance)
{
  return GetMaxRect();
}

void
SVGFETileElement::ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance)
{
  // Just assume we need the entire source bounding box, so do nothing.
}

nsIntRect
SVGFETileElement::ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
                                    const nsSVGFilterInstance& aInstance)
{
  return GetMaxRect();
}

//----------------------------------------------------------------------
// nsSVGElement methods

/*
 * This function computes the size of partial match on either side of the tile.
 * eg: If we are talking about the X-axis direction, then it computes, the 
 * size of the tile that would be copied to the lesser X-axis side (usually
 * left), the higher X-axis side (usualy right) and the centre.
 * This is needed because often, the tile doesn't exactly align to the target
 * region and is partially copied on the edges. This function computes the
 * dimensions of the partially copied regions in one axis.
 *
 * OUTPUT:
 * aLesserSidePartialMatchSize: The size of the partial match on the lesser
 *                              side of the axis being considered.
 *                              eg: for X-axis, usually left side and
 *                                  for Y-axis, usually top
 * aHigherSidePartialMatchSize: The size of the partial match on the higher
 *                              side of the axis being considered.
 *                              eg: for X-axis, usually right side and
 *                                  for Y-axis, usually bottom
 * aCentreSize: The size of the target area where the tile is copied in full.
 *              This lies between the lesser and higher side partial matches.
 *              (the partially matched areas may be of zero width)
 *
 * INPUT:
 * aLesserTargetExtent: Edge of the target area on the axis being considered
 *                      on the lesser side. (eg: usually left on the X-axis)
 * aTargetSize: Size of the target area on the axis being considered (eg:
 *              usually width for X-axis)
 * aLesserTileExtent: Edge of the tile on the axis being considered on the
 *                    lesser side.
 * aTileSize: Size of the tile on the axis being considered.
 */
static inline void
ComputePartialTileExtents(int32_t *aLesserSidePartialMatchSize,
                          int32_t *aHigherSidePartialMatchSize,
                          int32_t *aCentreSize,
                          int32_t aLesserTargetExtent,
                          int32_t aTargetSize,
                          int32_t aLesserTileExtent,
                          int32_t aTileSize)
{
  int32_t targetExtentMost = aLesserTargetExtent + aTargetSize;
  int32_t tileExtentMost = aLesserTileExtent + aTileSize;

  int32_t lesserSidePartialMatchSize;
  if (aLesserTileExtent < aLesserTargetExtent) {
    lesserSidePartialMatchSize = tileExtentMost - aLesserTargetExtent;
  } else {
    lesserSidePartialMatchSize = (aLesserTileExtent - aLesserTargetExtent) %
                                    aTileSize;
  }

  int32_t higherSidePartialMatchSize;
  if (lesserSidePartialMatchSize > aTargetSize) {
    lesserSidePartialMatchSize = aTargetSize;
    higherSidePartialMatchSize = 0;
  } else if (tileExtentMost > targetExtentMost) {
      higherSidePartialMatchSize = targetExtentMost - aLesserTileExtent;
  } else {
    higherSidePartialMatchSize = (targetExtentMost - tileExtentMost) %
                                    aTileSize;
  }

  if (lesserSidePartialMatchSize + higherSidePartialMatchSize >
        aTargetSize) {
    higherSidePartialMatchSize = aTargetSize - lesserSidePartialMatchSize;
  }

  /*
   * To understand the conditon below, let us consider the X-Axis:
   * Lesser side is left and the Higher side is right.
   * This implies:
   * aTargetSize is rect.width.
   * lesserSidePartialMatchSize would mean leftPartialTileWidth.
   * higherSidePartialMatchSize would mean rightPartialTileWidth.
   *
   * leftPartialTileWidth == rect.width only happens when the tile entirely
   * overlaps with the target area in the X-axis and exceeds its bounds by at
   * least one pixel on the lower X-Axis side.
   *
   * leftPartialTileWidth + rightPartialTileWidth == rect.width only happens
   * when the tile overlaps the target area in such a way that the edge of the
   * tile on the higher X-Axis side cuts through the target area and there is no
   * space for a complete tile in the X-Axis in the target area on either side
   * of that edge. In this scenario, centre will be of zero width and the
   * partial widths on left and right will add up to the width of the rect. In
   * case the tile is bigger than the rect in the X-axis, it will get clipped
   * and remain equal to rect.width.
   *
   * Therefore, those two conditions are separate cases which lead to centre
   * being of zero width.
   *
   * The condition below is the same logic as above expressed independent of
   * the axis in consideration.
   */

  int32_t centreSize;
  if (lesserSidePartialMatchSize == aTargetSize ||
        lesserSidePartialMatchSize + higherSidePartialMatchSize ==
        aTargetSize) {
    centreSize = 0;
  } else {
    centreSize = aTargetSize -
                   (lesserSidePartialMatchSize + higherSidePartialMatchSize);
  }

  *aLesserSidePartialMatchSize = lesserSidePartialMatchSize;
  *aHigherSidePartialMatchSize = higherSidePartialMatchSize;
  *aCentreSize = centreSize;
}

static inline void
TilePixels(uint8_t *aTargetData,
           const uint8_t *aSourceData,
           const nsIntRect &targetRegion,
           const nsIntRect &aTile,
           uint32_t aStride)
{
  if (targetRegion.IsEmpty()) {
    return;
  }

  uint32_t tileRowCopyMemSize = aTile.width * 4;
  uint32_t numTimesToCopyTileRows = targetRegion.width / aTile.width;

  uint8_t *targetFirstRowOffset = aTargetData + 4 * targetRegion.x;
  const uint8_t *tileFirstRowOffset = aSourceData + 4 * aTile.x;

  int32_t tileYOffset = 0;
  for (int32_t targetY = targetRegion.y;
       targetY < targetRegion.YMost();
       ++targetY) {
    uint8_t *targetRowOffset = targetFirstRowOffset + aStride * targetY;
    const uint8_t *tileRowOffset = tileFirstRowOffset +
                                             aStride * (aTile.y + tileYOffset);

    for (uint32_t i = 0; i < numTimesToCopyTileRows; ++i) {
      memcpy(targetRowOffset + i * tileRowCopyMemSize,
             tileRowOffset,
             tileRowCopyMemSize);
    }

    tileYOffset = (tileYOffset + 1) % aTile.height;
  }
}

nsresult
SVGFETileElement::Filter(nsSVGFilterInstance *instance,
                         const nsTArray<const Image*>& aSources,
                         const Image* aTarget,
                         const nsIntRect& rect)
{
  // XXX This code depends on the surface rect containing the filter
  // primitive subregion. ComputeTargetBBox, ComputeNeededSourceBBoxes
  // and ComputeChangeBBox are all pessimal, so that will normally be OK,
  // but nothing clips mFilterPrimitiveSubregion so this should be changed.

  nsIntRect tile;
  bool res = gfxUtils::GfxRectToIntRect(aSources[0]->mFilterPrimitiveSubregion,
                                        &tile);

  NS_ENSURE_TRUE(res, NS_ERROR_FAILURE); // asserts on failure (not 
  if (tile.IsEmpty())
    return NS_OK;

  const nsIntRect &surfaceRect = instance->GetSurfaceRect();
  if (!tile.Intersects(surfaceRect)) {
    // nothing to draw
    return NS_OK;
  }

  // clip tile
  tile = tile.Intersect(surfaceRect);

  // Get it into surface space
  tile -= surfaceRect.TopLeft();

  uint8_t* sourceData = aSources[0]->mImage->Data();
  uint8_t* targetData = aTarget->mImage->Data();
  uint32_t stride = aTarget->mImage->Stride();

  /*
   * priority: left before right before centre
   *             and
   *           top before bottom before centre
   *
   * eg: If we have a target area which is 1.5 times the width of a tile,
   *     then, based on alignment, we get:
   *       'left and right'
   *         or
   *       'left and centre'
   *
   */

  int32_t leftPartialTileWidth;
  int32_t rightPartialTileWidth;
  int32_t centreWidth;
  ComputePartialTileExtents(&leftPartialTileWidth,
                            &rightPartialTileWidth,
                            &centreWidth,
                            rect.x,
                            rect.width,
                            tile.x,
                            tile.width);

  int32_t topPartialTileHeight;
  int32_t bottomPartialTileHeight;
  int32_t centreHeight;
  ComputePartialTileExtents(&topPartialTileHeight,
                            &bottomPartialTileHeight,
                            &centreHeight,
                            rect.y,
                            rect.height,
                            tile.y,
                            tile.height);

  /* We have nine regions of the target area which have to be tiled differetly:
   *
   * Top Left,    Top Middle,    Top Right,
   * Left Middle, Centre,        Right Middle,
   * Bottom Left, Bottom Middle, Bottom Right
   *
   * + Centre is tiled by repeating the tiled image in full.
   * + Top Left, Top Middle and Top Right:
   *     Some of the rows from the top of the tile will be clipped here.
   * + Bottom Left, Bottom Middle and Bottom Right:
   *     Some of the rows from the bottom of the tile will be clipped here.
   * + Top Left, Left Middle and Bottom left:
   *     Some of the columns from the Left of the tile will be clipped here.
   * + Top Right, Right Middle and Bottom Right:
   *     Some of the columns from the right of the tile will be clipped here.
   *
   * If the sizes and positions of the target and tile are such that the tile
   * aligns exactly on any (or all) of the edges, then some (or all) of the
   * regions above (except Centre) will be zero sized.
   */

  nsIntRect targetRects[] = {
    // Top Left
    nsIntRect(rect.x, rect.y, leftPartialTileWidth, topPartialTileHeight),
    // Top Middle
    nsIntRect(rect.x + leftPartialTileWidth,
              rect.y,
              centreWidth,
              topPartialTileHeight),
    // Top Right
    nsIntRect(rect.XMost() - rightPartialTileWidth,
              rect.y,
              rightPartialTileWidth,
              topPartialTileHeight),
    // Left Middle
    nsIntRect(rect.x,
              rect.y + topPartialTileHeight,
              leftPartialTileWidth,
              centreHeight),
    // Centre
    nsIntRect(rect.x + leftPartialTileWidth,
              rect.y + topPartialTileHeight,
              centreWidth,
              centreHeight),
    // Right Middle
    nsIntRect(rect.XMost() - rightPartialTileWidth,
              rect.y + topPartialTileHeight,
              rightPartialTileWidth,
              centreHeight),
    // Bottom Left
    nsIntRect(rect.x,
              rect.YMost() - bottomPartialTileHeight,
              leftPartialTileWidth,
              bottomPartialTileHeight),
    // Bottom Middle
    nsIntRect(rect.x + leftPartialTileWidth,
              rect.YMost() - bottomPartialTileHeight,
              centreWidth,
              bottomPartialTileHeight),
    // Bottom Right
    nsIntRect(rect.XMost() - rightPartialTileWidth,
              rect.YMost() - bottomPartialTileHeight,
              rightPartialTileWidth,
              bottomPartialTileHeight)
  };

  nsIntRect tileRects[] = {
    // Top Left
    nsIntRect(tile.XMost() - leftPartialTileWidth,
              tile.YMost() - topPartialTileHeight,
              leftPartialTileWidth,
              topPartialTileHeight),
    // Top Middle
    nsIntRect(tile.x,
              tile.YMost() - topPartialTileHeight,
              tile.width,
              topPartialTileHeight),
    // Top Right
    nsIntRect(tile.x,
              tile.YMost() - topPartialTileHeight,
              rightPartialTileWidth,
              topPartialTileHeight),
    // Left Middle
    nsIntRect(tile.XMost() - leftPartialTileWidth,
              tile.y,
              leftPartialTileWidth,
              tile.height),
    // Centre
    nsIntRect(tile.x,
              tile.y,
              tile.width,
              tile.height),
    // Right Middle
    nsIntRect(tile.x,
              tile.y,
              rightPartialTileWidth,
              tile.height),
    // Bottom Left
    nsIntRect(tile.XMost() - leftPartialTileWidth,
              tile.y,
              leftPartialTileWidth,
              bottomPartialTileHeight),
    // Bottom Middle
    nsIntRect(tile.x,
              tile.y,
              tile.width,
              bottomPartialTileHeight),
    // Bottom Right
    nsIntRect(tile.x,
              tile.y,
              rightPartialTileWidth,
              bottomPartialTileHeight)
  };

  for (uint32_t i = 0; i < ArrayLength(targetRects); ++i) {
    TilePixels(targetData,
               sourceData,
               targetRects[i],
               tileRects[i],
               stride);
  }

  return NS_OK;
}

bool
SVGFETileElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                            nsIAtom* aAttribute) const
{
  return SVGFETileElementBase::AttributeAffectsRendering(aNameSpaceID,
                                                         aAttribute) ||
           (aNameSpaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::in);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::StringAttributesInfo
SVGFETileElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

} // namespace dom
} // namespace mozilla
