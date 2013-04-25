/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEFloodElement_h
#define mozilla_dom_SVGFEFloodElement_h

#include "nsSVGFilters.h"

nsresult NS_NewSVGFEFloodElement(nsIContent **aResult,
                                 already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

typedef nsSVGFE SVGFEFloodElementBase;

class SVGFEFloodElement : public SVGFEFloodElementBase
{
  friend nsresult (::NS_NewSVGFEFloodElement(nsIContent **aResult,
                                             already_AddRefed<nsINodeInfo> aNodeInfo));
protected:
  SVGFEFloodElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : SVGFEFloodElementBase(aNodeInfo)
  {
  }
  virtual JSObject* WrapNode(JSContext *cx,
                             JS::Handle<JSObject*> scope) MOZ_OVERRIDE;

public:
  virtual bool SubregionIsUnionOfRegions() { return false; }

  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);
  virtual nsSVGString& GetResultImageName() { return mStringAttributes[RESULT]; }
  virtual nsIntRect ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
          const nsSVGFilterInstance& aInstance);

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:
  virtual bool OperatesOnSRGB(nsSVGFilterInstance*,
                              int32_t, Image*) { return true; }

  virtual StringAttributesInfo GetStringInfo();

  enum { RESULT };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGFEFloodElement_h
