/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMLinkStyle.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsILink.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIDOMStyleSheet.h"
#include "nsIStyleSheet.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsStyleLinkElement.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsHTMLUtils.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMEventTarget.h"

class nsHTMLLinkElement : public nsGenericHTMLLeafElement,
                          public nsIDOMHTMLLinkElement,
                          public nsILink,
                          public nsStyleLinkElement
{
public:
  nsHTMLLinkElement();
  virtual ~nsHTMLLinkElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLLeafElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLLeafElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLLeafElement::)

  // nsIDOMHTMLLinkElement
  NS_DECL_NSIDOMHTMLLINKELEMENT

  // nsILink
  NS_IMETHOD    GetLinkState(nsLinkState &aState);
  NS_IMETHOD    SetLinkState(nsLinkState aState);
  NS_IMETHOD    GetHrefCString(char* &aBuf);

  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep,
                         PRBool aCompileEventHandlers) {
    nsIDocument *oldDoc = mDocument;

    nsAutoString rel;
    GetAttr(kNameSpaceID_None, nsHTMLAtoms::rel, rel);
    
    CreateAndDispatchEvent(oldDoc, rel, NS_LITERAL_STRING("DOMLinkRemoved"));

    // Do the removal and addition into the new doc.
    nsresult rv = nsGenericHTMLLeafElement::SetDocument(aDocument, aDeep,
                                                        aCompileEventHandlers);
    UpdateStyleSheet(PR_TRUE, oldDoc);
		
    CreateAndDispatchEvent(mDocument, rel, NS_LITERAL_STRING("DOMLinkAdded"));

    return rv;
  }
 
  void CreateAndDispatchEvent(nsIDocument* aDoc, const nsString& aRel, 
                              const nsAString& aEventName) {
    if (aDoc && !aRel.IsEmpty() && !aRel.EqualsIgnoreCase("stylesheet")) {
      nsCOMPtr<nsIDOMDocumentEvent> docEvent(do_QueryInterface(aDoc));
      nsCOMPtr<nsIDOMEvent> event;
      docEvent->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
      if (event) {
        event->InitEvent(aEventName, PR_TRUE, PR_TRUE);
        PRBool noDefault;
        nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(NS_STATIC_CAST(nsIDOMNode*, this)));
        if (target)
          target->DispatchEvent(event, &noDefault);
      }
    }
  }

  NS_IMETHOD SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                     const nsAReadableString& aValue, PRBool aNotify) {
    nsresult rv = nsGenericHTMLLeafElement::SetAttr(aNameSpaceID, aName,
                                                    aValue, aNotify);
    UpdateStyleSheet(aNotify);
    return rv;
  }
  NS_IMETHOD SetAttr(nsINodeInfo* aNodeInfo, const nsAReadableString& aValue,
                     PRBool aNotify) {
    nsresult rv = nsGenericHTMLLeafElement::SetAttr(aNodeInfo, aValue,
                                                    aNotify);

    // nsGenericHTMLLeafElement::SetAttr(nsINodeInfo* aNodeInfo,
    //                                   const nsAReadableString& aValue,
    //                                   PRBool aNotify)
    //
    // calls
    //
    // nsHTMLLinkElement::SetAttr(PRInt32 aNameSpaceID,
    //                            nsIAtom* aName,
    //                            const nsAReadableString& aValue,
    //                            PRBool aNotify)
    //
    // which ends up calling UpdateStyleSheet so we don't call UpdateStyleSheet
    // here ourselves.

    return rv;
  }
  NS_IMETHOD UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                       PRBool aNotify) {
    nsresult rv = nsGenericHTMLLeafElement::UnsetAttr(aNameSpaceID,
                                                      aAttribute,
                                                      aNotify);
    UpdateStyleSheet(aNotify);
    return rv;
  }

  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent, PRUint32 aFlags,
                            nsEventStatus* aEventStatus);
#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

protected:
  virtual void GetStyleSheetInfo(nsAWritableString& aUrl,
                                 nsAWritableString& aTitle,
                                 nsAWritableString& aType,
                                 nsAWritableString& aMedia,
                                 PRBool* aDoBlock);
 
  // The cached visited state
  nsLinkState mLinkState;
};

nsresult
NS_NewHTMLLinkElement(nsIHTMLContent** aInstancePtrResult,
                      nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLLinkElement* it = new nsHTMLLinkElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = it->Init(aNodeInfo);

  if (NS_FAILED(rv)) {
    delete it;

    return rv;
  }

  *aInstancePtrResult = NS_STATIC_CAST(nsIHTMLContent *, it);
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}


nsHTMLLinkElement::nsHTMLLinkElement()
  : mLinkState(eLinkState_Unknown)
{
  nsHTMLUtils::AddRef(); // for GetHrefCString
}

nsHTMLLinkElement::~nsHTMLLinkElement()
{
  nsHTMLUtils::Release(); // for GetHrefCString
}


NS_IMPL_ADDREF_INHERITED(nsHTMLLinkElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLLinkElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLLinkElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLLinkElement,
                                    nsGenericHTMLLeafElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLLinkElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLinkStyle)
  NS_INTERFACE_MAP_ENTRY(nsIStyleSheetLinkingElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLLinkElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


nsresult
nsHTMLLinkElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLLinkElement* it = new nsHTMLLinkElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);

  nsresult rv = it->Init(mNodeInfo);

  if (NS_FAILED(rv))
    return rv;

  CopyInnerTo(this, it, aDeep);

  *aReturn = NS_STATIC_CAST(nsIDOMNode *, it);

  NS_ADDREF(*aReturn);

  return NS_OK;
}


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
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Hreflang, hreflang)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Media, media)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Rel, rel)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Rev, rev)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Target, target)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Type, type)


NS_IMETHODIMP
nsHTMLLinkElement::GetHref(nsAWritableString& aValue)
{
  char *buf;
  nsresult rv = GetHrefCString(buf);
  if (NS_FAILED(rv)) return rv;
  if (buf) {
    aValue.Assign(NS_ConvertASCIItoUCS2(buf));
    nsCRT::free(buf);
  }

  // NS_IMPL_STRING_ATTR does nothing where we have (buf == null)

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLinkElement::SetHref(const nsAReadableString& aValue)
{
  // Clobber our "cache", so we'll recompute it the next time
  // somebody asks for it.
  mLinkState = eLinkState_Unknown;

  nsresult rv = nsGenericHTMLLeafElement::SetAttr(kNameSpaceID_HTML,
                                                  nsHTMLAtoms::href,
                                                  aValue,
                                                  PR_TRUE);
  UpdateStyleSheet(PR_TRUE);
  return rv;
}

NS_IMETHODIMP
nsHTMLLinkElement::HandleDOMEvent(nsIPresContext* aPresContext,
                           nsEvent* aEvent,
                           nsIDOMEvent** aDOMEvent,
                           PRUint32 aFlags,
                           nsEventStatus* aEventStatus)
{
  return HandleDOMEventForAnchors(this, aPresContext, aEvent, aDOMEvent,
                                  aFlags, aEventStatus);
}

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLLinkElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
#endif

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
nsHTMLLinkElement::GetHrefCString(char* &aBuf)
{
  // Get href= attribute (relative URL).
  nsAutoString relURLSpec;

  if (NS_CONTENT_ATTR_HAS_VALUE ==
      nsGenericHTMLLeafElement::GetAttr(kNameSpaceID_HTML,
                                        nsHTMLAtoms::href, relURLSpec)) {
    // Clean up any leading or trailing whitespace
    relURLSpec.Trim(" \t\n\r");

    // Get base URL.
    nsCOMPtr<nsIURI> baseURL;
    GetBaseURL(*getter_AddRefs(baseURL));

    if (baseURL) {
      // Get absolute URL.
      NS_MakeAbsoluteURIWithCharset(&aBuf, relURLSpec, mDocument, baseURL,
                                    nsHTMLUtils::IOService,
                                    nsHTMLUtils::CharsetMgr);
    }
    else {
      // Absolute URL is same as relative URL.
      aBuf = ToNewUTF8String(relURLSpec);
    }
  }
  else {
    // Absolute URL is empty because we have no HREF.
    aBuf = nsnull;
  }

  return NS_OK;
}

void
nsHTMLLinkElement::GetStyleSheetInfo(nsAWritableString& aUrl,
                                     nsAWritableString& aTitle,
                                     nsAWritableString& aType,
                                     nsAWritableString& aMedia,
                                     PRBool* aIsAlternate)
{
  nsresult rv = NS_OK;

  aUrl.Truncate();
  aTitle.Truncate();
  aType.Truncate();
  aMedia.Truncate();
  *aIsAlternate = PR_FALSE;

  nsAutoString href, rel, title, type;

  GetHref(href);
  if (href.IsEmpty()) {
    // if href is empty then just bail
    return;
  }

  GetAttribute(NS_LITERAL_STRING("rel"), rel);
  rel.CompressWhitespace();

  nsStringArray linkTypes(4);
  nsStyleLinkElement::ParseLinkTypes(rel, linkTypes);
  // is it a stylesheet link?
  if (linkTypes.IndexOf(NS_LITERAL_STRING("stylesheet")) < 0)
    return;

  GetAttribute(NS_LITERAL_STRING("title"), title);
  title.CompressWhitespace();
  aTitle.Assign(title);

  // if alternate, does it have title?
  if (-1 != linkTypes.IndexOf(NS_LITERAL_STRING("alternate"))) {
    if (aTitle.IsEmpty()) { // alternates must have title
      return;
    } else {
      *aIsAlternate = PR_TRUE;
    }
  }

  GetAttribute(NS_LITERAL_STRING("media"), aMedia);
  ToLowerCase(aMedia); // HTML4.0 spec is inconsistent, make it case INSENSITIVE

  GetAttribute(NS_LITERAL_STRING("type"), type);
  aType.Assign(type);

  nsAutoString mimeType;
  nsAutoString notUsed;
  nsStyleLinkElement::SplitMimeType(type, mimeType, notUsed);
  if (!mimeType.IsEmpty() && !mimeType.EqualsIgnoreCase("text/css")) {
    return;
  }

  // If we get here we assume that we're loading a css file, so set the
  // type to 'text/css'
  aType.Assign(NS_LITERAL_STRING("text/css"));
  
  nsCOMPtr<nsIURI> url, base;

  nsCOMPtr<nsIURI> baseURL;
  GetBaseURL(*getter_AddRefs(baseURL));
  rv = NS_MakeAbsoluteURI(aUrl, href, baseURL);

  if (!*aIsAlternate) {
    if (!aTitle.IsEmpty()) {  // possibly preferred sheet
      nsAutoString prefStyle;
      mDocument->GetHeaderData(nsHTMLAtoms::headerDefaultStyle,
                               prefStyle);

      if (prefStyle.IsEmpty()) {
        mDocument->SetHeaderData(nsHTMLAtoms::headerDefaultStyle,
                                 title);
      }
    }
  }

  return;
}

