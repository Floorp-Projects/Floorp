/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEMergeNodeElement_h
#define mozilla_dom_SVGFEMergeNodeElement_h

#include "nsSVGFilters.h"

nsresult NS_NewSVGFEMergeNodeElement(nsIContent** aResult,
                                     already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

typedef SVGFEUnstyledElement SVGFEMergeNodeElementBase;

class SVGFEMergeNodeElement : public SVGFEMergeNodeElementBase
{
  friend nsresult (::NS_NewSVGFEMergeNodeElement(nsIContent **aResult,
                                                 already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
protected:
  explicit SVGFEMergeNodeElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : SVGFEMergeNodeElementBase(aNodeInfo)
  {
  }
  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

public:
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;

  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const override;

  const nsSVGString* GetIn1() { return &mStringAttributes[IN1]; }

  // WebIDL
  already_AddRefed<SVGAnimatedString> In1();

protected:
  virtual StringAttributesInfo GetStringInfo() override;

  enum { IN1 };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGFEMergeNodeElement_h
