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
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIDOMStyleSheet.h"
#include "nsIStyleSheet.h"
#include "nsStyleLinkElement.h"
#include "nsNetUtil.h"
#include "nsIDocument.h"
#include "nsUnicharUtils.h"
#include "nsParserUtils.h"


class nsHTMLStyleElement : public nsGenericHTMLElement,
                           public nsIDOMHTMLStyleElement,
                           public nsStyleLinkElement
{
public:
  nsHTMLStyleElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLStyleElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLStyleElement
  NS_DECL_NSIDOMHTMLSTYLEELEMENT

  virtual nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                 PRBool aNotify, PRBool aDeepSetDocument);
  virtual nsresult AppendChildTo(nsIContent* aKid, PRBool aNotify,
                                 PRBool aDeepSetDocument);
  virtual nsresult RemoveChildAt(PRUint32 aIndex, PRBool aNotify);
  virtual void SetDocument(nsIDocument* aDocument, PRBool aDeep,
                           PRBool aCompileEventHandlers);
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify);
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             PRBool aNotify);

  virtual nsresult GetInnerHTML(nsAString& aInnerHTML);
  virtual nsresult SetInnerHTML(const nsAString& aInnerHTML);

protected:
  void GetStyleSheetURL(PRBool* aIsInline,
                        nsIURI** aURI);
  void GetStyleSheetInfo(nsAString& aTitle,
                         nsAString& aType,
                         nsAString& aMedia,
                         PRBool* aIsAlternate);
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Style)


nsHTMLStyleElement::nsHTMLStyleElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLStyleElement::~nsHTMLStyleElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLStyleElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLStyleElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLStyleElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLStyleElement, nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLStyleElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLinkStyle)
  NS_INTERFACE_MAP_ENTRY(nsIStyleSheetLinkingElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLStyleElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMPL_HTML_DOM_CLONENODE(Style)


NS_IMETHODIMP
nsHTMLStyleElement::GetDisabled(PRBool* aDisabled)
{
  nsresult result = NS_OK;
  
  if (mStyleSheet) {
    nsCOMPtr<nsIDOMStyleSheet> ss(do_QueryInterface(mStyleSheet));

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
nsHTMLStyleElement::SetDisabled(PRBool aDisabled)
{
  nsresult result = NS_OK;
  
  if (mStyleSheet) {
    nsCOMPtr<nsIDOMStyleSheet> ss(do_QueryInterface(mStyleSheet));

    if (ss) {
      result = ss->SetDisabled(aDisabled);
    }
  }

  return result;
}

NS_IMPL_STRING_ATTR(nsHTMLStyleElement, Media, media)
NS_IMPL_STRING_ATTR(nsHTMLStyleElement, Type, type)

nsresult
nsHTMLStyleElement::InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                  PRBool aNotify, PRBool aDeepSetDocument)
{
  nsresult rv =
    nsGenericHTMLElement::InsertChildAt(aKid, aIndex, aNotify,
                                        aDeepSetDocument);
  if (NS_SUCCEEDED(rv)) {
    UpdateStyleSheet();
  }

  return rv;
}

nsresult
nsHTMLStyleElement::AppendChildTo(nsIContent* aKid, PRBool aNotify,
                                  PRBool aDeepSetDocument)
{
  nsresult rv =
    nsGenericHTMLElement::AppendChildTo(aKid, aNotify, aDeepSetDocument);
  if (NS_SUCCEEDED(rv)) {
    UpdateStyleSheet();
  }

  return rv;
}

nsresult
nsHTMLStyleElement::RemoveChildAt(PRUint32 aIndex, PRBool aNotify)
{
  nsresult rv = nsGenericHTMLElement::RemoveChildAt(aIndex, aNotify);
  if (NS_SUCCEEDED(rv)) {
    UpdateStyleSheet();
  }

  return rv;
}

void
nsHTMLStyleElement::SetDocument(nsIDocument* aDocument, PRBool aDeep,
                                PRBool aCompileEventHandlers)
{
  nsCOMPtr<nsIDocument> oldDoc = mDocument;

  nsGenericHTMLElement::SetDocument(aDocument, aDeep, aCompileEventHandlers);

  UpdateStyleSheet(oldDoc);
}
 
nsresult
nsHTMLStyleElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                            nsIAtom* aPrefix, const nsAString& aValue,
                            PRBool aNotify)
{
  nsresult rv = nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix,
                                              aValue, aNotify);
  if (NS_SUCCEEDED(rv)) {
    UpdateStyleSheet();
  }

  return rv;
}

nsresult
nsHTMLStyleElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                              PRBool aNotify)
{
  nsresult rv = nsGenericHTMLElement::UnsetAttr(aNameSpaceID, aAttribute,
                                                aNotify);
  if (NS_SUCCEEDED(rv)) {
    UpdateStyleSheet();
  }

  return rv;
}

nsresult
nsHTMLStyleElement::GetInnerHTML(nsAString& aInnerHTML)
{
  GetContentsAsText(aInnerHTML);
  return NS_OK;
}

nsresult
nsHTMLStyleElement::SetInnerHTML(const nsAString& aInnerHTML)
{
  SetEnableUpdates(PR_FALSE);

  nsresult rv = ReplaceContentsWithText(aInnerHTML, PR_TRUE);
  
  SetEnableUpdates(PR_TRUE);
  
  UpdateStyleSheet();
  return rv;
}

void
nsHTMLStyleElement::GetStyleSheetURL(PRBool* aIsInline,
                                     nsIURI** aURI)
{
  *aURI = nsnull;
  *aIsInline = !HasAttr(kNameSpaceID_None, nsHTMLAtoms::src);
  if (*aIsInline) {
    return;
  }
  if (mNodeInfo->NamespaceEquals(kNameSpaceID_XHTML)) {
    // We stopped supporting <style src="..."> for XHTML as it is
    // non-standard.
    *aIsInline = PR_TRUE;
    return;
  }

  GetHrefURIForAnchors(aURI);
  return;
}

void
nsHTMLStyleElement::GetStyleSheetInfo(nsAString& aTitle,
                                      nsAString& aType,
                                      nsAString& aMedia,
                                      PRBool* aIsAlternate)
{
  aTitle.Truncate();
  aType.Truncate();
  aMedia.Truncate();
  *aIsAlternate = PR_FALSE;

  nsAutoString title;
  GetAttr(kNameSpaceID_None, nsHTMLAtoms::title, title);
  title.CompressWhitespace();
  aTitle.Assign(title);

  GetAttr(kNameSpaceID_None, nsHTMLAtoms::media, aMedia);
  ToLowerCase(aMedia); // HTML4.0 spec is inconsistent, make it case INSENSITIVE

  GetAttr(kNameSpaceID_None, nsHTMLAtoms::type, aType);

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
