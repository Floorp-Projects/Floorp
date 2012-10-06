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

    return nullptr;
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
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLFormElement::)
  virtual void Focus(mozilla::ErrorResult& aError) MOZ_OVERRIDE;

  // nsIFormControl
  NS_IMETHOD_(uint32_t) GetType() const { return NS_FORM_LABEL; }
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

  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify);
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify);
  virtual void PerformAccesskey(bool aKeyCausesActivation,
                                bool aIsTrustedEvent);
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

  mozilla::dom::Element* GetLabeledElement();
protected:
  mozilla::dom::Element* GetFirstLabelableDescendant();

  // XXX It would be nice if we could use an event flag instead.
  bool mHandlingEvent;
};

#endif /* nsHTMLLabelElement_h */
