/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGIMAGEELEMENT_H_
#define DOM_SVG_SVGIMAGEELEMENT_H_

#include "nsImageLoadingContent.h"
#include "mozilla/dom/SVGAnimatedLength.h"
#include "mozilla/dom/SVGAnimatedString.h"
#include "mozilla/dom/SVGGeometryElement.h"
#include "mozilla/dom/SVGAnimatedPreserveAspectRatio.h"
#include "mozilla/gfx/2D.h"

nsresult NS_NewSVGImageElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
class SVGImageFrame;

namespace dom {
class DOMSVGAnimatedPreserveAspectRatio;

using SVGImageElementBase = SVGGraphicsElement;

class SVGImageElement final : public SVGImageElementBase,
                              public nsImageLoadingContent {
  friend class mozilla::SVGImageFrame;

 protected:
  explicit SVGImageElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  virtual ~SVGImageElement();
  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;
  friend nsresult(::NS_NewSVGImageElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED

  // EventTarget
  void AsyncEventRunning(AsyncEventDispatcher* aEvent) override;

  // nsImageLoadingContent interface
  CORSMode GetCORSMode() override;

  // nsIContent interface
  bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                      const nsAString& aValue,
                      nsIPrincipal* aMaybeScriptedPrincipal,
                      nsAttrValue& aResult) override;
  void AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                    const nsAttrValue* aValue, const nsAttrValue* aOldValue,
                    nsIPrincipal* aSubjectPrincipal, bool aNotify) override;

  nsresult BindToTree(BindContext&, nsINode& aParent) override;
  void UnbindFromTree(bool aNullParent) override;

  void DestroyContent() override;

  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* name) const override;

  // SVGSVGElement methods:
  bool HasValidDimensions() const override;

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  void MaybeLoadSVGImage();

  // WebIDL
  already_AddRefed<DOMSVGAnimatedLength> X();
  already_AddRefed<DOMSVGAnimatedLength> Y();
  already_AddRefed<DOMSVGAnimatedLength> Width();
  already_AddRefed<DOMSVGAnimatedLength> Height();
  already_AddRefed<DOMSVGAnimatedPreserveAspectRatio> PreserveAspectRatio();
  already_AddRefed<DOMSVGAnimatedString> Href();
  void GetCrossOrigin(nsAString& aCrossOrigin) {
    // Null for both missing and invalid defaults is ok, since we
    // always parse to an enum value, so we don't need an invalid
    // default, and we _want_ the missing default to be null.
    GetEnumAttr(nsGkAtoms::crossorigin, nullptr, aCrossOrigin);
  }
  void SetCrossOrigin(const nsAString& aCrossOrigin, ErrorResult& aError) {
    SetOrRemoveNullableStringAttr(nsGkAtoms::crossorigin, aCrossOrigin, aError);
  }

  void SetDecoding(const nsAString& aDecoding, ErrorResult& aError) {
    SetAttr(nsGkAtoms::decoding, aDecoding, aError);
  }
  void GetDecoding(nsAString& aValue);

  already_AddRefed<Promise> Decode(ErrorResult& aRv);

  static nsCSSPropertyID GetCSSPropertyIdForAttrEnum(uint8_t aAttrEnum);

  gfx::Rect GeometryBounds(const gfx::Matrix& aToBoundsSpace);

 protected:
  nsresult LoadSVGImage(bool aForce, bool aNotify);
  bool ShouldLoadImage() const;

  LengthAttributesInfo GetLengthInfo() override;
  SVGAnimatedPreserveAspectRatio* GetAnimatedPreserveAspectRatio() override;
  StringAttributesInfo GetStringInfo() override;

  // Override for nsImageLoadingContent.
  nsIContent* AsContent() override { return this; }

  enum { ATTR_X, ATTR_Y, ATTR_WIDTH, ATTR_HEIGHT };
  SVGAnimatedLength mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];

  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;

  enum { HREF, XLINK_HREF };
  SVGAnimatedString mStringAttributes[2];
  static StringInfo sStringInfo[2];
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGIMAGEELEMENT_H_
