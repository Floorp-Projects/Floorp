/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEMergeNodeElement_h
#define mozilla_dom_SVGFEMergeNodeElement_h

#include "nsSVGFilters.h"

nsresult NS_NewSVGFEMergeNodeElement(nsIContent** aResult,
                                     already_AddRefed<nsINodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

typedef SVGFEUnstyledElement SVGFEMergeNodeElementBase;

class SVGFEMergeNodeElement : public SVGFEMergeNodeElementBase
{
  friend nsresult (::NS_NewSVGFEMergeNodeElement(nsIContent **aResult,
                                                 already_AddRefed<nsINodeInfo>&& aNodeInfo));
protected:
  SVGFEMergeNodeElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
    : SVGFEMergeNodeElementBase(aNodeInfo)
  {
  }
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

public:
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const MOZ_OVERRIDE;

  const nsSVGString* GetIn1() { return &mStringAttributes[IN1]; }

  // WebIDL
  already_AddRefed<SVGAnimatedString> In1();

protected:
  virtual StringAttributesInfo GetStringInfo() MOZ_OVERRIDE;

  enum { IN1 };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGFEMergeNodeElement_h
