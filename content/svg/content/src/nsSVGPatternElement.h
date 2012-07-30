/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGPATTERNELEMENT_H__
#define __NS_SVGPATTERNELEMENT_H__

#include "DOMSVGTests.h"
#include "nsIDOMSVGFitToViewBox.h"
#include "nsIDOMSVGPatternElement.h"
#include "nsIDOMSVGUnitTypes.h"
#include "nsIDOMSVGURIReference.h"
#include "nsSVGEnum.h"
#include "nsSVGLength2.h"
#include "nsSVGString.h"
#include "nsSVGStylableElement.h"
#include "nsSVGViewBox.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGAnimatedTransformList.h"

//--------------------- Patterns ------------------------

typedef nsSVGStylableElement nsSVGPatternElementBase;

class nsSVGPatternElement : public nsSVGPatternElementBase,
                            public nsIDOMSVGPatternElement,
                            public DOMSVGTests,
                            public nsIDOMSVGURIReference,
                            public nsIDOMSVGFitToViewBox,
                            public nsIDOMSVGUnitTypes
{
  friend class nsSVGPatternFrame;

protected:
  friend nsresult NS_NewSVGPatternElement(nsIContent **aResult,
                                          already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGPatternElement(already_AddRefed<nsINodeInfo> aNodeInfo);

public:
  typedef mozilla::SVGAnimatedPreserveAspectRatio SVGAnimatedPreserveAspectRatio;

  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // Pattern Element
  NS_DECL_NSIDOMSVGPATTERNELEMENT

  // URI Reference
  NS_DECL_NSIDOMSVGURIREFERENCE

  // FitToViewbox
  NS_DECL_NSIDOMSVGFITTOVIEWBOX

  NS_FORWARD_NSIDOMNODE(nsSVGElement::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGElement::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGElement::)

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* name) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

  // nsSVGSVGElement methods:
  virtual bool HasValidDimensions() const;

  virtual mozilla::SVGAnimatedTransformList*
    GetAnimatedTransformList(PRUint32 aFlags = 0);
  virtual nsIAtom* GetTransformListAttrName() const {
    return nsGkAtoms::patternTransform;
  }
protected:

  virtual LengthAttributesInfo GetLengthInfo();
  virtual EnumAttributesInfo GetEnumInfo();
  virtual nsSVGViewBox *GetViewBox();
  virtual SVGAnimatedPreserveAspectRatio *GetPreserveAspectRatio();
  virtual StringAttributesInfo GetStringInfo();

  // nsIDOMSVGPatternElement values
  enum { X, Y, WIDTH, HEIGHT };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];

  enum { PATTERNUNITS, PATTERNCONTENTUNITS };
  nsSVGEnum mEnumAttributes[2];
  static EnumInfo sEnumInfo[2];

  nsAutoPtr<mozilla::SVGAnimatedTransformList> mPatternTransform;

  // nsIDOMSVGURIReference properties
  enum { HREF };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];

  // nsIDOMSVGFitToViewbox properties
  nsSVGViewBox mViewBox;
  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;
};

#endif
