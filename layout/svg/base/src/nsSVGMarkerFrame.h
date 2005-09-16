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
 * Portions created by the Initial Developer are Copyright (C) 2004
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

#ifndef __NS_SVGMARKERFRAME_H__
#define __NS_SVGMARKERFRAME_H__

#include "nsSVGDefsFrame.h"
#include "nsIDOMSVGLength.h"
#include "nsIDOMSVGAngle.h"
#include "nsIDOMSVGRect.h"
#include "nsIDOMSVGAnimatedEnum.h"
#include "nsISVGMarkable.h"
#include "nsLayoutAtoms.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class nsSVGPathGeometryFrame;

#define NS_SVGMARKERFRAME_CID \
{0x899a645c, 0xf817, 0x4a1a, {0xaa, 0x3d, 0xa9, 0x1a, 0xbf, 0xa2, 0xb2, 0x0a}}

class nsSVGMarkerFrame : public nsSVGDefsFrame
{
protected:
  friend nsresult
  NS_NewSVGMarkerFrame(nsIPresShell* aPresShell,
                       nsIContent* aContent,
                       nsIFrame** aNewFrame);

  virtual ~nsSVGMarkerFrame();
  NS_IMETHOD InitSVG();

public:
  NS_DECL_ISUPPORTS

  NS_DEFINE_STATIC_CID_ACCESSOR(NS_SVGMARKERFRAME_CID)
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_SVGMARKERFRAME_CID)

  // nsISVGValueObserver interface:
  NS_IMETHOD DidModifySVGObservable(nsISVGValue* observable,
                                    nsISVGValue::modificationType aModType);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgMarkerFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGMarker"), aResult);
  }
#endif

  void PaintMark(nsISVGRendererCanvas *aCanvas,
                 nsSVGPathGeometryFrame *aParent,
                 nsSVGMark *aMark,
                 float aStrokeWidth);

  NS_IMETHOD_(already_AddRefed<nsISVGRendererRegion>)
    RegionMark(nsSVGPathGeometryFrame *aParent,
               nsSVGMark *aMark, float aStrokeWidth);

  static float bisect(float a1, float a2);

private:
  nsCOMPtr<nsIDOMSVGLength>              mRefX;
  nsCOMPtr<nsIDOMSVGLength>              mRefY;
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mMarkerUnits;
  nsCOMPtr<nsIDOMSVGLength>              mMarkerWidth;
  nsCOMPtr<nsIDOMSVGLength>              mMarkerHeight;
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mOrientType;
  nsCOMPtr<nsIDOMSVGAngle>               mOrientAngle;
  nsCOMPtr<nsIDOMSVGRect>                mViewBox;

  // stuff needed for callback
  float mStrokeWidth, mX, mY, mAngle;
  nsSVGPathGeometryFrame *mMarkerParent;

  // nsISVGContainerFrame interface:
  already_AddRefed<nsIDOMSVGMatrix> GetCanvasTM();

  // recursion prevention flag
  PRPackedBool mInUse;

  // second recursion prevention flag, for GetCanvasTM()
  PRPackedBool mInUse2;
};

nsresult
NS_GetSVGMarkerFrame(nsSVGMarkerFrame **aResult,
                     nsIURI *aURI,
                     nsIContent *aContent);

#endif
