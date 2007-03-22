/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *   Chak Nanga <chak@netscape.com>
 *
 * ***** END LICENSE BLOCK ***** */

// File Overview....
// This is the class which implements all the interfaces
// required(and optional) by the mozilla embeddable browser engine
//
// Note that this obj gets passed in the IBrowserFrameGlue* using the 
// Init() method. Many of the interface implementations use this
// to get the actual task done. Ex: to update the status bar
//
// Look at the INTERFACE_MAP_ENTRY's below for the list of 
// the currently implemented interfaces
//
// This file currently has the implementation for all the interfaces
// which are required of an app embedding Gecko
// Implementation of other optional interfaces are in separate files
// to avoid cluttering this one and to demonstrate other embedding
// principles. For example, nsIPrompt is implemented in a separate DLL.
// 
//    nsIWebBrowserChrome    - This is a required interface to be implemented
//                          by embeddors
//
//    nsIEmbeddingSiteWindow - This is a required interface to be implemented
//                         by embedders            
//                       - SetTitle() gets called after a document
//                         load giving us the chance to update our
//                         titlebar
//
// Some points to note...
//
// nsIWebBrowserChrome interface's SetStatus() gets called when a user 
// mouses over the links on a page
//
// nsIWebProgressListener interface's OnStatusChange() gets called
// to indicate progress when a page is being loaded
//
// Next suggested file(s) to look at : 
//            Any of the BrowserImpl*.cpp files for other interface
//            implementation details
//

#ifdef _WINDOWS
  #include "stdafx.h"
#endif

#include "nsIDOMWindow.h"
#include "BrowserImpl.h"

CBrowserImpl::CBrowserImpl()
{
  m_pBrowserFrameGlue = NULL;
  mWebBrowser = nsnull;
}


CBrowserImpl::~CBrowserImpl()
{
}

// It's very important that the creator of this CBrowserImpl object
// Call on this Init() method with proper values after creation
//
NS_METHOD CBrowserImpl::Init(PBROWSERFRAMEGLUE pBrowserFrameGlue, 
                             nsIWebBrowser* aWebBrowser)
{
      m_pBrowserFrameGlue = pBrowserFrameGlue;
      
      SetWebBrowser(aWebBrowser);

      return NS_OK;
}

//*****************************************************************************
// CBrowserImpl::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF(CBrowserImpl)
NS_IMPL_RELEASE(CBrowserImpl)

NS_INTERFACE_MAP_BEGIN(CBrowserImpl)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserChrome)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
   NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChromeFocus)
   NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
   NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow2)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
   NS_INTERFACE_MAP_ENTRY(nsIContextMenuListener2)
   NS_INTERFACE_MAP_ENTRY(nsITooltipListener)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

//*****************************************************************************
// CBrowserImpl::nsIInterfaceRequestor
//*****************************************************************************   

NS_IMETHODIMP CBrowserImpl::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
    if(aIID.Equals(NS_GET_IID(nsIDOMWindow)))
    {
        if (mWebBrowser)
            return mWebBrowser->GetContentDOMWindow((nsIDOMWindow **) aInstancePtr);
        return NS_ERROR_NOT_INITIALIZED;
    }

    return QueryInterface(aIID, aInstancePtr);
}

//*****************************************************************************
// CBrowserImpl::nsIWebBrowserChrome
//*****************************************************************************   

//Gets called when you mouseover links etc. in a web page
//
NS_IMETHODIMP CBrowserImpl::SetStatus(PRUint32 aType, const PRUnichar* aStatus)
{
    if(! m_pBrowserFrameGlue)
        return NS_ERROR_FAILURE;

    m_pBrowserFrameGlue->UpdateStatusBarText(aStatus);

    return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::GetWebBrowser(nsIWebBrowser** aWebBrowser)
{
   NS_ENSURE_ARG_POINTER(aWebBrowser);

   *aWebBrowser = mWebBrowser;

   NS_IF_ADDREF(*aWebBrowser);

   return NS_OK;
}

// Currently called from Init(). I'm not sure who else
// calls this
//
NS_IMETHODIMP CBrowserImpl::SetWebBrowser(nsIWebBrowser* aWebBrowser)
{
   NS_ENSURE_ARG_POINTER(aWebBrowser);

   mWebBrowser = aWebBrowser;

   return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::GetChromeFlags(PRUint32* aChromeMask)
{
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CBrowserImpl::SetChromeFlags(PRUint32 aChromeMask)
{
   return NS_ERROR_NOT_IMPLEMENTED;
}

// Will get called in response to JavaScript window.close()
//
NS_IMETHODIMP CBrowserImpl::DestroyBrowserWindow()
{
    if(! m_pBrowserFrameGlue)
        return NS_ERROR_FAILURE;

    m_pBrowserFrameGlue->DestroyBrowserFrame();

    return NS_OK;
}

// Gets called in response to set the size of a window
// Ex: In response to a JavaScript Window.Open() call of
// the form 
//
//        window.open("http://www.mozilla.org", "theWin", "width=200, height=400");
//
// First the CreateBrowserWindow() call will be made followed by a 
// call to this method to resize the window
//
NS_IMETHODIMP CBrowserImpl::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
    if(! m_pBrowserFrameGlue)
        return NS_ERROR_FAILURE;

    HWND w = m_pBrowserFrameGlue->GetBrowserFrameNativeWnd();

    CRect rcNewFrame(CPoint(0, 0), CSize(aCX, aCY));
    CRect rcFrame;
    CRect rcClient;

    // Adjust for 3D edge on client area
    AdjustWindowRectEx(&rcNewFrame, WS_VISIBLE, FALSE, WS_EX_CLIENTEDGE);

    GetClientRect(w, &rcClient);
    GetWindowRect(w, &rcFrame);

    rcNewFrame.right += rcFrame.Width() - rcClient.Width();
    rcNewFrame.bottom += rcFrame.Height() - rcClient.Height();

    m_pBrowserFrameGlue->SetBrowserFrameSize(rcNewFrame.Width(), rcNewFrame.Height());

    return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::ShowAsModal(void)
{
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CBrowserImpl::IsWindowModal(PRBool *retval)
{
  // We're not modal
  *retval = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::ExitModalEventLoop(nsresult aStatus)
{
  return NS_OK;
}

//*****************************************************************************
// CBrowserImpl::nsIWebBrowserChromeFocus
//*****************************************************************************

NS_IMETHODIMP CBrowserImpl::FocusNextElement()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CBrowserImpl::FocusPrevElement()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//*****************************************************************************
// CBrowserImpl::nsIEmbeddingSiteWindow
//*****************************************************************************   

NS_IMETHODIMP CBrowserImpl::SetDimensions(PRUint32 aFlags, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)
{
    if(! m_pBrowserFrameGlue)
        return NS_ERROR_FAILURE;

    if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION &&
        (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER || 
         aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER))
    {
        m_pBrowserFrameGlue->SetBrowserFramePositionAndSize(x, y, cx, cy, PR_TRUE);
    }
    else
    {
        if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION)
        {
            m_pBrowserFrameGlue->SetBrowserFramePosition(x, y);
        }
        if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER || 
            aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER)
        {
            m_pBrowserFrameGlue->SetBrowserFrameSize(cx, cy);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::GetDimensions(PRUint32 aFlags, PRInt32 *x, PRInt32 *y, PRInt32 *cx, PRInt32 *cy)
{
    if(! m_pBrowserFrameGlue)
        return NS_ERROR_FAILURE;
    
    if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION)
    {
        m_pBrowserFrameGlue->GetBrowserFramePosition(x, y);
    }
    if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER || 
        aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER)
    {
        m_pBrowserFrameGlue->GetBrowserFrameSize(cx, cy);
    }

    return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::GetSiteWindow(void** aSiteWindow)
{
  if (!aSiteWindow)
    return NS_ERROR_NULL_POINTER;

  *aSiteWindow = 0;
  if (m_pBrowserFrameGlue) {
    HWND w = m_pBrowserFrameGlue->GetBrowserFrameNativeWnd();
    *aSiteWindow = NS_REINTERPRET_CAST(void *, w);
  }
  return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::SetFocus()
{
    if(! m_pBrowserFrameGlue)
        return NS_ERROR_FAILURE;

    m_pBrowserFrameGlue->SetFocus();

    return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::GetTitle(PRUnichar** aTitle)
{
    if(! m_pBrowserFrameGlue)
        return NS_ERROR_FAILURE;

    m_pBrowserFrameGlue->GetBrowserFrameTitle(aTitle);
    
    return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::SetTitle(const PRUnichar* aTitle)
{
    if(! m_pBrowserFrameGlue)
        return NS_ERROR_FAILURE;

    m_pBrowserFrameGlue->SetBrowserFrameTitle(aTitle);
    
    return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::GetVisibility(PRBool *aVisibility)
{
    if(! m_pBrowserFrameGlue)
        return NS_ERROR_FAILURE;

    m_pBrowserFrameGlue->GetBrowserFrameVisibility(aVisibility);

    return NS_OK;
}

NS_IMETHODIMP CBrowserImpl::SetVisibility(PRBool aVisibility)
{
    if(! m_pBrowserFrameGlue)
        return NS_ERROR_FAILURE;

    m_pBrowserFrameGlue->ShowBrowserFrame(aVisibility);

    return NS_OK;
}

//*****************************************************************************
// CBrowserImpl::nsIEmbeddingSiteWindow2
//*****************************************************************************   

NS_IMETHODIMP CBrowserImpl::Blur()
{
    return NS_OK;
}


//*****************************************************************************
// CBrowserImpl::nsITooltipListener
//*****************************************************************************   

/* void onShowTooltip (in long aXCoords, in long aYCoords, in wstring aTipText); */
NS_IMETHODIMP CBrowserImpl::OnShowTooltip(PRInt32 aXCoords, PRInt32 aYCoords, const PRUnichar *aTipText)
{
    if(! m_pBrowserFrameGlue)
        return NS_ERROR_FAILURE;

    m_pBrowserFrameGlue->ShowTooltip(aXCoords, aYCoords, aTipText);

    return NS_OK;
}

/* void onHideTooltip (); */
NS_IMETHODIMP CBrowserImpl::OnHideTooltip()
{
    if(! m_pBrowserFrameGlue)
        return NS_ERROR_FAILURE;

    m_pBrowserFrameGlue->HideTooltip();

    return NS_OK;
}
