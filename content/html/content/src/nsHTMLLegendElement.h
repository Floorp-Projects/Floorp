/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsHTMLLegendElement_h___
#define nsHTMLLegendElement_h___

#include "nsIDOMHTMLLegendElement.h"
#include "nsGenericHTMLElement.h"

class nsHTMLLegendElement : public nsGenericHTMLElement,
                            public nsIDOMHTMLLegendElement
{
public:
  nsHTMLLegendElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLLegendElement();

  static nsHTMLLegendElement* FromContent(nsIContent *aContent)
  {
    if (aContent->IsHTML(nsGkAtoms::legend)) {
      return static_cast<nsHTMLLegendElement*>(aContent);
    }
    return nsnull;
  }

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLLegendElement
  NS_DECL_NSIDOMHTMLLEGENDELEMENT

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_BASIC(nsGenericHTMLElement::)
  NS_SCRIPTABLE NS_IMETHOD Click() {
    return nsGenericHTMLElement::Click();
  }
  NS_SCRIPTABLE NS_IMETHOD GetTabIndex(PRInt32* aTabIndex) {
    return nsGenericHTMLElement::GetTabIndex(aTabIndex);
  }
  NS_SCRIPTABLE NS_IMETHOD SetTabIndex(PRInt32 aTabIndex) {
    return nsGenericHTMLElement::SetTabIndex(aTabIndex);
  }
  NS_SCRIPTABLE NS_IMETHOD Focus();
  NS_SCRIPTABLE NS_IMETHOD GetDraggable(bool* aDraggable) {
    return nsGenericHTMLElement::GetDraggable(aDraggable);
  }
  NS_SCRIPTABLE NS_IMETHOD GetInnerHTML(nsAString& aInnerHTML) {
    return nsGenericHTMLElement::GetInnerHTML(aInnerHTML);
  }
  NS_SCRIPTABLE NS_IMETHOD SetInnerHTML(const nsAString& aInnerHTML) {
    return nsGenericHTMLElement::SetInnerHTML(aInnerHTML);
  }

  virtual void PerformAccesskey(bool aKeyCausesActivation,
                                bool aIsTrustedEvent);

  // nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);
  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              PRInt32 aModType) const;
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

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  mozilla::dom::Element *GetFormElement()
  {
    nsCOMPtr<nsIFormControl> fieldsetControl = do_QueryInterface(GetFieldSet());

    return fieldsetControl ? fieldsetControl->GetFormElement() : nsnull;
  }

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:
  /**
   * Get the fieldset content element that contains this legend.
   * Returns null if there is no fieldset containing this legend.
   */
  nsIContent* GetFieldSet();
};

#endif /* nsHTMLLegendElement_h___ */
