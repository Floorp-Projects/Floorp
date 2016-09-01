/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
                               already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

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
  explicit SVGImageElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  virtual ~SVGImageElement();
  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;
  friend nsresult (::NS_NewSVGImageElement(nsIContent **aResult,
                                           already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED

  // nsIContent interface
  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify) override;
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep, bool aNullParent) override;

  virtual EventStates IntrinsicState() const override;

  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* name) const override;

  // nsSVGPathGeometryElement methods:
  virtual bool GetGeometryBounds(Rect* aBounds, const StrokeOptions& aStrokeOptions,
                                 const Matrix& aToBoundsSpace,
                                 const Matrix* aToNonScalingStrokeSpace = nullptr) override;
  virtual already_AddRefed<Path> BuildPath(PathBuilder* aBuilder) override;

  // nsSVGSVGElement methods:
  virtual bool HasValidDimensions() const override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;

  nsresult CopyInnerTo(mozilla::dom::Element* aDest);

  void MaybeLoadSVGImage();

  // WebIDL
  already_AddRefed<SVGAnimatedLength> X();
  already_AddRefed<SVGAnimatedLength> Y();
  already_AddRefed<SVGAnimatedLength> Width();
  already_AddRefed<SVGAnimatedLength> Height();
  already_AddRefed<DOMSVGAnimatedPreserveAspectRatio> PreserveAspectRatio();
  already_AddRefed<SVGAnimatedString> Href();

protected:
  nsresult LoadSVGImage(bool aForce, bool aNotify);

  virtual LengthAttributesInfo GetLengthInfo() override;
  virtual SVGAnimatedPreserveAspectRatio *GetPreserveAspectRatio() override;
  virtual StringAttributesInfo GetStringInfo() override;

  enum { ATTR_X, ATTR_Y, ATTR_WIDTH, ATTR_HEIGHT };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];

  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;

  enum { HREF, XLINK_HREF };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGImageElement_h
