/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGTextPathElement_h
#define mozilla_dom_SVGTextPathElement_h

#include "nsSVGEnum.h"
#include "nsSVGLength2.h"
#include "nsSVGString.h"
#include "mozilla/dom/SVGAnimatedPathSegList.h"
#include "mozilla/dom/SVGTextContentElement.h"

class nsAtom;
class nsIContent;

nsresult NS_NewSVGTextPathElement(nsIContent **aResult,
                                  already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

// textPath side types
static const uint16_t TEXTPATH_SIDETYPE_LEFT    = 1;
static const uint16_t TEXTPATH_SIDETYPE_RIGHT   = 2;

typedef SVGTextContentElement SVGTextPathElementBase;

class SVGTextPathElement final : public SVGTextPathElementBase
{
friend class ::SVGTextFrame;

protected:
  friend nsresult (::NS_NewSVGTextPathElement(nsIContent **aResult,
                                              already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGTextPathElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

public:
  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  SVGAnimatedPathSegList* GetAnimPathSegList() override {
    return &mPath;
  }

  nsAtom* GetPathDataAttrName() const override {
    return nsGkAtoms::path;
  }

  // WebIDL
  already_AddRefed<SVGAnimatedLength> StartOffset();
  already_AddRefed<SVGAnimatedEnumeration> Method();
  already_AddRefed<SVGAnimatedEnumeration> Spacing();
  already_AddRefed<SVGAnimatedEnumeration> Side();
  already_AddRefed<SVGAnimatedString> Href();

  void HrefAsString(nsAString& aHref);

 protected:

  virtual LengthAttributesInfo GetLengthInfo() override;
  virtual EnumAttributesInfo GetEnumInfo() override;
  virtual StringAttributesInfo GetStringInfo() override;

  enum { /* TEXTLENGTH, */ STARTOFFSET = 1 };
  nsSVGLength2 mLengthAttributes[2];
  virtual nsSVGLength2* LengthAttributes() override
    { return mLengthAttributes; }
  static LengthInfo sLengthInfo[2];

  enum { /* LENGTHADJUST, */ METHOD = 1, SPACING, SIDE };
  nsSVGEnum mEnumAttributes[4];
  virtual nsSVGEnum* EnumAttributes() override
    { return mEnumAttributes; }
  static nsSVGEnumMapping sMethodMap[];
  static nsSVGEnumMapping sSpacingMap[];
  static nsSVGEnumMapping sSideMap[];
  static EnumInfo sEnumInfo[4];

  enum { HREF, XLINK_HREF };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];

  SVGAnimatedPathSegList mPath;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGTextPathElement_h
