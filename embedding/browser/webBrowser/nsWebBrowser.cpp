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

#include "nsIComponentManager.h"
#include "nsGfxCIID.h"
#include "nsWidgetsCID.h"
#include "nsIDeviceContext.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"

#include "nsWebBrowser.h"

static NS_DEFINE_IID(kChildCID,               NS_CHILD_CID);
static NS_DEFINE_IID(kDeviceContextCID,       NS_DEVICE_CONTEXT_CID);

//*****************************************************************************
//***    nsWebBrowser: Object Management
//*****************************************************************************

nsWebBrowser::nsWebBrowser() : mCreated(PR_FALSE)
{
	NS_INIT_REFCNT();
   mInitInfo = new nsWebBrowserInitInfo();
}

nsWebBrowser::~nsWebBrowser()
{
   if(mInitInfo)
      {
      delete mInitInfo;
      mInitInfo = nsnull;
      }
}

NS_IMETHODIMP nsWebBrowser::Create(nsISupports* aOuter, const nsIID& aIID, 
	void** ppv)
{
	NS_ENSURE_ARG_POINTER(ppv);
	NS_ENSURE_NO_AGGREGATION(aOuter);

	nsWebBrowser* browser = new  nsWebBrowser();
	NS_ENSURE_TRUE(browser, NS_ERROR_OUT_OF_MEMORY);

	NS_ADDREF(browser);
	nsresult rv = browser->QueryInterface(aIID, ppv);
	NS_RELEASE(browser);  
	return rv;
}

//*****************************************************************************
// nsWebBrowser::nsISupports
//*****************************************************************************   

NS_IMPL_ISUPPORTS6(nsWebBrowser, nsIWebBrowser, nsIWebBrowserNav, nsIProgress, 
   nsIGenericWindow, nsIScrollable, nsITextScroll)

//*****************************************************************************
// nsWebBrowser::nsIWebBrowser
//*****************************************************************************   

NS_IMETHODIMP nsWebBrowser::AddWebBrowserListener(nsIInterfaceRequestor* listener, 
   PRInt32* cookie)
{                   
   if(!mListenerList)
      NS_ENSURE_SUCCESS(NS_NewISupportsArray(getter_AddRefs(mListenerList)), 
         NS_ERROR_FAILURE);

   // Make sure it isn't already in the list...  This is bad!
   NS_ENSURE_TRUE(mListenerList->IndexOf(listener) == -1, NS_ERROR_INVALID_ARG);

   NS_ENSURE_SUCCESS(mListenerList->AppendElement(listener), NS_ERROR_FAILURE);

   if(cookie)
      *cookie = (PRInt32)listener;

   if(mCreated)
      UpdateListeners();
   
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::RemoveWebBrowserListener(nsIInterfaceRequestor* listener,
   PRInt32 cookie)
{
   NS_ENSURE_STATE(mListenerList);

   if(!listener)
      listener = (nsIInterfaceRequestor*)cookie;

   NS_ENSURE_TRUE(listener, NS_ERROR_INVALID_ARG);

   NS_ENSURE_TRUE(mListenerList->RemoveElement(listener), NS_ERROR_INVALID_ARG);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetDocShell(nsIDocShell** docShell)
{
   NS_ENSURE_ARG_POINTER(docShell);

   *docShell = mDocShell;
   NS_IF_ADDREF(*docShell);

   return NS_OK;
}

//*****************************************************************************
// nsWebBrowser::nsIWebBrowserNav
//*****************************************************************************

NS_IMETHODIMP nsWebBrowser::GetCanGoBack(PRBool* pCanGoBack)
{
   //XXX First Check
   /*
	Indicates if the browser if it can go back.  If true this indicates that
	there is back session history available to navigate to.
	*/

   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GetCanGoForward(PRBool* pCanGoForward)
{
   //XXX First Check
	/*
	Indicates if the browser if it can go forward.  If true this indicates that
	there is forward session history available to navigate to.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GoBack()
{
   NS_ENSURE_STATE(mDocShell);
   PRBool canGoBack;
   NS_ENSURE_SUCCESS(GetCanGoBack(&canGoBack), NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(canGoBack, NS_ERROR_UNEXPECTED);

   //XXX First Check
	/*
	Tells the browser to navigate to the next Back session history item.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GoForward()
{
   NS_ENSURE_STATE(mDocShell);
   PRBool canGoForward;
   NS_ENSURE_SUCCESS(GetCanGoForward(&canGoForward), NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(canGoForward, NS_ERROR_UNEXPECTED);

   //XXX First Check
	/*
	Tells the browser to navigate to the next Forward session history item.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::LoadURI(const PRUnichar* uri)
{
   //NS_ENSURE_ARG(uri);  // Done in LoadURIVia for us.

   return LoadURIVia(uri, 0); // Can blindly return because we know this 
}                             // method to return the same errors.

NS_IMETHODIMP nsWebBrowser::LoadURIVia(const PRUnichar* uri, 
   PRUint32 adapterBinding)
{
   //XXX First Check
	/*
	Loads a given URI through the specified adapter.  This will give priority
	to loading the requested URI in the object implementing this interface.
	If it can't be loaded here	however, the URL dispatcher will go through its
	normal process of content loading.

	@param uri - The URI to load.
	@param adapterBinding - The local IP address of the adapter to bind to.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::Reload()
{
   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::Stop()
{
   //XXX First Check
	/*
	Stops a load of a URI.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::SetDocument(nsIDOMDocument* aDocument, 
   const PRUnichar* aContentType)
{
   NS_ENSURE_ARG(aDocument);

   nsAutoString contentType(aContentType);
   if(contentType.IsEmpty())
      {
      nsCOMPtr<nsIDocument> doc(do_QueryInterface(aDocument));
      if(doc)
         doc->GetContentType(contentType);
      }
   if(contentType.IsEmpty())
      contentType.Assign("HTML");
      
   NS_ENSURE_SUCCESS(CreateDocShell(contentType.GetUnicode()), 
      NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(mDocShell->SetDocument(aDocument, nsnull), 
      NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetDocument(nsIDOMDocument** document)
{
   NS_ENSURE_ARG_POINTER(document);
   NS_ENSURE_STATE(mDocShell);

   NS_ENSURE_SUCCESS(mDocShell->GetDocument(document), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetCurrentURI(PRUnichar** aCurrentURI)
{
   NS_ENSURE_ARG_POINTER(aCurrentURI);
   NS_ENSURE_STATE(mDocShell);

   nsCOMPtr<nsIURI> uri;
   NS_ENSURE_SUCCESS(mDocShell->GetCurrentURI(getter_AddRefs(uri)), 
      NS_ERROR_FAILURE);

   char* spec;
   NS_ENSURE_SUCCESS(uri->GetSpec(&spec), NS_ERROR_FAILURE);

   *aCurrentURI = nsAutoString(spec).ToNewUnicode();

   return NS_OK;
}

//*****************************************************************************
// nsWebBrowser::nsIProgress
//*****************************************************************************

NS_IMETHODIMP nsWebBrowser::AddProgressListener(nsIProgressListener* listener, 
   PRInt32* cookie)
{
   //XXX First Check
	/*
	Registers a listener to be notified of Progress Events

	@param listener - The listener interface to be called when a progress event
			occurs.

	@param cookie - This is an optional parameter to receieve a cookie to use
			to unregister rather than the original interface pointer.  This may
			be nsnull.

	@return	NS_OK - Listener was registered successfully.
				NS_INVALID_ARG - The listener passed in was either nsnull, 
						or was already registered with this progress interface.
	 */
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::RemoveProgressListener(nsIProgressListener* listener, 
   PRInt32 cookie)
{
   //XXX First Check
	/* 
	Removes a previously registered listener of Progress Events
		
	@param listener - The listener interface previously registered with 
			AddListener() this may be nsnull if a valid cookie is provided.

	@param cookie - A cookie that was returned from a previously called
		AddListener() call.  This may be nsnull if a valid listener interface
		is passed in.

	@return	NS_OK - Listener was successfully unregistered.
				NS_ERROR_INVALID_ARG - Neither the cookie nor the listener point
					to a previously registered listener.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GetConnectionStatus(PRInt32* connectionStatus)
{
   //XXX First Check
	/*
	Current connection Status of the browser.  This will be one of the enumerated
	connection progress steps.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GetActive(PRBool* active)
{
   //XXX First Check
	/*
	Simple boolean to know if the browser is active or not.  This provides the 
	same information that the connectionStatus attribute does.  This however
	allows you to avoid having to check the various connection steps.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GetCurSelfProgress(PRInt32* curSelfProgress)
{
   //XXX First Check
	/*
	The current position of progress.  This is between 0 and maxSelfProgress.
	This is the position of only this progress object.  It doesn not include
	the progress of all children.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GetMaxSelfProgress(PRInt32* maxSelfProgress)
{
   //XXX First Check
	/*
	The maximum position that progress will go to.  This sets a relative
	position point for the current progress to relate to.  This is the max
	position of only this progress object.  It does not include the progress of
	all the children.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GetCurTotalProgress(PRInt32* curTotalProgress)
{
   //XXX First Check
	/*
	The current position of progress for this object and all children added
	together.  This is between 0 and maxTotalProgress.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GetMaxTotalProgress(PRInt32* maxTotalProgress)
{
   //XXX First Check
	/*
	The maximum position that progress will go to for the max of this progress
	object and all children.  This sets the relative position point for the
	current progress to relate to.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GetChildPart(PRInt32 childPart, 
   nsIProgress** childProgress)
{
   //XXX First Check
	/*
	Retrieves the progress object for a particular child part.

	@param childPart - The number of the child part you wish to retrieve.
	@param childProgress - The returned progress interface for the requested
		child.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GetNumChildParts(PRInt32* numChildParts)
{
   NS_ENSURE_ARG_POINTER(numChildParts);
   //XXX First Check
	/*
	Number of Child progress parts.
	*/
   return NS_ERROR_FAILURE;
}
   
//*****************************************************************************
// nsWebBrowser::nsIGenericWindow
//*****************************************************************************

NS_IMETHODIMP nsWebBrowser::InitWindow(nativeWindow parentNativeWindow,
   nsIWidget* parentWidget, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)   
{
   NS_ENSURE_ARG(parentNativeWindow || parentWidget);
   NS_ENSURE_STATE(!mCreated || mInitInfo);

   mParentNativeWindow = parentNativeWindow;
   mParentWidget = parentWidget;
   mInitInfo->x = x;
   mInitInfo->y = y;
   mInitInfo->cx = cx;
   mInitInfo->cy = cy;

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::Create()
{
   NS_ENSURE_STATE(!mCreated && (mParentNativeWindow || mParentWidget));

   if(mParentWidget)
      {
      mParentNativeWindow = mParentWidget->GetNativeData(NS_NATIVE_WIDGET);
      }
   else // We need to create a widget
      {
      // XXX Review this code, carried over from old webShell
      // Create a device context
      nsCOMPtr<nsIDeviceContext> deviceContext = 
                                          do_CreateInstance(kDeviceContextCID);
      NS_ENSURE_TRUE(deviceContext, NS_ERROR_FAILURE);

      deviceContext->Init(mParentNativeWindow);
      float dev2twip;
      deviceContext->GetDevUnitsToTwips(dev2twip);
      deviceContext->SetDevUnitsToAppUnits(dev2twip);
      float twip2dev;
      deviceContext->GetTwipsToDevUnits(twip2dev);
      deviceContext->SetAppUnitsToDevUnits(twip2dev);
      deviceContext->SetGamma(1.0f);

      // Create the widget
      NS_ENSURE_TRUE(mInternalWidget = do_CreateInstance(kChildCID), NS_ERROR_FAILURE);

      nsWidgetInitData  widgetInit;

      widgetInit.clipChildren = PR_FALSE;
      widgetInit.mWindowType = eWindowType_child;
      nsRect bounds(mInitInfo->x, mInitInfo->y, mInitInfo->cx, mInitInfo->cy);
      
      mInternalWidget->Create(mParentNativeWindow, bounds, nsnull /* was nsWebShell::HandleEvent*/,
         deviceContext, nsnull, nsnull, &widgetInit);  
      }
   //XXX First Check
	/*
	Tells the window that intialization and setup is complete.  When this is
	called the window can actually create itself based on the setup
	information handed to it.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::Destroy()
{
   /* For now we don't support dynamic tear down and rebuild.  In the future we
    may */
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsWebBrowser::SetPosition(PRInt32 x, PRInt32 y)
{
   if(!mCreated)
      {
      mInitInfo->x = x;
      mInitInfo->y = y;
      }
   else
      {
      // If there is an internal widget you just want to move it as the 
      // DocShell View will implicitly be moved as the widget moves
      if(mInternalWidget)
         NS_ENSURE_SUCCESS(mInternalWidget->Move(x, y), NS_ERROR_FAILURE);
      else // Else rely on the docShell to set it
         {
         nsCOMPtr<nsIGenericWindow> docShellWin(do_QueryInterface(mDocShell));
         NS_ENSURE_SUCCESS(docShellWin->SetPosition(x, y), NS_ERROR_FAILURE);
         }
      }
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetPosition(PRInt32* x, PRInt32* y)
{
   NS_ENSURE_ARG_POINTER(x && y);

   if(!mCreated)
      {
      *x = mInitInfo->x;
      *y = mInitInfo->y;
      }
   else
      {
      //If there is an internal widget you just want to get the position of the 
      //widget as that is the implied position of the docshell as well.
      if(mInternalWidget)
         {
         nsRect bounds;
         mInternalWidget->GetBounds(bounds);

         *x = bounds.x;
         *y = bounds.y;
         }
      else //Else Rely on the docShell to tell us
         {
         nsCOMPtr<nsIGenericWindow> docShellWin(do_QueryInterface(mDocShell));
         NS_ENSURE_SUCCESS(docShellWin->GetPosition(x, y), NS_ERROR_FAILURE);
         }
      }

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
   if(!mCreated)
      {
      mInitInfo->cx = cx;
      mInitInfo->cy = cy;
      }
   else
      {
      // If there is an internal widget set the size of it as well
      if(mInternalWidget)
         mInternalWidget->Resize(cx, cy, fRepaint);

      // Now size the docShell as well
      nsCOMPtr<nsIGenericWindow> docShellWindow(do_QueryInterface(mDocShell));

      NS_ENSURE_SUCCESS(docShellWindow->SetSize(cx, cy, fRepaint), NS_ERROR_FAILURE);
      }

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetSize(PRInt32* cx, PRInt32* cy)
{
   NS_ENSURE_ARG_POINTER(cx && cy);

   if(!mCreated)
      {
      *cx = mInitInfo->cx;
      *cy = mInitInfo->cy;
      }
   else
      {
      // We can ignore the internal widget and just rely on the docShell for 
      // this question.
      nsCOMPtr<nsIGenericWindow> docShellWindow(do_QueryInterface(mDocShell));

      NS_ENSURE_SUCCESS(docShellWindow->GetSize(cx, cy), NS_ERROR_FAILURE);
      }

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx,
   PRInt32 cy, PRBool fRepaint)
{
   if(!mCreated)
      {
      mInitInfo->x = x;
      mInitInfo->y = y;
      mInitInfo->cx = cx;
      mInitInfo->cy = cy;
      }
   else
      {
      PRInt32 doc_x = x;
      PRInt32 doc_y = y;

      // If there is an internal widget we need to make the docShell coordinates
      // relative to the internal widget rather than the calling app's parent.
      // We also need to resize our widget then.
      if(mInternalWidget)
         {
         doc_x = doc_y = 0;
         NS_ENSURE_SUCCESS(mInternalWidget->Resize(x, y, cx, cy, fRepaint),
            NS_ERROR_FAILURE);
         }

      nsCOMPtr<nsIGenericWindow> docShellWindow(do_QueryInterface(mDocShell));

      // Now reposition/ resize the doc
      NS_ENSURE_SUCCESS(docShellWindow->SetPositionAndSize(doc_x, doc_y, cx, cy, 
         fRepaint), NS_ERROR_FAILURE);
      }

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::Repaint(PRBool fForce)
{
   NS_ENSURE_STATE(mDocShell);
   nsCOMPtr<nsIGenericWindow> docWnd(do_QueryInterface(mDocShell));
   NS_ENSURE_TRUE(docWnd, NS_ERROR_FAILURE);
   return docWnd->Repaint(fForce); // Can directly return this as it is the
}                                     // same interface, thus same returns.

NS_IMETHODIMP nsWebBrowser::GetParentWidget(nsIWidget** parentWidget)
{
   NS_ENSURE_ARG_POINTER(parentWidget);

   *parentWidget = mParentWidget;

   NS_IF_ADDREF(*parentWidget);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetParentWidget(nsIWidget* parentWidget)
{
   NS_ENSURE_STATE(!mCreated);

   mParentWidget = parentWidget;

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetParentNativeWindow(nativeWindow* parentNativeWindow)
{
   NS_ENSURE_ARG_POINTER(parentNativeWindow);
   
   if(mParentNativeWindow)
      *parentNativeWindow = mParentNativeWindow;
   else if(mParentWidget)
      *parentNativeWindow = mParentWidget->GetNativeData(NS_NATIVE_WIDGET);
   else
      *parentNativeWindow = nsnull;

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetParentNativeWindow(nativeWindow parentNativeWindow)
{
   NS_ENSURE_STATE(!mCreated);

   mParentNativeWindow = parentNativeWindow;

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetVisibility(PRBool* visibility)
{
   NS_ENSURE_ARG_POINTER(visibility);

   if(!mCreated)
      *visibility = mInitInfo->visible;
   else
      {
      nsCOMPtr<nsIGenericWindow> docShellWindow(do_QueryInterface(mDocShell));

      NS_ENSURE_SUCCESS(docShellWindow->GetVisibility(visibility), 
         NS_ERROR_FAILURE);
      }

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetVisibility(PRBool visibility)
{
   if(!mCreated)
      mInitInfo->visible = visibility;
   else
      {
      nsCOMPtr<nsIGenericWindow> docShellWindow(do_QueryInterface(mDocShell));

      NS_ENSURE_SUCCESS(docShellWindow->SetVisibility(visibility), 
         NS_ERROR_FAILURE);
      }

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetMainWidget(nsIWidget** mainWidget)
{
   NS_ENSURE_ARG_POINTER(mainWidget);

   if(mInternalWidget)
      *mainWidget = mInternalWidget;
   else
      *mainWidget = mParentWidget;

   NS_IF_ADDREF(*mainWidget);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetFocus()
{
   NS_ENSURE_STATE(mDocShell);

   nsCOMPtr<nsIGenericWindow> docShellWindow(do_QueryInterface(mDocShell));
   
   NS_ENSURE_SUCCESS(docShellWindow->SetFocus(), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetTitle(PRUnichar** aTitle)
{
   NS_ENSURE_ARG_POINTER(aTitle);
   NS_ENSURE_STATE(mDocShell);

   nsCOMPtr<nsIGenericWindow> docShellWindow(do_QueryInterface(mDocShell));

   NS_ENSURE_SUCCESS(docShellWindow->GetTitle(aTitle), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetTitle(const PRUnichar* aTitle)
{
   NS_ENSURE_STATE(mDocShell);

   nsCOMPtr<nsIGenericWindow> docShellWindow(do_QueryInterface(mDocShell));

   NS_ENSURE_SUCCESS(docShellWindow->SetTitle(aTitle), NS_ERROR_FAILURE);

   return NS_OK;
}

//*****************************************************************************
// nsWebBrowser::nsIScrollable
//*****************************************************************************

NS_IMETHODIMP nsWebBrowser::GetCurScrollPos(PRInt32 scrollOrientation, 
   PRInt32* curPos)
{
   NS_ENSURE_ARG_POINTER(curPos);
   NS_ENSURE_STATE(mDocShell);

   nsCOMPtr<nsIScrollable> scroll(do_QueryInterface(mDocShell));

   NS_ENSURE_TRUE(scroll, NS_ERROR_FAILURE);

   return scroll->GetCurScrollPos(scrollOrientation, curPos);
}

NS_IMETHODIMP nsWebBrowser::SetCurScrollPos(PRInt32 scrollOrientation, 
   PRInt32 curPos)
{
   NS_ENSURE_STATE(mDocShell);

   nsCOMPtr<nsIScrollable> scroll(do_QueryInterface(mDocShell));

   NS_ENSURE_TRUE(scroll, NS_ERROR_FAILURE);

   return scroll->SetCurScrollPos(scrollOrientation, curPos);
}

NS_IMETHODIMP nsWebBrowser::SetCurScrollPosEx(PRInt32 curHorizontalPos, 
   PRInt32 curVerticalPos)
{
   NS_ENSURE_STATE(mDocShell);

   nsCOMPtr<nsIScrollable> scroll(do_QueryInterface(mDocShell));

   NS_ENSURE_TRUE(scroll, NS_ERROR_FAILURE);

   return scroll->SetCurScrollPosEx(curHorizontalPos, curVerticalPos);
}

NS_IMETHODIMP nsWebBrowser::GetScrollRange(PRInt32 scrollOrientation,
   PRInt32* minPos, PRInt32* maxPos)
{
   NS_ENSURE_ARG_POINTER(minPos && maxPos);
   NS_ENSURE_STATE(mDocShell);

   nsCOMPtr<nsIScrollable> scroll(do_QueryInterface(mDocShell));

   NS_ENSURE_TRUE(scroll, NS_ERROR_FAILURE);

   return scroll->GetScrollRange(scrollOrientation, minPos, maxPos);
}

NS_IMETHODIMP nsWebBrowser::SetScrollRange(PRInt32 scrollOrientation,
   PRInt32 minPos, PRInt32 maxPos)
{
   NS_ENSURE_STATE(mDocShell);

   nsCOMPtr<nsIScrollable> scroll(do_QueryInterface(mDocShell));

   NS_ENSURE_TRUE(scroll, NS_ERROR_FAILURE);

   return scroll->SetScrollRange(scrollOrientation, minPos, maxPos);
}

NS_IMETHODIMP nsWebBrowser::SetScrollRangeEx(PRInt32 minHorizontalPos,
   PRInt32 maxHorizontalPos, PRInt32 minVerticalPos, PRInt32 maxVerticalPos)
{
   NS_ENSURE_STATE(mDocShell);

   nsCOMPtr<nsIScrollable> scroll(do_QueryInterface(mDocShell));

   NS_ENSURE_TRUE(scroll, NS_ERROR_FAILURE);

   return scroll->SetScrollRangeEx(minHorizontalPos, maxHorizontalPos, 
      minVerticalPos, maxVerticalPos);
}

NS_IMETHODIMP nsWebBrowser::GetScrollbarPreferences(PRInt32 scrollOrientation,
   PRInt32* scrollbarPref)
{
   NS_ENSURE_ARG_POINTER(scrollbarPref);
   NS_ENSURE_STATE(mDocShell);

   nsCOMPtr<nsIScrollable> scroll(do_QueryInterface(mDocShell));

   NS_ENSURE_TRUE(scroll, NS_ERROR_FAILURE);

   return scroll->GetScrollbarPreferences(scrollOrientation, scrollbarPref);
}

NS_IMETHODIMP nsWebBrowser::SetScrollbarPreferences(PRInt32 scrollOrientation,
   PRInt32 scrollbarPref)
{
   NS_ENSURE_STATE(mDocShell);

   nsCOMPtr<nsIScrollable> scroll(do_QueryInterface(mDocShell));

   NS_ENSURE_TRUE(scroll, NS_ERROR_FAILURE);

   return scroll->SetScrollbarPreferences(scrollOrientation, scrollbarPref);
}

NS_IMETHODIMP nsWebBrowser::GetScrollbarVisibility(PRBool* verticalVisible,
   PRBool* horizontalVisible)
{
   NS_ENSURE_STATE(mDocShell);

   nsCOMPtr<nsIScrollable> scroll(do_QueryInterface(mDocShell));

   NS_ENSURE_TRUE(scroll, NS_ERROR_FAILURE);

   return scroll->GetScrollbarVisibility(verticalVisible, horizontalVisible);
}

//*****************************************************************************
// nsWebBrowser::nsITextScroll
//*****************************************************************************   

NS_IMETHODIMP nsWebBrowser::ScrollByLines(PRInt32 numLines)
{
   NS_ENSURE_STATE(mDocShell);

   nsCOMPtr<nsITextScroll> scroll(do_QueryInterface(mDocShell));

   NS_ENSURE_TRUE(scroll, NS_ERROR_FAILURE);

   return scroll->ScrollByLines(numLines);
}

NS_IMETHODIMP nsWebBrowser::ScrollByPages(PRInt32 numPages)
{
   NS_ENSURE_STATE(mDocShell);

   nsCOMPtr<nsITextScroll> scroll(do_QueryInterface(mDocShell));

   NS_ENSURE_TRUE(scroll, NS_ERROR_FAILURE);

   return scroll->ScrollByPages(numPages);
}


//*****************************************************************************
// nsWebBrowser: Listener Helpers
//*****************************************************************************   

void nsWebBrowser::UpdateListeners()
{
   // XXX
   // Should walk the mListenerList and call each asking for our needed
   // interfaces.
}

nsresult nsWebBrowser::CreateDocShell(const PRUnichar* contentType)
{
   if(mDocShell)
      return NS_OK;

   // XXX
   // Should walk category list looking for a component that can 
   // handle the specific contentType being offered to us.
   // For now......  We will simply instantiate an nsHTMLDocShell.

   return NS_ERROR_FAILURE;
} 



