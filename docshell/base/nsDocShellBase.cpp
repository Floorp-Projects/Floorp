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
   NS_ENSURE_STATE(m_PresShell);

   nsCOMPtr<nsIDOMSelection> selection;
   NS_ENSURE_SUCCESS(m_PresShell->GetSelection(SELECTION_NORMAL, 
      getter_AddRefs(selection)), NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(selection->ClearSelection(), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::SelectAll()
{
   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::CopySelection()
{
   NS_ENSURE_STATE(m_PresShell);

   // the pres shell knows how to copy, so let it do the work
   NS_ENSURE_SUCCESS(m_PresShell->DoCopy(), NS_ERROR_FAILURE);
   return NS_OK;
}

/* the docShell is "copyable" if it has a selection and the selection is not
 * collapsed (that is, at least one token is selected, a character or a node
 */
NS_IMETHODIMP nsDocShellBase::GetCopyable(PRBool *aCopyable)
{
   NS_ENSURE_ARG_POINTER(aCopyable);

   NS_ENSURE_STATE(m_PresShell);

   nsCOMPtr<nsIDOMSelection> selection;
   NS_ENSURE_SUCCESS(m_PresShell->GetSelection(SELECTION_NORMAL, 
      getter_AddRefs(selection)), NS_ERROR_FAILURE);

   if(!selection)
      {
      *aCopyable = PR_FALSE;
      return NS_OK;
      }

   PRBool isCollapsed;
   NS_ENSURE_SUCCESS(selection->GetIsCollapsed(&isCollapsed), NS_ERROR_FAILURE);
   *aCopyable = !isCollapsed;

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::CutSelection()
{
   //XXX Implement
   //Should check to find the current focused object.  Then cut the contents.
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::GetCutable(PRBool* cutable)
{
   NS_ENSURE_ARG_POINTER(cutable);
   //XXX Implement
   //Should check to find the current focused object.  Then see if it can
   //be cut out of.  For now the answer is always no since CutSelection()
   // has not been implemented.
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::Paste()
{
   //XXX Implement
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::GetPasteable(PRBool* pasteable)
{
   NS_ENSURE_ARG_POINTER(pasteable);

   //XXX Implement
   //Should check to find the current focused object.  Then see if it can
   //be pasted into.  For now the answer is always no since Paste()
   // has not been implemented.
   *pasteable = PR_FALSE;
   return NS_OK;
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
   // XXX Implement
   // Should check if a doc is loaded and if it is in a state that that is
   // ready to save.  For now the answer is always no since saving isn't
   // implemented.

   *saveable = PR_FALSE;
   return NS_OK;
} 

NS_IMETHODIMP nsDocShellBase::Print()
{
   //XXX First Check
   return NS_ERROR_FAILURE;
} 

NS_IMETHODIMP nsDocShellBase::GetPrintable(PRBool* printable)
{
   NS_ENSURE_ARG_POINTER(printable);
   // XXX Implement
   // Should check if a doc is loaded and if it is in a state that that is
   // ready to print.  For now the answer is always no since printing isn't
   // implemented.

   *printable = PR_FALSE;
   return NS_OK;
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
      /* XXX Implement below is code from old webShell
      We don't have a heavy-weight window, we want to talk to our view I think.
      We also don't want to duplicate the bounds locally.  No need, let the
      view keep up with that.

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
      */
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
      /* XXX Implement below is code from old webShell
      We don't have a heavy-weight window, we want to talk to our view I think.
      We also don't want to duplicate the bounds locally.  No need, let the
      view keep up with that.

        nsRect result;
        if (nsnull != mWindow) {
          mWindow->GetClientBounds(result);
        } else {
          result = mBounds;
        }

        *aX = result.x;
        *aY = result.y;

        return NS_OK;
      */
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
      /*  XXX Implement below is code from old webShell
      We don't have a heavy-weight window, we want to talk to our view I think.
      We also don't want to duplicate the bounds locally.  No need, let the
      view keep up with that.


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
      */
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
      /* XXX Implement below is code from old webShell
      We don't have a heavy-weight window, we want to talk to our view I think.
      We also don't want to duplicate the bounds locally.  No need, let the
      view keep up with that.

        nsRect result;
     if (nsnull != mWindow) {
       mWindow->GetClientBounds(result);
     } else {
       result = mBounds;
     }

     *aCX = result.width;
     *aCY = result.height;

     return NS_OK;
      */
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

      /* XXX Implement below is code from old webShell
      We don't have a heavy-weight window, we want to talk to our view I think.
      We also don't want to duplicate the bounds locally.  No need, let the
      view keep up with that.

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
      */
      }

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::Repaint(PRBool fForce)
{
   //XXX First Check
	/** 
	 * Tell the window to repaint itself
	 * @param aForce - if true, repaint immediately
	 *                 if false, the window may defer repainting as it sees fit.
	 */

   /* XXX Implement Tell our view to repaint

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

   /* XXX implement

     if (mWindow) {
    mWindow->SetFocus();
  }

  return NS_OK;

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

   nsCOMPtr<nsIScrollableView> scrollView;
   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)), 
      NS_ERROR_FAILURE);

   nscoord x, y;
   NS_ENSURE_SUCCESS(scrollView->GetScrollPosition(x, y), NS_ERROR_FAILURE);

   switch(scrollOrientation)
      {
      case ScrollOrientation_X:
         *curPos = x;
         return NS_OK;

      case ScrollOrientation_Y:
         *curPos = y;
         return NS_OK;

      default:
         NS_ENSURE(PR_FALSE, NS_ERROR_INVALID_ARG);
      }
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::SetCurScrollPos(PRInt32 scrollOrientation, 
   PRInt32 curPos)
{
   nsCOMPtr<nsIScrollableView> scrollView;
   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)), 
      NS_ERROR_FAILURE);

   PRInt32 other;
   PRInt32 x;
   PRInt32 y;

   GetCurScrollPos(scrollOrientation, &other);

   switch(scrollOrientation)
      {
      case ScrollOrientation_X:
         x = curPos;
         y = other;
         break;

      case ScrollOrientation_Y:
         x = other;
         y = curPos;
         break;

      default:
         NS_ENSURE(PR_FALSE, NS_ERROR_INVALID_ARG);
      }

   NS_ENSURE_SUCCESS(scrollView->ScrollTo(x, y, NS_VMREFRESH_IMMEDIATE),
      NS_ERROR_FAILURE);
   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::SetCurScrollPosEx(PRInt32 curHorizontalPos, 
   PRInt32 curVerticalPos)
{
   nsCOMPtr<nsIScrollableView> scrollView;
   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)), 
      NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(scrollView->ScrollTo(curHorizontalPos, curVerticalPos, 
      NS_VMREFRESH_IMMEDIATE), NS_ERROR_FAILURE);
   return NS_OK;
}

// XXX This is wrong
NS_IMETHODIMP nsDocShellBase::GetScrollRange(PRInt32 scrollOrientation,
   PRInt32* minPos, PRInt32* maxPos)
{
   NS_ENSURE_ARG_POINTER(minPos && maxPos);

   nsCOMPtr<nsIScrollableView> scrollView;
   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)), 
      NS_ERROR_FAILURE);

   PRInt32 cx;
   PRInt32 cy;
   
   NS_ENSURE_SUCCESS(scrollView->GetContainerSize(&cx, &cy), NS_ERROR_FAILURE);
   *minPos = 0;
   
   switch(scrollOrientation)
      {
      case ScrollOrientation_X:
         *maxPos = cx;
         return NS_OK;

      case ScrollOrientation_Y:
         *maxPos = cy;
         return NS_OK;

      default:
         NS_ENSURE(PR_FALSE, NS_ERROR_INVALID_ARG);
      }

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

NS_IMETHODIMP nsDocShellBase::SetScrollRangeEx(PRInt32 minHorizontalPos,
   PRInt32 maxHorizontalPos, PRInt32 minVerticalPos, PRInt32 maxVerticalPos)
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

   nsCOMPtr<nsIScrollableView> scrollView;
   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)), 
      NS_ERROR_FAILURE);

   // XXX This is all evil, we need to convert.  We don't know our prefs
   // are the same as this interfaces.
 /*  nsScrollPreference scrollPref;

   NS_ENSURE_SUCCESS(scrollView->GetScrollPreference(scrollPref), 
      NS_ERROR_FAILURE);

   *scrollbarPref = scrollPref; */

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::SetScrollbarPreferences(PRInt32 scrollOrientation,
   PRInt32 scrollbarPref)
{
   nsCOMPtr<nsIScrollableView> scrollView;
   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)), 
      NS_ERROR_FAILURE);

   // XXX This is evil.  We should do a mapping, we don't know our prefs
   // are the same as this interface.  In fact it doesn't compile
  /* nsScrollPreference scrollPref = scrollbarPref;
   NS_ENSURE_SUCCESS(scrollView->SetScrollPreference(scrollPref), 
      NS_ERROR_FAILURE);  */

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::GetScrollbarVisibility(PRBool* verticalVisible,
   PRBool* horizontalVisible)
{
   nsCOMPtr<nsIScrollableView> scrollView;
   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)), 
      NS_ERROR_FAILURE);

   PRBool vertVisible;
   PRBool horizVisible;

   NS_ENSURE_SUCCESS(scrollView->GetScrollbarVisibility(&vertVisible,
      &horizVisible), NS_ERROR_FAILURE);

   if(verticalVisible)
      *verticalVisible = vertVisible;
   if(horizontalVisible)
      *horizontalVisible = horizVisible;

   return NS_OK;
}

//*****************************************************************************
// nsDocShellBase::nsITextScroll
//*****************************************************************************   

NS_IMETHODIMP nsDocShellBase::ScrollByLines(PRInt32 numLines)
{
   nsCOMPtr<nsIScrollableView> scrollView;

   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)),
      NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(scrollView->ScrollByLines(numLines), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::ScrollByPages(PRInt32 numPages)
{
   nsCOMPtr<nsIScrollableView> scrollView;

   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)),
      NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(scrollView->ScrollByPages(numPages), NS_ERROR_FAILURE);

   return NS_OK;
}

//*****************************************************************************
// nsDocShellBase: Helper Routines
//*****************************************************************************   

nsresult nsDocShellBase::GetChildOffset(nsIDOMNode *aChild, nsIDOMNode* aParent,
   PRInt32* aOffset)
{
   NS_ENSURE_ARG_POINTER(aChild || aParent);
   
   nsCOMPtr<nsIDOMNodeList> childNodes;
   NS_ENSURE_SUCCESS(aParent->GetChildNodes(getter_AddRefs(childNodes)),
      NS_ERROR_FAILURE);
   NS_ENSURE(childNodes, NS_ERROR_FAILURE);

   PRInt32 i=0;

   for( ; PR_TRUE; i++)
      {
      nsCOMPtr<nsIDOMNode> childNode;
      NS_ENSURE_SUCCESS(childNodes->Item(i, getter_AddRefs(childNode)), 
         NS_ERROR_FAILURE);
      NS_ENSURE(childNode, NS_ERROR_FAILURE);

      if(childNode.get() == aChild)
         {
         *aOffset = i;
         return NS_OK;
         }
      }

   return NS_ERROR_FAILURE;
}

nsresult nsDocShellBase::GetRootScrollableView(nsIScrollableView** aOutScrollView)
{
   NS_ENSURE_ARG_POINTER(aOutScrollView);
   
   nsCOMPtr<nsIViewManager> viewManager;

   //XXX Get ViewManager Somewhere.

   NS_ENSURE_SUCCESS(viewManager->GetRootScrollableView(aOutScrollView),
      NS_ERROR_FAILURE);

   return NS_OK;
}        
