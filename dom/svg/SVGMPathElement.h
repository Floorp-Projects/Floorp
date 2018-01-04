/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGMPathElement_h
#define mozilla_dom_SVGMPathElement_h

#include "mozilla/dom/IDTracker.h"
#include "nsSVGElement.h"
#include "nsStubMutationObserver.h"
#include "nsSVGString.h"

nsresult NS_NewSVGMPathElement(nsIContent **aResult,
                               already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

typedef nsSVGElement SVGMPathElementBase;

namespace mozilla {
namespace dom {
class SVGPathElement;

class SVGMPathElement final : public SVGMPathElementBase,
                              public nsStubMutationObserver
{
protected:
  friend nsresult (::NS_NewSVGMPathElement(nsIContent **aResult,
                                           already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGMPathElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  ~SVGMPathElement();

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SVGMPathElement,
                                           SVGMPathElementBase)

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED

  // nsIContent interface
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep, bool aNullParent) override;

  // Element specializations
  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsAtom* aAttribute,
                                const nsAString& aValue,
                                nsIPrincipal* aMaybeScriptedPrincipal,
                                nsAttrValue& aResult) override;
  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aMaybeScriptedPrincipal,
                                bool aNotify) override;

  // Public helper method: If our xlink:href attribute links to a <path>
  // element, this method returns a pointer to that element. Otherwise,
  // this returns nullptr.
  SVGPathElement* GetReferencedPath();

  // WebIDL
  already_AddRefed<SVGAnimatedString> Href();

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
      : mMpathElement(aMpathElement)
    {}
  protected:
    // We need to be notified when target changes, in order to request a sample
    // (which will clear animation effects that used the old target-path
    // and recompute the animation effects using the new target-path).
    virtual void ElementChanged(Element* aFrom, Element* aTo) override {
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
    virtual bool IsPersistent() override { return true; }
  private:
    SVGMPathElement* const mMpathElement;
  };

  virtual StringAttributesInfo GetStringInfo() override;

  void UpdateHrefTarget(nsIContent* aParent, const nsAString& aHrefStr);
  void UnlinkHrefTarget(bool aNotifyParent);
  void NotifyParentOfMpathChange(nsIContent* aParent);

  enum { HREF, XLINK_HREF };
  nsSVGString        mStringAttributes[2];
  static StringInfo  sStringInfo[2];
  PathElementTracker mPathTracker;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGMPathElement_h
