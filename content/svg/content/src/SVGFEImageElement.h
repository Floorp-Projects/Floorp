/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEImageElement_h
#define mozilla_dom_SVGFEImageElement_h

#include "nsSVGFilters.h"
#include "SVGAnimatedPreserveAspectRatio.h"

class SVGFEImageFrame;

nsresult NS_NewSVGFEImageElement(nsIContent **aResult,
                                 already_AddRefed<nsINodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

typedef nsSVGFE SVGFEImageElementBase;

class SVGFEImageElement : public SVGFEImageElementBase,
                          public nsImageLoadingContent
{
  friend class ::SVGFEImageFrame;

protected:
  friend nsresult (::NS_NewSVGFEImageElement(nsIContent **aResult,
                                             already_AddRefed<nsINodeInfo>&& aNodeInfo));
  SVGFEImageElement(already_AddRefed<nsINodeInfo>& aNodeInfo);
  virtual ~SVGFEImageElement();
  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;

public:
  virtual bool SubregionIsUnionOfRegions() MOZ_OVERRIDE { return false; }

  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  virtual FilterPrimitiveDescription
    GetPrimitiveDescription(nsSVGFilterInstance* aInstance,
                            const IntRect& aFilterSubregion,
                            const nsTArray<bool>& aInputsAreTainted,
                            nsTArray<mozilla::RefPtr<SourceSurface>>& aInputImages) MOZ_OVERRIDE;
  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const MOZ_OVERRIDE;
  virtual nsSVGString& GetResultImageName() MOZ_OVERRIDE { return mStringAttributes[RESULT]; }
  virtual bool OutputIsTainted(const nsTArray<bool>& aInputsAreTainted,
                               nsIPrincipal* aReferencePrincipal) MOZ_OVERRIDE;

  // nsIContent
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const MOZ_OVERRIDE;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify) MOZ_OVERRIDE;
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE;
  virtual void UnbindFromTree(bool aDeep, bool aNullParent) MOZ_OVERRIDE;
  virtual EventStates IntrinsicState() const MOZ_OVERRIDE;

  NS_IMETHODIMP Notify(imgIRequest *aRequest, int32_t aType, const nsIntRect* aData) MOZ_OVERRIDE;

  void MaybeLoadSVGImage();

  // WebIDL
  already_AddRefed<SVGAnimatedString> Href();
  already_AddRefed<DOMSVGAnimatedPreserveAspectRatio> PreserveAspectRatio();

private:
  // Invalidate users of the filter containing this element.
  void Invalidate();

  nsresult LoadSVGImage(bool aForce, bool aNotify);

protected:
  virtual bool ProducesSRGB() MOZ_OVERRIDE { return true; }

  virtual SVGAnimatedPreserveAspectRatio *GetPreserveAspectRatio() MOZ_OVERRIDE;
  virtual StringAttributesInfo GetStringInfo() MOZ_OVERRIDE;

  enum { RESULT, HREF };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];

  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;
};

} // namespace dom
} // namespace mozilla

#endif
