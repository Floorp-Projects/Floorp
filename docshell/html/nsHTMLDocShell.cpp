/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Travis Bogard <travis@netscape.com>
 */

#include "nsHTMLDocShell.h"
#include "nsString.h"

//*****************************************************************************
//***    nsHTMLDocShell: Object Management
//*****************************************************************************

nsHTMLDocShell::nsHTMLDocShell() : nsDocShellBase(), mAllowPlugins(PR_TRUE),
   mMarginWidth(0), mMarginHeight(0), mIsFrame(PR_FALSE)
{
}

nsHTMLDocShell::~nsHTMLDocShell()
{
}

NS_IMETHODIMP nsHTMLDocShell::Create(nsISupports* aOuter, const nsIID& aIID, 
	void** ppv)
{
	NS_ENSURE_ARG_POINTER(ppv);
	NS_ENSURE_NO_AGGREGATION(aOuter);

	nsHTMLDocShell* docShell = new  nsHTMLDocShell();
	NS_ENSURE_TRUE(docShell, NS_ERROR_OUT_OF_MEMORY);

	NS_ADDREF(docShell);
	nsresult rv = docShell->QueryInterface(aIID, ppv);
	NS_RELEASE(docShell);  
	return rv;
}

//*****************************************************************************
// nsHTMLDocShell::nsISupports Overrides
//*****************************************************************************   

NS_IMPL_ISUPPORTS7(nsHTMLDocShell, nsIDocShell, nsIHTMLDocShell, 
   nsIDocShellEdit, nsIDocShellFile, nsIGenericWindow, nsIScrollable, 
   nsITextScroll)

//*****************************************************************************
// nsHTMLDocShell::nsIDocShell Overrides
//*****************************************************************************   

NS_IMETHODIMP nsHTMLDocShell::CanHandleContentType(const PRUnichar* contentType, 
   PRBool* canHandle)
{
   NS_ENSURE_ARG_POINTER(canHandle);

   nsAutoString aType(contentType);
                                         
   if(aType.EqualsIgnoreCase("text/html"))
      *canHandle = PR_TRUE;
   else   
      *canHandle = PR_FALSE;
   return NS_OK;
}

//*****************************************************************************
// nsHTMLDocShell::nsIHTMLDocShell
//*****************************************************************************   

NS_IMETHODIMP nsHTMLDocShell::ScrollToNode(nsIDOMNode* aNode)
{
   NS_ENSURE_ARG(aNode);
   NS_ENSURE_STATE(mContentViewer);
   nsCOMPtr<nsIPresShell> presShell;
   NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(presShell)), NS_ERROR_FAILURE);

   // Get the nsIContent interface, because that's what we need to 
   // get the primary frame
   
   nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
   NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

   // Get the primary frame
   nsIFrame* frame;  // Remember Frames aren't ref-counted.  They are in their 
                     // own special little world.

   NS_ENSURE_SUCCESS(GetPrimaryFrameFor(content, &frame),
      NS_ERROR_FAILURE);

   // tell the pres shell to scroll to the frame
   NS_ENSURE_SUCCESS(presShell->ScrollFrameIntoView(frame, 
      NS_PRESSHELL_SCROLL_TOP, NS_PRESSHELL_SCROLL_ANYWHERE), NS_ERROR_FAILURE); 
   return NS_OK; 
}

NS_IMETHODIMP nsHTMLDocShell::GetAllowPlugins(PRBool* aAllowPlugins)
{
   NS_ENSURE_ARG_POINTER(aAllowPlugins);

   *aAllowPlugins = mAllowPlugins;
   return NS_OK;
}

NS_IMETHODIMP nsHTMLDocShell::SetAllowPlugins(PRBool aAllowPlugins)
{
   mAllowPlugins = aAllowPlugins;
   return NS_OK;
}

NS_IMETHODIMP nsHTMLDocShell::GetMarginWidth(PRInt32* aMarginWidth)
{
   NS_ENSURE_ARG_POINTER(aMarginWidth);

   *aMarginWidth = mMarginWidth;
   return NS_OK;  
}

NS_IMETHODIMP nsHTMLDocShell::SetMarginWidth(PRInt32 aMarginWidth)
{
   mMarginWidth = aMarginWidth;
   return NS_OK;
}

NS_IMETHODIMP nsHTMLDocShell::GetMarginHeight(PRInt32* aMarginHeight)
{
   NS_ENSURE_ARG_POINTER(aMarginHeight);

   *aMarginHeight = mMarginHeight;
   return NS_OK; 
}

NS_IMETHODIMP nsHTMLDocShell::SetMarginHeight(PRInt32 aMarginHeight)
{
   mMarginHeight = aMarginHeight;
   return NS_OK;
}

NS_IMETHODIMP nsHTMLDocShell::GetIsFrame(PRBool* aIsFrame)
{
  NS_ENSURE_ARG_POINTER(aIsFrame);

  *aIsFrame = mIsFrame;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLDocShell::SetIsFrame(PRBool aIsFrame)
{
  mIsFrame = aIsFrame;
  return NS_OK;
}

// XXX: SEMANTIC CHANGE! 
//      returns a copy of the string.  Caller is responsible for freeing result
//      using Recycle(aDefaultCharacterSet)
NS_IMETHODIMP nsHTMLDocShell::GetDefaultCharacterSet(PRUnichar** aDefaultCharacterSet)
{
  NS_ENSURE_ARG_POINTER(aDefaultCharacterSet);

/*  if (0 == mDefaultCharacterSet.Length()) 
  {
    if ((nsnull == gDefCharset) || (nsnull == *gDefCharset)) 
    {
      if(mPrefs)
        mPrefs->CopyCharPref("intl.charset.default", &gDefCharset);
    }
    if ((nsnull == gDefCharset) || (nsnull == *gDefCharset))
      mDefaultCharacterSet = "ISO-8859-1";
    else
      mDefaultCharacterSet = gDefCharset;
  }
  *aDefaultCharacterSet = mDefaultCharacterSet.ToNewUnicode();     */
  return NS_OK;
}

NS_IMETHODIMP nsHTMLDocShell::SetDefaultCharacterSet(const PRUnichar* aDefaultCharacterSet)
{
/*  mDefaultCharacterSet = aDefaultCharacterSet;
  PRInt32 i, n = mChildren.Count();
  for (i = 0; i < n; i++) {
    nsIWebShell* child = (nsIWebShell*) mChildren.ElementAt(i);
    if (nsnull != child) {
      child->SetDefaultCharacterSet(aDefaultCharacterSet);
    }
  }     */
  return NS_OK;
}

// XXX: SEMANTIC CHANGE! 
//      returns a copy of the string.  Caller is responsible for freeing result
//      using Recycle(aForceCharacterSet)
NS_IMETHODIMP nsHTMLDocShell::GetForceCharacterSet(PRUnichar** aForceCharacterSet)
{
  NS_ENSURE_ARG_POINTER(aForceCharacterSet);

/*  nsAutoString emptyStr;
  if (mForceCharacterSet.Equals(emptyStr)) {
    *aForceCharacterSet = nsnull;
  }
  else {
    *aForceCharacterSet = mForceCharacterSet.ToNewUnicode();
  }        */
  return NS_OK;
}

NS_IMETHODIMP nsHTMLDocShell::SetForceCharacterSet(const PRUnichar* forceCharacterSet)
{
/*  mForceCharacterSet = aForceCharacterSet;
  PRInt32 i, n = mChildren.Count();
  for (i = 0; i < n; i++) {
    nsIWebShell* child = (nsIWebShell*) mChildren.ElementAt(i);
    if (nsnull != child) {
      child->SetForceCharacterSet(aForceCharacterSet);
    }
  }   */
  return NS_OK;
}

// XXX: poor error checking
NS_IMETHODIMP nsHTMLDocShell::SizeToContent()
{
/*  nsresult rv;

  // get the presentation shell
  nsCOMPtr<nsIPresShell> presShell;
  nsresult rv = GetPresShell(getter_AddRefs(presShell));
  if (NS_FAILED(rv)) { return rv; }
  if (!presShell) { return NS_ERROR_NOT_INITIALIZED; }

  nsRect  shellArea;
  PRInt32 width, height;
  float   pixelScale;
  rv = presShell->ResizeReflow(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  if (NS_FAILED(rv)) { return rv; }

  // so how big is it?
  pcx->GetVisibleArea(shellArea);
  pcx->GetTwipsToPixels(&pixelScale);
  width = PRInt32((float)shellArea.width*pixelScale);
  height = PRInt32((float)shellArea.height*pixelScale);

  // if we're the outermost webshell for this window, size the window
  if (mContainer) 
  {
    nsCOMPtr<nsIBrowserWindow> browser = do_QueryInterface(mContainer);
    if (browser) 
    {
      nsCOMPtr<nsIWebShell> browserWebShell;
      PRInt32 oldX, oldY, oldWidth, oldHeight,
              widthDelta, heightDelta;
      nsRect  windowBounds;

      GetBounds(oldX, oldY, oldWidth, oldHeight);
      widthDelta = width - oldWidth;
      heightDelta = height - oldHeight;
      browser->GetWindowBounds(windowBounds);
      browser->SizeWindowTo(windowBounds.width + widthDelta,
                            windowBounds.height + heightDelta);
    }
  }
  return rv; */
  return NS_ERROR_FAILURE;
}

//*****************************************************************************
// nsHTMLDocShell::nsIDocShellEdit Overrides
//*****************************************************************************   


/* the basic idea here is to grab the topmost content object
 * (for HTML documents, that's the BODY)
 * and select all it's children
 */
NS_IMETHODIMP nsHTMLDocShell::SelectAll()
{
   NS_ENSURE_STATE(mContentViewer);
   nsCOMPtr<nsIPresShell> presShell;
   NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(presShell)), NS_ERROR_FAILURE);
/*  XXX Implement - There is something not quite right with the objects
   being retrieved.  bodyNode isn't defined and bodyElement isn't used
   and nor is node.
   // get the selection object
   nsCOMPtr<nsIDOMSelection> selection;
   NS_ENSURE_SUCCESS(mPresShell->GetSelection(SELECTION_NORMAL, 
      getter_AddRefs(selection)), NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

   // get the body node
   nsCOMPtr<nsIDOMNodeList> nodeList;
   nsCOMPtr<nsIDOMElement> bodyElement;
   nsAutoString bodyTag = "body";

   nsCOMPtr<nsIDOMDocument> document;
   NS_ENSURE_SUCCESS(GetDocument(getter_AddRefs(document)), NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(document->GetElementsByTagName(bodyTag, 
      getter_AddRefs(nodeList)), NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(nodeList, NS_OK); // this means the document has no body, so nothing to select

   PRUint32 count; 
   NS_ENSURE_SUCCESS(nodeList->GetLength(&count), NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(count != 1, NS_OK); // could be true for a frameset doc

   // select all children of the body
   nsCOMPtr<nsIDOMNode> node;
   NS_ENSURE_SUCCESS(nodeList->Item(0, getter_AddRefs(node)), NS_ERROR_FAILURE); 
   NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(selection->Collapse(bodyNode, 0), NS_ERROR_FAILURE);

   PRInt32 numBodyChildren=0;
   nsCOMPtr<nsIDOMNode>lastChild;
   NS_ENSURE_SUCCESS(bodyNode->GetLastChild(getter_AddRefs(lastChild)), 
      NS_ERROR_FAILURE);
   
   NS_ENSURE_TRUE(lastChild, NS_OK); // body isn't required to have any children, so it's ok if lastChild is null

   NS_ENSURE_SUCCESS(GetChildOffset(lastChild, bodyNode, numBodyChildren), 
      NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(selection->Extend(bodyNode, numBodyChildren+1);
   */
   return NS_OK;
}

//*****************************************************************************
// nsHTMLDocShell::nsIDocShellFile Overrides
//*****************************************************************************   

//*****************************************************************************
// nsHTMLDocShell::nsIGenericWindow Overrides
//*****************************************************************************   


/*NS_IMETHODIMP GetVisibility(PRBool *aVisibility)
{
  NS_ENSURE_ARG_POINTER(aVisibility);

  nsCOMPtr<nsIViewManager> viewManager;
	nsresult rv = GetViewManager(getter_AddRefs(viewManager));
  if (NS_FAILED(rv)) { return rv; }
  if (!viewManager) { return NS_ERROR_NOT_INITIALIZED; }

  nsIView *rootView; // views are not ref counted
  viewManager->GetRootView(&rootView);
  if (NS_FAILED(rv)) { return rv; }
  if (!rootView) { return NS_ERROR_NOT_INITIALIZED; }

  rv = rootView->GetVisibility(*aVisibility);
  return rv;
}

NS_IMETHODIMP SetVisibility(PRBool aVisibility)
{
  nsCOMPtr<nsIViewManager> viewManager;
	nsresult rv = GetViewManager(getter_AddRefs(viewManager));
  if (NS_FAILED(rv)) { return rv; }
  if (!viewManager) { return NS_ERROR_NOT_INITIALIZED; }

  nsIView *rootView; // views are not ref counted
  viewManager->GetRootView(&rootView);
  if (NS_FAILED(rv)) { return rv; }
  if (!rootView) { return NS_ERROR_NOT_INITIALIZED; }

  rv = rootView->SetVisibility(aVisibility);
  return rv;
}      */


// XXX: SEMANTIC CHANGE! 
//      returns a copy of the string.  Caller is responsible for freeing result
//      using Recycle(aTitle)
/*NS_IMETHODIMP GetTitle(PRUnichar **aTitle)
{
  NS_ENSURE_ARG_POINTER(aTitle);

  *aTitle = mTitle.GetNewUnicode();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP SetTitle(const PRUnichar * aTitle)
{
  NS_ENSURE_ARG_POINTER(aTitle);
  
  // Record local title
  mTitle = aTitle;

  // Title's set on the top level web-shell are passed on to the container
  nsIWebShell* parent;
  GetParent(parent);
  if (nsnull == parent) { // null parent means it's top-level doc shell
    nsIBrowserWindow *browserWindow = GetBrowserWindow();
    if (nsnull != browserWindow) {
      browserWindow->SetTitle(aTitle);
      NS_RELEASE(browserWindow);

      // XXX: remove when global history is pulled out of docShell
      // Oh this hack sucks. But there isn't any other way that I can
      // reliably get the title text. Sorry.
      do {
        nsresult rv;
        NS_WITH_SERVICE(nsIGlobalHistory, history, "component://netscape/browser/global-history", &rv);
        if (NS_FAILED(rv)) break;

        rv = history->SetPageTitle(nsCAutoString(mURL), aTitle);
        if (NS_FAILED(rv)) break;
      } while (0);
    }
  } else {
    parent->SetTitle(aTitle);
    NS_RELEASE(parent);
  }

  return NS_OK;
}       */

/*NS_IMETHODIMP GetZoom(float *aZoom)
{
  NS_ENSURE_ARG_POINTER(aZoom);

  *aZoom = mZoom;
  return NS_OK;
}

// XXX: poor error checking
NS_IMETHODIMP SetZoom(float aZoom)
{
  mZoom = aZoom;

  if (mDeviceContext)
    mDeviceContext->SetZoom(mZoom);

	nsCOMPtr<nsIViewManager> viewManager;
	rv = GetViewManager(getter_AddRefs(viewManager));
  if (NS_FAILED(rv)) { return rv; }
  if (!viewManager) { return NS_ERROR_NULL_POINTER; }

  nsIView *rootview = nsnull;
  nsIScrollableView *sv = nsnull;
  viewManager->GetRootScrollableView(&sv);
  if (nsnull != sv) {
    sv->ComputeScrollOffsets();
  }
  vm->GetRootView(rootview);
  if (nsnull != rootview) {
    vm->UpdateView(rootview, nsnull, 0);
  }
  return NS_OK;
}    */

//*****************************************************************************
// nsHTMLDocShell::nsIScrollable Overrides
//*****************************************************************************   

//*****************************************************************************
// nsHTMLDocShell::nsITextScroll Overrides
//*****************************************************************************   

//*****************************************************************************
// nsHTMLDocShell: Helper Routines
//***************************************************************************** 

nsresult nsHTMLDocShell::GetPrimaryFrameFor(nsIContent* content, nsIFrame** frame)
{
   //XXX Implement
   return NS_ERROR_FAILURE;
}