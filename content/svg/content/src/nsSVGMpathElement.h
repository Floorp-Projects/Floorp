/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SVGMPATHELEMENT_H_
#define NS_SVGMPATHELEMENT_H_

#include "nsIDOMSVGMpathElement.h"
#include "nsIDOMSVGURIReference.h"
#include "nsSVGElement.h"
#include "nsStubMutationObserver.h"
#include "nsSVGPathElement.h"
#include "nsSVGString.h"
#include "nsReferencedElement.h"


typedef nsSVGElement nsSVGMpathElementBase;

class nsSVGMpathElement : public nsSVGMpathElementBase,
                          public nsIDOMSVGMpathElement,
                          public nsIDOMSVGURIReference,
                          public nsStubMutationObserver
{
protected:
  friend nsresult NS_NewSVGMpathElement(nsIContent **aResult,
                                        already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGMpathElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  ~nsSVGMpathElement();


public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGMPATHELEMENT
  NS_DECL_NSIDOMSVGURIREFERENCE

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsSVGMpathElement,
                                           nsSVGMpathElementBase)

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED

  // Forward interface implementations to base class
  NS_FORWARD_NSIDOMNODE(nsSVGMpathElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGMpathElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGMpathElementBase::)

  // nsIContent interface
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep, bool aNullParent);

  virtual nsresult UnsetAttr(PRInt32 aNamespaceID, nsIAtom* aAttribute,
                             bool aNotify);
  // nsGenericElement specializations
  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);

  // Public helper method: If our xlink:href attribute links to a <path>
  // element, this method returns a pointer to that element. Otherwise,
  // this returns nsnull.
  nsSVGPathElement* GetReferencedPath();

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:
  class PathReference : public nsReferencedElement {
  public:
    PathReference(nsSVGMpathElement* aMpathElement) :
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
    nsSVGMpathElement* const mMpathElement;
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

#endif // NS_SVGMPATHELEMENT_H_
