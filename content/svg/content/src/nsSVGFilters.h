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

#ifndef __NS_SVGFILTERSELEMENT_H__
#define __NS_SVGFILTERSELEMENT_H__

#include "nsSVGStylableElement.h"
#include "nsSVGLength2.h"
#include "nsIFrame.h"

class nsSVGFilterResource;
class nsIDOMSVGAnimatedString;

typedef nsSVGStylableElement nsSVGFEBase;

#define NS_SVG_FE_CID \
{ 0x60483958, 0xd229, 0x4a77, \
  { 0x96, 0xb2, 0x62, 0x3e, 0x69, 0x95, 0x1e, 0x0e } }

class nsSVGFE : public nsSVGFEBase
//, public nsIDOMSVGFilterPrimitiveStandardAttributes
{
  friend class nsSVGFilterInstance;

protected:
  nsSVGFE(nsINodeInfo *aNodeInfo) : nsSVGFEBase(aNodeInfo) {}
  nsresult Init();

  struct ScaleInfo {
    nsRefPtr<gfxImageSurface> mRealSource;
    nsRefPtr<gfxImageSurface> mRealTarget;
    nsRefPtr<gfxImageSurface> mSource;
    nsRefPtr<gfxImageSurface> mTarget;
    nsRect mRect; // rect in mSource and mTarget to operate on
    PRPackedBool mRescaling;
  };

  nsresult SetupScalingFilter(nsSVGFilterInstance *aInstance,
                              nsSVGFilterResource *aResource,
                              nsIDOMSVGAnimatedString *aIn,
                              nsSVGNumber2 *aUnitX, nsSVGNumber2 *aUnitY,
                              ScaleInfo *aScaleInfo);

  void FinishScalingFilter(nsSVGFilterResource *aResource,
                           ScaleInfo *aScaleInfo);


public:
  nsSVGFilterInstance::ColorModel
  GetColorModel(nsSVGFilterInstance* aInstance, nsIDOMSVGAnimatedString* aIn) {
    return nsSVGFilterInstance::ColorModel (
          (OperatesOnSRGB(aInstance, aIn) ?
             nsSVGFilterInstance::ColorModel::SRGB :
             nsSVGFilterInstance::ColorModel::LINEAR_RGB),
          (OperatesOnPremultipledAlpha() ?
             nsSVGFilterInstance::ColorModel::PREMULTIPLIED :
             nsSVGFilterInstance::ColorModel::UNPREMULTIPLIED));
  }

  // See http://www.w3.org/TR/SVG/filters.html#FilterPrimitiveSubRegion
  virtual PRBool SubregionIsUnionOfRegions() { return PR_TRUE; }

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_SVG_FE_CID)
  
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES

  nsIDOMSVGAnimatedString* GetResultImageName() { return mResult; }
  // Return a list of all image names used as sources. Default is to
  // return no sources.
  virtual void GetSourceImageNames(nsTArray<nsIDOMSVGAnimatedString*>* aSources);
  // Compute the bounding-box of the filter output. The default is just the
  // union of the source bounding-boxes. The caller is
  // responsible for clipping this to the filter primitive subregion, so
  // if the filter fills its filter primitive subregion, it can just
  // return GetMaxRect() here.
  // The source bounding-boxes are ordered corresponding to GetSourceImageNames.
  virtual nsRect ComputeTargetBBox(const nsTArray<nsRect>& aSourceBBoxes,
          const nsSVGFilterInstance& aInstance);
  // Given a bounding-box for what we need to compute in the target,
  // compute which regions of the inputs are needed. On input
  // aSourceBBoxes contains the bounding box of what's rendered by
  // each source; this function should change those boxes to indicate
  // which region of the source's output it needs.
  // The default implementation sets all the source bboxes to the
  // target bbox.
  virtual void ComputeNeededSourceBBoxes(const nsRect& aTargetBBox,
          nsTArray<nsRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance);
  // Perform the actual filter operation.
  virtual nsresult Filter(nsSVGFilterInstance* aInstance) = 0;

  static nsRect GetMaxRect() {
    // Try to avoid overflow errors dealing with this rect. It will
    // be intersected with some other reasonable-sized rect eventually.
    return nsRect(PR_INT32_MIN/2, PR_INT32_MIN/2, PR_INT32_MAX, PR_INT32_MAX);
  }

  operator nsISupports*() { return static_cast<nsIContent*>(this); }
  
protected:
  virtual PRBool OperatesOnPremultipledAlpha() { return PR_TRUE; }

  virtual PRBool OperatesOnSRGB(nsSVGFilterInstance*,
                                nsIDOMSVGAnimatedString*) {
    nsIFrame* frame = GetPrimaryFrame();
    if (!frame) return PR_FALSE;

    nsStyleContext* style = frame->GetStyleContext();
    return style->GetStyleSVG()->mColorInterpolationFilters ==
             NS_STYLE_COLOR_INTERPOLATION_SRGB;
  }

  // nsSVGElement specializations:
  virtual LengthAttributesInfo GetLengthInfo();

  // nsIDOMSVGFitlerPrimitiveStandardAttributes values
  enum { X, Y, WIDTH, HEIGHT };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];

  nsCOMPtr<nsIDOMSVGAnimatedString> mResult;
};

#endif
