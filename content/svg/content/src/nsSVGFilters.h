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
    nsRect mRect;
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
  GetColorModel(nsIDOMSVGAnimatedString* aIn) {
    return nsSVGFilterInstance::ColorModel (
          (OperatesOnSRGB(aIn) ?
             nsSVGFilterInstance::ColorModel::SRGB :
             nsSVGFilterInstance::ColorModel::LINEAR_RGB),
          (OperatesOnPremultipledAlpha() ?
             nsSVGFilterInstance::ColorModel::PREMULTIPLIED :
             nsSVGFilterInstance::ColorModel::UNPREMULTIPLIED));
  }

  // See http://www.w3.org/TR/SVG/filters.html#FilterPrimitiveSubRegion
  virtual PRBool SubregionIsUnionOfRegions() { return PR_TRUE; }

  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES

protected:
  virtual PRBool OperatesOnPremultipledAlpha() { return PR_TRUE; }

  virtual PRBool OperatesOnSRGB(nsIDOMSVGAnimatedString*) {
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
