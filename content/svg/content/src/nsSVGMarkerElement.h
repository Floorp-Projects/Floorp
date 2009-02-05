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

#ifndef __NS_SVGMARKERELEMENT_H__
#define __NS_SVGMARKERELEMENT_H__

#include "nsSVGGraphicElement.h"
#include "nsIDOMSVGMarkerElement.h"
#include "nsIDOMSVGFitToViewBox.h"
#include "nsSVGLength2.h"
#include "nsSVGEnum.h"
#include "nsSVGAngle.h"
#include "nsSVGViewBox.h"
#include "nsSVGPreserveAspectRatio.h"

class nsSVGOrientType
{
public:
  nsSVGOrientType()
   : mAnimVal(nsIDOMSVGMarkerElement::SVG_MARKER_ORIENT_ANGLE),
     mBaseVal(nsIDOMSVGMarkerElement::SVG_MARKER_ORIENT_ANGLE) {}

  nsresult SetBaseValue(PRUint16 aValue,
                        nsSVGElement *aSVGElement);

  void SetBaseValue(PRUint16 aValue)
    { mAnimVal = mBaseVal = PRUint8(aValue); }

  PRUint16 GetBaseValue() const
    { return mBaseVal; }
  PRUint16 GetAnimValue() const
    { return mAnimVal; }

  nsresult ToDOMAnimatedEnum(nsIDOMSVGAnimatedEnumeration **aResult,
                             nsSVGElement* aSVGElement);

private:
  nsSVGEnumValue mAnimVal;
  nsSVGEnumValue mBaseVal;

  struct DOMAnimatedEnum : public nsIDOMSVGAnimatedEnumeration
  {
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(DOMAnimatedEnum)

    DOMAnimatedEnum(nsSVGOrientType* aVal,
                    nsSVGElement *aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}

    nsSVGOrientType *mVal; // kept alive because it belongs to content
    nsRefPtr<nsSVGElement> mSVGElement;

    NS_IMETHOD GetBaseVal(PRUint16* aResult)
      { *aResult = mVal->GetBaseValue(); return NS_OK; }
    NS_IMETHOD SetBaseVal(PRUint16 aValue)
      { return mVal->SetBaseValue(aValue, mSVGElement); }
    NS_IMETHOD GetAnimVal(PRUint16* aResult)
      { *aResult = mVal->GetAnimValue(); return NS_OK; }
  };
};

typedef nsSVGGraphicElement nsSVGMarkerElementBase;

class nsSVGMarkerElement : public nsSVGMarkerElementBase,
                           public nsIDOMSVGMarkerElement,
                           public nsIDOMSVGFitToViewBox
{
  friend class nsSVGMarkerFrame;

protected:
  friend nsresult NS_NewSVGMarkerElement(nsIContent **aResult,
                                         nsINodeInfo *aNodeInfo);
  nsSVGMarkerElement(nsINodeInfo* aNodeInfo);

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGMARKERELEMENT
  NS_DECL_NSIDOMSVGFITTOVIEWBOX

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGElement::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGElement::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGElement::)

  // nsIContent interface
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* name) const;

  virtual PRBool GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                         nsAString& aResult) const;
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             PRBool aNotify);

  // nsSVGElement specializations:
  virtual void DidChangeLength(PRUint8 aAttrEnum, PRBool aDoSetAttr);
  virtual void DidChangeViewBox(PRBool aDoSetAttr);
  virtual void DidChangePreserveAspectRatio(PRBool aDoSetAttr);

  // public helpers
  nsresult GetMarkerTransform(float aStrokeWidth,
                              float aX, float aY, float aAngle,
                              nsIDOMSVGMatrix **_retval);
  nsresult GetViewboxToViewportTransform(nsIDOMSVGMatrix **_retval);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:

  virtual PRBool ParseAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                                const nsAString& aValue,
                                nsAttrValue& aResult);

  void SetParentCoordCtxProvider(nsSVGSVGElement *aContext);

  virtual LengthAttributesInfo GetLengthInfo();
  virtual AngleAttributesInfo GetAngleInfo();
  virtual EnumAttributesInfo GetEnumInfo();
  virtual nsSVGViewBox *GetViewBox();
  virtual nsSVGPreserveAspectRatio *GetPreserveAspectRatio();

  enum { REFX, REFY, MARKERWIDTH, MARKERHEIGHT };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];

  enum { MARKERUNITS };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sUnitsMap[];
  static EnumInfo sEnumInfo[1];

  enum { ORIENT };
  nsSVGAngle mAngleAttributes[1];
  static AngleInfo sAngleInfo[1];

  nsSVGViewBox             mViewBox;
  nsSVGPreserveAspectRatio mPreserveAspectRatio;

  // derived properties (from 'orient') handled separately
  nsSVGOrientType                        mOrientType;

  nsSVGSVGElement                       *mCoordCtx;
  nsCOMPtr<nsIDOMSVGMatrix>         mViewBoxToViewportTransform;
};

#endif
