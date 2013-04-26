/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGMPathElement_h
#define mozilla_dom_SVGMPathElement_h

#include "nsSVGElement.h"
#include "nsStubMutationObserver.h"
#include "nsSVGString.h"
#include "nsReferencedElement.h"

nsresult NS_NewSVGMPathElement(nsIContent **aResult,
                               already_AddRefed<nsINodeInfo> aNodeInfo);

typedef nsSVGElement SVGMPathElementBase;

namespace mozilla {
namespace dom {
class SVGPathElement;

class SVGMPathElement MOZ_FINAL : public SVGMPathElementBase,
                                  public nsStubMutationObserver
{
protected:
  friend nsresult (::NS_NewSVGMPathElement(nsIContent **aResult,
                                           already_AddRefed<nsINodeInfo> aNodeInfo));
  SVGMPathElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  ~SVGMPathElement();

  virtual JSObject* WrapNode(JSContext *aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SVGMPathElement,
                                           SVGMPathElementBase)

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED

  // nsIContent interface
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep, bool aNullParent);

  virtual nsresult UnsetAttr(int32_t aNamespaceID, nsIAtom* aAttribute,
                             bool aNotify);
  // Element specializations
  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);

  // Public helper method: If our xlink:href attribute links to a <path>
  // element, this method returns a pointer to that element. Otherwise,
  // this returns nullptr.
  SVGPathElement* GetReferencedPath();

  // WebIDL
  already_AddRefed<nsIDOMSVGAnimatedString> Href();

protected:
  class PathReference : public nsReferencedElement {
  public:
    PathReference(SVGMPathElement* aMpathElement) :
      mMpathElement(aMpathElement) {}
  protected:
    // We need to be notified when target changes, in order to request a sample
    // (which will clear animation effects that used the old target-path
    // and recompute the animation effects using the new target-path).
    virtual void ElementChanged(Element* aFrom, Element* aTo) {
      nsReferencedElement::ElementChanged(aFrom, aTo);
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
    virtual bool IsPersistent() { return true; }
  private:
    SVGMPathElement* const mMpathElement;
  };

  virtual StringAttributesInfo GetStringInfo();

  void UpdateHrefTarget(nsIContent* aParent, const nsAString& aHrefStr);
  void UnlinkHrefTarget(bool aNotifyParent);
  void NotifyParentOfMpathChange(nsIContent* aParent);

  enum { HREF };
  nsSVGString        mStringAttributes[1];
  static StringInfo  sStringInfo[1];
  PathReference      mHrefTarget;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGMPathElement_h
