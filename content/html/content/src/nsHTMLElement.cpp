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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mounir Lamouri <mounir.lamouri@mozilla.com> (original author)
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

#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLElement.h"

#include "nsContentUtils.h"

using namespace mozilla::dom;

class nsHTMLElement : public nsGenericHTMLElement,
                      public nsIDOMHTMLElement
{
public:
  nsHTMLElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  virtual nsresult GetInnerHTML(nsAString& aInnerHTML);

  nsresult Clone(nsINodeInfo* aNodeInfo, nsINode** aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
};

// Here, we expand 'NS_IMPL_NS_NEW_HTML_ELEMENT()' by hand.
// (Calling the macro directly (with no args) produces compiler warnings.)
nsGenericHTMLElement*
NS_NewHTMLElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                  FromParser aFromParser)
{
  return new nsHTMLElement(aNodeInfo);
}

nsHTMLElement::nsHTMLElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLElement::~nsHTMLElement()
{
}

NS_IMPL_ADDREF_INHERITED(nsHTMLElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLElement, nsGenericElement)

DOMCI_NODE_DATA(HTMLElement, nsHTMLElement)

NS_INTERFACE_TABLE_HEAD(nsHTMLElement)
  NS_HTML_CONTENT_INTERFACE_TABLE0(nsHTMLElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLElement)

NS_IMPL_ELEMENT_CLONE(nsHTMLElement)

nsresult
nsHTMLElement::GetInnerHTML(nsAString& aInnerHTML)
{
  /**
   * nsGenericHTMLElement::GetInnerHTML escapes < and > characters (at least).
   * .innerHTML should return the HTML code for xmp and plaintext element.
   *
   * This code is a workaround until we implement a HTML5 Serializer
   * with this behavior.
   */
  if (mNodeInfo->Equals(nsGkAtoms::xmp) ||
      mNodeInfo->Equals(nsGkAtoms::plaintext)) {
    nsContentUtils::GetNodeTextContent(this, PR_FALSE, aInnerHTML);
    return NS_OK;
  }

  return nsGenericHTMLElement::GetInnerHTML(aInnerHTML);
}

