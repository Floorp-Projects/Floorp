/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGDescElement_h
#define mozilla_dom_SVGDescElement_h

#include "mozilla/Attributes.h"
#include "nsSVGElement.h"

nsresult NS_NewSVGDescElement(nsIContent **aResult,
                              already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

typedef nsSVGElement SVGDescElementBase;

namespace mozilla {
namespace dom {

class SVGDescElement final : public SVGDescElementBase
{
protected:
  friend nsresult (::NS_NewSVGDescElement(nsIContent **aResult,
                                          already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGDescElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

public:
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGDescElement_h

