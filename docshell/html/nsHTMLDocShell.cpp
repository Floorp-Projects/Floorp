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

nsHTMLDocShell::nsHTMLDocShell() : nsDocShellBase()
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
	NS_ENSURE(docShell, NS_ERROR_OUT_OF_MEMORY);

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
  NS_ENSURE_ARG(node);

  // get the presentation shell
  nsCOMPtr<nsIPresShell> presShell;
  nsresult rv = GetPresShell(getter_AddRefs(presShell));
  if (NS_FAILED(rv)) { return rv; }
  if (!presShell) { return NS_ERROR_NOT_INITIALIZED; }
  
  // Get the nsIContent interface, because that's what we need to 
  // get the primary frame 
  rv = aNode->QueryInterface(kIContentIID, getter_AddRefs(content)); 
  if (NS_FAILED(rv)) { return rv; }
  if (!content) { return NS_ERROR_NULL_POINTER; }

  // Get the primary frame 
  nsIFrame* frame;
  rv = GetPrimaryFrameFor(content, &frame); 
  if (NS_FAILED(rv)) { return rv; }
  if (!frame) { return NS_ERROR_NULL_POINTER; }
 
  // tell the pres shell to scroll to the frame
  rv = presShell->ScrollFrameIntoView(frame, NS_PRESSHELL_SCROLL_TOP, 
                                      NS_PRESSHELL_SCROLL_ANYWHERE); 
  return rv; 
}

NS_IMETHODIMP nsHTMLDocShell::GetAllowPlugins(PRBool* aAllowPlugins)
{
  NS_ENSURE_ARG_POINTER(allowPlugins);
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

  if (0 == mDefaultCharacterSet.Length()) 
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
  *aDefaultCharacterSet = mDefaultCharacterSet.ToNewUnicode();
  return NS_OK;
}

NS_IMETHODIMP nsHTMLDocShell::SetDefaultCharacterSet(const PRUnichar* aDefaultCharacterSet)
{
  mDefaultCharacterSet = aDefaultCharacterSet;
  PRInt32 i, n = mChildren.Count();
  for (i = 0; i < n; i++) {
    nsIWebShell* child = (nsIWebShell*) mChildren.ElementAt(i);
    if (nsnull != child) {
      child->SetDefaultCharacterSet(aDefaultCharacterSet);
    }
  }
  return NS_OK;
}

// XXX: SEMANTIC CHANGE! 
//      returns a copy of the string.  Caller is responsible for freeing result
//      using Recycle(aForceCharacterSet)
NS_IMETHODIMP nsHTMLDocShell::GetForceCharacterSet(PRUnichar** aForceCharacterSet)
{
  NS_ENSURE_ARG_POINTER(forceCharacterSet);

  nsAutoString emptyStr;
  if (mForceCharacterSet.Equals(emptyStr)) {
    *aForceCharacterSet = nsnull;
  }
  else {
    *aForceCharacterSet = mForceCharacterSet.ToNewUnicode();
  }
  return NS_OK;
}

NS_IMETHODIMP nsHTMLDocShell::SetForceCharacterSet(const PRUnichar* forceCharacterSet)
{
  mForceCharacterSet = aForceCharacterSet;
  PRInt32 i, n = mChildren.Count();
  for (i = 0; i < n; i++) {
    nsIWebShell* child = (nsIWebShell*) mChildren.ElementAt(i);
    if (nsnull != child) {
      child->SetForceCharacterSet(aForceCharacterSet);
    }
  }
  return NS_OK;
}

//*****************************************************************************
// nsHTMLDocShell::nsIDocShellEdit Overrides
//*****************************************************************************   

NS_IMETHODIMP ClearSelection()
{
  NS_ENSURE(mDoc, NS_ERROR_NOT_INITIALIZED);

  // get the presentation shell
  nsCOMPtr<nsIPresShell> presShell;
  nsresult rv = GetPresShell(getter_AddRefs(presShell));
  if (NS_FAILED(rv)) { return rv; }
  if (!presShell) { return NS_ERROR_NOT_INITIALIZED; }

  // get the selection object
  nsresult rv = presShell->GetSelection(SELECTION_NORMAL, getter_AddRefs(selection));
  if (NS_FAILED(rv)) { return rv; }
  if (!selection) { return NS_ERROR_NULL_POINTER; }

  // clear the selection
  selection->ClearSelection();

  return rv;
}

/* the basic idea here is to grab the topmost content object
 * (for HTML documents, that's the BODY)
 * and select all it's children
 */
NS_IMETHODIMP SelectAll()
{
  NS_ENSURE(mDoc, NS_ERROR_NOT_INITIALIZED);

  // get the presentation shell
  nsCOMPtr<nsIPresShell> presShell;
  nsresult rv = GetPresShell(getter_AddRefs(presShell));
  if (NS_FAILED(rv)) { return rv; }
  if (!presShell) { return NS_ERROR_NOT_INITIALIZED; }

  // get the selection object
  nsresult rv = presShell->GetSelection(SELECTION_NORMAL, getter_AddRefs(selection));
  if (NS_FAILED(rv)) { return rv; }
  if (!selection) { return NS_ERROR_NULL_POINTER; }

  // get the body node
  nsCOMPtr<nsIDOMNodeList>nodeList;
  nsCOMPtr<nsIDOMElement>bodyElement;
  nsAutoString bodyTag = "body";
  rv = mDoc->GetElementsByTagName(bodyTag, getter_AddRefs(nodeList));
  if (NS_FAILED(rv)) { return rv; }
  if (!nodeList) { return NS_OK; } // this means the document has no body, so nothing to select

  PRUint32 count; 
  nodeList->GetLength(&count);
  if (count != 1) { return NS_ERROR_FAILURE; }  // could be true for a frameset doc

  // select all children of the body
  nsCOMPtr<nsIDOMNode> node;
  rv = nodeList->Item(0, getter_AddRefs(node)); 
  if (NS_FAILED(rv)) { return rv; }
  if (!node) { return NS_ERROR_NULL_POINTER; }

  rv = aSelection->Collapse(bodyNode, 0);
  if (NS_FAILED(rv)) { return rv; }
  PRInt32 numBodyChildren=0;
  nsCOMPtr<nsIDOMNode>lastChild;
  rv = bodyNode->GetLastChild(getter_AddRefs(lastChild));
  if (NS_FAILED(rv)) { return rv; }
  if (lastChild)
  { // body isn't required to have any children, so it's ok if lastChild is null
    rv = GetChildOffset(lastChild, bodyNode, numBodyChildren);
    if (NS_FAILED(rv)) { return rv; }
    rv = aSelection->Extend(bodyNode, numBodyChildren+1);
  }
  return rv;
}

// the pres shell knows how to copy, so let it do the work
NS_IMETHODIMP CopySelection()
{
  // get the presentation shell
  nsCOMPtr<nsIPresShell> presShell;
  nsresult rv = GetPresShell(getter_AddRefs(presShell));
  if (NS_FAILED(rv)) { return rv; }
  if (!presShell) { return NS_ERROR_NOT_INITIALIZED; }

  return presShell->DoCopy();
}

/* the docShell is "copyable" if it has a selection and the selection is not
 * collapsed (that is, at least one token is selected, a character or a node
 */
NS_IMETHODIMP GetCopyable(PRBool *aCopyable)
{
  NS_ENSURE_ARG_POINTER(aCopyable);
  
  *aCopyable = PR_FALSE;

  // get the presentation shell
  nsCOMPtr<nsIPresShell> presShell;
  nsresult rv = GetPresShell(getter_AddRefs(presShell));
  if (NS_FAILED(rv)) { return rv; }
  if (!presShell) { return NS_ERROR_NOT_INITIALIZED; }

  // get the selection object
  nsresult rv = presShell->GetSelection(SELECTION_NORMAL, getter_AddRefs(selection));
  if (NS_FAILED(rv)) { return rv; }
  if (!selection) { return NS_OK; }   // no selection means not copyable

  PRBool isCollapsed=PR_TRUE;
  selection->GetIsCollapsed(&isCollapsed);
  *aCopyable = (PRBool)(!isCollapsed);

  return NS_OK;
}

/* cut is an editing operation, disallowed by base html doc shell */
/* XXX: we could set this up so we look for the editor, and if there is
 *      one, we let it do the cut.
 *      This comment extends to GetCutable, Paste, and GetPasteable
 */
NS_IMETHODIMP CutSelection()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

  /* readonly attribute boolean cutable; */
NS_IMETHODIMP GetCutable(PRBool *aCutable)
{
  NS_ENSURE_ARG_POINTER(aCutable);
  *aCutable = PR_FALSE;
  return NS_OK; 
}

  /* void Paste (); */
NS_IMETHODIMP Paste()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

  /* readonly attribute boolean pasteable; */
NS_IMETHODIMP GetPasteable(PRBool *aPasteable)
{
  NS_ENSURE_ARG_POINTER(aCutable);
  *aCutable = PR_FALSE;
  return NS_OK; 
}

//*****************************************************************************
// nsHTMLDocShell::nsIDocShellFile Overrides
//*****************************************************************************   

NS_IMETHODIMP Save()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

  /* readonly attribute boolean saveable; */
NS_IMETHODIMP GetSaveable(PRBool *aSaveable)
{
  NS_ENSURE_ARG_POINTER(aSaveable);
  *aSaveable = PR_TRUE;
  return NS_OK;
}

  /* void Print (); */
NS_IMETHODIMP Print()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
  /* readonly attribute boolean printable; */
NS_IMETHODIMP GetPrintable(PRBool *aPrintable)
{
  NS_ENSURE_ARG_POINTER(aPrintable);
  *aPrintable = PR_TRUE;
  return NS_OK;
}


//*****************************************************************************
// nsHTMLDocShell::nsIGenericWindow Overrides
//*****************************************************************************   

  /* [noscript] void initWindow (in nativeWindow parentNativeWindow, in nsIWidget parentWidget, in long x, in long y, in long cx, in long cy); */
NS_IMETHODIMP InitWindow(nativeWindow aParentNativeWindow, nsIWidget * aParentWidget, PRInt32 aX, PRInt32 aY, PRInt32 aCX, PRInt32 aCY)
{
  NS_ENSURE_ARG_POINTER(aParentWidget);

  return NS_ERROR_NOT_IMPLEMENTED;
}
  /* void create (); */
NS_IMETHODIMP Create()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

  /* void destroy (); */
NS_IMETHODIMP Destroy()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

  /* void setPosition (in long x, in long y); */
NS_IMETHODIMP SetPosition(PRInt32 aX, PRInt32 aY)
{
  PRInt32 w, h;
  nsresult rv = GetSize(w, h);
  if (NS_FAILED(rv)) { return rv; }

  PRInt32 borderWidth  = 0;
  PRInt32 borderHeight = 0;
  if (mWindow) 
  {
    mWindow->GetBorderSize(borderWidth, borderHeight);
    // Don't have the widget repaint. Layout will generate repaint requests
    // during reflow
    mWindow->Resize(aX, aY, w, h, PR_FALSE);
  }

  mBounds.SetRect(aX,aY,w,h);   // set the webshells bounds 

  return rv;
}

NS_IMETHODIMP GetPosition(PRInt32 *aX, PRInt32 *aY)
{
  NS_ENSURE_ARG_POINTER(aX);
  NS_ENSURE_ARG_POINTER(aY);

  nsRect result;
  if (nsnull != mWindow) {
    mWindow->GetClientBounds(result);
  } else {
    result = mBounds;
  }

  *aX = result.x;
  *aY = result.y;

  return NS_OK;
}

NS_IMETHODIMP SetSize(PRInt32 aCX, PRInt32 aCY, PRBool aRepaint)
{
  PRInt32 x, y;
  nsresult rv = GetPosition(x, y);
  if (NS_FAILED(rv)) { return rv; }

  PRInt32 borderWidth  = 0;
  PRInt32 borderHeight = 0;
  if (mWindow) 
  {
    mWindow->GetBorderSize(borderWidth, borderHeight);
    // Don't have the widget repaint. Layout will generate repaint requests
    // during reflow
    mWindow->Resize(x, y, aCX, aCY, PR_FALSE);
  }

  mBounds.SetRect(x, y, aCX, aCY);   // set the webshells bounds --dwc0001

  // Set the size of the content area, which is the size of the window
  // minus the borders
  if (nsnull != mContentViewer) {
    nsRect rr(0, 0, aCX-(borderWidth*2), aCY-(borderHeight*2));
    mContentViewer->SetBounds(rr);
  }
  return rv;
}

  /* void getSize (out long cx, out long cy); */
NS_IMETHODIMP GetSize(PRInt32 *aCX, PRInt32 *aCY)
{
  NS_ENSURE_ARG_POINTER(aCX);
  NS_ENSURE_ARG_POINTER(aCY);

  nsRect result;
  if (nsnull != mWindow) {
    mWindow->GetClientBounds(result);
  } else {
    result = mBounds;
  }

  *aCX = result.width;
  *aCY = result.height;

  return NS_OK;
}

/* XXX: aRepaint is unused in this implementation
 *      the only way I know how to use it is to turn off updates on the view manager, 
 *      but then when do they get turned back on?  We'd need a "repaint now" call
 *      from the caller, I guess?
 *      If aRepaint makes sense here, it should make sense on SetPosition and SetSize as well
 */

NS_IMETHODIMP SetPositionAndSize(PRInt32 aX, PRInt32 aY, PRInt32 aCX, PRInt32 aCY, PRBool aRepaint)
{
  PRInt32 borderWidth  = 0;
  PRInt32 borderHeight = 0;
  if (mWindow) 
  {
    mWindow->GetBorderSize(borderWidth, borderHeight);
    // Don't have the widget repaint. Layout will generate repaint requests
    // during reflow
    mWindow->Resize(aX, aY, aCX, aCY, PR_FALSE);
  }

  mBounds.SetRect(aX, aY, aCX, aCY);   // set the webshells bounds --dwc0001

  // Set the size of the content area, which is the size of the window
  // minus the borders
  if (nsnull != mContentViewer) {
    nsRect rr(0, 0, aCX-(borderWidth*2), aCY-(borderHeight*2));
    mContentViewer->SetBounds(rr);
  }
  return rv;
}

// XXX: poor error checking
NS_IMETHODIMP SizeToContent()
{
  nsresult rv;

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
  return rv;
}

NS_IMETHODIMP Repaint(PRBool aForce)
{
  if (mWindow) {
    mWindow->Invalidate(aForce);
  }

	nsresult rv;
	nsCOMPtr<nsIViewManager> viewManager;
	rv = GetViewManager(getter_AddRefs(viewManager));
  if (NS_FAILED(rv)) { return rv; }
  if (!viewManager) { return NS_ERROR_NULL_POINTER; }

  //XXX: what about aForce?
	rv = viewManager->UpdateAllViews(0);
	return rv;
}

NS_IMETHODIMP GetParentWidget(nsIWidget **aParentWidget)
{
  NS_ENSURE_ARG_POINTER(aParentWidget);

  aParentWidget = mParentWidget;
  return NS_OK;
}

NS_IMETHODIMP SetParentWidget(nsIWidget * aParentWidget)
{
  NS_ENSURE_ARG_POINTER(aParentWidget);
  
  mParentWidget = aParentWidget;
  return NS_OK;
}

  /* attribute nativeWindow parentNativeWindow; */
NS_IMETHODIMP GetParentNativeWindow(nativeWindow **aParentNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aParentWidget);

  aParentNativeWindow = mParentNativeWindow;
  return NS_OK;
}

NS_IMETHODIMP SetParentNativeWindow(nativeWindow *aParentNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aParentWidget);

  mParentNativeWindow = aParentNativeWindow;
  return NS_OK;
}

NS_IMETHODIMP GetVisibility(PRBool *aVisibility)
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
}

  /* readonly attribute nsIWidget mainWidget; */
NS_IMETHODIMP GetMainWidget(nsIWidget **aMainWidget)
{
  NS_ENSURE_ARG_POINTER(aMainWidget);

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP SetFocus()
{
  if (mWindow) {
    mWindow->SetFocus();
  }

  return NS_OK;

}

NS_IMETHODIMP RemoveFocus()
{
  if (nsnull != mWindow) {
    nsIWidget *parentWidget = mWindow->GetParent();
    if (nsnull != parentWidget) {
      parentWidget->SetFocus();
      NS_RELEASE(parentWidget);
    }
  }
  return NS_OK;
}

// XXX: SEMANTIC CHANGE! 
//      returns a copy of the string.  Caller is responsible for freeing result
//      using Recycle(aTitle)
NS_IMETHODIMP GetTitle(PRUnichar **aTitle)
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
}

NS_IMETHODIMP GetZoom(float *aZoom)
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
}

//*****************************************************************************
// nsHTMLDocShell::nsIScrollable Overrides
//*****************************************************************************   

NS_IMETHODIMP GetCurScrollPos(PRInt32 aScrollOrientation, PRInt32 *aCurPos)
{
  NS_ENSURE_ARG_POINTER(aCurPos);

  nsIScrollableView *scrollView=nsnull;
  nsresult rv = GetRootScrollableView(&scrollView);
  if (NS_FAILED(rv)) { return rv; }
  if (!scrollView) { return NS_ERROR_NOT_INITIALIZED; }

  nscoord x, y;
  rv = scrollView->GetScrollPosition(x, y);
  if (NS_FAILED(rv)) { return rv; }

  if (ScrollOrientation_X==aScrollOrientation)
    *aCurPos = x;
  else if (ScrollOrientation_Y==aScrollOrientation)
    *aCurPos = y;
  else
    rv = NS_ERROR_INVALID_ARG;
  return rv;
}

NS_IMETHODIMP GetCurScrollPos(PRInt32 aScrollOrientation, PRInt32 *aX, PRInt32 *aY)
{
  NS_ENSURE_ARG_POINTER(aX);
  NS_ENSURE_ARG_POINTER(aY);

  nsIScrollableView *scrollView=nsnull;
  nsresult rv = GetRootScrollableView(&scrollView);
  if (NS_FAILED(rv)) { return rv; }
  if (!scrollView) { return NS_ERROR_NOT_INITIALIZED; }

  rv = scrollView->GetScrollPosition(*aX, *aY);
  return rv;
}

NS_IMETHODIMP SetCurScrollPos(PRInt32 aScrollOrientation, PRInt32 aCurPos)
{
  nsIScrollableView *scrollView=nsnull;
  nsresult rv = GetRootScrollableView(&scrollView);
  if (NS_FAILED(rv)) { return rv; }
  if (!scrollView) { return NS_ERROR_NOT_INITIALIZED; }

  nscoord other, x, y;
  GetCurScrollPos(aScrollOrientation, &other);
  if (ScrollOrientation_X==aScrollOrientation)
  {
    x = aCurPos;
    y = other;
  }
  else if (ScrollOrientation_Y==aScrollOrientation)
  {
    x = other;
    y = aCurPos;
  }
  else
    return NS_ERROR_INVALID_ARG;

  rv = scrollView->ScrollTo(x, y, NS_VMREFRESH_IMMEDIATE);
  if (NS_FAILED(rv)) { return rv; }

  return rv;
}

NS_IMETHODIMP SetCurScrollPos(PRInt32 aX, PRInt32 aY)
{
  nsIScrollableView *scrollView=nsnull;
  nsresult rv = GetRootScrollableView(&scrollView);
  if (NS_FAILED(rv)) { return rv; }
  if (!scrollView) { return NS_ERROR_NOT_INITIALIZED; }

  rv = scrollView->ScrollTo(aX, aY, NS_VMREFRESH_IMMEDIATE);
  return rv;
}

//XXX: this is wrong
NS_IMETHODIMP GetScrollRange(PRInt32 aScrollOrientation, PRInt32 *aMinPos, PRInt32 *aMaxPos)
{
  NS_ENSURE_ARG_POINTER(aMinPos);
  NS_ENSURE_ARG_POINTER(aMaxPos);

  nsIScrollableView *scrollView=nsnull;
  nsresult rv = GetRootScrollableView(&scrollView);
  if (NS_FAILED(rv)) { return rv; }
  if (!scrollView) { return NS_ERROR_NOT_INITIALIZED; }

  nscoord width, height;
  rv = scrollView->GetContainerSize(width, height);
  *aMinPos = 0;
  if (ScrollOrientation_X==aScrollOrientation)
    *aMaxPos = width;
  else if (ScrollOrientation_Y==aScrollOrientation)
    *aMaxPos = height;
  else
    rv = NS_ERROR_INVALID_ARG;
  return rv;
}

NS_IMETHODIMP SetScrollRange(PRInt32 aScrollOrientation, PRInt32 minPos, PRInt32 aMaxPos)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GetScrollbarPreferences(PRInt32 aScrollOrientation, PRInt32 *aScrollbarPref)
{
  NS_ENSURE_ARG_POINTER(aScrollbarPref);

  nsIScrollableView *scrollView=nsnull;
  nsresult rv = GetRootScrollableView(&scrollView);
  if (NS_FAILED(rv)) { return rv; }
  if (!scrollView) { return NS_ERROR_NOT_INITIALIZED; }

  rv = scrollView->GetScrollPreference(*aScrollbarPref);
  return rv;
}

NS_IMETHODIMP SetScrollbarPreferences(PRInt32 aScrollOrientation, PRInt32 aScrollbarPref)
{
  nsIScrollableView *scrollView=nsnull;
  nsresult rv = GetRootScrollableView(&scrollView);
  if (NS_FAILED(rv)) { return rv; }
  if (!scrollView) { return NS_ERROR_NOT_INITIALIZED; }

  rv = scrollView->SetScrollPreference(aScrollbarPref);
  return rv;
}

NS_IMETHODIMP GetScrollbarVisibility(PRBool *aVerticalVisible, PRBool *aHorizontalVisible)
{
  NS_ENSURE_ARG_POINTER(aVerticalVisible);
  NS_ENSURE_ARG_POINTER(aHorizontalVisible);

  nsIScrollableView *scrollView=nsnull;
  nsresult rv = GetRootScrollableView(&scrollView);
  if (NS_FAILED(rv)) { return rv; }
  if (!scrollView) { return NS_ERROR_NOT_INITIALIZED; }

  rv = scrollView->GetScrollbarVisibility(aVerticalVisible, aHorizontalVisible);
  return rv;
}


//*****************************************************************************
// nsHTMLDocShell::nsITextScroll Overrides
//*****************************************************************************   

NS_IMETHODIMP ScrollByLines(PRInt32 aNumLines)
{
  nsIScrollableView *scrollView=nsnull;
  nsresult rv = GetRootScrollableView(&scrollView);
  if (NS_FAILED(rv)) { return rv; }
  if (!scrollView) { return NS_ERROR_NOT_INITIALIZED; }

  rv = scrollView->ScrollByLines(aNumLines);
  return rv;
}

NS_IMETHODIMP ScrollByPages(PRInt32 aNumPages)
{
  nsIScrollableView *scrollView=nsnull;
  nsresult rv = GetRootScrollableView(&scrollView);
  if (NS_FAILED(rv)) { return rv; }
  if (!scrollView) { return NS_ERROR_NOT_INITIALIZED; }

  rv = scrollView->ScrollByPages(aNumPages);
  return rv;
}


//*****************************************************************************
// private methods
//***************************************************************************** 

nsresult
nsDocShell::GetPresShell(nsIPresShell **aOutPresShell)
{
  NS_ENSURE_ARG_POINTER(aOutPresShell);

  *aOutPresShell = nsnull;
  if (!mPresShell) { return NS_ERROR_NOT_INITIALIZED;
  
  aOutPreShell = do_QueryInterface(mPresShell);
  return NS_OK;
}

nsresult 
nsDocShell::GetChildOffset(nsIDOMNode *aChild, nsIDOMNode *aParent, PRInt32 &aOffset)
{
  NS_ASSERTION((aChild && aParent), "bad args");
  nsresult result = NS_ERROR_NULL_POINTER;
  if (aChild && aParent)
  {
    nsCOMPtr<nsIDOMNodeList> childNodes;
    result = aParent->GetChildNodes(getter_AddRefs(childNodes));
    if ((NS_SUCCEEDED(result)) && (childNodes))
    {
      PRInt32 i=0;
      for ( ; NS_SUCCEEDED(result); i++)
      {
        nsCOMPtr<nsIDOMNode> childNode;
        result = childNodes->Item(i, getter_AddRefs(childNode));
        if ((NS_SUCCEEDED(result)) && (childNode))
        {
          if (childNode.get()==aChild)
          {
            aOffset = i;
            break;
          }
        }
        else if (!childNode)
          result = NS_ERROR_NULL_POINTER;
      }
    }
    else if (!childNodes)
      result = NS_ERROR_NULL_POINTER;
  }
  return result;
}


nsresult
nsDocShell::GetRootScrollableView(nsIScrollableView **aOutScrollView)
{
  NS_ENSURE_ARG_POINTER(aOutScrollView);

  rv = vm->GetRootScrollableView(aOutScrollView);
  return rv;
}