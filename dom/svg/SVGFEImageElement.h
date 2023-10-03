/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGFEIMAGEELEMENT_H_
#define DOM_SVG_SVGFEIMAGEELEMENT_H_

#include "mozilla/dom/SVGFilters.h"
#include "SVGAnimatedPreserveAspectRatio.h"

nsresult NS_NewSVGFEImageElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
class SVGFEImageFrame;

namespace dom {

using SVGFEImageElementBase = SVGFilterPrimitiveElement;

class SVGFEImageElement final : public SVGFEImageElementBase,
                                public nsImageLoadingContent {
  friend class mozilla::SVGFEImageFrame;

 protected:
  friend nsresult(::NS_NewSVGFEImageElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGFEImageElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  virtual ~SVGFEImageElement();
  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;

 public:
  bool SubregionIsUnionOfRegions() override { return false; }

  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // EventTarget
  void AsyncEventRunning(AsyncEventDispatcher* aEvent) override;

  FilterPrimitiveDescription GetPrimitiveDescription(
      SVGFilterInstance* aInstance, const IntRect& aFilterSubregion,
      const nsTArray<bool>& aInputsAreTainted,
      nsTArray<RefPtr<SourceSurface>>& aInputImages) override;
  bool AttributeAffectsRendering(int32_t aNameSpaceID,
                                 nsAtom* aAttribute) const override;
  SVGAnimatedString& GetResultImageName() override {
    return mStringAttributes[RESULT];
  }

  // nsImageLoadingContent
  CORSMode GetCORSMode() override;

  bool OutputIsTainted(const nsTArray<bool>& aInputsAreTainted,
                       nsIPrincipal* aReferencePrincipal) override;

  // nsIContent
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

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

  NS_DECL_IMGINOTIFICATIONOBSERVER

  // Override for nsIImageLoadingContent.
  NS_IMETHOD_(void) FrameCreated(nsIFrame* aFrame) override;

  void MaybeLoadSVGImage();

  // WebIDL
  already_AddRefed<DOMSVGAnimatedString> Href();
  already_AddRefed<DOMSVGAnimatedPreserveAspectRatio> PreserveAspectRatio();
  void GetCrossOrigin(nsAString& aCrossOrigin) {
    // Null for both missing and invalid defaults is ok, since we
    // always parse to an enum value, so we don't need an invalid
    // default, and we _want_ the missing default to be null.
    GetEnumAttr(nsGkAtoms::crossorigin, nullptr, aCrossOrigin);
  }
  void SetCrossOrigin(const nsAString& aCrossOrigin, ErrorResult& aError) {
    SetOrRemoveNullableStringAttr(nsGkAtoms::crossorigin, aCrossOrigin, aError);
  }

 private:
  nsresult LoadSVGImage(bool aForce, bool aNotify);
  bool ShouldLoadImage() const;

 protected:
  bool ProducesSRGB() override { return true; }

  SVGAnimatedPreserveAspectRatio* GetAnimatedPreserveAspectRatio() override;
  StringAttributesInfo GetStringInfo() override;

  // Override for nsImageLoadingContent.
  nsIContent* AsContent() override { return this; }

  enum { RESULT, HREF, XLINK_HREF };
  SVGAnimatedString mStringAttributes[3];
  static StringInfo sStringInfo[3];

  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;
  uint16_t mImageAnimationMode;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGFEIMAGEELEMENT_H_
