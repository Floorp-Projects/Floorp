/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEComponentTransferElement_h
#define mozilla_dom_SVGFEComponentTransferElement_h

#include "nsSVGFilters.h"

typedef nsSVGFE SVGFEComponentTransferElementBase;

nsresult NS_NewSVGFEComponentTransferElement(nsIContent **aResult,
                                             already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

class SVGFEComponentTransferElement : public SVGFEComponentTransferElementBase
{
  friend nsresult (::NS_NewSVGFEComponentTransferElement(nsIContent **aResult,
                                                         already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
protected:
  explicit SVGFEComponentTransferElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : SVGFEComponentTransferElementBase(aNodeInfo)
  {
  }
  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

public:
  virtual FilterPrimitiveDescription
    GetPrimitiveDescription(nsSVGFilterInstance* aInstance,
                            const IntRect& aFilterSubregion,
                            const nsTArray<bool>& aInputsAreTainted,
                            nsTArray<mozilla::RefPtr<SourceSurface>>& aInputImages) override;
  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const override;
  virtual nsSVGString& GetResultImageName() override { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources) override;

  // nsIContent
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;

  // WebIDL
  already_AddRefed<SVGAnimatedString> In1();

protected:
  virtual StringAttributesInfo GetStringInfo() override;

  enum { RESULT, IN1 };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];
};

} // namespace mozilla
} // namespace dom

#endif // mozilla_dom_SVGFEComponentTransferElement_h
