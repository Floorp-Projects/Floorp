/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHTMLFieldSetElement_h___
#define nsHTMLFieldSetElement_h___

#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLFieldSetElement.h"
#include "nsIConstraintValidation.h"


class nsHTMLFieldSetElement : public nsGenericHTMLFormElement,
                              public nsIDOMHTMLFieldSetElement,
                              public nsIConstraintValidation
{
public:
  using nsIConstraintValidation::GetValidationMessage;

  nsHTMLFieldSetElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLFieldSetElement();

  /** Typesafe, non-refcounting cast from nsIContent.  Cheaper than QI. **/
  static nsHTMLFieldSetElement* FromContent(nsIContent* aContent)
  {
    if (!aContent || !aContent->IsHTML(nsGkAtoms::fieldset)) {
      return nsnull;
    }
    return static_cast<nsHTMLFieldSetElement*>(aContent);
  }

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLFormElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLFieldSetElement
  NS_DECL_NSIDOMHTMLFIELDSETELEMENT

  // nsIContent
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult AfterSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify);

  virtual nsresult InsertChildAt(nsIContent* aChild, PRUint32 aIndex,
                                     bool aNotify);
  virtual void RemoveChildAt(PRUint32 aIndex, bool aNotify);

  // nsIFormControl
  NS_IMETHOD_(PRUint32) GetType() const { return NS_FORM_FIELDSET; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsFormSubmission* aFormSubmission);
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

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsHTMLFieldSetElement,
                                           nsGenericHTMLFormElement)
private:

  /**
   * Notify all elements (in mElements) that the first legend of the fieldset
   * has now changed.
   */
  void NotifyElementsForFirstLegendChange(bool aNotify);

  // This function is used to generate the nsContentList (listed form elements).
  static bool MatchListedElements(nsIContent* aContent, PRInt32 aNamespaceID,
                                    nsIAtom* aAtom, void* aData);

  // listed form controls elements.
  nsRefPtr<nsContentList> mElements;

  // List of elements which have this fieldset as first fieldset ancestor.
  nsTArray<nsGenericHTMLFormElement*> mDependentElements;

  nsIContent* mFirstLegend;
};

#endif /* nsHTMLFieldSetElement_h___ */

