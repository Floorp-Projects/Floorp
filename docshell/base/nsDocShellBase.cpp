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


#include "nsDocShellBase.h"

//*****************************************************************************
//***    nsDocShellBase: Object Management
//*****************************************************************************

nsDocShellBase::nsDocShellBase()
{
	NS_INIT_REFCNT();
}

nsDocShellBase::~nsDocShellBase()
{
}

//*****************************************************************************
// nsDocShellBase::nsISupports
//*****************************************************************************   

NS_IMPL_ISUPPORTS6(nsDocShellBase, nsIDocShell, nsIDocShellEdit, 
   nsIDocShellFile, nsIGenericWindow, nsIScrollable, nsITextScroll)

//*****************************************************************************
// nsDocShellBase::nsIDocShell
//*****************************************************************************   

NS_IMETHODIMP nsDocShellBase::LoadURI(const PRUnichar* uri)
{
   NS_ENSURE_ARG(uri);
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

NS_IMETHODIMP nsDocShellBase::LoadURIVia(const PRUnichar* uri, 
   PRUint32 adapterBinding)
{
   NS_ENSURE_ARG(uri);
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

NS_IMETHODIMP nsDocShellBase::GetDocument(nsIDOMDocument** document)
{
   NS_ENSURE_ARG_POINTER(document);
   //XXX First Check
	/*
	The current document that is loaded in the DocShell.  When setting this it
	will will simulate the normal load process.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::SetDocument(nsIDOMDocument* document)
{
   //XXX First Check
	/*
	The current document that is loaded in the DocShell.  When setting this it
	will will simulate the normal load process.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::GetName(PRUnichar** name)
{
   NS_ENSURE_ARG_POINTER(name);
   //XXX First Check
	/*
	name of the DocShell
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::SetName(const PRUnichar* name)
{
   //XXX First Check
	/*
	name of the DocShell
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::GetPresContext(nsIPresContext** presContext)
{
   NS_ENSURE_ARG_POINTER(presContext);
   //XXX First Check
	/*
	Presentation context
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::SetPresContext(nsIPresContext* presContext)
{
   //XXX First Check
   /*
   Presentation context
   */
   return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsDocShellBase::GetParent(nsIDocShell** parent)
{
   NS_ENSURE_ARG_POINTER(parent);
   //XXX First Check
	/*
	Parent DocShell
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::SetParent(nsIDocShell* parent)
{
   //XXX First Check
	/*
	Parent DocShell
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::CanHandleContentType(const PRUnichar* contentType, 
   PRBool* canHandle)
{
   NS_ENSURE_ARG_POINTER(canHandle);
   
   NS_WARN_IF_FALSE(PR_FALSE, "Subclasses should override this method!!!!");

   *canHandle = PR_FALSE;
   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::GetPrefs(nsIPref** prefs)
{
   NS_ENSURE_ARG_POINTER(prefs);
   //XXX First Check
	/*
	Prefs to use for the DocShell.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::SetPrefs(nsIPref* prefs)
{
   //XXX First Check
	/*
	Prefs to use for the DocShell.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::GetRootDocShell(nsIDocShell** rootDocShell)
{
   NS_ENSURE_ARG_POINTER(rootDocShell);
   //XXX First Check
	/*
	Returns the root DocShell instance.  Since DocShells can be nested
	(when frames are present for example) this instance represents the 
	outermost DocShell.
	*/
   return NS_ERROR_FAILURE;
}

//*****************************************************************************
// nsDocShellBase::nsIDocShellEdit
//*****************************************************************************   

NS_IMETHODIMP nsDocShellBase::Search()
{
   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::GetSearchable(PRBool* searchable)
{
   NS_ENSURE_ARG_POINTER(searchable);
   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::ClearSelection()
{
   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::SelectAll()
{
   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::CopySelection()
{
   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::GetCopyable(PRBool* copyable)
{
   NS_ENSURE_ARG_POINTER(copyable);
   
   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::CutSelection()
{
   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::GetCutable(PRBool* cutable)
{
   NS_ENSURE_ARG_POINTER(cutable);
   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::Paste()
{
   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::GetPasteable(PRBool* pasteable)
{
   NS_ENSURE_ARG_POINTER(pasteable);
   //XXX First Check
   return NS_ERROR_FAILURE;
}

//*****************************************************************************
// nsDocShellBase::nsIDocShellFile
//*****************************************************************************   

NS_IMETHODIMP nsDocShellBase::Save()
{
   //XXX First Check
   return NS_ERROR_FAILURE;
} 

NS_IMETHODIMP nsDocShellBase::GetSaveable(PRBool* saveable)
{
   NS_ENSURE_ARG_POINTER(saveable);
   //XXX First Check
   return NS_ERROR_FAILURE;
} 

NS_IMETHODIMP nsDocShellBase::Print()
{
   //XXX First Check
   return NS_ERROR_FAILURE;
} 

NS_IMETHODIMP nsDocShellBase::GetPrintable(PRBool* printable)
{
   NS_ENSURE_ARG_POINTER(printable);
   //XXX First Check
   return NS_ERROR_FAILURE;
} 

//*****************************************************************************
// nsDocShellBase::nsIGenericWindow
//*****************************************************************************   

NS_IMETHODIMP nsDocShellBase::InitWindow(nativeWindow parentNativeWindow,
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

NS_IMETHODIMP nsDocShellBase::Create()
{
   //XXX First Check
	/*
	Tells the window that intialization and setup is complete.  When this is
	called the window can actually create itself based on the setup
	information handed to it.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::Destroy()
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

NS_IMETHODIMP nsDocShellBase::SetPosition(PRInt32 x, PRInt32 y)
{
   //XXX First Check
	/*
	Sets the current x and y coordinates of the control.  This is relative to
	the parent window.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::GetPosition(PRInt32* x, PRInt32* y)
{
   NS_ENSURE_ARG_POINTER(x && y);
   //XXX First Check
	/*
	Gets the current x and y coordinates of the control.  This is relatie to the
	parent window.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
   //XXX First Check
	/*
	Sets the width and height of the control.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::GetSize(PRInt32* cx, PRInt32* cy)
{
   NS_ENSURE_ARG_POINTER(cx && cy);

   //XXX First Check
	/*
	Gets the width and height of the control.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx,
   PRInt32 cy, PRBool fRepaint)
{
   //XXX First Check
	/*
	Convenience function combining the SetPosition and SetSize into one call.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::SizeToContent()
{
   //XXX First Check
	/**
	* Tell the window to shrink-to-fit its contents
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::Repaint(PRBool fForce)
{
   //XXX First Check
	/** 
	 * Tell the window to repaint itself
	 * @param aForce - if true, repaint immediately
	 *                 if false, the window may defer repainting as it sees fit.
	 */
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::GetParentWidget(nsIWidget** parentWidget)
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

NS_IMETHODIMP nsDocShellBase::SetParentWidget(nsIWidget* parentWidget)
{
   //XXX First Check
	/*			  
	This is the parenting widget for the control.  This may be null if only the
	native window was handed in for the parent during initialization.  If this
	is returned, it should refer to the same object as parentNativeWindow.
	*/
   return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsDocShellBase::GetParentNativeWindow(nativeWindow* parentNativeWindow)
{
   NS_ENSURE_ARG_POINTER(parentNativeWindow);
   //XXX First Check
	/*
	This is the native window parent of the control.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::SetParentNativeWindow(nativeWindow parentNativeWindow)
{
   //XXX First Check
	/*
	This is the native window parent of the control.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::GetVisibility(PRBool* visibility)
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

NS_IMETHODIMP nsDocShellBase::SetVisibility(PRBool visibility)
{
   //XXX First Check
	/*
	Attribute controls the visibility of the object behind this interface.
	Setting this attribute to false will hide the control.  Setting it to 
	true will show it.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::GetMainWidget(nsIWidget** mainWidget)
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

NS_IMETHODIMP nsDocShellBase::SetFocus()
{
   //XXX First Check
	/**
	* Give the window focus.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::RemoveFocus()
{
   //XXX First Check
	/**
	* Remove focus from the window
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::GetTitle(PRUnichar** title)
{
   NS_ENSURE_ARG_POINTER(title);

   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::SetTitle(const PRUnichar* title)
{
   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::GetZoom(float* zoom)
{
   NS_ENSURE_ARG_POINTER(zoom);

   //XXX First Check
	/**
	* Set/Get the document scale factor
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::SetZoom(float zoom)
{
   //XXX First Check
	/**
	* Set/Get the document scale factor
	*/
   return NS_ERROR_FAILURE;
}

//*****************************************************************************
// nsDocShellBase::nsIScrollable
//*****************************************************************************   

NS_IMETHODIMP nsDocShellBase::GetCurScrollPos(PRInt32 scrollOrientation, 
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

NS_IMETHODIMP nsDocShellBase::SetCurScrollPos(PRInt32 scrollOrientation, 
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

NS_IMETHODIMP nsDocShellBase::GetScrollRange(PRInt32 scrollOrientation,
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

NS_IMETHODIMP nsDocShellBase::SetScrollRange(PRInt32 scrollOrientation,
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

NS_IMETHODIMP nsDocShellBase::GetScrollbarPreferences(PRInt32 scrollOrientation,
   PRInt32* scrollbarPref)
{
   NS_ENSURE_ARG_POINTER(scrollbarPref);
   //XXX First Check
	/*
	Retrieves of Set the preferences for the scroll bar.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::SetScrollbarPreferences(PRInt32 scrollOrientation,
   PRInt32 scrollbarPref)
{
   //XXX First Check
	/*
	Retrieves of Set the preferences for the scroll bar.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::GetScrollbarVisibility(PRBool* verticalVisible,
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
// nsDocShellBase::nsITextScroll
//*****************************************************************************   

NS_IMETHODIMP nsDocShellBase::ScrollByLines(PRInt32 numLines)
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

NS_IMETHODIMP nsDocShellBase::ScrollByPages(PRInt32 numPages)
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