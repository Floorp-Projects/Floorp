/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Double <chris.double@double.co.nz>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#include "nsIDOMHTMLSourceElement.h"
#include "nsIDOMEventTarget.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsMappedAttributes.h"
#include "nsRuleData.h"
#include "nsHTMLMediaElement.h"
#include "nsCOMPtr.h"
#include "nsThreadUtils.h"

class nsHTMLSourceElement : public nsGenericHTMLElement,
                            public nsIDOMHTMLSourceElement
{
public:
  nsHTMLSourceElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLSourceElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLSourceElement
  NS_DECL_NSIDOMHTMLSOURCEELEMENT

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // Override BindToTree() so that we can trigger a load when we add a
  // child source element.
  virtual nsresult BindToTree(nsIDocument *aDocument, nsIContent *aParent,
                              nsIContent *aBindingParent,
                              bool aCompileEventHandlers);

  virtual nsXPCClassInfo* GetClassInfo();
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Source)


nsHTMLSourceElement::nsHTMLSourceElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLSourceElement::~nsHTMLSourceElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLSourceElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLSourceElement, nsGenericElement)


DOMCI_NODE_DATA(HTMLSourceElement, nsHTMLSourceElement)

// QueryInterface implementation for nsHTMLSourceElement
NS_INTERFACE_TABLE_HEAD(nsHTMLSourceElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLSourceElement, nsIDOMHTMLSourceElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLSourceElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLSourceElement)


NS_IMPL_ELEMENT_CLONE(nsHTMLSourceElement)


NS_IMPL_URI_ATTR(nsHTMLSourceElement, Src, src)
NS_IMPL_STRING_ATTR(nsHTMLSourceElement, Type, type)

nsresult
nsHTMLSourceElement::BindToTree(nsIDocument *aDocument,
                                nsIContent *aParent,
                                nsIContent *aBindingParent,
                                bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument,
                                                 aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aParent || !aParent->IsNodeOfType(nsINode::eMEDIA))
    return NS_OK;

  nsHTMLMediaElement* media = static_cast<nsHTMLMediaElement*>(aParent);
  media->NotifyAddedSource();

  return NS_OK;
}

