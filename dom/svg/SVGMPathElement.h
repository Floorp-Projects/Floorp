/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGMPATHELEMENT_H_
#define DOM_SVG_SVGMPATHELEMENT_H_

#include "mozilla/dom/SVGElement.h"
#include "SVGAnimatedString.h"

nsresult NS_NewSVGMPathElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla::dom {
class SVGGeometryElement;

using SVGMPathElementBase = SVGElement;

class SVGMPathElement final : public SVGMPathElementBase {
 protected:
  friend nsresult(::NS_NewSVGMPathElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGMPathElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  ~SVGMPathElement() = default;

  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;

 public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SVGMPathElement, SVGMPathElementBase)

  // nsIContent interface
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;
  void UnbindFromTree(bool aNullParent) override;
  void AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                    const nsAttrValue* aValue, const nsAttrValue* aOldValue,
                    nsIPrincipal* aMaybeScriptedPrincipal,
                    bool aNotify) override;

  // Public helper method: If our xlink:href attribute links to a Shape
  // element, this method returns a pointer to that element. Otherwise,
  // this returns nullptr.
  SVGGeometryElement* GetReferencedPath();

  // WebIDL
  already_AddRefed<DOMSVGAnimatedString> Href();

  void HrefAsString(nsAString& aHref);

  void NotifyParentOfMpathChange();

  RefPtr<nsISupports> mMPathObserver;

 protected:
  StringAttributesInfo GetStringInfo() override;

  enum { HREF, XLINK_HREF };
  SVGAnimatedString mStringAttributes[2];
  static StringInfo sStringInfo[2];
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGMPATHELEMENT_H_
