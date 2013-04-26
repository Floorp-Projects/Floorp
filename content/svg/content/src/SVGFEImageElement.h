/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEImageElement_h
#define mozilla_dom_SVGFEImageElement_h

#include "nsSVGFilters.h"

class SVGFEImageFrame;

nsresult NS_NewSVGFEImageElement(nsIContent **aResult,
                                 already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

typedef nsSVGFE SVGFEImageElementBase;

class SVGFEImageElement : public SVGFEImageElementBase,
                          public nsImageLoadingContent
{
  friend class ::SVGFEImageFrame;

protected:
  friend nsresult (::NS_NewSVGFEImageElement(nsIContent **aResult,
                                             already_AddRefed<nsINodeInfo> aNodeInfo));
  SVGFEImageElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~SVGFEImageElement();
  virtual JSObject* WrapNode(JSContext *aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

public:
  virtual bool SubregionIsUnionOfRegions() { return false; }

  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);
  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const;
  virtual nsSVGString& GetResultImageName() { return mStringAttributes[RESULT]; }
  virtual nsIntRect ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
          const nsSVGFilterInstance& aInstance);

  // nsIContent
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify);
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep, bool aNullParent);
  virtual nsEventStates IntrinsicState() const;

  NS_IMETHODIMP Notify(imgIRequest *aRequest, int32_t aType, const nsIntRect* aData);

  void MaybeLoadSVGImage();

  // WebIDL
  already_AddRefed<nsIDOMSVGAnimatedString> Href();
  already_AddRefed<DOMSVGAnimatedPreserveAspectRatio> PreserveAspectRatio();

private:
  // Invalidate users of the filter containing this element.
  void Invalidate();

  nsresult LoadSVGImage(bool aForce, bool aNotify);

protected:
  virtual bool OperatesOnSRGB(nsSVGFilterInstance*,
                                int32_t, Image*) { return true; }

  virtual SVGAnimatedPreserveAspectRatio *GetPreserveAspectRatio();
  virtual StringAttributesInfo GetStringInfo();

  enum { RESULT, HREF };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];

  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;
};

} // namespace dom
} // namespace mozilla

#endif
