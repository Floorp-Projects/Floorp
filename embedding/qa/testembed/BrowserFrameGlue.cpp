/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Chak Nanga <chak@netscape.com> 
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

// File Overview....
//
// This file has the IBrowserFrameGlueObj implementation
// This frame glue object is nested inside of the BrowserFrame
// object(See BrowserFrm.h for more info)
//
// This is the place where all the platform specific interaction
// with the browser frame window takes place in response to 
// callbacks from Gecko interface methods
// 
// The main purpose of this interface is to separate the cross 
// platform code in BrowserImpl*.cpp from the platform specific
// code(which is in this file)
//
// You'll also notice the use of a macro named "METHOD_PROLOGUE"
// through out this file. This macro essentially makes the pointer
// to a "containing" class available inside of the class which is
// being contained via a var named "pThis". In our case, the 
// BrowserFrameGlue object is contained inside of the BrowserFrame
// object so "pThis" will be a pointer to a BrowserFrame object
// Refer to MFC docs for more info on METHOD_PROLOGUE macro


#include "stdafx.h"
#include "TestEmbed.h"
#include "BrowserFrm.h"
#include "Dialogs.h"

/////////////////////////////////////////////////////////////////////////////
// IBrowserFrameGlue implementation

void CBrowserFrame::BrowserFrameGlueObj::UpdateStatusBarText(const PRUnichar *aMessage)
{
	METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)

	nsCString strStatus; 

    if(aMessage)
        strStatus.AssignWithConversion(aMessage);

    pThis->m_wndStatusBar.SetPaneText(0, strStatus.get());
}

void CBrowserFrame::BrowserFrameGlueObj::UpdateProgress(PRInt32 aCurrent, PRInt32 aMax)
{
	METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)

    pThis->m_wndProgressBar.SetRange32(0, aMax);
    pThis->m_wndProgressBar.SetPos(aCurrent);
}

void CBrowserFrame::BrowserFrameGlueObj::UpdateBusyState(PRBool aBusy)
{	
	METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)

	// Just notify the view of the busy state
	// There's code in there which will take care of
	// updating the STOP toolbar btn. etc

	pThis->m_wndBrowserView.UpdateBusyState(aBusy);
}

// Called from the OnLocationChange() method in the nsIWebProgressListener 
// interface implementation in CBrowserImpl to update the current URI
// Will get called after a URI is successfully loaded in the browser
// We use this info to update the URL bar's edit box
//
void CBrowserFrame::BrowserFrameGlueObj::UpdateCurrentURI(nsIURI *aLocation)
{
	METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)

	if(aLocation)
	{
		nsXPIDLCString uriString;
		aLocation->GetSpec(getter_Copies(uriString));

		pThis->m_wndUrlBar.SetCurrentURL(uriString.get());
	}
}

void CBrowserFrame::BrowserFrameGlueObj::GetBrowserFrameTitle(PRUnichar **aTitle)
{
	METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)

	CString title;
	pThis->GetWindowText(title);

	if(!title.IsEmpty())
	{
		nsString nsTitle;
		nsTitle.AssignWithConversion(title.GetBuffer(0));

		*aTitle = nsTitle.ToNewUnicode();
	}
}

void CBrowserFrame::BrowserFrameGlueObj::SetBrowserFrameTitle(const PRUnichar *aTitle)
{
	METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)

	USES_CONVERSION;

	if(W2T(aTitle))
	{
		pThis->SetWindowText(W2T(aTitle));
	}
	else
	{
		// Use the AppName i.e. testembed as the title if we
		// do not get one from GetBrowserWindowTitle()
		//
		CString cs;
		cs.LoadString(AFX_IDS_APP_TITLE);
		pThis->SetWindowText(cs);
	}
}

void CBrowserFrame::BrowserFrameGlueObj::SetBrowserFrameSize(PRInt32 aCX, PRInt32 aCY)
{
	METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)
	
	pThis->SetWindowPos(NULL, 0, 0, aCX, aCY, 
				SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER
			);
}

void CBrowserFrame::BrowserFrameGlueObj::GetBrowserFrameSize(PRInt32 *aCX, PRInt32 *aCY)
{
	METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)

	RECT wndRect;
	pThis->GetWindowRect(&wndRect);

	if (aCX)
		*aCX = wndRect.right - wndRect.left;

	if (aCY)
		*aCY = wndRect.bottom - wndRect.top;
}

void CBrowserFrame::BrowserFrameGlueObj::SetBrowserFramePosition(PRInt32 aX, PRInt32 aY)
{
	METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)	

	pThis->SetWindowPos(NULL, aX, aY, 0, 0, 
				SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
}

void CBrowserFrame::BrowserFrameGlueObj::GetBrowserFramePosition(PRInt32 *aX, PRInt32 *aY)
{
	METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)

	RECT wndRect;
	pThis->GetWindowRect(&wndRect);

	if (aX)
		*aX = wndRect.left;

	if (aY)
		*aY = wndRect.top;
}

void CBrowserFrame::BrowserFrameGlueObj::GetBrowserFramePositionAndSize(PRInt32 *aX, PRInt32 *aY, PRInt32 *aCX, PRInt32 *aCY)
{
	METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)

	RECT wndRect;
	pThis->GetWindowRect(&wndRect);

	if (aX)
		*aX = wndRect.left;

	if (aY)
		*aY = wndRect.top;

	if (aCX)
		*aCX = wndRect.right - wndRect.left;

	if (aCY)
		*aCY = wndRect.bottom - wndRect.top;
}

void CBrowserFrame::BrowserFrameGlueObj::SetBrowserFramePositionAndSize(PRInt32 aX, PRInt32 aY, PRInt32 aCX, PRInt32 aCY, PRBool fRepaint)
{
	METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)

	pThis->SetWindowPos(NULL, aX, aY, aCX, aCY, 
				SWP_NOACTIVATE | SWP_NOZORDER);
}

void CBrowserFrame::BrowserFrameGlueObj::SetFocus()
{
	METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)

	pThis->SetFocus();
}

void CBrowserFrame::BrowserFrameGlueObj::FocusAvailable(PRBool *aFocusAvail)
{
	METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)

	HWND focusWnd = GetFocus()->m_hWnd;

	if ((focusWnd == pThis->m_hWnd) || ::IsChild(pThis->m_hWnd, focusWnd))
		*aFocusAvail = PR_TRUE;
	else
		*aFocusAvail = PR_FALSE;
}

void CBrowserFrame::BrowserFrameGlueObj::ShowBrowserFrame(PRBool aShow)
{
	METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)

	if(aShow)
	{
		pThis->ShowWindow(SW_SHOW);
		pThis->SetActiveWindow();
		pThis->UpdateWindow();
	}
	else
	{
		pThis->ShowWindow(SW_HIDE);
	}
}

void CBrowserFrame::BrowserFrameGlueObj::GetBrowserFrameVisibility(PRBool *aVisible)
{
	METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)

	// Is the current BrowserFrame the active one?
	if (GetActiveWindow()->m_hWnd != pThis->m_hWnd)
	{
		*aVisible = PR_FALSE;
		return;
	}

	// We're the active one
	//Return FALSE if we're minimized
	WINDOWPLACEMENT wpl;
	pThis->GetWindowPlacement(&wpl);

	if ((wpl.showCmd == SW_RESTORE) || (wpl.showCmd == SW_MAXIMIZE))
		*aVisible = PR_TRUE;
	else
		*aVisible = PR_FALSE;
}

PRBool CBrowserFrame::BrowserFrameGlueObj::CreateNewBrowserFrame(PRUint32 chromeMask, 
							PRInt32 x, PRInt32 y, 
							PRInt32 cx, PRInt32 cy,
							nsIWebBrowser** aWebBrowser)
{
   NS_ENSURE_ARG_POINTER(aWebBrowser);

   *aWebBrowser = nsnull;

	CTestEmbedApp *pApp = (CTestEmbedApp *)AfxGetApp();
	if(!pApp)
		return PR_FALSE;

	// Note that we're calling with the last param set to "false" i.e.
	// this instructs not to show the frame window
	// This is mainly needed when the window size is specified in the window.open()
	// JS call. In those cases Gecko calls us to create the browser with a default
	// size (all are -1) and then it calls the SizeBrowserTo() method to set
	// the proper window size. If this window were to be visible then you'll see
	// the window size changes on the screen causing an unappealing flicker
	//

	CBrowserFrame* pFrm = pApp->CreateNewBrowserFrame(chromeMask, x, y, cx, cy, PR_FALSE);
    if(!pFrm)
		return PR_FALSE;

	// At this stage we have a new CBrowserFrame and a new CBrowserView
	// objects. The CBrowserView also would have an embedded browser
	// object created. Get the mWebBrowser member from the CBrowserView
	// and return it. (See CBrowserView's CreateBrowser() on how the
	// embedded browser gets created and how it's mWebBrowser member
	// gets initialized)

	NS_IF_ADDREF(*aWebBrowser = pFrm->m_wndBrowserView.mWebBrowser);

	return PR_TRUE;
}

void CBrowserFrame::BrowserFrameGlueObj::DestroyBrowserFrame()
{
	METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)

	pThis->PostMessage(WM_CLOSE);
}

void CBrowserFrame::BrowserFrameGlueObj::ShowContextMenu(PRUint32 aContextFlags, nsIDOMNode *aNode)
{
	METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)

	UINT nIDResource = IDR_CTXMENU_DOCUMENT;

	if(aContextFlags & nsIContextMenuListener::CONTEXT_DOCUMENT)
		nIDResource = IDR_CTXMENU_DOCUMENT;
	else if(aContextFlags & nsIContextMenuListener::CONTEXT_TEXT)		
		nIDResource = IDR_CTXMENU_TEXT;
	else if(aContextFlags & nsIContextMenuListener::CONTEXT_LINK)
	{
		nIDResource = IDR_CTXMENU_LINK;

		// Since we handle all the browser menu/toolbar commands
		// in the View, we basically setup the Link's URL in the
		// BrowserView object. When a menu selection in the context
		// menu is made, the appropriate command handler in the
		// BrowserView will be invoked and the value of the URL
		// will be accesible in the view
		
		// Reset the value from the last invocation
		// (A new value will be set after we determine it below)
		//
		nsAutoString strUrlUcs2;
		pThis->m_wndBrowserView.SetCtxMenuLinkUrl(strUrlUcs2);

		// Get the URL from the link. This is two step process
		// 1. We first get the nsIDOMHTMLAnchorElement
		// 2. We then get the URL associated with the link
		nsresult rv = NS_OK;
		nsCOMPtr<nsIDOMHTMLAnchorElement> linkElement(do_QueryInterface(aNode, &rv));
		if(NS_FAILED(rv))
			return;

		rv = linkElement->GetHref(strUrlUcs2);
		if(NS_FAILED(rv))
			return;

		// Update the view with the new LinkUrl
		// Note that this string is in UCS2 format
		pThis->m_wndBrowserView.SetCtxMenuLinkUrl(strUrlUcs2);
	}
	else if(aContextFlags & nsIContextMenuListener::CONTEXT_IMAGE)
	{
		nIDResource = IDR_CTXMENU_IMAGE;

		nsAutoString strImgSrcUcs2;
		pThis->m_wndBrowserView.SetCtxMenuImageSrc(strImgSrcUcs2); // Clear it

		// Get the IMG SRC
		nsresult rv = NS_OK;
		nsCOMPtr<nsIDOMHTMLImageElement> imgElement(do_QueryInterface(aNode, &rv));
		if(NS_FAILED(rv))
			return;

		rv = imgElement->GetSrc(strImgSrcUcs2);
		if(NS_FAILED(rv))
			return;

		pThis->m_wndBrowserView.SetCtxMenuImageSrc(strImgSrcUcs2); // Set the new Img Src
	}

	CMenu ctxMenu;
	if(ctxMenu.LoadMenu(nIDResource))
	{
		POINT cursorPos;
		GetCursorPos(&cursorPos);

		(ctxMenu.GetSubMenu(0))->TrackPopupMenu(TPM_LEFTALIGN, cursorPos.x, cursorPos.y, pThis);
	}
}

HWND CBrowserFrame::BrowserFrameGlueObj::GetBrowserFrameNativeWnd()
{
	METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)
	return pThis->m_hWnd;
}
