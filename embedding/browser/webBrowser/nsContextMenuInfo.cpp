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
#include "nsIImageFrame.h"
#include "imgIRequest.h"
#include "nsIStyleContext.h"
#include "nsICanvasFrame.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsNetUtil.h"

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
NS_IMETHODIMP nsContextMenuInfo::GetMouseEvent(nsIDOMEvent **aEvent)
{
  NS_ENSURE_ARG_POINTER(aEvent);
  NS_IF_ADDREF(*aEvent = mMouseEvent);
  return NS_OK;
}

/* readonly attribute nsIDOMNode targetNode; */
NS_IMETHODIMP nsContextMenuInfo::GetTargetNode(nsIDOMNode **aNode)
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
NS_IMETHODIMP nsContextMenuInfo::GetImageContainer(imgIContainer **aImageContainer)
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
NS_IMETHODIMP nsContextMenuInfo::GetImageSrc(nsIURI **aURI)
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
  nsCOMPtr<nsIContent> content = do_QueryInterface(aDOMNode);
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

  // Get Document
  nsCOMPtr<nsIDocument> document;
  content->GetDocument(*getter_AddRefs(document));
  NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);
  
  // Get shell
  nsCOMPtr<nsIPresShell> presShell;
  document->GetShellAt(0, getter_AddRefs(presShell));
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  // Get PresContext
  nsCOMPtr<nsIPresContext> presContext;
  presShell->GetPresContext(getter_AddRefs(presContext));
  NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

  nsIFrame *frame = nsnull;
  presShell->GetPrimaryFrameFor(content, &frame); // does not AddRef
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);
  
  nsIImageFrame *imgFrame = nsnull;
  frame->QueryInterface(NS_GET_IID(nsIImageFrame), (void **)&imgFrame); // does not AddRef
  NS_ENSURE_TRUE(imgFrame, NS_ERROR_FAILURE);

  return imgFrame->GetImageRequest(aRequest);
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
  nsCOMPtr<nsIDocument> document;
  content->GetDocument(*getter_AddRefs(document));
  NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);
  
  // Get shell
  nsCOMPtr<nsIPresShell> presShell;
  document->GetShellAt(0, getter_AddRefs(presShell));
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
    nsCOMPtr<nsIStyleContext> styleContext;
    rv = presShell->GetStyleContextFor(frame, getter_AddRefs(styleContext));
    if (NS_FAILED(rv) || !styleContext) return rv;

    do {
      bg = (const nsStyleBackground*)
      styleContext->GetStyleData(eStyleStruct_Background);
      styleContext = dont_AddRef(styleContext->GetParent());
    } while (bg && bg->mBackgroundImage.IsEmpty() && styleContext.get());
     
    if (bg && !bg->mBackgroundImage.IsEmpty())
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
  nsCOMPtr<nsIContent> rootContent;
  document->GetRootContent(getter_AddRefs(rootContent));
  NS_ENSURE_TRUE(rootContent, NS_ERROR_FAILURE);
 
  presShell->GetPrimaryFrameFor(rootContent, &frame);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  frame->GetParent(&frame);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);
  
  nsICanvasFrame* canvasFrame;
  if (NS_SUCCEEDED(frame->QueryInterface(NS_GET_IID(nsICanvasFrame), (void**)&canvasFrame))) {
    PRBool isCanvas;
    FindBackground(presContext, frame, &bg, &isCanvas);
    if (bg && !bg->mBackgroundImage.IsEmpty())
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

// nethod GetFrameForBackgroundUpdate
//
// If the frame (aFrame) is the HTML or BODY frame then find the canvas frame and set the
// aBGFrame param to that. This is used when we need a frame to invalidate after an asynch
// image load for the background.
// 
// The check is a bit expensive, however until the canvas frame is somehow cached on the 
// body frame, or the root element, we need to walk the frames up until we find the canvas
//
nsresult nsContextMenuInfo::GetFrameForBackgroundUpdate(nsIPresContext *aPresContext,nsIFrame *aFrame, nsIFrame **aBGFrame)
{
  NS_ASSERTION(aFrame && aBGFrame, "illegal null parameter");

  nsresult rv = NS_OK;

  if (aFrame && aBGFrame) {
    *aBGFrame = aFrame; // default to the frame passed in

    nsCOMPtr<nsIContent> pContent;
    aFrame->GetContent(getter_AddRefs(pContent));
    if (pContent) {
       // make sure that this is the HTML or BODY element
      nsCOMPtr<nsIAtom> tag;
      pContent->GetTag(*(getter_AddRefs(tag)));
      nsCOMPtr<nsIAtom> mTag_html = getter_AddRefs(NS_NewAtom("html"));
      nsCOMPtr<nsIAtom> mTag_body = getter_AddRefs(NS_NewAtom("body"));
      if (tag && 
          tag.get() == mTag_html ||
          tag.get() == mTag_body) {
        // the frame is the body frame, so we provide the canvas frame
        nsIFrame *pCanvasFrame = nsnull;
        aFrame->GetParent(&pCanvasFrame);
        while (pCanvasFrame) {
          nsCOMPtr<nsIAtom>  parentType;
          pCanvasFrame->GetFrameType(getter_AddRefs(parentType));
          nsCOMPtr<nsIAtom> mTag_canvasFrame = getter_AddRefs(NS_NewAtom("CanvasFrame"));   
          if (parentType.get() == mTag_canvasFrame) {
            *aBGFrame = pCanvasFrame;
            break;
          }
          pCanvasFrame->GetParent(&pCanvasFrame);
        }
      }// if tag == html or body
    }
  } else {
    rv = NS_ERROR_NULL_POINTER;
  }

  return rv;
}

inline PRBool
nsContextMenuInfo::FindCanvasBackground(nsIPresContext* aPresContext,
                     nsIFrame* aForFrame,
                     const nsStyleBackground** aBackground)
{

  // XXXldb What if the root element is positioned, etc.?  (We don't
  // allow that yet, do we?)
  nsIFrame *firstChild;
  aForFrame->FirstChild(aPresContext, nsnull, &firstChild);
  if (firstChild) {
    const nsStyleBackground *result;
    GetStyleData(firstChild, &result);
  
    // for printing and print preview.. this should be a pageContentFrame
    nsCOMPtr<nsIAtom> frameType;
    nsCOMPtr<nsIStyleContext> parentContext;

    firstChild->GetFrameType(getter_AddRefs(frameType));
    nsCOMPtr<nsIAtom> mTag_pageContentFrame = getter_AddRefs(NS_NewAtom("PageContentFrame"));   
    if ( (frameType == mTag_pageContentFrame) ){
      // we have to find the background style ourselves.. since the 
      // pageContentframe does not have content
      while(firstChild){
        for (nsIFrame* kidFrame = firstChild; nsnull != kidFrame; ) {
          kidFrame->GetStyleContext(getter_AddRefs(parentContext));
          result = (nsStyleBackground*)parentContext->GetStyleData(eStyleStruct_Background);
          if (!result->BackgroundIsTransparent()){
            GetStyleData(kidFrame, aBackground);
            return PR_TRUE;
          } else {
            kidFrame->GetNextSibling(&kidFrame); 
          }
        }
        firstChild->FirstChild(aPresContext, nsnull, &firstChild);
      }
      return PR_FALSE;    // nothing found for this
    }

    // Check if we need to do propagation from BODY rather than HTML.
    if (result->BackgroundIsTransparent()) {
      nsCOMPtr<nsIContent> content;
      aForFrame->GetContent(getter_AddRefs(content));
      if (content) {
        nsCOMPtr<nsIDOMNode> node( do_QueryInterface(content) );
        // Use |GetOwnerDocument| so it works during destruction.
        nsCOMPtr<nsIDOMDocument> doc;
        node->GetOwnerDocument(getter_AddRefs(doc));
        nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(doc);
        if (htmlDoc) {
          nsCOMPtr<nsIDOMHTMLElement> body;
          htmlDoc->GetBody(getter_AddRefs(body));
          nsCOMPtr<nsIContent> bodyContent = do_QueryInterface(body);
          // We need to null check the body node (bug 118829) since
          // there are cases, thanks to the fix for bug 5569, where we
          // will reflow a document with no body.  In particular, if a
          // SCRIPT element in the head blocks the parser and then has a
          // SCRIPT that does "document.location.href = 'foo'", then
          // nsParser::Terminate will call |DidBuildModel| methods
          // through to the content sink, which will call |StartLayout|
          // and thus |InitialReflow| on the pres shell.  See bug 119351
          // for the ugly details.
          if (bodyContent) {
            nsCOMPtr<nsIPresShell> shell;
            aPresContext->GetShell(getter_AddRefs(shell));
            nsIFrame *bodyFrame;
            nsresult rv = shell->GetPrimaryFrameFor(bodyContent, &bodyFrame);
            if (NS_SUCCEEDED(rv) && bodyFrame)
              ::GetStyleData(bodyFrame, &result);
          }
        }
      }
    }

    *aBackground = result;
  } else {
    // This should always give transparent, so we'll fill it in with the
    // default color if needed.  This seems to happen a bit while a page is
    // being loaded.
    GetStyleData(aForFrame, aBackground);
  }

  return PR_TRUE;
}

inline PRBool
nsContextMenuInfo::FindElementBackground(nsIPresContext* aPresContext,
                      nsIFrame* aForFrame,
                      const nsStyleBackground** aBackground)
{
  nsIFrame *parentFrame;
  aForFrame->GetParent(&parentFrame);
  // XXXldb We shouldn't have to null-check |parentFrame| here.
  if (parentFrame && IsCanvasFrame(parentFrame)) {
    // Check that we're really the root (rather than in another child list).
    nsIFrame *childFrame;
    parentFrame->FirstChild(aPresContext, nsnull, &childFrame);
    if (childFrame == aForFrame)
      return PR_FALSE; // Background was already drawn for the canvas.
  }

  ::GetStyleData(aForFrame, aBackground);

  nsCOMPtr<nsIContent> content;
  aForFrame->GetContent(getter_AddRefs(content));
  if (!content || !content->IsContentOfType(nsIContent::eHTML))
    return PR_TRUE;  // not frame for an HTML element
  
  if (!parentFrame)
    return PR_TRUE; // no parent to look at
  
  nsCOMPtr<nsIAtom> tag;
  content->GetTag(*getter_AddRefs(tag));
  nsCOMPtr<nsIAtom> mTag_body = getter_AddRefs(NS_NewAtom("body"));
  if (tag != mTag_body)
    return PR_TRUE; // not frame for <BODY> element

  // We should only look at the <html> background if we're in an HTML document
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(content));
  nsCOMPtr<nsIDOMDocument> doc;
  node->GetOwnerDocument(getter_AddRefs(doc));
  nsCOMPtr<nsIDOMHTMLDocument> htmlDoc(do_QueryInterface(doc));
  if (!htmlDoc)
    return PR_TRUE;

  const nsStyleBackground *htmlBG;
  ::GetStyleData(parentFrame, &htmlBG);
  return !htmlBG->BackgroundIsTransparent();
}

PRBool
nsContextMenuInfo::IsCanvasFrame(nsIFrame *aFrame)
{
  nsCOMPtr<nsIAtom> mTag_canvasFrame      = getter_AddRefs(NS_NewAtom("CanvasFrame"));   
  nsCOMPtr<nsIAtom> mTag_rootFrame        = getter_AddRefs(NS_NewAtom("RootFrame"));  
  nsCOMPtr<nsIAtom> mTag_pageFrame        = getter_AddRefs(NS_NewAtom("PageFrame")); 
  nsCOMPtr<nsIAtom> frameType;
  aFrame->GetFrameType(getter_AddRefs(frameType));
  return (frameType == mTag_canvasFrame ||
          frameType == mTag_rootFrame ||
          frameType == mTag_pageFrame);
}

PRBool
nsContextMenuInfo::FindBackground(nsIPresContext* aPresContext,
                               nsIFrame* aForFrame,
                               const nsStyleBackground** aBackground,
                               PRBool* aIsCanvas)
{
  *aIsCanvas = PR_TRUE;
  return FindCanvasBackground(aPresContext, aForFrame, aBackground);
}

