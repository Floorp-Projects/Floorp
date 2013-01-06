/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGDefsElement_h
#define mozilla_dom_SVGDefsElement_h

#include "mozilla/Util.h"
#include "SVGGraphicsElement.h"
#include "nsIDOMSVGDefsElement.h"

nsresult NS_NewSVGDefsElement(nsIContent **aResult,
                              already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

class SVGDefsElement MOZ_FINAL : public SVGGraphicsElement,
                                 public nsIDOMSVGDefsElement
{
protected:
  friend nsresult (::NS_NewSVGDefsElement(nsIContent **aResult,
                                          already_AddRefed<nsINodeInfo> aNodeInfo));
  SVGDefsElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx, JSObject *scope, bool *triedToWrap) MOZ_OVERRIDE;

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGDEFSELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC
  NS_FORWARD_NSIDOMSVGELEMENT(SVGGraphicsElement::)

  // nsIContent
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGDefsElement_h
