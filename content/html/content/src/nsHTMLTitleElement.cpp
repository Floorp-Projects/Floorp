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
 *   Ryan Jones <sciguyryan@gmail.com>
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
#include "nsIDOMHTMLTitleElement.h"
#include "nsIDOMEventTarget.h"
#include "nsGenericHTMLElement.h"
#include "nsStyleConsts.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsContentUtils.h"

class nsHTMLTitleElement : public nsGenericHTMLElement,
                           public nsIDOMHTMLTitleElement,
                           public nsStubMutationObserver
{
public:
  using nsGenericElement::GetText;
  using nsGenericElement::SetText;

  nsHTMLTitleElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLTitleElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLTitleElement
  NS_DECL_NSIDOMHTMLTITLEELEMENT

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsresult BindToTree(nsIDocument *aDocument, nsIContent *aParent,
                              nsIContent *aBindingParent,
                              bool aCompileEventHandlers);

  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);

  virtual nsresult DoneAddingChildren(bool aHaveNotified);

  virtual nsXPCClassInfo* GetClassInfo();
private:
  void SendTitleChangeEvent(bool aBound);
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Title)


nsHTMLTitleElement::nsHTMLTitleElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
  AddMutationObserver(this);
}

nsHTMLTitleElement::~nsHTMLTitleElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLTitleElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLTitleElement, nsGenericElement) 


DOMCI_NODE_DATA(HTMLTitleElement, nsHTMLTitleElement)

// QueryInterface implementation for nsHTMLTitleElement
NS_INTERFACE_TABLE_HEAD(nsHTMLTitleElement)
  NS_HTML_CONTENT_INTERFACE_TABLE2(nsHTMLTitleElement,
                                   nsIDOMHTMLTitleElement,
                                   nsIMutationObserver)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLTitleElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLTitleElement)


NS_IMPL_ELEMENT_CLONE(nsHTMLTitleElement)


NS_IMETHODIMP 
nsHTMLTitleElement::GetText(nsAString& aTitle)
{
  nsContentUtils::GetNodeTextContent(this, PR_FALSE, aTitle);
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLTitleElement::SetText(const nsAString& aTitle)
{
  return nsContentUtils::SetNodeTextContent(this, aTitle, PR_TRUE);
}

void
nsHTMLTitleElement::CharacterDataChanged(nsIDocument *aDocument,
                                         nsIContent *aContent,
                                         CharacterDataChangeInfo *aInfo)
{
  SendTitleChangeEvent(PR_FALSE);
}

void
nsHTMLTitleElement::ContentAppended(nsIDocument *aDocument,
                                    nsIContent *aContainer,
                                    nsIContent *aFirstNewContent,
                                    PRInt32 aNewIndexInContainer)
{
  SendTitleChangeEvent(PR_FALSE);
}

void
nsHTMLTitleElement::ContentInserted(nsIDocument *aDocument,
                                    nsIContent *aContainer,
                                    nsIContent *aChild,
                                    PRInt32 aIndexInContainer)
{
  SendTitleChangeEvent(PR_FALSE);
}

void
nsHTMLTitleElement::ContentRemoved(nsIDocument *aDocument,
                                   nsIContent *aContainer,
                                   nsIContent *aChild,
                                   PRInt32 aIndexInContainer,
                                   nsIContent *aPreviousSibling)
{
  SendTitleChangeEvent(PR_FALSE);
}

nsresult
nsHTMLTitleElement::BindToTree(nsIDocument *aDocument,
                               nsIContent *aParent,
                               nsIContent *aBindingParent,
                               bool aCompileEventHandlers)
{
  // Let this fall through.
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  SendTitleChangeEvent(PR_TRUE);

  return NS_OK;
}

void
nsHTMLTitleElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  SendTitleChangeEvent(PR_FALSE);

  // Let this fall through.
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

nsresult
nsHTMLTitleElement::DoneAddingChildren(bool aHaveNotified)
{
  if (!aHaveNotified) {
    SendTitleChangeEvent(PR_FALSE);
  }
  return NS_OK;
}

void
nsHTMLTitleElement::SendTitleChangeEvent(bool aBound)
{
  nsIDocument* doc = GetCurrentDoc();
  if (doc) {
    doc->NotifyPossibleTitleChange(aBound);
  }
}
