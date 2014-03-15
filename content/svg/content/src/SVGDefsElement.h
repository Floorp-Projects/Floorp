/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGDefsElement_h
#define mozilla_dom_SVGDefsElement_h

#include "SVGGraphicsElement.h"

nsresult NS_NewSVGDefsElement(nsIContent **aResult,
                              already_AddRefed<nsINodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

class SVGDefsElement MOZ_FINAL : public SVGGraphicsElement
{
protected:
  friend nsresult (::NS_NewSVGDefsElement(nsIContent **aResult,
                                          already_AddRefed<nsINodeInfo>&& aNodeInfo));
  SVGDefsElement(already_AddRefed<nsINodeInfo>& aNodeInfo);
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

public:
  // nsIContent
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGDefsElement_h
