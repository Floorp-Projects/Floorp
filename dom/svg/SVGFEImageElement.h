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

using SVGFEImageElementBase = SVGFE;

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
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  virtual bool SubregionIsUnionOfRegions() override { return false; }

  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // EventTarget
  virtual void AsyncEventRunning(AsyncEventDispatcher* aEvent) override;

  virtual FilterPrimitiveDescription GetPrimitiveDescription(
      SVGFilterInstance* aInstance, const IntRect& aFilterSubregion,
      const nsTArray<bool>& aInputsAreTainted,
      nsTArray<RefPtr<SourceSurface>>& aInputImages) override;
  virtual bool AttributeAffectsRendering(int32_t aNameSpaceID,
                                         nsAtom* aAttribute) const override;
  virtual SVGAnimatedString& GetResultImageName() override {
    return mStringAttributes[RESULT];
  }
  virtual bool OutputIsTainted(const nsTArray<bool>& aInputsAreTainted,
                               nsIPrincipal* aReferencePrincipal) override;

  // nsIContent
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;
  virtual nsresult BindToTree(BindContext&, nsINode& aParent) override;
  virtual void UnbindFromTree(bool aNullParent) override;
  virtual EventStates IntrinsicState() const override;
  virtual void DestroyContent() override;

  void Notify(imgIRequest* aRequest, int32_t aType,
              const nsIntRect* aData) override;

  // Override for nsIImageLoadingContent.
  NS_IMETHOD_(void) FrameCreated(nsIFrame* aFrame) override;

  void MaybeLoadSVGImage();

  // WebIDL
  already_AddRefed<DOMSVGAnimatedString> Href();
  already_AddRefed<DOMSVGAnimatedPreserveAspectRatio> PreserveAspectRatio();

 private:
  nsresult LoadSVGImage(bool aForce, bool aNotify);

 protected:
  virtual bool ProducesSRGB() override { return true; }

  virtual SVGAnimatedPreserveAspectRatio* GetAnimatedPreserveAspectRatio()
      override;
  virtual StringAttributesInfo GetStringInfo() override;

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
