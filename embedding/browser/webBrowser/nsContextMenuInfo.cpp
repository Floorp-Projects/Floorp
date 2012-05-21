/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "nsIDOMWindow.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMCSSValue.h"
#include "nsIDOMCSSPrimitiveValue.h"
#include "nsNetUtil.h"
#include "nsUnicharUtils.h"
#include "nsIDocument.h"
#include "nsIPrincipal.h"
#include "nsIChannelPolicy.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIContentPolicy.h"

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
    bool hasAttr;
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
        bool hasAttr;
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
  
  nsCOMPtr<nsIImageLoadingContent> content(do_QueryInterface(mDOMNode));
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);
  return content->GetCurrentURI(aURI);
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

bool
nsContextMenuInfo::HasBackgroundImage(nsIDOMNode * aDOMNode)
{
  NS_ENSURE_TRUE(aDOMNode, false);

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
      NS_ENSURE_TRUE(domNode, NS_ERROR_FAILURE);
    }
  }
  return GetBackgroundImageRequestInternal(domNode, aRequest);
}

nsresult
nsContextMenuInfo::GetBackgroundImageRequestInternal(nsIDOMNode *aDOMNode, imgIRequest **aRequest)
{
  NS_ENSURE_ARG_POINTER(aDOMNode);

  nsCOMPtr<nsIDOMNode> domNode = aDOMNode;
  nsCOMPtr<nsIDOMNode> parentNode;

  nsCOMPtr<nsIDOMDocument> document;
  domNode->GetOwnerDocument(getter_AddRefs(document));
  NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMWindow> window;
  document->GetDefaultView(getter_AddRefs(window));
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMCSSPrimitiveValue> primitiveValue;
  nsAutoString bgStringValue;

  // get Content Security Policy to pass to LoadImage
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(document));
  nsCOMPtr<nsIPrincipal> principal;
  nsCOMPtr<nsIChannelPolicy> channelPolicy;
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  if (doc) {
    principal = doc->NodePrincipal();
    nsresult rv = principal->GetCsp(getter_AddRefs(csp));
    NS_ENSURE_SUCCESS(rv, rv);
    if (csp) {
      channelPolicy = do_CreateInstance("@mozilla.org/nschannelpolicy;1");
      channelPolicy->SetContentSecurityPolicy(csp);
      channelPolicy->SetLoadType(nsIContentPolicy::TYPE_IMAGE);
    }
  }
  
  while (true) {
    nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(domNode));
    // bail for the parent node of the root element or null argument
    if (!domElement)
      break;
    
    nsCOMPtr<nsIDOMCSSStyleDeclaration> computedStyle;
    window->GetComputedStyle(domElement, EmptyString(),
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

          return il->LoadImage(bgUri, nsnull, nsnull, principal, nsnull,
                               nsnull, nsnull, nsIRequest::LOAD_NORMAL, nsnull,
                               nsnull, channelPolicy, aRequest);
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
