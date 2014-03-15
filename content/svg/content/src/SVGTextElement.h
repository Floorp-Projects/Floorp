/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGTextElement_h
#define mozilla_dom_SVGTextElement_h

#include "mozilla/dom/SVGTextPositioningElement.h"

nsresult NS_NewSVGTextElement(nsIContent **aResult,
                              already_AddRefed<nsINodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

typedef SVGTextPositioningElement SVGTextElementBase;

class SVGTextElement MOZ_FINAL : public SVGTextElementBase
{
protected:
  SVGTextElement(already_AddRefed<nsINodeInfo>& aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx,
                             JS::Handle<JSObject*> scope) MOZ_OVERRIDE;

  friend nsresult (::NS_NewSVGTextElement(nsIContent **aResult,
                                          already_AddRefed<nsINodeInfo>&& aNodeInfo));

public:
  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:
  virtual EnumAttributesInfo GetEnumInfo() MOZ_OVERRIDE;
  virtual LengthAttributesInfo GetLengthInfo() MOZ_OVERRIDE;

  nsSVGEnum mEnumAttributes[1];
  virtual nsSVGEnum* EnumAttributes() MOZ_OVERRIDE
    { return mEnumAttributes; }

  nsSVGLength2 mLengthAttributes[1];
  virtual nsSVGLength2* LengthAttributes() MOZ_OVERRIDE
    { return mLengthAttributes; }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGTextElement_h
