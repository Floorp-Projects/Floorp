/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGTEXTPATHELEMENT_H_
#define DOM_SVG_SVGTEXTPATHELEMENT_H_

#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedPathSegList.h"
#include "SVGAnimatedString.h"
#include "mozilla/dom/SVGTextContentElement.h"

class nsAtom;
class nsIContent;

nsresult NS_NewSVGTextPathElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla::dom {

// textPath side types
static const uint16_t TEXTPATH_SIDETYPE_LEFT = 1;
static const uint16_t TEXTPATH_SIDETYPE_RIGHT = 2;

using SVGTextPathElementBase = SVGTextContentElement;

class SVGTextPathElement final : public SVGTextPathElementBase {
  friend class mozilla::SVGTextFrame;

 protected:
  friend nsresult(::NS_NewSVGTextPathElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGTextPathElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  JSObject* WrapNode(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;

 public:
  // nsIContent interface
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  SVGAnimatedPathSegList* GetAnimPathSegList() override { return &mPath; }

  nsStaticAtom* GetPathDataAttrName() const override { return nsGkAtoms::path; }

  // WebIDL
  already_AddRefed<DOMSVGAnimatedLength> StartOffset();
  already_AddRefed<DOMSVGAnimatedEnumeration> Method();
  already_AddRefed<DOMSVGAnimatedEnumeration> Spacing();
  already_AddRefed<DOMSVGAnimatedEnumeration> Side();
  already_AddRefed<DOMSVGAnimatedString> Href();

  void HrefAsString(nsAString& aHref);

 protected:
  LengthAttributesInfo GetLengthInfo() override;
  EnumAttributesInfo GetEnumInfo() override;
  StringAttributesInfo GetStringInfo() override;

  enum { /* TEXTLENGTH, */ STARTOFFSET = 1 };
  SVGAnimatedLength mLengthAttributes[2];
  SVGAnimatedLength* LengthAttributes() override { return mLengthAttributes; }
  static LengthInfo sLengthInfo[2];

  enum { /* LENGTHADJUST, */ METHOD = 1, SPACING, SIDE };
  SVGAnimatedEnumeration mEnumAttributes[4];
  SVGAnimatedEnumeration* EnumAttributes() override { return mEnumAttributes; }
  static SVGEnumMapping sMethodMap[];
  static SVGEnumMapping sSpacingMap[];
  static SVGEnumMapping sSideMap[];
  static EnumInfo sEnumInfo[4];

  enum { HREF, XLINK_HREF };
  SVGAnimatedString mStringAttributes[2];
  static StringInfo sStringInfo[2];

  SVGAnimatedPathSegList mPath;
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGTEXTPATHELEMENT_H_
