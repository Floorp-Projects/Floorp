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

#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDocumentViewer.h"

#include "nsDocShellBase.h"

//*****************************************************************************
//***    nsDocShellBase: Object Management
//*****************************************************************************

nsDocShellBase::nsDocShellBase() : mCreated(PR_FALSE)
{
	NS_INIT_REFCNT();
   mBaseInitInfo = new nsDocShellInitInfo();
}

nsDocShellBase::~nsDocShellBase()
{
   if(mBaseInitInfo)
      {
      delete mBaseInitInfo;
      mBaseInitInfo = nsnull;
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

NS_IMETHODIMP nsDocShellBase::LoadURI(const PRUnichar* uri, 
   nsIPresContext* presContext)
{
   //NS_ENSURE_ARG(uri);  // Done in LoadURIVia for us.

   return LoadURIVia(uri, presContext, 0);
}

NS_IMETHODIMP nsDocShellBase::LoadURIVia(const PRUnichar* uri, 
   nsIPresContext* presContext, PRUint32 adapterBinding)
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

NS_IMETHODIMP nsDocShellBase::GetDocument(nsIDOMDocument** aDocument)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  NS_ENSURE_STATE(mContentViewer);

  nsCOMPtr<nsIPresShell> presShell;
  NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(presShell)), NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocument>doc;
  NS_ENSURE_SUCCESS(presShell->GetDocument(getter_AddRefs(doc)), NS_ERROR_FAILURE);
  NS_ENSURE(doc, NS_ERROR_NULL_POINTER);

  // the result's addref comes from this QueryInterface call
  NS_ENSURE_SUCCESS(CallQueryInterface(doc, aDocument), NS_ERROR_FAILURE);

  return NS_OK;
}

// SetDocument is only meaningful for doc shells that support DOM documents.  Not all do.
NS_IMETHODIMP nsDocShellBase::SetDocument(nsIDOMDocument* aDocument, 
   nsIPresContext* presContext)
{
  NS_WARN_IF_FALSE(PR_FALSE, "Subclasses should override this method!!!!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// caller is responsible for calling nsString::Recycle(*aName);
NS_IMETHODIMP nsDocShellBase::GetName(PRUnichar** aName)
{
  NS_ENSURE_ARG_POINTER(aName);
  *aName = nsnull;
  if (0!=mName.Length())
  {
    *aName = mName.ToNewUnicode();
  }
  return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::SetName(const PRUnichar* aName)
{
  if (aName) {
    mName = aName;  // this does a copy of aName
  }
  else {
    mName = "";
  }
  return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::GetPresContext(nsIPresContext** aPresContext)
{
   NS_ENSURE_ARG_POINTER(aPresContext);

   if(!mContentViewer)
      {
      *aPresContext = nsnull;
      return NS_OK;
      }

   nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(mContentViewer));
   NS_ENSURE(docv, NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(docv->GetPresContext(*aPresContext), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::GetParent(nsIDocShell** parent)
{
   NS_ENSURE_ARG_POINTER(parent);

   *parent = mParent;
   NS_IF_ADDREF(*parent);

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::SetParent(nsIDocShell* aParent)
{
  // null aParent is ok
  mParent = aParent;   // this assignment does an addref
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

NS_IMETHODIMP nsDocShellBase::GetPrefs(nsIPref** aPrefs)
{
   NS_ENSURE_ARG_POINTER(aPrefs);

   *aPrefs = mPrefs;
   NS_IF_ADDREF(*aPrefs);

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::SetPrefs(nsIPref* aPrefs)
{
  // null aPrefs is ok
  mPrefs = aPrefs;    // this assignment does an addref
  return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::GetRootDocShell(nsIDocShell** aRootDocShell)
{
  NS_ENSURE_ARG_POINTER(aRootDocShell);
  *aRootDocShell = this;

  nsCOMPtr<nsIDocShell> parent;
  NS_ENSURE(GetParent(getter_AddRefs(parent)), NS_ERROR_FAILURE);
  while (parent)
  {
    *aRootDocShell = parent;
    NS_ENSURE(GetParent(getter_AddRefs(parent)), NS_ERROR_FAILURE);
  }
  NS_IF_ADDREF(*aRootDocShell);
  return NS_OK;
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
// nsDocShellBase::nsIDocShellEdit
//*****************************************************************************   

NS_IMETHODIMP nsDocShellBase::Search()
{
  NS_WARN_IF_FALSE(PR_FALSE, "Subclasses should override this method!!!!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDocShellBase::GetSearchable(PRBool* aSearchable)
{
   NS_ENSURE_ARG_POINTER(aSearchable);
   *aSearchable = PR_FALSE;
   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::ClearSelection()
{
   NS_ENSURE_STATE(mContentViewer);
   nsCOMPtr<nsIPresShell> presShell;
   NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(presShell)), NS_ERROR_FAILURE);

   nsCOMPtr<nsIDOMSelection> selection;
   NS_ENSURE_SUCCESS(presShell->GetSelection(SELECTION_NORMAL, 
      getter_AddRefs(selection)), NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(selection->ClearSelection(), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::SelectAll()
{
  NS_WARN_IF_FALSE(PR_FALSE, "Subclasses should override this method!!!!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDocShellBase::CopySelection()
{
   NS_ENSURE_STATE(mContentViewer);
   nsCOMPtr<nsIPresShell> presShell;
   NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(presShell)), NS_ERROR_FAILURE);

   // the pres shell knows how to copy, so let it do the work
   NS_ENSURE_SUCCESS(presShell->DoCopy(), NS_ERROR_FAILURE);
   return NS_OK;
}

/* the docShell is "copyable" if it has a selection and the selection is not
 * collapsed (that is, at least one token is selected, a character or a node
 */
NS_IMETHODIMP nsDocShellBase::GetCopyable(PRBool *aCopyable)
{
   NS_ENSURE_ARG_POINTER(aCopyable);

   NS_ENSURE_STATE(mContentViewer);
   nsCOMPtr<nsIPresShell> presShell;
   NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(presShell)), NS_ERROR_FAILURE);

   nsCOMPtr<nsIDOMSelection> selection;
   NS_ENSURE_SUCCESS(presShell->GetSelection(SELECTION_NORMAL, 
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

NS_IMETHODIMP nsDocShellBase::GetCutable(PRBool* aCutable)
{
   NS_ENSURE_ARG_POINTER(aCutable);
   //XXX Implement
   //Should check to find the current focused object.  Then see if it can
   //be cut out of.  For now the answer is always no since CutSelection()
   // has not been implemented.
   *aCutable = PR_FALSE;
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::Paste()
{
   //XXX Implement
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellBase::GetPasteable(PRBool* aPasteable)
{
   NS_ENSURE_ARG_POINTER(aPasteable);

   //XXX Implement
   //Should check to find the current focused object.  Then see if it can
   //be pasted into.  For now the answer is always no since Paste()
   // has not been implemented.
   *aPasteable = PR_FALSE;
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
   NS_ENSURE_STATE(!mCreated && mBaseInitInfo);

   mParentWidget = parentWidget;
   mBaseInitInfo->x = x;
   mBaseInitInfo->y = y;
   mBaseInitInfo->cx = cx;
   mBaseInitInfo->cy = cy;

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::Create()
{
   NS_ENSURE_STATE(!mCreated);

   // Use mBaseInitInfo to do create
   // Then delete mBaseInitInfo
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
   if(!mCreated)
      {
      mBaseInitInfo->x = x;
      mBaseInitInfo->y = y;
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

   if(!mCreated)
      {
      *x = mBaseInitInfo->x;
      *y = mBaseInitInfo->y;
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
   if(!mCreated)
      {
      mBaseInitInfo->cx = cx;
      mBaseInitInfo->cy = cy;
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

   if(!mCreated)
      {
      *cx = mBaseInitInfo->cx;
      *cy = mBaseInitInfo->cy;
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
   if(!mCreated)
      {
      mBaseInitInfo->x = x;
      mBaseInitInfo->y = y;
      mBaseInitInfo->cx = cx;
      mBaseInitInfo->cy = cy;
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

   *parentWidget = mParentWidget;

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::SetParentWidget(nsIWidget* parentWidget)
{
   NS_ENSURE_STATE(!mCreated);

   mParentWidget = parentWidget;

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::GetParentNativeWindow(nativeWindow* parentNativeWindow)
{
   NS_ENSURE_ARG_POINTER(parentNativeWindow);

   if(mParentWidget)
      *parentNativeWindow = mParentWidget->GetNativeData(NS_NATIVE_WIDGET);
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

   if(!mCreated)
      *visibility = mBaseInitInfo->visible;
   else
      {
      //XXX Query underlying control
      }

   return NS_OK;
}

NS_IMETHODIMP nsDocShellBase::SetVisibility(PRBool visibility)
{
   if(!mCreated)
      mBaseInitInfo->visible = visibility;
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
   *mainWidget = mParentWidget;
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

nsresult nsDocShellBase::GetPresShell(nsIPresShell** aPresShell)
{
   NS_ENSURE_ARG_POINTER(aPresShell);
   
   nsCOMPtr<nsIPresContext> presContext;
   NS_ENSURE_SUCCESS(GetPresContext(getter_AddRefs(presContext)), 
      NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(presContext->GetShell(aPresShell), NS_ERROR_FAILURE);

   return NS_OK;
}

/*

// null result aPresContext is legal 
NS_IMETHODIMP nsDocShellBase::GetPresContext(nsIPresContext** aPresContext) 
{ 
  NS_ENSURE_ARG_POINTER(aPresContext); 

  nsCOMPtr<nsIContentViewer> cv; 
  NS_ENSURE_SUCCESS(GetContentViewer(getter_AddRefs(cv))); 
  // null content viewer is legal 

  if (cv) 
  { 
    nsIDocumentViewer* docv = nsnull; 
    cv->QueryInterface(kIDocumentViewerIID, (void**) &docv); 
    if (docv) 
    { 
      nsIPresContext* cx; 
      NS_ENSURE_SUCCESS(docv->GetPresContext(aPresContext)); 
    } 
  } 

  return NS_OK; 
} 


NS_IMETHODIMP nsDocShellBase::GetDocument(nsIDOMDocument** aDocument) 
{ 
  NS_ENSURE_ARG_POINTER(aDocument); 

  nsCOMPtr<nsIPresShell> presShell; 
  NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(presShell))); 
  NS_ENSURE(presShell, NS_ERROR_FAILURE); 

  nsCOMPtr<nsIDocument>doc; 
  NS_ENSURE_SUCCESS(PresShell->GetDocument(getter_AddRefs(doc)), NS_ERROR_FAILURE); 
  NS_ENSURE(doc, NS_ERROR_NULL_POINTER); 

  // the result's addref comes from this QueryInterface call 
  NS_ENSURE_SUCCESS(CallQueryInterface(doc, aDocument), NS_ERROR_FAILURE); 

  return NS_OK; 
} 

       
         */