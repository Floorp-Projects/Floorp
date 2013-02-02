/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGForeignObjectElement_h
#define mozilla_dom_SVGForeignObjectElement_h

#include "mozilla/dom/SVGGraphicsElement.h"
#include "nsSVGLength2.h"

nsresult NS_NewSVGForeignObjectElement(nsIContent **aResult,
                                       already_AddRefed<nsINodeInfo> aNodeInfo);

class nsSVGForeignObjectFrame;

namespace mozilla {
namespace dom {

class SVGForeignObjectElement MOZ_FINAL : public SVGGraphicsElement,
                                          public nsIDOMSVGElement
{
  friend class ::nsSVGForeignObjectFrame;

protected:
  friend nsresult (::NS_NewSVGForeignObjectElement(nsIContent **aResult,
                                                   already_AddRefed<nsINodeInfo> aNodeInfo));
  SVGForeignObjectElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx, JSObject *scope, bool *triedToWrap) MOZ_OVERRIDE;

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC
  NS_FORWARD_NSIDOMSVGELEMENT(SVGGraphicsElement::)

  // nsSVGElement specializations:
  virtual gfxMatrix PrependLocalTransformsTo(const gfxMatrix &aMatrix,
                      TransformTypes aWhich = eAllTransforms) const;
  virtual bool HasValidDimensions() const;

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* name) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsIDOMNode* AsDOMNode() { return this; }

  // WebIDL
  already_AddRefed<SVGAnimatedLength> X();
  already_AddRefed<SVGAnimatedLength> Y();
  already_AddRefed<SVGAnimatedLength> Width();
  already_AddRefed<SVGAnimatedLength> Height();

protected:

  virtual LengthAttributesInfo GetLengthInfo();

  enum { ATTR_X, ATTR_Y, ATTR_WIDTH, ATTR_HEIGHT };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGForeignObjectElement_h

