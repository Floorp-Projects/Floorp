/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGTextPathElement_h
#define mozilla_dom_SVGTextPathElement_h

#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMSVGElement.h"
#include "nsIDOMSVGTextContentElement.h"
#include "nsIDOMSVGTextPathElement.h"
#include "nsIDOMSVGURIReference.h"
#include "nsSVGEnum.h"
#include "nsSVGLength2.h"
#include "nsSVGString.h"
#include "SVGTextContentElement.h"

class nsIAtom;
class nsIContent;
class nsINode;
class nsINodeInfo;
class nsXPCClassInfo;
class nsSVGTextPathFrame;

nsresult NS_NewSVGTextPathElement(nsIContent **aResult,
                                  already_AddRefed<nsINodeInfo> aNodeInfo);

typedef mozilla::dom::SVGTextContentElement nsSVGTextPathElementBase;

namespace mozilla {
namespace dom {

typedef SVGTextContentElement SVGTextPathElementBase;

class SVGTextPathElement MOZ_FINAL : public SVGTextPathElementBase,
                                     public nsIDOMSVGTextPathElement,
                                     public nsIDOMSVGURIReference
{
friend class ::nsSVGTextPathFrame;

protected:
  friend nsresult (::NS_NewSVGTextPathElement(nsIContent **aResult,
                                              already_AddRefed<nsINodeInfo> aNodeInfo));
  SVGTextPathElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx, JSObject *scope, bool *triedToWrap) MOZ_OVERRIDE;

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGTEXTPATHELEMENT
  NS_DECL_NSIDOMSVGURIREFERENCE

  // xxx If xpcom allowed virtual inheritance we wouldn't need to
  // forward here :-(
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC
  NS_FORWARD_NSIDOMSVGELEMENT(SVGTextPathElementBase::)
  NS_FORWARD_NSIDOMSVGTEXTCONTENTELEMENT(SVGTextPathElementBase::)

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

  virtual bool IsEventAttributeName(nsIAtom* aName) MOZ_OVERRIDE;

  // WebIDL
  already_AddRefed<SVGAnimatedLength> StartOffset();
  already_AddRefed<nsIDOMSVGAnimatedEnumeration> Method();
  already_AddRefed<nsIDOMSVGAnimatedEnumeration> Spacing();
  already_AddRefed<nsIDOMSVGAnimatedString> Href();

 protected:

  virtual LengthAttributesInfo GetLengthInfo();
  virtual EnumAttributesInfo GetEnumInfo();
  virtual StringAttributesInfo GetStringInfo();


  enum { STARTOFFSET };
  nsSVGLength2 mLengthAttributes[1];
  static LengthInfo sLengthInfo[1];

  enum { METHOD, SPACING };
  nsSVGEnum mEnumAttributes[2];
  static nsSVGEnumMapping sMethodMap[];
  static nsSVGEnumMapping sSpacingMap[];
  static EnumInfo sEnumInfo[2];

  enum { HREF };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGTextPathElement_h
