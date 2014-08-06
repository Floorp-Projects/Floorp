/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEMergeElement_h
#define mozilla_dom_SVGFEMergeElement_h

#include "nsSVGFilters.h"

nsresult NS_NewSVGFEMergeElement(nsIContent **aResult,
                                 already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

typedef nsSVGFE SVGFEMergeElementBase;

class SVGFEMergeElement : public SVGFEMergeElementBase
{
  friend nsresult (::NS_NewSVGFEMergeElement(nsIContent **aResult,
                                             already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
protected:
  SVGFEMergeElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : SVGFEMergeElementBase(aNodeInfo)
  {
  }
  virtual JSObject* WrapNode(JSContext *cx) MOZ_OVERRIDE;

public:
  virtual FilterPrimitiveDescription
    GetPrimitiveDescription(nsSVGFilterInstance* aInstance,
                            const IntRect& aFilterSubregion,
                            const nsTArray<bool>& aInputsAreTainted,
                            nsTArray<mozilla::RefPtr<SourceSurface>>& aInputImages) MOZ_OVERRIDE;
  virtual nsSVGString& GetResultImageName() MOZ_OVERRIDE { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources) MOZ_OVERRIDE;

  // nsIContent
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;
protected:
  virtual StringAttributesInfo GetStringInfo() MOZ_OVERRIDE;

  enum { RESULT };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGFEMergeElement_h
