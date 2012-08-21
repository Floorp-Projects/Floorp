/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGenericHTMLFrameElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMGetSVGDocument.h"
#include "nsContentUtils.h"

class nsHTMLIFrameElement : public nsGenericHTMLFrameElement
                          , public nsIDOMHTMLIFrameElement
                          , public nsIDOMGetSVGDocument
{
public:
  nsHTMLIFrameElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                      mozilla::dom::FromParser aFromParser = mozilla::dom::NOT_FROM_PARSER);
  virtual ~nsHTMLIFrameElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLFrameElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFrameElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLFrameElement::)

  // nsIDOMHTMLIFrameElement
  NS_DECL_NSIDOMHTMLIFRAMEELEMENT

  // nsIDOMGetSVGDocument
  NS_DECL_NSIDOMGETSVGDOCUMENT

  // nsIContent
  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual nsXPCClassInfo* GetClassInfo();
  virtual nsIDOMNode* AsDOMNode() { return this; }

  virtual nsresult AfterSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue,
                                bool aNotify);

  PRUint32 GetSandboxFlags()
  {
    nsAutoString sandboxAttr;

    if (GetAttr(kNameSpaceID_None, nsGkAtoms::sandbox, sandboxAttr)) {
      return nsContentUtils::ParseSandboxAttributeToFlags(sandboxAttr);
    }

    // No sandbox attribute, no sandbox flags.
    return 0;
  }

  static nsHTMLIFrameElement* FromContent(nsIContent *aContent)
  {
    if (aContent->IsHTML(nsGkAtoms::iframe)) {
      return static_cast<nsHTMLIFrameElement*>(aContent);
    }
    return nullptr;
  }

protected:
  virtual void GetItemValueText(nsAString& text);
  virtual void SetItemValueText(const nsAString& text);
};
