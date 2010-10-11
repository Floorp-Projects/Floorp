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
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
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

#ifndef __NS_SVGPATHSEG_H__
#define __NS_SVGPATHSEG_H__

#include "nsIDOMSVGPathSeg.h"
#include "nsIWeakReference.h"
#include "nsSVGUtils.h"
#include "nsSVGPathDataParser.h"

// 0dfd1b3c-5638-4813-a6f8-5a325a35d06e
#define NS_SVGPATHSEG_IID \
  { 0x0dfd1b3c, 0x5638, 0x4813, \
    { 0xa6, 0xf8, 0x5a, 0x32, 0x5a, 0x35, 0xd0, 0x6e } }

#define NS_ENSURE_NATIVE_PATH_SEG(obj, retval)            \
  {                                                       \
    nsCOMPtr<nsSVGPathSeg> path = do_QueryInterface(obj); \
    if (!path) {                                          \
      *retval = nsnull;                                   \
      return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;             \
    }                                                     \
  }

class nsSVGPathSegList;
class nsISVGValue;

struct nsSVGPathSegTraversalState {
  float curPosX, startPosX, quadCPX, cubicCPX;
  float curPosY, startPosY, quadCPY, cubicCPY;
  nsSVGPathSegTraversalState()
  {
    curPosX = startPosX = quadCPX = cubicCPX = 0;
    curPosY = startPosY = quadCPY = cubicCPY = 0;
  }
};

class nsSVGPathSeg : public nsIDOMSVGPathSeg
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_SVGPATHSEG_IID)

  nsSVGPathSeg() : mCurrentList(nsnull) {}
  nsresult SetCurrentList(nsISVGValue* aList);
  nsQueryReferent GetCurrentList() const;

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGPathSeg interface:
  NS_IMETHOD GetPathSegType(PRUint16 *aPathSegType) = 0;
  NS_IMETHOD GetPathSegTypeAsLetter(nsAString & aPathSegTypeAsLetter) = 0;

  // nsSVGPathSeg methods:
  NS_IMETHOD GetValueString(nsAString& aValue) = 0;
  virtual float GetLength(nsSVGPathSegTraversalState *ts) = 0;

protected:
  void DidModify();

private:
  nsCOMPtr<nsIWeakReference> mCurrentList;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsSVGPathSeg, NS_SVGPATHSEG_IID)

nsIDOMSVGPathSeg*
NS_NewSVGPathSegClosePath();

nsIDOMSVGPathSeg*
NS_NewSVGPathSegMovetoAbs(float x, float y);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegMovetoRel(float x, float y);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegLinetoAbs(float x, float y);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegLinetoRel(float x, float y);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoCubicAbs(float x, float y,
                                float x1, float y1,
                                float x2, float y2);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoCubicRel(float x, float y,
                                float x1, float y1,
                                float x2, float y2);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoQuadraticAbs(float x, float y,
                                    float x1, float y1);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoQuadraticRel(float x, float y,
                                    float x1, float y1);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegArcAbs(float x, float y,
                       float r1, float r2, float angle,
                       PRBool largeArcFlag, PRBool sweepFlag);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegArcRel(float x, float y,
                       float r1, float r2, float angle,
                       PRBool largeArcFlag, PRBool sweepFlag);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegLinetoHorizontalAbs(float x);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegLinetoHorizontalRel(float x);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegLinetoVerticalAbs(float y);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegLinetoVerticalRel(float y);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoCubicSmoothAbs(float x, float y,
                                      float x2, float y2);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoCubicSmoothRel(float x, float y,
                                      float x2, float y2);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoQuadraticSmoothRel(float x, float y);


#endif //__NS_SVGPATHSEG_H__
