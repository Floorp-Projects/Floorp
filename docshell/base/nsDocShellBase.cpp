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

nsDocShellBase::nsDocShellBase() : m_Created(PR_FALSE)
{
	NS_INIT_REFCNT();
   m_BaseInitInfo = new nsDocShellInitInfo();
}

nsDocShellBase::~nsDocShellBase()
{
   if(m_BaseInitInfo)
      {
      delete m_BaseInitInfo;
      m_BaseInitInfo = nsnull;
      }
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

   *presContext = m_PresContext;
   NS_IF_ADDREF(*presContext);

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::SetPresContext(nsIPresContext* presContext)
{
   m_PresContext = presContext;
   
   return NS_OK;
}


NS_IMETHODIMP nsDocShellBase::GetParent(nsIDocShell** parent)
{
   NS_ENSURE_ARG_POINTER(parent);

   *parent = m_Parent;
   NS_IF_ADDREF(*parent);

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::SetParent(nsIDocShell* parent)
{
   m_Parent = parent;
   return NS_OK;
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
   NS_ENSURE_ARG(parentWidget);  // DocShells must get a widget for a parent
   NS_ENSURE_STATE(!m_Created && m_BaseInitInfo);

   m_ParentWidget = parentWidget;
   m_BaseInitInfo->x = x;
   m_BaseInitInfo->y = y;
   m_BaseInitInfo->cx = cx;
   m_BaseInitInfo->cy = cy;

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::Create()
{
   NS_ENSURE_STATE(!m_Created);

   // Use m_BaseInitInfo to do create
   // Then delete m_BaseInitInfo
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
   // We don't support the dynamic destroy and recreate on the object.  Just
   // create a new object!
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDocShellBase::SetPosition(PRInt32 x, PRInt32 y)
{
   if(!m_Created)
      {
      m_BaseInitInfo->x = x;
      m_BaseInitInfo->y = y;
      }
   else
      {
      //XXX Manipulate normal position stuff
      }

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::GetPosition(PRInt32* x, PRInt32* y)
{
   NS_ENSURE_ARG_POINTER(x && y);

   if(!m_Created)
      {
      *x = m_BaseInitInfo->x;
      *y = m_BaseInitInfo->y;
      }
   else
      {
      //XXX query normal position objects
      }

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
   if(!m_Created)
      {
      m_BaseInitInfo->cx = cx;
      m_BaseInitInfo->cy = cy;
      }
   else
      {
      // XXX Do Normal Size Stuff
      }

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::GetSize(PRInt32* cx, PRInt32* cy)
{
   NS_ENSURE_ARG_POINTER(cx && cy);

   if(!m_Created)
      {
      *cx = m_BaseInitInfo->cx;
      *cy = m_BaseInitInfo->cy;
      }
   else
      {
      //XXX Query normal Size Objects
      }

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx,
   PRInt32 cy, PRBool fRepaint)
{
   if(!m_Created)
      {
      m_BaseInitInfo->x = x;
      m_BaseInitInfo->y = y;
      m_BaseInitInfo->cx = cx;
      m_BaseInitInfo->cy = cy;
      }
   else
      {
      // XXX Do normal size and position stuff.  Could just call 
      // Size and then Position, but underlying control probably supports 
      // some optimized setting of both like this.
      }

   return NS_OK;
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

   *parentWidget = m_ParentWidget;

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::SetParentWidget(nsIWidget* parentWidget)
{
   NS_ENSURE_STATE(!m_Created);

   m_ParentWidget = parentWidget;

   return NS_OK;
}


NS_IMETHODIMP nsDocShellBase::GetParentNativeWindow(nativeWindow* parentNativeWindow)
{
   NS_ENSURE_ARG_POINTER(parentNativeWindow);

   if(m_ParentWidget)
      *parentNativeWindow = m_ParentWidget->GetNativeData(NS_NATIVE_WIDGET);
   else
      *parentNativeWindow = nsnull;

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::SetParentNativeWindow(nativeWindow parentNativeWindow)
{
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDocShellBase::GetVisibility(PRBool* visibility)
{
   NS_ENSURE_ARG_POINTER(visibility);

   if(!m_Created)
      *visibility = m_BaseInitInfo->visible;
   else
      {
      //XXX Query underlying control
      }

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::SetVisibility(PRBool visibility)
{
   if(!m_Created)
      m_BaseInitInfo->visible = visibility;
   else
      {
      // XXX Set underlying control visibility
      }

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::GetMainWidget(nsIWidget** mainWidget)
{
   NS_ENSURE_ARG_POINTER(mainWidget);

   // For now we don't create our own widget, so simply return the parent one. 
   *mainWidget = m_ParentWidget;
   NS_IF_ADDREF(*mainWidget);

   return NS_OK;
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