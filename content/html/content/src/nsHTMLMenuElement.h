/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDOMHTMLMenuElement.h"
#include "nsIHTMLMenu.h"
#include "nsGenericHTMLElement.h"

class nsHTMLMenuElement : public nsGenericHTMLElement,
                          public nsIDOMHTMLMenuElement,
                          public nsIHTMLMenu
{
public:
  nsHTMLMenuElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLMenuElement();

  /** Typesafe, non-refcounting cast from nsIContent.  Cheaper than QI. **/
  static nsHTMLMenuElement* FromContent(nsIContent* aContent)
  {
    if (aContent && aContent->IsHTML(nsGkAtoms::menu))
      return static_cast<nsHTMLMenuElement*>(aContent);
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

  // nsIDOMHTMLMenuElement
  NS_DECL_NSIDOMHTMLMENUELEMENT

  // nsIHTMLMenu
  NS_DECL_NSIHTMLMENU

  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

  PRUint8 GetType() const { return mType; }

protected:
  static bool CanLoadIcon(nsIContent* aContent, const nsAString& aIcon);

  void BuildSubmenu(const nsAString& aLabel,
                    nsIContent* aContent,
                    nsIMenuBuilder* aBuilder);

  void TraverseContent(nsIContent* aContent,
                       nsIMenuBuilder* aBuilder,
                       PRInt8& aSeparator);

  void AddSeparator(nsIMenuBuilder* aBuilder, PRInt8& aSeparator);

  PRUint8 mType;
};
