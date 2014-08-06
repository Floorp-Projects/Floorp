/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGStopElement_h
#define mozilla_dom_SVGStopElement_h

#include "nsSVGElement.h"
#include "nsSVGNumber2.h"

nsresult NS_NewSVGStopElement(nsIContent **aResult,
                              already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

typedef nsSVGElement SVGStopElementBase;

namespace mozilla {
namespace dom {

class SVGStopElement MOZ_FINAL : public SVGStopElementBase
{
protected:
  friend nsresult (::NS_NewSVGStopElement(nsIContent **aResult,
                                          already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  SVGStopElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;

public:
  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const MOZ_OVERRIDE;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  // WebIDL
  already_AddRefed<SVGAnimatedNumber> Offset();

protected:

  virtual NumberAttributesInfo GetNumberInfo() MOZ_OVERRIDE;
  nsSVGNumber2 mOffset;
  static NumberInfo sNumberInfo;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGStopElement_h
