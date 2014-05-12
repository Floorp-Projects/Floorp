/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGImageElement_h
#define mozilla_dom_SVGImageElement_h

#include "nsImageLoadingContent.h"
#include "nsSVGLength2.h"
#include "nsSVGPathGeometryElement.h"
#include "nsSVGString.h"
#include "SVGAnimatedPreserveAspectRatio.h"

nsresult NS_NewSVGImageElement(nsIContent **aResult,
                               already_AddRefed<nsINodeInfo>&& aNodeInfo);

typedef nsSVGPathGeometryElement SVGImageElementBase;

class nsSVGImageFrame;

namespace mozilla {
namespace dom {
class DOMSVGAnimatedPreserveAspectRatio;

class SVGImageElement : public SVGImageElementBase,
                        public nsImageLoadingContent
{
  friend class ::nsSVGImageFrame;

protected:
  SVGImageElement(already_AddRefed<nsINodeInfo>& aNodeInfo);
  virtual ~SVGImageElement();
  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;
  friend nsresult (::NS_NewSVGImageElement(nsIContent **aResult,
                                           already_AddRefed<nsINodeInfo>&& aNodeInfo));

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED

  // nsIContent interface
  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify) MOZ_OVERRIDE;
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE;
  virtual void UnbindFromTree(bool aDeep, bool aNullParent) MOZ_OVERRIDE;

  virtual EventStates IntrinsicState() const MOZ_OVERRIDE;

  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* name) const MOZ_OVERRIDE;

  // nsSVGPathGeometryElement methods:
  virtual void ConstructPath(gfxContext *aCtx) MOZ_OVERRIDE;
  virtual TemporaryRef<Path> BuildPath() MOZ_OVERRIDE;

  // nsSVGSVGElement methods:
  virtual bool HasValidDimensions() const MOZ_OVERRIDE;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  nsresult CopyInnerTo(mozilla::dom::Element* aDest);

  void MaybeLoadSVGImage();

  bool IsImageSrcSetDisabled() const;

  // WebIDL
  already_AddRefed<SVGAnimatedLength> X();
  already_AddRefed<SVGAnimatedLength> Y();
  already_AddRefed<SVGAnimatedLength> Width();
  already_AddRefed<SVGAnimatedLength> Height();
  already_AddRefed<DOMSVGAnimatedPreserveAspectRatio> PreserveAspectRatio();
  already_AddRefed<SVGAnimatedString> Href();

protected:
  nsresult LoadSVGImage(bool aForce, bool aNotify);

  virtual LengthAttributesInfo GetLengthInfo() MOZ_OVERRIDE;
  virtual SVGAnimatedPreserveAspectRatio *GetPreserveAspectRatio() MOZ_OVERRIDE;
  virtual StringAttributesInfo GetStringInfo() MOZ_OVERRIDE;

  enum { ATTR_X, ATTR_Y, ATTR_WIDTH, ATTR_HEIGHT };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];

  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;

  enum { HREF };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGImageElement_h
