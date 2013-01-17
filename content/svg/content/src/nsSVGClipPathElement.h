/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGCLIPPATHELEMENT_H__
#define __NS_SVGCLIPPATHELEMENT_H__

#include "nsIDOMSVGClipPathElement.h"
#include "nsIDOMSVGUnitTypes.h"
#include "nsSVGEnum.h"
#include "mozilla/dom/SVGTransformableElement.h"

typedef mozilla::dom::SVGTransformableElement nsSVGClipPathElementBase;

class nsSVGClipPathElement : public nsSVGClipPathElementBase,
                             public nsIDOMSVGClipPathElement,
                             public nsIDOMSVGUnitTypes
{
  friend class nsSVGClipPathFrame;

protected:
  friend nsresult NS_NewSVGClipPathElement(nsIContent **aResult,
                                           already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGClipPathElement(already_AddRefed<nsINodeInfo> aNodeInfo);

public:
  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGCLIPPATHELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGClipPathElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:

  // nsIDOMSVGClipPathElement values
  enum { CLIPPATHUNITS };
  nsSVGEnum mEnumAttributes[1];
  static EnumInfo sEnumInfo[1];

  virtual EnumAttributesInfo GetEnumInfo();
};

#endif
