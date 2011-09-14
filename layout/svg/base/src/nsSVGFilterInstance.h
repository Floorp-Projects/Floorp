/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef __NS_SVGFILTERINSTANCE_H__
#define __NS_SVGFILTERINSTANCE_H__

#include "nsIDOMSVGLength.h"
#include "nsIDOMSVGFilters.h"
#include "nsRect.h"
#include "nsIContent.h"
#include "nsAutoPtr.h"
#include "nsSVGFilters.h"
#include "nsSVGNumber2.h"
#include "nsSVGNumberPair.h"

#include "gfxImageSurface.h"

class nsSVGElement;
class nsSVGFilterElement;
class nsSVGFilterPaintCallback;
struct gfxRect;

/**
 * This class performs all filter processing.
 * 
 * We build a graph of the filter image data flow, essentially
 * converting the filter graph to SSA. This lets us easily propagate
 * analysis data (such as bounding-boxes) over the filter primitive graph.
 */
class NS_STACK_CLASS nsSVGFilterInstance
{
public:
  nsSVGFilterInstance(nsIFrame *aTargetFrame,
                      nsSVGFilterPaintCallback *aPaintCallback,
                      nsSVGFilterElement *aFilterElement,
                      const gfxRect &aTargetBBox,
                      const gfxRect& aFilterRect,
                      const nsIntSize& aFilterSpaceSize,
                      const gfxMatrix &aFilterSpaceToDeviceSpaceTransform,
                      const nsIntRect& aDirtyOutputRect,
                      const nsIntRect& aDirtyInputRect,
                      PRUint16 aPrimitiveUnits) :
    mTargetFrame(aTargetFrame),
    mPaintCallback(aPaintCallback),
    mFilterElement(aFilterElement),
    mTargetBBox(aTargetBBox),
    mFilterSpaceToDeviceSpaceTransform(aFilterSpaceToDeviceSpaceTransform),
    mFilterRect(aFilterRect),
    mFilterSpaceSize(aFilterSpaceSize),
    mDirtyOutputRect(aDirtyOutputRect),
    mDirtyInputRect(aDirtyInputRect),
    mSurfaceRect(nsIntPoint(0, 0), aFilterSpaceSize),
    mPrimitiveUnits(aPrimitiveUnits) {
  }
  
  // The area covered by temporary images, in filter space
  void SetSurfaceRect(const nsIntRect& aRect) { mSurfaceRect = aRect; }

  gfxRect GetFilterRect() const { return mFilterRect; }

  const nsIntSize& GetFilterSpaceSize() { return mFilterSpaceSize; }
  PRUint32 GetFilterResX() const { return mFilterSpaceSize.width; }
  PRUint32 GetFilterResY() const { return mFilterSpaceSize.height; }
  
  const nsIntRect& GetSurfaceRect() const { return mSurfaceRect; }
  PRInt32 GetSurfaceWidth() const { return mSurfaceRect.width; }
  PRInt32 GetSurfaceHeight() const { return mSurfaceRect.height; }
  
  nsresult Render(gfxASurface** aOutput);
  nsresult ComputeOutputDirtyRect(nsIntRect* aDirty);
  nsresult ComputeSourceNeededRect(nsIntRect* aDirty);
  nsresult ComputeOutputBBox(nsIntRect* aBBox);

  float GetPrimitiveNumber(PRUint8 aCtxType, const nsSVGNumber2 *aNumber) const
  {
    return GetPrimitiveNumber(aCtxType, aNumber->GetAnimValue());
  }
  float GetPrimitiveNumber(PRUint8 aCtxType, const nsSVGNumberPair *aNumberPair,
                           nsSVGNumberPair::PairIndex aIndex) const
  {
    return GetPrimitiveNumber(aCtxType, aNumberPair->GetAnimValue(aIndex));
  }
  /**
   * Converts a point and a length in filter primitive units into filter space.
   * For object-bounding-box units, the object bounding box offset is applied
   * to the point.
   */
  void ConvertLocation(float aValues[3]) const;

  gfxMatrix GetUserSpaceToFilterSpaceTransform() const;
  gfxMatrix GetFilterSpaceToDeviceSpaceTransform() const {
    return mFilterSpaceToDeviceSpaceTransform;
  }
  gfxPoint FilterSpaceToUserSpace(const gfxPoint& aPt) const;

private:
  typedef nsSVGFE::Image Image;
  typedef nsSVGFE::ColorModel ColorModel;

  struct PrimitiveInfo {
    nsSVGFE*  mFE;
    // the bounding box of the result image produced by this primitive, in
    // filter space
    nsIntRect mResultBoundingBox;
    // the bounding box of the part of the result image that is actually
    // needed by other primitives or by the filter result, in filter space
    // (used for Render only)
    nsIntRect mResultNeededBox;
    // the bounding box of the part of the result image that could be
    // changed by changes to mDirtyInputRect in the source image(s)
    // (used for ComputeOutputDirtyRect only)
    nsIntRect mResultChangeBox;
    Image     mImage;
    PRInt32   mImageUsers;
  
    // Can't use nsAutoTArray here, because these Info objects
    // live in nsTArrays themselves and nsTArray moves the elements
    // around in memory, which breaks nsAutoTArray.
    nsTArray<PrimitiveInfo*> mInputs;

    PrimitiveInfo() : mFE(nsnull), mImageUsers(0) {}
  };

  class ImageAnalysisEntry : public nsStringHashKey {
  public:
    ImageAnalysisEntry(KeyTypePointer aStr) : nsStringHashKey(aStr) { }
    ImageAnalysisEntry(const ImageAnalysisEntry& toCopy) : nsStringHashKey(toCopy),
      mInfo(toCopy.mInfo) { }

    PrimitiveInfo* mInfo;
  };

  nsresult BuildSources();
  // Build graph of PrimitiveInfo nodes describing filter primitives
  nsresult BuildPrimitives();
  // Compute bounding boxes of the filter primitive outputs
  void ComputeResultBoundingBoxes();
  // Compute bounding boxes of what we actually *need* from the filter
  // primitive outputs
  void ComputeNeededBoxes();
  // Compute bounding boxes of what could have changed given some changes
  // to the source images.
  void ComputeResultChangeBoxes();
  nsIntRect ComputeUnionOfAllNeededBoxes();
  nsresult BuildSourceImages();

  // Allocates an image surface that covers mSurfaceRect (it uses
  // device offsets so that its origin is positioned at mSurfaceRect.TopLeft()
  // when using cairo to draw into the surface). The surface is cleared
  // to transparent black.
  already_AddRefed<gfxImageSurface> CreateImage();

  void ComputeFilterPrimitiveSubregion(PrimitiveInfo* aInfo);
  void EnsureColorModel(PrimitiveInfo* aPrimitive,
                        ColorModel aColorModel);

  /**
   * Scales a numeric filter primitive length in the X, Y or "XY" directions
   * into a length in filter space (no offset is applied).
   */
  float GetPrimitiveNumber(PRUint8 aCtxType, float aValue) const;

  gfxRect UserSpaceToFilterSpace(const gfxRect& aUserSpace) const;
  void ClipToFilterSpace(nsIntRect* aRect) const
  {
    nsIntRect filterSpace(nsIntPoint(0, 0), mFilterSpaceSize);
    aRect->IntersectRect(*aRect, filterSpace);
  }
  void ClipToGfxRect(nsIntRect* aRect, const gfxRect& aGfx) const;

  nsIFrame*               mTargetFrame;
  nsSVGFilterPaintCallback* mPaintCallback;
  nsSVGFilterElement*     mFilterElement;
  // Bounding box of the target element, in user space
  gfxRect                 mTargetBBox;
  gfxMatrix               mFilterSpaceToDeviceSpaceTransform;
  gfxRect                 mFilterRect;
  nsIntSize               mFilterSpaceSize;
  nsIntRect               mDirtyOutputRect;
  nsIntRect               mDirtyInputRect;
  nsIntRect               mSurfaceRect;
  PRUint16                mPrimitiveUnits;

  PrimitiveInfo           mSourceColorAlpha;
  PrimitiveInfo           mSourceAlpha;
  nsTArray<PrimitiveInfo> mPrimitives;
};

#endif
