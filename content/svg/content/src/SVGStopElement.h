/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGStopElement_h
#define mozilla_dom_SVGStopElement_h

#include "nsSVGElement.h"
#include "nsSVGNumber2.h"
#include "nsIDOMSVGStopElement.h"

nsresult NS_NewSVGStopElement(nsIContent **aResult,
                              already_AddRefed<nsINodeInfo> aNodeInfo);

typedef nsSVGElement SVGStopElementBase;

namespace mozilla {
namespace dom {

class SVGStopElement MOZ_FINAL : public SVGStopElementBase,
                                 public nsIDOMSVGStopElement
{
protected:
  friend nsresult (::NS_NewSVGStopElement(nsIContent **aResult,
                                          already_AddRefed<nsINodeInfo> aNodeInfo));
  SVGStopElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual JSObject* WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap) MOZ_OVERRIDE;

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGSTOPELEMENT

  // xxx If xpcom allowed virtual inheritance we wouldn't need to
  // forward here :-(
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC
  NS_FORWARD_NSIDOMSVGELEMENT(SVGStopElementBase::)

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

  // WebIDL
  already_AddRefed<nsIDOMSVGAnimatedNumber> Offset();

protected:

  virtual NumberAttributesInfo GetNumberInfo();
  // nsIDOMSVGStopElement properties:
  nsSVGNumber2 mOffset;
  static NumberInfo sNumberInfo;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGStopElement_h
