/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Chris Saari <saari@netscape.com>
 *   Conrad Carlen <ccarlen@netscape.com>
 */

#include "nsContextMenuInfo.h"

#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIImageLoadingContent.h"
#include "imgIRequest.h"
#include "nsICanvasFrame.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsNetUtil.h"
#include "nsUnicharUtils.h"

//*****************************************************************************
// class nsContextMenuInfo
//*****************************************************************************

NS_IMPL_ISUPPORTS1(nsContextMenuInfo, nsIContextMenuInfo)

nsContextMenuInfo::nsContextMenuInfo() :
  mCachedBGImageRequestNode(nsnull)
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
  if (localName.Equals(NS_LITERAL_STRING("a")) ||
      localName.Equals(NS_LITERAL_STRING("area")) ||
      localName.Equals(NS_LITERAL_STRING("link"))) {
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
      if (localName.Equals(NS_LITERAL_STRING("a"))) {
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
nsContextMenuInfo::GetImageRequest(nsIDOMNode * aDOMNode, imgIRequest ** aRequest)
{
  NS_ENSURE_ARG(aDOMNode);
  NS_ENSURE_ARG_POINTER(aRequest);

  // Get content
  nsCOMPtr<nsIImageLoadingContent> content = do_QueryInterface(aDOMNode);
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
nsContextMenuInfo::GetBackgroundImageRequest(nsIDOMNode * aDOMNode, imgIRequest ** aRequest)
{
  nsresult rv = NS_ERROR_FAILURE;

  NS_ENSURE_ARG(aDOMNode);
  NS_ENSURE_ARG_POINTER(aRequest);

  if (mCachedBGImageRequest && (aDOMNode == mCachedBGImageRequestNode)) {
    *aRequest = mCachedBGImageRequest;
    NS_ADDREF(*aRequest);
    return NS_OK;
  }

  // Get content
  nsCOMPtr<nsIContent> content = do_QueryInterface(aDOMNode);
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

  // Get Document
  nsCOMPtr<nsIDocument> document = content->GetDocument();
  NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);
  
  // Get shell
  nsIPresShell *presShell = document->GetShellAt(0);
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  // Get PresContext
  nsCOMPtr<nsIPresContext> presContext;
  presShell->GetPresContext(getter_AddRefs(presContext));
  NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);
   
  const nsStyleBackground* bg;
  nsIFrame* frame = nsnull;
  rv = presShell->GetPrimaryFrameFor(content, &frame);
  if (NS_SUCCEEDED(rv) && frame)
  {
    // look for a background image on the element
    do {
      bg = frame->GetStyleBackground();
      frame = frame->GetParent();
    } while (!bg->mBackgroundImage && frame);
     
    if (bg->mBackgroundImage)
    {
      nsIFrame *pBGFrame = nsnull;
      rv = GetFrameForBackgroundUpdate(presContext, frame, &pBGFrame);
      if (NS_SUCCEEDED(rv) &&  pBGFrame)
      {
        // Lookup the background image
        mCachedBGImageRequestNode = aDOMNode;
        rv = presContext->LoadImage(bg->mBackgroundImage, pBGFrame, getter_AddRefs(mCachedBGImageRequest));
        *aRequest = mCachedBGImageRequest;
        NS_IF_ADDREF(*aRequest);
        return rv;
      }
    } 
  } // if (NS_SUCCEEDED(rv) && frame)

  // nothing on the element or its parent style contexts, fall back to canvas frame for the whole page
  rv = NS_ERROR_FAILURE;
  nsIContent *rootContent = document->GetRootContent();
  NS_ENSURE_TRUE(rootContent, NS_ERROR_FAILURE);
 
  presShell->GetPrimaryFrameFor(rootContent, &frame);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  frame = frame->GetParent();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);
  
  nsICanvasFrame* canvasFrame;
  if (NS_SUCCEEDED(frame->QueryInterface(NS_GET_IID(nsICanvasFrame), (void**)&canvasFrame))) {
    PRBool isCanvas;
    PRBool foundBackground;
    presContext->FindFrameBackground(frame, &bg, &isCanvas, &foundBackground);
    if (bg && bg->mBackgroundImage)
    {
      nsIFrame *pBGFrame = nsnull;
      rv = GetFrameForBackgroundUpdate(presContext, frame, &pBGFrame);
      if (NS_SUCCEEDED(rv) &&  pBGFrame)
      {
        // Lookup the background image
        mCachedBGImageRequestNode = aDOMNode;
        rv = presContext->LoadImage(bg->mBackgroundImage, pBGFrame, getter_AddRefs(mCachedBGImageRequest));
        *aRequest = mCachedBGImageRequest;
        NS_IF_ADDREF(*aRequest);
        return rv;
      }
    }
  }
  
  return rv;
}

////////
// Everything following this is copied from nsCSSRendering.cpp. The methods were not publically available,
// Perhaps there is a better, pubically supported way to get the same thing done?
////////

// method GetFrameForBackgroundUpdate
//
// If the frame (aFrame) is the HTML or BODY frame then find the canvas frame and set the
// aBGFrame param to that. This is used when we need a frame to invalidate after an asynch
// image load for the background.
// 
// The check is a bit expensive, however until the canvas frame is somehow cached on the 
// body frame, or the root element, we need to walk the frames up until we find the canvas
//
nsresult
nsContextMenuInfo::GetFrameForBackgroundUpdate(nsIPresContext *aPresContext,
                                               nsIFrame *aFrame,
                                               nsIFrame **aBGFrame)
{
  NS_ASSERTION(aFrame && aBGFrame, "illegal null parameter");

  nsresult rv = NS_OK;

  if (aFrame && aBGFrame) {
    *aBGFrame = aFrame; // default to the frame passed in

    nsIContent* pContent = aFrame->GetContent();
    if (pContent) {
       // make sure that this is the HTML or BODY element
      nsIAtom *tag = pContent->Tag();

      nsCOMPtr<nsIAtom> tag_html = do_GetAtom("html");
      nsCOMPtr<nsIAtom> tag_body = do_GetAtom("body");
      if (tag &&
          tag == tag_html ||
          tag == tag_body) {
        // the frame is the body frame, so we provide the canvas frame
        nsIFrame *pCanvasFrame = aFrame->GetParent();
        while (pCanvasFrame) {
          nsCOMPtr<nsIAtom> mTag_canvasFrame = do_GetAtom("CanvasFrame");
          if (pCanvasFrame->GetType() == mTag_canvasFrame) {
            *aBGFrame = pCanvasFrame;
            break;
          }
          pCanvasFrame = pCanvasFrame->GetParent();
        }
      }// if tag == html or body
    }
  } else {
    rv = NS_ERROR_NULL_POINTER;
  }

  return rv;
}
