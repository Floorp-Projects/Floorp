/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGImageElement_h
#define mozilla_dom_SVGImageElement_h

#include "nsImageLoadingContent.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedString.h"
#include "SVGGeometryElement.h"
#include "SVGAnimatedPreserveAspectRatio.h"

nsresult NS_NewSVGImageElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

class nsSVGImageFrame;

namespace mozilla {
namespace dom {
class DOMSVGAnimatedPreserveAspectRatio;

typedef SVGGeometryElement SVGImageElementBase;

class SVGImageElement : public SVGImageElementBase,
                        public nsImageLoadingContent {
  friend class ::nsSVGImageFrame;

 protected:
  explicit SVGImageElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  virtual ~SVGImageElement();
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;
  friend nsresult(::NS_NewSVGImageElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED

  // EventTarget
  virtual void AsyncEventRunning(AsyncEventDispatcher* aEvent) override;

  // nsIContent interface
  bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                      const nsAString& aValue,
                      nsIPrincipal* aMaybeScriptedPrincipal,
                      nsAttrValue& aResult) override;
  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;
  virtual nsresult BindToTree(Document* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent) override;
  virtual void UnbindFromTree(bool aDeep, bool aNullParent) override;

  virtual EventStates IntrinsicState() const override;

  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* name) const override;

  // SVGGeometryElement methods:
  virtual bool GetGeometryBounds(
      Rect* aBounds, const StrokeOptions& aStrokeOptions,
      const Matrix& aToBoundsSpace,
      const Matrix* aToNonScalingStrokeSpace = nullptr) override;
  virtual already_AddRefed<Path> BuildPath(PathBuilder* aBuilder) override;

  // nsSVGSVGElement methods:
  virtual bool HasValidDimensions() const override;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  nsresult CopyInnerTo(mozilla::dom::Element* aDest);

  void MaybeLoadSVGImage();

  // WebIDL
  already_AddRefed<DOMSVGAnimatedLength> X();
  already_AddRefed<DOMSVGAnimatedLength> Y();
  already_AddRefed<DOMSVGAnimatedLength> Width();
  already_AddRefed<DOMSVGAnimatedLength> Height();
  already_AddRefed<DOMSVGAnimatedPreserveAspectRatio> PreserveAspectRatio();
  already_AddRefed<DOMSVGAnimatedString> Href();

  void SetDecoding(const nsAString& aDecoding, ErrorResult& aError) {
    SetAttr(nsGkAtoms::decoding, aDecoding, aError);
  }
  void GetDecoding(nsAString& aValue);

  already_AddRefed<Promise> Decode(ErrorResult& aRv);

 protected:
  nsresult LoadSVGImage(bool aForce, bool aNotify);

  virtual LengthAttributesInfo GetLengthInfo() override;
  virtual SVGAnimatedPreserveAspectRatio* GetAnimatedPreserveAspectRatio()
      override;
  virtual StringAttributesInfo GetStringInfo() override;

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

#endif  // mozilla_dom_SVGImageElement_h
