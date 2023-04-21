/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGMPATHELEMENT_H_
#define DOM_SVG_SVGMPATHELEMENT_H_

#include "mozilla/dom/IDTracker.h"
#include "mozilla/dom/SVGElement.h"
#include "nsStubMutationObserver.h"
#include "SVGAnimatedString.h"

nsresult NS_NewSVGMPathElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla::dom {
class SVGGeometryElement;

using SVGMPathElementBase = SVGElement;

class SVGMPathElement final : public SVGMPathElementBase,
                              public nsStubMutationObserver {
 protected:
  friend nsresult(::NS_NewSVGMPathElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGMPathElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  ~SVGMPathElement();

  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;

 public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SVGMPathElement, SVGMPathElementBase)

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED

  // nsIContent interface
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;
  nsresult BindToTree(BindContext&, nsINode& aParent) override;
  void UnbindFromTree(bool aNullParent) override;

  // Element specializations
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

 protected:
  /**
   * Helper that provides a reference to the 'path' element with the ID that is
   * referenced by the 'mpath' element's 'href' attribute, and that will
   * invalidate the parent of the 'mpath' and update mutation observers to the
   * new path element if the element that that ID identifies changes to a
   * different element (or none).
   */
  class PathElementTracker final : public IDTracker {
   public:
    explicit PathElementTracker(SVGMPathElement* aMpathElement)
        : mMpathElement(aMpathElement) {}

   protected:
    // We need to be notified when target changes, in order to request a sample
    // (which will clear animation effects that used the old target-path
    // and recompute the animation effects using the new target-path).
    void ElementChanged(Element* aFrom, Element* aTo) override {
      IDTracker::ElementChanged(aFrom, aTo);
      if (aFrom) {
        aFrom->RemoveMutationObserver(mMpathElement);
      }
      if (aTo) {
        aTo->AddMutationObserver(mMpathElement);
      }
      mMpathElement->NotifyParentOfMpathChange(mMpathElement->GetParent());
    }

    // We need to override IsPersistent to get persistent tracking (beyond the
    // first time the target changes)
    bool IsPersistent() override { return true; }

   private:
    SVGMPathElement* const mMpathElement;
  };

  StringAttributesInfo GetStringInfo() override;

  void UpdateHrefTarget(nsIContent* aParent, const nsAString& aHrefStr);
  void UnlinkHrefTarget(bool aNotifyParent);
  void NotifyParentOfMpathChange(nsIContent* aParent);

  enum { HREF, XLINK_HREF };
  SVGAnimatedString mStringAttributes[2];
  static StringInfo sStringInfo[2];
  PathElementTracker mPathTracker;
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGMPATHELEMENT_H_
