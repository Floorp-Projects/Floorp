/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Saari <saari@netscape.com>
 *   Conrad Carlen <ccarlen@netscape.com>
 *   Pierre Chanial <p_ch@verizon.net>
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

#include "nsContextMenuInfo.h"

#include "nsIImageLoadingContent.h"
#include "imgILoader.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLHtmlElement.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMViewCSS.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMCSSValue.h"
#include "nsIDOMCSSPrimitiveValue.h"
#include "nsNetUtil.h"
#include "nsUnicharUtils.h"

//*****************************************************************************
// class nsContextMenuInfo
//*****************************************************************************

NS_IMPL_ISUPPORTS1(nsContextMenuInfo, nsIContextMenuInfo)

nsContextMenuInfo::nsContextMenuInfo()
{
}

nsContextMenuInfo::~nsContextMenuInfo()
{
}

/* readonly attribute nsIDOMEvent mouseEvent; */
NS_IMETHODIMP
nsContextMenuInfo::GetMouseEvent(nsIDOMEvent **aEvent)
{
  NS_ENSURE_ARG_POINTER(aEvent);
  NS_IF_ADDREF(*aEvent = mMouseEvent);
  return NS_OK;
}

/* readonly attribute nsIDOMNode targetNode; */
NS_IMETHODIMP
nsContextMenuInfo::GetTargetNode(nsIDOMNode **aNode)
{
  NS_ENSURE_ARG_POINTER(aNode);
  NS_IF_ADDREF(*aNode = mDOMNode);
  return NS_OK;
}

/* readonly attribute AString associatedLink; */
NS_IMETHODIMP
nsContextMenuInfo::GetAssociatedLink(nsAString& aHRef)
{
  NS_ENSURE_STATE(mAssociatedLink);
  aHRef.Truncate(0);
    
  nsCOMPtr<nsIDOMElement> content(do_QueryInterface(mAssociatedLink));
  nsAutoString localName;
  if (content)
    content->GetLocalName(localName);

  nsCOMPtr<nsIDOMElement> linkContent;
  ToLowerCase(localName);
  if (localName.EqualsLiteral("a") ||
      localName.EqualsLiteral("area") ||
      localName.EqualsLiteral("link")) {
    PRBool hasAttr;
    content->HasAttribute(NS_LITERAL_STRING("href"), &hasAttr);
    if (hasAttr) {
      linkContent = content;
      nsCOMPtr<nsIDOMHTMLAnchorElement> anchor(do_QueryInterface(linkContent));
      if (anchor)
        anchor->GetHref(aHRef);
      else {
        nsCOMPtr<nsIDOMHTMLAreaElement> area(do_QueryInterface(linkContent));
        if (area)
          area->GetHref(aHRef);
        else {
          nsCOMPtr<nsIDOMHTMLLinkElement> link(do_QueryInterface(linkContent));
          if (link)
            link->GetHref(aHRef);
        }
      }
    }
  }
  else {
    nsCOMPtr<nsIDOMNode> curr;
    mAssociatedLink->GetParentNode(getter_AddRefs(curr));
    while (curr) {
      content = do_QueryInterface(curr);
      if (!content)
        break;
      content->GetLocalName(localName);
      ToLowerCase(localName);
      if (localName.EqualsLiteral("a")) {
        PRBool hasAttr;
        content->HasAttribute(NS_LITERAL_STRING("href"), &hasAttr);
        if (hasAttr) {
          linkContent = content;
          nsCOMPtr<nsIDOMHTMLAnchorElement> anchor(do_QueryInterface(linkContent));
          if (anchor)
            anchor->GetHref(aHRef);
        }
        else
          linkContent = nsnull; // Links can't be nested.
        break;
      }

      nsCOMPtr<nsIDOMNode> temp = curr;
      temp->GetParentNode(getter_AddRefs(curr));
    }
  }

  return NS_OK;
}

/* readonly attribute imgIContainer imageContainer; */
NS_IMETHODIMP
nsContextMenuInfo::GetImageContainer(imgIContainer **aImageContainer)
{
  NS_ENSURE_ARG_POINTER(aImageContainer);
  NS_ENSURE_STATE(mDOMNode);
  
  nsCOMPtr<imgIRequest> request;
  GetImageRequest(mDOMNode, getter_AddRefs(request));
  if (request)
    return request->GetImage(aImageContainer);

  return NS_ERROR_FAILURE;
}

/* readonly attribute nsIURI imageSrc; */
NS_IMETHODIMP
nsContextMenuInfo::GetImageSrc(nsIURI **aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_STATE(mDOMNode);
  
  // First try the easy case of our node being a nsIDOMHTMLImageElement
  nsCOMPtr<nsIDOMHTMLImageElement> imgElement(do_QueryInterface(mDOMNode));
  if (imgElement) {
    nsAutoString imgSrcSpec;
    nsresult rv = imgElement->GetSrc(imgSrcSpec);
    if (NS_SUCCEEDED(rv))
      return NS_NewURI(aURI, NS_ConvertUCS2toUTF8(imgSrcSpec));
  }
  
  // If not, dig deeper.
  nsCOMPtr<imgIRequest> request;
  GetImageRequest(mDOMNode, getter_AddRefs(request));
  if (request)
    return request->GetURI(aURI);

  return NS_ERROR_FAILURE;
}

/* readonly attribute imgIContainer backgroundImageContainer; */
NS_IMETHODIMP
nsContextMenuInfo::GetBackgroundImageContainer(imgIContainer **aImageContainer)
{
  NS_ENSURE_ARG_POINTER(aImageContainer);
  NS_ENSURE_STATE(mDOMNode);
  
  nsCOMPtr<imgIRequest> request;
  GetBackgroundImageRequest(mDOMNode, getter_AddRefs(request));
  if (request)
    return request->GetImage(aImageContainer);

  return NS_ERROR_FAILURE;
}

/* readonly attribute nsIURI backgroundImageSrc; */
NS_IMETHODIMP
nsContextMenuInfo::GetBackgroundImageSrc(nsIURI **aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_STATE(mDOMNode);
  
  nsCOMPtr<imgIRequest> request;
  GetBackgroundImageRequest(mDOMNode, getter_AddRefs(request));
  if (request)
    return request->GetURI(aURI);

  return NS_ERROR_FAILURE;
}

//*****************************************************************************

nsresult
nsContextMenuInfo::GetImageRequest(nsIDOMNode *aDOMNode, imgIRequest **aRequest)
{
  NS_ENSURE_ARG(aDOMNode);
  NS_ENSURE_ARG_POINTER(aRequest);

  // Get content
  nsCOMPtr<nsIImageLoadingContent> content(do_QueryInterface(aDOMNode));
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

  return content->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                             aRequest);
}

PRBool
nsContextMenuInfo::HasBackgroundImage(nsIDOMNode * aDOMNode)
{
  NS_ENSURE_ARG(aDOMNode);

  nsCOMPtr<imgIRequest> request;
  GetBackgroundImageRequest(aDOMNode, getter_AddRefs(request));
  
  return (request != nsnull);
}

nsresult
nsContextMenuInfo::GetBackgroundImageRequest(nsIDOMNode *aDOMNode, imgIRequest **aRequest)
{

  NS_ENSURE_ARG(aDOMNode);
  NS_ENSURE_ARG_POINTER(aRequest);

  nsCOMPtr<nsIDOMNode> domNode = aDOMNode;

  // special case for the <html> element: if it has no background-image
  // we'll defer to <body>
  nsCOMPtr<nsIDOMHTMLHtmlElement> htmlElement = do_QueryInterface(domNode);
  if (htmlElement) {
    nsAutoString nameSpace;
    htmlElement->GetNamespaceURI(nameSpace);
    if (nameSpace.IsEmpty()) {
      nsresult rv = GetBackgroundImageRequestInternal(domNode, aRequest);
      if (NS_SUCCEEDED(rv) && *aRequest)
        return NS_OK;

      // no background-image found
      nsCOMPtr<nsIDOMDocument> document;
      domNode->GetOwnerDocument(getter_AddRefs(document));
      nsCOMPtr<nsIDOMHTMLDocument> htmlDocument(do_QueryInterface(document));
      NS_ENSURE_TRUE(htmlDocument, NS_ERROR_FAILURE);

      nsCOMPtr<nsIDOMHTMLElement> body;
      htmlDocument->GetBody(getter_AddRefs(body));
      domNode = do_QueryInterface(body);
    }
  }
  return GetBackgroundImageRequestInternal(domNode, aRequest);
}

nsresult
nsContextMenuInfo::GetBackgroundImageRequestInternal(nsIDOMNode *aDOMNode, imgIRequest **aRequest)
{

  nsCOMPtr<nsIDOMNode> domNode = aDOMNode;
  nsCOMPtr<nsIDOMNode> parentNode;

  nsCOMPtr<nsIDOMDocument> document;
  domNode->GetOwnerDocument(getter_AddRefs(document));
  nsCOMPtr<nsIDOMDocumentView> docView(do_QueryInterface(document));
  NS_ENSURE_TRUE(docView, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMAbstractView> defaultView;
  docView->GetDefaultView(getter_AddRefs(defaultView));
  nsCOMPtr<nsIDOMViewCSS> defaultCSSView(do_QueryInterface(defaultView));
  NS_ENSURE_TRUE(defaultCSSView, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMCSSPrimitiveValue> primitiveValue;
  nsAutoString bgStringValue;

  while (PR_TRUE) {
    nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(domNode));
    // bail for the parent node of the root element or null argument
    if (!domElement)
      break;
    
    nsCOMPtr<nsIDOMCSSStyleDeclaration> computedStyle;
    defaultCSSView->GetComputedStyle(domElement, EmptyString(),
                                     getter_AddRefs(computedStyle));
    if (computedStyle) {
      nsCOMPtr<nsIDOMCSSValue> cssValue;
      computedStyle->GetPropertyCSSValue(NS_LITERAL_STRING("background-image"),
                                         getter_AddRefs(cssValue));
      primitiveValue = do_QueryInterface(cssValue);
      if (primitiveValue) {
        primitiveValue->GetStringValue(bgStringValue);
        if (!bgStringValue.EqualsLiteral("none")) {
          nsCOMPtr<nsIURI> bgUri;
          NS_NewURI(getter_AddRefs(bgUri), bgStringValue);
          NS_ENSURE_TRUE(bgUri, NS_ERROR_FAILURE);

          nsCOMPtr<imgILoader> il(do_GetService(
                                    "@mozilla.org/image/loader;1"));
          NS_ENSURE_TRUE(il, NS_ERROR_FAILURE);

          return il->LoadImage(bgUri, nsnull, nsnull, nsnull, nsnull, nsnull,
                               nsIRequest::LOAD_NORMAL, nsnull, nsnull,
                               aRequest);
        }
      }

      // bail if we encounter non-transparent background-color
      computedStyle->GetPropertyCSSValue(NS_LITERAL_STRING("background-color"),
                                         getter_AddRefs(cssValue));
      primitiveValue = do_QueryInterface(cssValue);
      if (primitiveValue) {
        primitiveValue->GetStringValue(bgStringValue);
        if (!bgStringValue.EqualsLiteral("transparent"))
          return NS_ERROR_FAILURE;
      }
    }

    domNode->GetParentNode(getter_AddRefs(parentNode));
    domNode = parentNode;
  }

  return NS_ERROR_FAILURE;
}
