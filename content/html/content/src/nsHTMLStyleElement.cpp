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
 *   Daniel Glazman <glazman@netscape.com>
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
#include "nsIDOMHTMLStyleElement.h"
#include "nsIDOMLinkStyle.h"
#include "nsIDOMEventTarget.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsIDOMStyleSheet.h"
#include "nsIStyleSheet.h"
#include "nsStyleLinkElement.h"
#include "nsNetUtil.h"
#include "nsIDocument.h"
#include "nsUnicharUtils.h"
#include "nsParserUtils.h"
#include "nsThreadUtils.h"
#include "nsContentUtils.h"

class nsHTMLStyleElement : public nsGenericHTMLElement,
                           public nsIDOMHTMLStyleElement,
                           public nsStyleLinkElement,
                           public nsStubMutationObserver
{
public:
  nsHTMLStyleElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLStyleElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLStyleElement
  NS_DECL_NSIDOMHTMLSTYLEELEMENT

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);
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

  virtual nsresult GetInnerHTML(nsAString& aInnerHTML);
  virtual nsresult SetInnerHTML(const nsAString& aInnerHTML);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  already_AddRefed<nsIURI> GetStyleSheetURL(bool* aIsInline);
  void GetStyleSheetInfo(nsAString& aTitle,
                         nsAString& aType,
                         nsAString& aMedia,
                         bool* aIsAlternate);
  /**
   * Common method to call from the various mutation observer methods.
   * aContent is a content node that's either the one that changed or its
   * parent; we should only respond to the change if aContent is non-anonymous.
   */
  void ContentChanged(nsIContent* aContent);
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Style)


nsHTMLStyleElement::nsHTMLStyleElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
  AddMutationObserver(this);
}

nsHTMLStyleElement::~nsHTMLStyleElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLStyleElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLStyleElement, nsGenericElement) 


DOMCI_NODE_DATA(HTMLStyleElement, nsHTMLStyleElement)

// QueryInterface implementation for nsHTMLStyleElement
NS_INTERFACE_TABLE_HEAD(nsHTMLStyleElement)
  NS_HTML_CONTENT_INTERFACE_TABLE4(nsHTMLStyleElement,
                                   nsIDOMHTMLStyleElement,
                                   nsIDOMLinkStyle,
                                   nsIStyleSheetLinkingElement,
                                   nsIMutationObserver)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLStyleElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLStyleElement)


NS_IMPL_ELEMENT_CLONE(nsHTMLStyleElement)


NS_IMETHODIMP
nsHTMLStyleElement::GetDisabled(bool* aDisabled)
{
  nsresult result = NS_OK;
  
  if (GetStyleSheet()) {
    nsCOMPtr<nsIDOMStyleSheet> ss(do_QueryInterface(GetStyleSheet()));

    if (ss) {
      result = ss->GetDisabled(aDisabled);
    }
  }
  else {
    *aDisabled = PR_FALSE;
  }

  return result;
}

NS_IMETHODIMP 
nsHTMLStyleElement::SetDisabled(bool aDisabled)
{
  nsresult result = NS_OK;
  
  if (GetStyleSheet()) {
    nsCOMPtr<nsIDOMStyleSheet> ss(do_QueryInterface(GetStyleSheet()));

    if (ss) {
      result = ss->SetDisabled(aDisabled);
    }
  }

  return result;
}

NS_IMPL_STRING_ATTR(nsHTMLStyleElement, Media, media)
NS_IMPL_STRING_ATTR(nsHTMLStyleElement, Type, type)

void
nsHTMLStyleElement::CharacterDataChanged(nsIDocument* aDocument,
                                         nsIContent* aContent,
                                         CharacterDataChangeInfo* aInfo)
{
  ContentChanged(aContent);
}

void
nsHTMLStyleElement::ContentAppended(nsIDocument* aDocument,
                                    nsIContent* aContainer,
                                    nsIContent* aFirstNewContent,
                                    PRInt32 aNewIndexInContainer)
{
  ContentChanged(aContainer);
}

void
nsHTMLStyleElement::ContentInserted(nsIDocument* aDocument,
                                    nsIContent* aContainer,
                                    nsIContent* aChild,
                                    PRInt32 aIndexInContainer)
{
  ContentChanged(aChild);
}

void
nsHTMLStyleElement::ContentRemoved(nsIDocument* aDocument,
                                   nsIContent* aContainer,
                                   nsIContent* aChild,
                                   PRInt32 aIndexInContainer,
                                   nsIContent* aPreviousSibling)
{
  ContentChanged(aChild);
}

void
nsHTMLStyleElement::ContentChanged(nsIContent* aContent)
{
  if (nsContentUtils::IsInSameAnonymousTree(this, aContent)) {
    UpdateStyleSheetInternal(nsnull);
  }
}

nsresult
nsHTMLStyleElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                               nsIContent* aBindingParent,
                               bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  void (nsHTMLStyleElement::*update)() = &nsHTMLStyleElement::UpdateStyleSheetInternal;
  nsContentUtils::AddScriptRunner(NS_NewRunnableMethod(this, update));

  return rv;  
}

void
nsHTMLStyleElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsCOMPtr<nsIDocument> oldDoc = GetCurrentDoc();

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
  UpdateStyleSheetInternal(oldDoc);
}

nsresult
nsHTMLStyleElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                            nsIAtom* aPrefix, const nsAString& aValue,
                            bool aNotify)
{
  nsresult rv = nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix,
                                              aValue, aNotify);
  if (NS_SUCCEEDED(rv)) {
    UpdateStyleSheetInternal(nsnull,
                             aNameSpaceID == kNameSpaceID_None &&
                             (aName == nsGkAtoms::title ||
                              aName == nsGkAtoms::media ||
                              aName == nsGkAtoms::type));
  }

  return rv;
}

nsresult
nsHTMLStyleElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                              bool aNotify)
{
  nsresult rv = nsGenericHTMLElement::UnsetAttr(aNameSpaceID, aAttribute,
                                                aNotify);
  if (NS_SUCCEEDED(rv)) {
    UpdateStyleSheetInternal(nsnull,
                             aNameSpaceID == kNameSpaceID_None &&
                             (aAttribute == nsGkAtoms::title ||
                              aAttribute == nsGkAtoms::media ||
                              aAttribute == nsGkAtoms::type));
  }

  return rv;
}

nsresult
nsHTMLStyleElement::GetInnerHTML(nsAString& aInnerHTML)
{
  nsContentUtils::GetNodeTextContent(this, PR_FALSE, aInnerHTML);
  return NS_OK;
}

nsresult
nsHTMLStyleElement::SetInnerHTML(const nsAString& aInnerHTML)
{
  SetEnableUpdates(PR_FALSE);

  nsresult rv = nsContentUtils::SetNodeTextContent(this, aInnerHTML, PR_TRUE);
  
  SetEnableUpdates(PR_TRUE);
  
  UpdateStyleSheetInternal(nsnull);
  return rv;
}

already_AddRefed<nsIURI>
nsHTMLStyleElement::GetStyleSheetURL(bool* aIsInline)
{
  *aIsInline = PR_TRUE;
  return nsnull;
}

void
nsHTMLStyleElement::GetStyleSheetInfo(nsAString& aTitle,
                                      nsAString& aType,
                                      nsAString& aMedia,
                                      bool* aIsAlternate)
{
  aTitle.Truncate();
  aType.Truncate();
  aMedia.Truncate();
  *aIsAlternate = PR_FALSE;

  nsAutoString title;
  GetAttr(kNameSpaceID_None, nsGkAtoms::title, title);
  title.CompressWhitespace();
  aTitle.Assign(title);

  GetAttr(kNameSpaceID_None, nsGkAtoms::media, aMedia);
  ToLowerCase(aMedia); // HTML4.0 spec is inconsistent, make it case INSENSITIVE

  GetAttr(kNameSpaceID_None, nsGkAtoms::type, aType);

  nsAutoString mimeType;
  nsAutoString notUsed;
  nsParserUtils::SplitMimeType(aType, mimeType, notUsed);
  if (!mimeType.IsEmpty() && !mimeType.LowerCaseEqualsLiteral("text/css")) {
    return;
  }

  // If we get here we assume that we're loading a css file, so set the
  // type to 'text/css'
  aType.AssignLiteral("text/css");

  return;
}
