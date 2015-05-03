/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGAltGlyphElement_h
#define mozilla_dom_SVGAltGlyphElement_h

#include "mozilla/dom/SVGTextPositioningElement.h"
#include "nsSVGString.h"

nsresult NS_NewSVGAltGlyphElement(nsIContent **aResult,
                                  already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

typedef SVGTextPositioningElement SVGAltGlyphElementBase;

class SVGAltGlyphElement final : public SVGAltGlyphElementBase
{
protected:
  friend nsresult (::NS_NewSVGAltGlyphElement(nsIContent **aResult,
                                              already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGAltGlyphElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

public:
  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;

  // WebIDL
  already_AddRefed<SVGAnimatedString> Href();
  void GetGlyphRef(nsAString & aGlyphRef);
  void SetGlyphRef(const nsAString & aGlyphRef, ErrorResult& rv);
  void GetFormat(nsAString & aFormat);
  void SetFormat(const nsAString & aFormat, ErrorResult& rv);

protected:

  // nsSVGElement overrides
  virtual EnumAttributesInfo GetEnumInfo() override;
  virtual LengthAttributesInfo GetLengthInfo() override;
  virtual StringAttributesInfo GetStringInfo() override;

  enum { HREF };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];

  nsSVGEnum mEnumAttributes[1];
  virtual nsSVGEnum* EnumAttributes() override
    { return mEnumAttributes; }

  nsSVGLength2 mLengthAttributes[1];
  virtual nsSVGLength2* LengthAttributes() override
    { return mLengthAttributes; }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGAltGlyphElement_h
