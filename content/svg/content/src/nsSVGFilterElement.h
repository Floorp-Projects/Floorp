/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGFILTERELEMENT_H__
#define __NS_SVGFILTERELEMENT_H__

#include "DOMSVGTests.h"
#include "nsIDOMSVGFilterElement.h"
#include "nsIDOMSVGUnitTypes.h"
#include "nsIDOMSVGURIReference.h"
#include "nsSVGEnum.h"
#include "nsSVGGraphicElement.h"
#include "nsSVGIntegerPair.h"
#include "nsSVGLength2.h"
#include "nsSVGString.h"

typedef nsSVGGraphicElement nsSVGFilterElementBase;

class nsSVGFilterElement : public nsSVGFilterElementBase,
                           public nsIDOMSVGFilterElement,
                           public DOMSVGTests,
                           public nsIDOMSVGURIReference,
                           public nsIDOMSVGUnitTypes
{
  friend class nsSVGFilterFrame;
  friend class nsAutoFilterInstance;

protected:
  friend nsresult NS_NewSVGFilterElement(nsIContent **aResult,
                                         already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGFilterElement(already_AddRefed<nsINodeInfo> aNodeInfo);

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGFILTERELEMENT
  NS_DECL_NSIDOMSVGURIREFERENCE

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGFilterElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFilterElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFilterElementBase::)

  // nsIContent
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  // Invalidate users of this filter
  void Invalidate();

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

  // nsSVGSVGElement methods:
  virtual bool HasValidDimensions() const;
protected:

  virtual LengthAttributesInfo GetLengthInfo();
  virtual IntegerPairAttributesInfo GetIntegerPairInfo();
  virtual EnumAttributesInfo GetEnumInfo();
  virtual StringAttributesInfo GetStringInfo();

  enum { X, Y, WIDTH, HEIGHT };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];

  enum { FILTERRES };
  nsSVGIntegerPair mIntegerPairAttributes[1];
  static IntegerPairInfo sIntegerPairInfo[1];

  enum { FILTERUNITS, PRIMITIVEUNITS };
  nsSVGEnum mEnumAttributes[2];
  static EnumInfo sEnumInfo[2];

  enum { HREF };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];
};

#endif
