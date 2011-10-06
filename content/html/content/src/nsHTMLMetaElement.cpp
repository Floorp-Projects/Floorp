/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsIDOMHTMLMetaElement.h"
#include "nsIDOMEventTarget.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsPLDOMEvent.h"
#include "nsContentUtils.h"


class nsHTMLMetaElement : public nsGenericHTMLElement,
                          public nsIDOMHTMLMetaElement
{
public:
  nsHTMLMetaElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLMetaElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLMetaElement
  NS_DECL_NSIDOMHTMLMETAELEMENT

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);
  void CreateAndDispatchEvent(nsIDocument* aDoc, const nsAString& aEventName);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Meta)


nsHTMLMetaElement::nsHTMLMetaElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLMetaElement::~nsHTMLMetaElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLMetaElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLMetaElement, nsGenericElement) 


DOMCI_NODE_DATA(HTMLMetaElement, nsHTMLMetaElement)

// QueryInterface implementation for nsHTMLMetaElement
NS_INTERFACE_TABLE_HEAD(nsHTMLMetaElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLMetaElement, nsIDOMHTMLMetaElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLMetaElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLMetaElement)


NS_IMPL_ELEMENT_CLONE(nsHTMLMetaElement)


NS_IMPL_STRING_ATTR(nsHTMLMetaElement, Content, content)
NS_IMPL_STRING_ATTR(nsHTMLMetaElement, HttpEquiv, httpEquiv)
NS_IMPL_STRING_ATTR(nsHTMLMetaElement, Name, name)
NS_IMPL_STRING_ATTR(nsHTMLMetaElement, Scheme, scheme)

nsresult
nsHTMLMetaElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aDocument &&
      AttrValueIs(kNameSpaceID_None, nsGkAtoms::name, nsGkAtoms::viewport, eIgnoreCase)) {
    nsAutoString content;
    rv = GetContent(content);
    NS_ENSURE_SUCCESS(rv, rv);
    nsContentUtils::ProcessViewportInfo(aDocument, content);  
  }
  CreateAndDispatchEvent(aDocument, NS_LITERAL_STRING("DOMMetaAdded"));
  return rv;
}

void
nsHTMLMetaElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsCOMPtr<nsIDocument> oldDoc = GetCurrentDoc();
  CreateAndDispatchEvent(oldDoc, NS_LITERAL_STRING("DOMMetaRemoved"));
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

void
nsHTMLMetaElement::CreateAndDispatchEvent(nsIDocument* aDoc,
                                          const nsAString& aEventName)
{
  if (!aDoc)
    return;

  nsRefPtr<nsPLDOMEvent> event = new nsPLDOMEvent(this, aEventName, PR_TRUE,
                                                  PR_TRUE);
  if (event) {
    event->PostDOMEvent();
  }
}
