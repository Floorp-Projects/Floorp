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
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMLinkStyle.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsILink.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIDOMStyleSheet.h"
#include "nsIStyleSheet.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsStyleLinkElement.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsParserUtils.h"

class nsHTMLLinkElement : public nsGenericHTMLElement,
                          public nsIDOMHTMLLinkElement,
                          public nsILink,
                          public nsStyleLinkElement
{
public:
  nsHTMLLinkElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLLinkElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLLinkElement
  NS_DECL_NSIDOMHTMLLINKELEMENT

  // nsILink
  NS_IMETHOD    GetLinkState(nsLinkState &aState);
  NS_IMETHOD    SetLinkState(nsLinkState aState);
  NS_IMETHOD    GetHrefURI(nsIURI** aURI);

  virtual void SetDocument(nsIDocument* aDocument, PRBool aDeep,
                           PRBool aCompileEventHandlers);
  void CreateAndDispatchEvent(nsIDocument* aDoc, const nsString& aRel,
                              const nsString& aRev,
                              const nsAString& aEventName);
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

  virtual nsresult HandleDOMEvent(nsPresContext* aPresContext,
                                  nsEvent* aEvent, nsIDOMEvent** aDOMEvent,
                                  PRUint32 aFlags,
                                  nsEventStatus* aEventStatus);

protected:
  virtual void GetStyleSheetURL(PRBool* aIsInline,
                                nsIURI** aURI);
  virtual void GetStyleSheetInfo(nsAString& aTitle,
                                 nsAString& aType,
                                 nsAString& aMedia,
                                 PRBool* aIsAlternate);
 
  // The cached visited state
  nsLinkState mLinkState;
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Link)


nsHTMLLinkElement::nsHTMLLinkElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo),
    mLinkState(eLinkState_Unknown)
{
}

nsHTMLLinkElement::~nsHTMLLinkElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLLinkElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLLinkElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLLinkElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLLinkElement, nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLLinkElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLinkStyle)
  NS_INTERFACE_MAP_ENTRY(nsILink)
  NS_INTERFACE_MAP_ENTRY(nsIStyleSheetLinkingElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLLinkElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMPL_HTML_DOM_CLONENODE(Link)


NS_IMETHODIMP
nsHTMLLinkElement::GetDisabled(PRBool* aDisabled)
{
  nsCOMPtr<nsIDOMStyleSheet> ss(do_QueryInterface(mStyleSheet));
  nsresult result = NS_OK;

  if (ss) {
    result = ss->GetDisabled(aDisabled);
  } else {
    *aDisabled = PR_FALSE;
  }

  return result;
}

NS_IMETHODIMP 
nsHTMLLinkElement::SetDisabled(PRBool aDisabled)
{
  nsCOMPtr<nsIDOMStyleSheet> ss(do_QueryInterface(mStyleSheet));
  nsresult result = NS_OK;

  if (ss) {
    result = ss->SetDisabled(aDisabled);
  }

  return result;
}


NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Charset, charset)
NS_IMPL_URI_ATTR(nsHTMLLinkElement, Href, href)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Hreflang, hreflang)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Media, media)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Rel, rel)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Rev, rev)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Target, target)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Type, type)

void
nsHTMLLinkElement::SetDocument(nsIDocument* aDocument, PRBool aDeep,
                               PRBool aCompileEventHandlers)
{
  nsCOMPtr<nsIDocument> oldDoc = mDocument;

  nsAutoString rel;
  nsAutoString rev;
  GetAttr(kNameSpaceID_None, nsHTMLAtoms::rel, rel);
  GetAttr(kNameSpaceID_None, nsHTMLAtoms::rev, rev);
    
  CreateAndDispatchEvent(oldDoc, rel, rev,
                         NS_LITERAL_STRING("DOMLinkRemoved"));

  // Do the removal and addition into the new doc.
  nsGenericHTMLElement::SetDocument(aDocument, aDeep, aCompileEventHandlers);
  UpdateStyleSheet(oldDoc);
		
  CreateAndDispatchEvent(mDocument, rel, rev,
                         NS_LITERAL_STRING("DOMLinkAdded"));
}
 
void
nsHTMLLinkElement::CreateAndDispatchEvent(nsIDocument* aDoc,
                                          const nsString& aRel,
                                          const nsString& aRev, 
                                          const nsAString& aEventName)
{
  if (!aDoc)
    return;

  // In the unlikely case that both rev is specified *and* rel=stylesheet,
  // this code will cause the event to fire, on the principle that maybe the
  // page really does want to specify that it's author is a stylesheet. Since
  // this should never actually happen and the performance hit is minimal,
  // doing the "right" thing costs virtually nothing here, even if it doesn't
  // make much sense.
  if (aRev.IsEmpty() &&
      (aRel.IsEmpty() || aRel.LowerCaseEqualsLiteral("stylesheet")))
    return;

  nsCOMPtr<nsIDOMDocumentEvent> docEvent(do_QueryInterface(aDoc));
  nsCOMPtr<nsIDOMEvent> event;
  docEvent->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
  if (event) {
    event->InitEvent(aEventName, PR_TRUE, PR_TRUE);
    PRBool noDefault;
    nsCOMPtr<nsIDOMEventTarget> target =
      do_QueryInterface(NS_STATIC_CAST(nsIDOMNode*, this));
    if (target)
      target->DispatchEvent(event, &noDefault);
  }
}

nsresult
nsHTMLLinkElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify)
{
  if (aName == nsHTMLAtoms::href && kNameSpaceID_None == aNameSpaceID) {
    SetLinkState(eLinkState_Unknown);
  }

  nsresult rv = nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix,
                                              aValue, aNotify);
  if (NS_SUCCEEDED(rv)) {
    UpdateStyleSheet();
  }

  return rv;
}

nsresult
nsHTMLLinkElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
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
nsHTMLLinkElement::HandleDOMEvent(nsPresContext* aPresContext,
                           nsEvent* aEvent,
                           nsIDOMEvent** aDOMEvent,
                           PRUint32 aFlags,
                           nsEventStatus* aEventStatus)
{
  return HandleDOMEventForAnchors(aPresContext, aEvent, aDOMEvent,
                                  aFlags, aEventStatus);
}

NS_IMETHODIMP
nsHTMLLinkElement::GetLinkState(nsLinkState &aState)
{
  aState = mLinkState;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLinkElement::SetLinkState(nsLinkState aState)
{
  mLinkState = aState;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLinkElement::GetHrefURI(nsIURI** aURI)
{
  return GetHrefURIForAnchors(aURI);
}

void
nsHTMLLinkElement::GetStyleSheetURL(PRBool* aIsInline,
                                    nsIURI** aURI)
{
  *aIsInline = PR_FALSE;
  GetHrefURIForAnchors(aURI);
  return;
}

void
nsHTMLLinkElement::GetStyleSheetInfo(nsAString& aTitle,
                                     nsAString& aType,
                                     nsAString& aMedia,
                                     PRBool* aIsAlternate)
{
  aTitle.Truncate();
  aType.Truncate();
  aMedia.Truncate();
  *aIsAlternate = PR_FALSE;

  nsAutoString rel;
  nsStringArray linkTypes(4);
  GetAttr(kNameSpaceID_None, nsHTMLAtoms::rel, rel);
  nsStyleLinkElement::ParseLinkTypes(rel, linkTypes);
  // Is it a stylesheet link?
  if (linkTypes.IndexOf(NS_LITERAL_STRING("stylesheet")) < 0) {
    return;
  }

  nsAutoString title;
  GetAttr(kNameSpaceID_None, nsHTMLAtoms::title, title);
  title.CompressWhitespace();
  aTitle.Assign(title);

  // If alternate, does it have title?
  if (-1 != linkTypes.IndexOf(NS_LITERAL_STRING("alternate"))) {
    if (aTitle.IsEmpty()) { // alternates must have title
      return;
    } else {
      *aIsAlternate = PR_TRUE;
    }
  }

  GetAttr(kNameSpaceID_None, nsHTMLAtoms::media, aMedia);
  ToLowerCase(aMedia); // HTML4.0 spec is inconsistent, make it case INSENSITIVE

  nsAutoString mimeType;
  nsAutoString notUsed;
  GetAttr(kNameSpaceID_None, nsHTMLAtoms::type, aType);
  nsParserUtils::SplitMimeType(aType, mimeType, notUsed);
  if (!mimeType.IsEmpty() && !mimeType.LowerCaseEqualsLiteral("text/css")) {
    return;
  }

  // If we get here we assume that we're loading a css file, so set the
  // type to 'text/css'
  aType.AssignLiteral("text/css");

  return;
}
