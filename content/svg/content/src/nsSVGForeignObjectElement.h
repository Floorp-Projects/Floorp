/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGFOREIGNOBJECTELEMENT_H__
#define __NS_SVGFOREIGNOBJECTELEMENT_H__

#include "DOMSVGTests.h"
#include "nsIDOMSVGForeignObjectElem.h"
#include "nsSVGGraphicElement.h"
#include "nsSVGLength2.h"

typedef nsSVGGraphicElement nsSVGForeignObjectElementBase;

class nsSVGForeignObjectElement : public nsSVGForeignObjectElementBase,
                                  public nsIDOMSVGForeignObjectElement,
                                  public DOMSVGTests
{
  friend class nsSVGForeignObjectFrame;

protected:
  friend nsresult NS_NewSVGForeignObjectElement(nsIContent **aResult,
                                                already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGForeignObjectElement(already_AddRefed<nsINodeInfo> aNodeInfo);

public:
  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGFOREIGNOBJECTELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGForeignObjectElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGForeignObjectElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGForeignObjectElementBase::)

  // nsSVGElement specializations:
  virtual gfxMatrix PrependLocalTransformsTo(const gfxMatrix &aMatrix,
                      TransformTypes aWhich = eAllTransforms) const;
  virtual bool HasValidDimensions() const;

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* name) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:

  virtual LengthAttributesInfo GetLengthInfo();
  
  enum { X, Y, WIDTH, HEIGHT };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];
};

#endif
