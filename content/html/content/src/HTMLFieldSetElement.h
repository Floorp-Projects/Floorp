/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLFieldSetElement_h
#define mozilla_dom_HTMLFieldSetElement_h

#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLFieldSetElement.h"
#include "nsIConstraintValidation.h"

namespace mozilla {
namespace dom {

class HTMLFieldSetElement : public nsGenericHTMLFormElement,
                            public nsIDOMHTMLFieldSetElement,
                            public nsIConstraintValidation
{
public:
  using nsIConstraintValidation::GetValidationMessage;

  HTMLFieldSetElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~HTMLFieldSetElement();

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLFieldSetElement, fieldset)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  // nsIDOMHTMLFieldSetElement
  NS_DECL_NSIDOMHTMLFIELDSETELEMENT

  // nsIContent
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify);

  virtual nsresult InsertChildAt(nsIContent* aChild, uint32_t aIndex,
                                     bool aNotify);
  virtual void RemoveChildAt(uint32_t aIndex, bool aNotify);

  // nsIFormControl
  NS_IMETHOD_(uint32_t) GetType() const { return NS_FORM_FIELDSET; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsFormSubmission* aFormSubmission);
  virtual bool IsDisabledForEvents(uint32_t aMessage);
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual nsXPCClassInfo* GetClassInfo();
  virtual nsIDOMNode* AsDOMNode() { return this; }

  const nsIContent* GetFirstLegend() const { return mFirstLegend; }

  void AddElement(nsGenericHTMLFormElement* aElement) {
    mDependentElements.AppendElement(aElement);
  }

  void RemoveElement(nsGenericHTMLFormElement* aElement) {
    mDependentElements.RemoveElement(aElement);
  }

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLFieldSetElement,
                                           nsGenericHTMLFormElement)
private:

  /**
   * Notify all elements (in mElements) that the first legend of the fieldset
   * has now changed.
   */
  void NotifyElementsForFirstLegendChange(bool aNotify);

  // This function is used to generate the nsContentList (listed form elements).
  static bool MatchListedElements(nsIContent* aContent, int32_t aNamespaceID,
                                    nsIAtom* aAtom, void* aData);

  // listed form controls elements.
  nsRefPtr<nsContentList> mElements;

  // List of elements which have this fieldset as first fieldset ancestor.
  nsTArray<nsGenericHTMLFormElement*> mDependentElements;

  nsIContent* mFirstLegend;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLFieldSetElement_h */

