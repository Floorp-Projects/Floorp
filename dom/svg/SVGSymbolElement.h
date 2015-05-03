/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGSymbolElement_h
#define mozilla_dom_SVGSymbolElement_h

#include "mozilla/dom/SVGTests.h"
#include "nsSVGElement.h"
#include "nsSVGViewBox.h"
#include "SVGAnimatedPreserveAspectRatio.h"

nsresult NS_NewSVGSymbolElement(nsIContent **aResult,
                                already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

typedef nsSVGElement SVGSymbolElementBase;

class SVGSymbolElement final : public SVGSymbolElementBase,
                               public SVGTests
{
protected:
  friend nsresult (::NS_NewSVGSymbolElement(nsIContent **aResult,
                                            already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGSymbolElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  ~SVGSymbolElement();
  virtual JSObject* WrapNode(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* name) const override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;

  // WebIDL
  already_AddRefed<SVGAnimatedRect> ViewBox();
  already_AddRefed<DOMSVGAnimatedPreserveAspectRatio> PreserveAspectRatio();

protected:
  virtual nsSVGViewBox *GetViewBox() override;
  virtual SVGAnimatedPreserveAspectRatio *GetPreserveAspectRatio() override;

  nsSVGViewBox mViewBox;
  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGSymbolElement_h
