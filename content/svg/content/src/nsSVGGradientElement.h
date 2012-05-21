/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGGRADIENTELEMENT_H__
#define __NS_SVGGRADIENTELEMENT_H__

#include "nsIDOMSVGURIReference.h"
#include "nsIDOMSVGGradientElement.h"
#include "DOMSVGTests.h"
#include "nsIDOMSVGUnitTypes.h"
#include "nsSVGStylableElement.h"
#include "nsSVGLength2.h"
#include "nsSVGEnum.h"
#include "nsSVGString.h"
#include "SVGAnimatedTransformList.h"

//--------------------- Gradients------------------------

typedef nsSVGStylableElement nsSVGGradientElementBase;

class nsSVGGradientElement : public nsSVGGradientElementBase,
                             public DOMSVGTests,
                             public nsIDOMSVGURIReference,
                             public nsIDOMSVGUnitTypes
{
  friend class nsSVGGradientFrame;

protected:
  nsSVGGradientElement(already_AddRefed<nsINodeInfo> aNodeInfo);

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // Gradient Element
  NS_DECL_NSIDOMSVGGRADIENTELEMENT

  // URI Reference
  NS_DECL_NSIDOMSVGURIREFERENCE

  // nsIContent
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual mozilla::SVGAnimatedTransformList* GetAnimatedTransformList();
  virtual nsIAtom* GetTransformListAttrName() const {
    return nsGkAtoms::gradientTransform;
  }

protected:
  virtual EnumAttributesInfo GetEnumInfo();
  virtual StringAttributesInfo GetStringInfo();

  enum { GRADIENTUNITS, SPREADMETHOD };
  nsSVGEnum mEnumAttributes[2];
  static nsSVGEnumMapping sSpreadMethodMap[];
  static EnumInfo sEnumInfo[2];

  enum { HREF };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];

  // nsIDOMSVGGradientElement values
  nsAutoPtr<mozilla::SVGAnimatedTransformList> mGradientTransform;
};

//---------------------Linear Gradients------------------------

typedef nsSVGGradientElement nsSVGLinearGradientElementBase;

class nsSVGLinearGradientElement : public nsSVGLinearGradientElementBase,
                                   public nsIDOMSVGLinearGradientElement
{
  friend class nsSVGLinearGradientFrame;

protected:
  friend nsresult NS_NewSVGLinearGradientElement(nsIContent **aResult,
                                                 already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGLinearGradientElement(already_AddRefed<nsINodeInfo> aNodeInfo);

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // Gradient Element
  NS_FORWARD_NSIDOMSVGGRADIENTELEMENT(nsSVGLinearGradientElementBase::)

  // Linear Gradient
  NS_DECL_NSIDOMSVGLINEARGRADIENTELEMENT

  // The Gradient Element base class implements these
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGLinearGradientElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGLinearGradientElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGLinearGradientElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:

  virtual LengthAttributesInfo GetLengthInfo();

  // nsIDOMSVGLinearGradientElement values
  enum { X1, Y1, X2, Y2 };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];
};

//-------------------------- Radial Gradients ----------------------------

typedef nsSVGGradientElement nsSVGRadialGradientElementBase;

class nsSVGRadialGradientElement : public nsSVGRadialGradientElementBase,
                                   public nsIDOMSVGRadialGradientElement
{
  friend class nsSVGRadialGradientFrame;

protected:
  friend nsresult NS_NewSVGRadialGradientElement(nsIContent **aResult,
                                                 already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGRadialGradientElement(already_AddRefed<nsINodeInfo> aNodeInfo);

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED

  // Gradient Element
  NS_FORWARD_NSIDOMSVGGRADIENTELEMENT(nsSVGRadialGradientElementBase::)

  // Radial Gradient
  NS_DECL_NSIDOMSVGRADIALGRADIENTELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGRadialGradientElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGRadialGradientElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGRadialGradientElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:

  virtual LengthAttributesInfo GetLengthInfo();

  // nsIDOMSVGRadialGradientElement values
  enum { CX, CY, R, FX, FY };
  nsSVGLength2 mLengthAttributes[5];
  static LengthInfo sLengthInfo[5];
};

#endif
