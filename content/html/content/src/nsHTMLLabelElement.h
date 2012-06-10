/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Declaration of HTML <label> elements.
 */
#ifndef nsHTMLLabelElement_h
#define nsHTMLLabelElement_h

#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLLabelElement.h"

class nsHTMLLabelElement : public nsGenericHTMLFormElement,
                           public nsIDOMHTMLLabelElement
{
public:
  nsHTMLLabelElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLLabelElement();

  static nsHTMLLabelElement* FromContent(nsIContent* aPossibleLabel)
  {
    if (aPossibleLabel->IsHTML(nsGkAtoms::label)) {
      return static_cast<nsHTMLLabelElement*>(aPossibleLabel);
    }

    return nsnull;
  }

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLFormElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLLabelElement
  NS_DECL_NSIDOMHTMLLABELELEMENT

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_BASIC(nsGenericHTMLFormElement::)
  NS_SCRIPTABLE NS_IMETHOD Click() {
    return nsGenericHTMLFormElement::Click();
  }
  NS_SCRIPTABLE NS_IMETHOD GetTabIndex(PRInt32* aTabIndex) {
    return nsGenericHTMLFormElement::GetTabIndex(aTabIndex);
  }
  NS_SCRIPTABLE NS_IMETHOD SetTabIndex(PRInt32 aTabIndex) {
    return nsGenericHTMLFormElement::SetTabIndex(aTabIndex);
  }
  NS_SCRIPTABLE NS_IMETHOD Focus();
  NS_SCRIPTABLE NS_IMETHOD GetDraggable(bool* aDraggable) {
    return nsGenericHTMLFormElement::GetDraggable(aDraggable);
  }
  NS_SCRIPTABLE NS_IMETHOD GetInnerHTML(nsAString& aInnerHTML) {
    return nsGenericHTMLFormElement::GetInnerHTML(aInnerHTML);
  }
  NS_SCRIPTABLE NS_IMETHOD SetInnerHTML(const nsAString& aInnerHTML) {
    return nsGenericHTMLFormElement::SetInnerHTML(aInnerHTML);
  }

  // nsIFormControl
  NS_IMETHOD_(PRUint32) GetType() const { return NS_FORM_LABEL; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsFormSubmission* aFormSubmission);

  virtual bool IsDisabled() const { return false; }

  // nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);

  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);

  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify);
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify);
  virtual void PerformAccesskey(bool aKeyCausesActivation,
                                bool aIsTrustedEvent);
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

  mozilla::dom::Element* GetLabeledElement();
protected:
  mozilla::dom::Element* GetFirstDescendantFormControl();

  // XXX It would be nice if we could use an event flag instead.
  bool mHandlingEvent;
};

#endif /* nsHTMLLabelElement_h */
