/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGTSpanElement_h
#define mozilla_dom_SVGTSpanElement_h

#include "mozilla/dom/SVGTextPositioningElement.h"

nsresult NS_NewSVGTSpanElement(nsIContent **aResult,
                               already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

typedef SVGTextPositioningElement SVGTSpanElementBase;

class SVGTSpanElement MOZ_FINAL : public SVGTSpanElementBase
{
protected:
  friend nsresult (::NS_NewSVGTSpanElement(nsIContent **aResult,
                                           already_AddRefed<nsINodeInfo> aNodeInfo));
  SVGTSpanElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx,
                             JS::Handle<JSObject*> scope) MOZ_OVERRIDE;

public:
  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
protected:

  // nsSVGElement overrides
  virtual bool IsEventName(nsIAtom* aName);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGTSpanElement_h
