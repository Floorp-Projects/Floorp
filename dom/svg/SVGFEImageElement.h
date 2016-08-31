/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEImageElement_h
#define mozilla_dom_SVGFEImageElement_h

#include "nsSVGFilters.h"
#include "SVGAnimatedPreserveAspectRatio.h"

class SVGFEImageFrame;

nsresult NS_NewSVGFEImageElement(nsIContent **aResult,
                                 already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

typedef nsSVGFE SVGFEImageElementBase;

class SVGFEImageElement final : public SVGFEImageElementBase,
                                public nsImageLoadingContent
{
  friend class ::SVGFEImageFrame;

protected:
  friend nsresult (::NS_NewSVGFEImageElement(nsIContent **aResult,
                                             already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGFEImageElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  virtual ~SVGFEImageElement();
  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

public:
  virtual bool SubregionIsUnionOfRegions() override { return false; }

  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  virtual FilterPrimitiveDescription
    GetPrimitiveDescription(nsSVGFilterInstance* aInstance,
                            const IntRect& aFilterSubregion,
                            const nsTArray<bool>& aInputsAreTainted,
                            nsTArray<RefPtr<SourceSurface>>& aInputImages) override;
  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const override;
  virtual nsSVGString& GetResultImageName() override { return mStringAttributes[RESULT]; }
  virtual bool OutputIsTainted(const nsTArray<bool>& aInputsAreTainted,
                               nsIPrincipal* aReferencePrincipal) override;

  // nsIContent
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;

  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify) override;
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep, bool aNullParent) override;
  virtual EventStates IntrinsicState() const override;

  NS_IMETHOD Notify(imgIRequest *aRequest, int32_t aType, const nsIntRect* aData) override;

  void MaybeLoadSVGImage();

  // WebIDL
  already_AddRefed<SVGAnimatedString> Href();
  already_AddRefed<DOMSVGAnimatedPreserveAspectRatio> PreserveAspectRatio();

private:
  // Invalidate users of the filter containing this element.
  void Invalidate();

  nsresult LoadSVGImage(bool aForce, bool aNotify);

protected:
  virtual bool ProducesSRGB() override { return true; }

  virtual SVGAnimatedPreserveAspectRatio *GetPreserveAspectRatio() override;
  virtual StringAttributesInfo GetStringInfo() override;

  enum { RESULT, HREF, XLINK_HREF };
  nsSVGString mStringAttributes[3];
  static StringInfo sStringInfo[3];

  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;
};

} // namespace dom
} // namespace mozilla

#endif
