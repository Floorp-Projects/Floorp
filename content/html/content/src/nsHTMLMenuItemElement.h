/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDOMHTMLMenuItemElement.h"
#include "nsGenericHTMLElement.h"

class Visitor;

class nsHTMLMenuItemElement : public nsGenericHTMLElement,
                              public nsIDOMHTMLMenuItemElement
{
public:
  using nsGenericElement::GetText;

  nsHTMLMenuItemElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                        mozilla::dom::FromParser aFromParser);
  virtual ~nsHTMLMenuItemElement();

  /** Typesafe, non-refcounting cast from nsIContent.  Cheaper than QI. **/
  static nsHTMLMenuItemElement* FromContent(nsIContent* aContent)
  {
    if (aContent && aContent->IsHTML(nsGkAtoms::menuitem)) {
      return static_cast<nsHTMLMenuItemElement*>(aContent);
    }
    return nullptr;
  }

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLCommandElement
  NS_DECL_NSIDOMHTMLCOMMANDELEMENT

  // nsIDOMHTMLMenuItemElement
  NS_DECL_NSIDOMHTMLMENUITEMELEMENT

  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);

  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);

  virtual void DoneCreatingElement();

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

  PRUint8 GetType() const { return mType; }

  /**
   * Syntax sugar to make it easier to check for checked and checked dirty
   */
  bool IsChecked() const { return mChecked; }
  bool IsCheckedDirty() const { return mCheckedDirty; }

  void GetText(nsAString& aText);

protected:
  virtual nsresult AfterSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify);

  void WalkRadioGroup(Visitor* aVisitor);

  nsHTMLMenuItemElement* GetSelectedRadio();

  void AddedToRadioGroup();

  void InitChecked();

  friend class ClearCheckedVisitor;
  friend class SetCheckedDirtyVisitor;

  void ClearChecked() { mChecked = false; }
  void SetCheckedDirty() { mCheckedDirty = true; }

private:
  PRUint8 mType : 2;
  bool mParserCreating : 1;
  bool mShouldInitChecked : 1;
  bool mCheckedDirty : 1;
  bool mChecked : 1;
};
