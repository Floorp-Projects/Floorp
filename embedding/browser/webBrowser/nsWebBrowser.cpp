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

//#include "nsIComponentManager.h"

#include "nsWebBrowser.h"

//*****************************************************************************
//***    nsWebBrowser: Object Management
//*****************************************************************************

nsWebBrowser::nsWebBrowser() : m_Created(PR_FALSE)
{
	NS_INIT_REFCNT();
}

nsWebBrowser::~nsWebBrowser()
{
}

NS_IMETHODIMP nsWebBrowser::Create(nsISupports* aOuter, const nsIID& aIID, 
	void** ppv)
{
	NS_ENSURE_ARG_POINTER(ppv);
	NS_ENSURE_NO_AGGREGATION(aOuter);

	nsWebBrowser* browser = new  nsWebBrowser();
	NS_ENSURE(browser, NS_ERROR_OUT_OF_MEMORY);

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
   if(!m_ListenerList)
      NS_ENSURE_SUCCESS(NS_NewISupportsArray(getter_AddRefs(m_ListenerList)), 
         NS_ERROR_FAILURE);

   // Make sure it isn't already in the list...  This is bad!
   NS_ENSURE(m_ListenerList->IndexOf(listener) == -1, NS_ERROR_INVALID_ARG);

   NS_ENSURE_SUCCESS(m_ListenerList->AppendElement(listener), NS_ERROR_FAILURE);

   if(cookie)
      *cookie = (PRInt32)listener;

   if(m_Created)
      UpdateListeners();
   
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::RemoveWebBrowserListener(nsIInterfaceRequestor* listener,
   PRInt32 cookie)
{
   NS_ENSURE_STATE(m_ListenerList);

   if(!listener)
      listener = (nsIInterfaceRequestor*)cookie;

   NS_ENSURE(listener, NS_ERROR_INVALID_ARG);

   NS_ENSURE(m_ListenerList->RemoveElement(listener), NS_ERROR_INVALID_ARG);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetDocShell(nsIDocShell** docShell)
{
   NS_ENSURE_ARG_POINTER(docShell);

   *docShell = m_DocShell;
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
   //XXX First Check
	/*
	Tells the browser to navigate to the next Back session history item.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GoForward()
{
   //XXX First Check
	/*
	Tells the browser to navigate to the next Forward session history item.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::LoadURI(const PRUnichar* uri)
{
   //XXX First Check
	/*
	Loads a given URI.  This will give priority to loading the requested URI
	in the object implementing	this interface.  If it can't be loaded here
	however, the URL dispatcher will go through its normal process of content
	loading.

	@param uri - The URI to load.
	*/
   return NS_ERROR_FAILURE;
}

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

NS_IMETHODIMP nsWebBrowser::SetDocument(nsIDOMDocument* document)
{
   //XXX First Check
	/*
	Retrieves or sets the current Document for the WebBrowser.  When setting
	this will simulate the normal load process.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GetDocument(nsIDOMDocument** document)
{
   NS_ENSURE_ARG_POINTER(document);
   NS_ENSURE_STATE(m_DocShell);

   NS_ENSURE_SUCCESS(m_DocShell->GetDocument(document), NS_ERROR_FAILURE);

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
   //XXX First Check
	/*
	Allows a client to initialize an object implementing this interface with
	the usually required window setup information.

	@param parentNativeWindow - This allows a system to pass in the parenting
		window as a native reference rather than relying on the calling
		application to have created the parent window as an nsIWidget.  This 
		value will be ignored (should be nsnull) if an nsIWidget is passed in to
		the parentWidget parameter.  One of the two parameters however must be
		passed.

	@param parentWidget - This allows a system to pass in the parenting widget.
		This allows some objects to optimize themselves and rely on the view
		system for event flow rather than creating numerous native windows.  If
		one of these is not available, nsnull should be passed and a 
		valid native window should be passed to the parentNativeWindow parameter.

	@param x - This is the x co-ordinate relative to the parent to place the
		window.

	@param y - This is the y co-ordinate relative to the parent to place the 
		window.

	@param cx - This is the width	for the window to be.

	@param cy - This is the height for the window to be.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::Create()
{
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
   //XXX First Check
	/*
	Tell the window that it can destroy itself.  This allows re-using the same
	object without re-doing a lot of setup.  This is not a required call 
	before a release.

	@return	NS_OK - Everything destroyed properly.
				NS_ERROR_NOT_IMPLEMENTED - State preservation is not supported.
					Release the interface and create a new object.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::SetPosition(PRInt32 x, PRInt32 y)
{
   //XXX First Check
	/*
	Sets the current x and y coordinates of the control.  This is relative to
	the parent window.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GetPosition(PRInt32* x, PRInt32* y)
{
   NS_ENSURE_ARG_POINTER(x && y);
   //XXX First Check
	/*
	Gets the current x and y coordinates of the control.  This is relatie to the
	parent window.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
   //XXX First Check
	/*
	Sets the width and height of the control.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GetSize(PRInt32* cx, PRInt32* cy)
{
   NS_ENSURE_ARG_POINTER(cx && cy);

   //XXX First Check
	/*
	Gets the width and height of the control.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx,
   PRInt32 cy, PRBool fRepaint)
{
   //XXX First Check
	/*
	Convenience function combining the SetPosition and SetSize into one call.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::SizeToContent()
{
   //XXX First Check
	/**
	* Tell the window to shrink-to-fit its contents
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::Repaint(PRBool fForce)
{
   //XXX First Check
	/** 
	 * Tell the window to repaint itself
	 * @param aForce - if true, repaint immediately
	 *                 if false, the window may defer repainting as it sees fit.
	 */
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GetParentWidget(nsIWidget** parentWidget)
{
   NS_ENSURE_ARG_POINTER(parentWidget);

   //XXX First Check
	/*			  
	This is the parenting widget for the control.  This may be null if only the
	native window was handed in for the parent during initialization.  If this
	is returned, it should refer to the same object as parentNativeWindow.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::SetParentWidget(nsIWidget* parentWidget)
{
   //XXX First Check
	/*			  
	This is the parenting widget for the control.  This may be null if only the
	native window was handed in for the parent during initialization.  If this
	is returned, it should refer to the same object as parentNativeWindow.
	*/
   return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsWebBrowser::GetParentNativeWindow(nativeWindow* parentNativeWindow)
{
   NS_ENSURE_ARG_POINTER(parentNativeWindow);
   //XXX First Check
	/*
	This is the native window parent of the control.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::SetParentNativeWindow(nativeWindow parentNativeWindow)
{
   //XXX First Check
	/*
	This is the native window parent of the control.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GetVisibility(PRBool* visibility)
{
   NS_ENSURE_ARG_POINTER(visibility);

   //XXX First Check
	/*
	Attribute controls the visibility of the object behind this interface.
	Setting this attribute to false will hide the control.  Setting it to 
	true will show it.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::SetVisibility(PRBool visibility)
{
   //XXX First Check
	/*
	Attribute controls the visibility of the object behind this interface.
	Setting this attribute to false will hide the control.  Setting it to 
	true will show it.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GetMainWidget(nsIWidget** mainWidget)
{
   NS_ENSURE_ARG_POINTER(mainWidget);
   //XXX First Check
	/*
	Allows you to find out what the widget is of a given object.  Depending
	on the object, this may return the parent widget in which this object
	lives if it has not had to create it's own widget.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::SetFocus()
{
   //XXX First Check
	/**
	* Give the window focus.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::RemoveFocus()
{
   //XXX First Check
	/**
	* Remove focus from the window
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GetTitle(PRUnichar** title)
{
   NS_ENSURE_ARG_POINTER(title);

   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::SetTitle(const PRUnichar* title)
{
   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GetZoom(float* zoom)
{
   NS_ENSURE_ARG_POINTER(zoom);

   //XXX First Check
	/**
	* Set/Get the document scale factor
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::SetZoom(float zoom)
{
   //XXX First Check
	/**
	* Set/Get the document scale factor
	*/
   return NS_ERROR_FAILURE;
}

//*****************************************************************************
// nsWebBrowser::nsIScrollable
//*****************************************************************************

NS_IMETHODIMP nsWebBrowser::GetCurScrollPos(PRInt32 scrollOrientation, 
   PRInt32* curPos)
{
   NS_ENSURE_ARG_POINTER(curPos);

   //XXX First Check
	/*
	Retrieves or Sets the current thumb position to the curPos passed in for the
	scrolling orientation passed in.  curPos should be between minPos and maxPos.

	@return	NS_OK - Setting or Getting completed successfully.
				NS_ERROR_INVALID_ARG - returned when curPos is not within the
					minPos and maxPos.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::SetCurScrollPos(PRInt32 scrollOrientation, 
   PRInt32 curPos)
{
   //XXX First Check
	/*
	Retrieves or Sets the current thumb position to the curPos passed in for the
	scrolling orientation passed in.  curPos should be between minPos and maxPos.

	@return	NS_OK - Setting or Getting completed successfully.
				NS_ERROR_INVALID_ARG - returned when curPos is not within the
					minPos and maxPos.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GetScrollRange(PRInt32 scrollOrientation,
   PRInt32* minPos, PRInt32* maxPos)
{
   NS_ENSURE_ARG_POINTER(minPos && maxPos);
   //XXX First Check
	/*
	Retrieves or Sets the valid ranges for the thumb.  When maxPos is set to 
	something less than the current thumb position, curPos is set = to maxPos.

	@return	NS_OK - Setting or Getting completed successfully.
				NS_ERROR_INVALID_ARG - returned when curPos is not within the
					minPos and maxPos.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::SetScrollRange(PRInt32 scrollOrientation,
   PRInt32 minPos, PRInt32 maxPos)
{
   //XXX First Check
	/*
	Retrieves or Sets the valid ranges for the thumb.  When maxPos is set to 
	something less than the current thumb position, curPos is set = to maxPos.

	@return	NS_OK - Setting or Getting completed successfully.
				NS_ERROR_INVALID_ARG - returned when curPos is not within the
					minPos and maxPos.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GetScrollbarPreferences(PRInt32 scrollOrientation,
   PRInt32* scrollbarPref)
{
   NS_ENSURE_ARG_POINTER(scrollbarPref);
   //XXX First Check
	/*
	Retrieves of Set the preferences for the scroll bar.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::SetScrollbarPreferences(PRInt32 scrollOrientation,
   PRInt32 scrollbarPref)
{
   //XXX First Check
	/*
	Retrieves of Set the preferences for the scroll bar.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GetScrollbarVisibility(PRBool* verticalVisible,
   PRBool* horizontalVisible)
{
   //XXX First Check
	/*
	Get information about whether the vertical and horizontal scrollbars are
	currently visible.
	*/
   return NS_ERROR_FAILURE;
}

//*****************************************************************************
// nsWebBrowser::nsITextScroll
//*****************************************************************************   

NS_IMETHODIMP nsWebBrowser::ScrollByLines(PRInt32 numLines)
{
   //XXX First Check
  /**
   * Scroll the view up or down by aNumLines lines. positive
   * values move down in the view. Prevents scrolling off the
   * end of the view.
   * @param numLines number of lines to scroll the view by
   */
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::ScrollByPages(PRInt32 numPages)
{
   //XXX First Check
	/**
   * Scroll the view up or down by numPages pages. a page
   * is considered to be the amount displayed by the clip view.
   * positive values move down in the view. Prevents scrolling
   * off the end of the view.
   * @param numPages number of pages to scroll the view by
   */
   return NS_ERROR_FAILURE;
}


//*****************************************************************************
// nsWebBrowser: Listener Helpers
//*****************************************************************************   

void nsWebBrowser::UpdateListeners()
{
}


