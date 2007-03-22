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
#include "MfcEmbed.h"
#include "BrowserFrm.h"
#include "Dialogs.h"

/////////////////////////////////////////////////////////////////////////////
// IBrowserFrameGlue implementation

void CBrowserFrame::BrowserFrameGlueObj::UpdateStatusBarText(const PRUnichar *aMessage)
{
    METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)
    USES_CONVERSION;
    pThis->m_wndStatusBar.SetPaneText(0, aMessage ? W2CT(aMessage) : _T(""));
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
        USES_CONVERSION;
        nsEmbedCString uriString;
        aLocation->GetSpec(uriString);
        pThis->m_wndUrlBar.SetCurrentURL(A2CT(uriString.get()));
    }
}

void CBrowserFrame::BrowserFrameGlueObj::GetBrowserFrameTitle(PRUnichar **aTitle)
{
    METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)

    CString title;
    pThis->GetWindowText(title);

    if(!title.IsEmpty())
    {
        USES_CONVERSION;
        nsEmbedString nsTitle;
        nsTitle.Assign(T2CW(title.GetBuffer(0)));
        *aTitle = NS_StringCloneData(nsTitle);
    }
}

void CBrowserFrame::BrowserFrameGlueObj::SetBrowserFrameTitle(const PRUnichar *aTitle)
{
    METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)

    CString title;
    if (aTitle)
    {
        USES_CONVERSION;
        title = W2CT(aTitle);
    }
    else
    {
        // Use the AppName i.e. mfcembed as the title if we
        // do not get one from GetBrowserWindowTitle()
        //
        title.LoadString(AFX_IDS_APP_TITLE);
    }
    pThis->SetWindowText(title);
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

    CMfcEmbedApp *pApp = (CMfcEmbedApp *)AfxGetApp();
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

#define GOTO_BUILD_CTX_MENU { bContentHasFrames = FALSE; goto BUILD_CTX_MENU; }

void CBrowserFrame::BrowserFrameGlueObj::ShowContextMenu(PRUint32 aContextFlags, nsIContextMenuInfo *aInfo)
{
    METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)

    BOOL bContentHasFrames = FALSE;
    UINT nIDResource = IDR_CTXMENU_DOCUMENT;

    // Reset the values from the last invocation
    // Clear image src & link url
    nsEmbedString empty;
    pThis->m_wndBrowserView.SetCtxMenuImageSrc(empty);  
    pThis->m_wndBrowserView.SetCtxMenuLinkUrl(empty);
    pThis->m_wndBrowserView.SetCurrentFrameURL(empty);

    // Display the Editor context menu if the we're inside
    // an EditorFrm which is hosting an editing session
    //
    if(pThis->GetEditable())
    {
        nIDResource = IDR_CTXMENU_EDITOR;

        GOTO_BUILD_CTX_MENU;
    }

    if(aContextFlags & nsIContextMenuListener2::CONTEXT_DOCUMENT)
    {
        nIDResource = IDR_CTXMENU_DOCUMENT;

        // Background image?
        if (aContextFlags & nsIContextMenuListener2::CONTEXT_BACKGROUND_IMAGE)
        {
            // Get the IMG SRC
            nsCOMPtr<nsIURI> imgURI;
            aInfo->GetBackgroundImageSrc(getter_AddRefs(imgURI));
            if (!imgURI)
                return; 
            nsEmbedCString uri;
            imgURI->GetSpec(uri);

            nsEmbedString uri2;
            NS_CStringToUTF16(uri, NS_CSTRING_ENCODING_UTF8, uri2);

            pThis->m_wndBrowserView.SetCtxMenuImageSrc(uri2); // Set the new Img Src
        }
    }
    else if(aContextFlags & nsIContextMenuListener2::CONTEXT_TEXT)        
        nIDResource = IDR_CTXMENU_TEXT;
    else if(aContextFlags & nsIContextMenuListener2::CONTEXT_LINK)
    {
        nIDResource = IDR_CTXMENU_LINK;

        // Since we handle all the browser menu/toolbar commands
        // in the View, we basically setup the Link's URL in the
        // BrowserView object. When a menu selection in the context
        // menu is made, the appropriate command handler in the
        // BrowserView will be invoked and the value of the URL
        // will be accesible in the view
        
        nsEmbedString strUrlUcs2;
        nsresult rv = aInfo->GetAssociatedLink(strUrlUcs2);
        if(NS_FAILED(rv))
            return;

        // Update the view with the new LinkUrl
        // Note that this string is in UCS2 format
        pThis->m_wndBrowserView.SetCtxMenuLinkUrl(strUrlUcs2);

        // Test if there is an image URL as well
        nsCOMPtr<nsIURI> imgURI;
        aInfo->GetImageSrc(getter_AddRefs(imgURI));
        if(imgURI)
        {
            nsEmbedCString strImgSrcUtf8;
            imgURI->GetSpec(strImgSrcUtf8);
            if(strImgSrcUtf8.Length() != 0)
            {
                // Set the new Img Src
                nsEmbedString strImgSrc;
                NS_CStringToUTF16(strImgSrcUtf8, NS_CSTRING_ENCODING_UTF8, strImgSrc);
                pThis->m_wndBrowserView.SetCtxMenuImageSrc(strImgSrc);
            }
        }
    }
    else if(aContextFlags & nsIContextMenuListener2::CONTEXT_IMAGE)
    {
        nIDResource = IDR_CTXMENU_IMAGE;

        // Get the IMG SRC
        nsCOMPtr<nsIURI> imgURI;
        aInfo->GetImageSrc(getter_AddRefs(imgURI));
        if(!imgURI)
            return;
        nsEmbedCString strImgSrcUtf8;
        imgURI->GetSpec(strImgSrcUtf8);
        if(strImgSrcUtf8.Length() == 0)
            return;

        // Set the new Img Src
        nsEmbedString strImgSrc;
        NS_CStringToUTF16(strImgSrcUtf8, NS_CSTRING_ENCODING_UTF8, strImgSrc);
        pThis->m_wndBrowserView.SetCtxMenuImageSrc(strImgSrc);
    }

    // Determine if we need to add the Frame related context menu items
    // such as "View Frame Source" etc.
    //
    if(pThis->m_wndBrowserView.ViewContentContainsFrames())
    {
        bContentHasFrames = TRUE;

        //Determine the current Frame URL
        //
        nsresult rv = NS_OK;
        nsCOMPtr<nsIDOMNode> node;
        aInfo->GetTargetNode(getter_AddRefs(node));
        if(!node)
            GOTO_BUILD_CTX_MENU;

        nsCOMPtr<nsIDOMDocument> domDoc;
        rv = node->GetOwnerDocument(getter_AddRefs(domDoc));
        if(NS_FAILED(rv))
            GOTO_BUILD_CTX_MENU;

        nsCOMPtr<nsIDOMHTMLDocument> htmlDoc(do_QueryInterface(domDoc, &rv));
        if(NS_FAILED(rv))
            GOTO_BUILD_CTX_MENU;

        nsEmbedString strFrameURL;
        rv = htmlDoc->GetURL(strFrameURL);
        if(NS_FAILED(rv))
            GOTO_BUILD_CTX_MENU;

        pThis->m_wndBrowserView.SetCurrentFrameURL(strFrameURL); //Set it to the new URL
    }

BUILD_CTX_MENU:

    CMenu ctxMenu;
    if(ctxMenu.LoadMenu(nIDResource))
    {
        //Append the Frame related menu items if content has frames
        if(bContentHasFrames) 
        {
            CMenu* pCtxMenu = ctxMenu.GetSubMenu(0);
            if(pCtxMenu)
            {
                pCtxMenu->AppendMenu(MF_SEPARATOR);

                CString strMenuItem;
                strMenuItem.LoadString(IDS_VIEW_FRAME_SOURCE);
                pCtxMenu->AppendMenu(MF_STRING, ID_VIEW_FRAME_SOURCE, strMenuItem);

                strMenuItem.LoadString(IDS_OPEN_FRAME_IN_NEW_WINDOW);
                pCtxMenu->AppendMenu(MF_STRING, ID_OPEN_FRAME_IN_NEW_WINDOW, strMenuItem);
            }
        }

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

void CBrowserFrame::BrowserFrameGlueObj::UpdateSecurityStatus(PRInt32 aState)
{
    METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)

    pThis->UpdateSecurityStatus(aState);
}

void CBrowserFrame::BrowserFrameGlueObj::ShowTooltip(PRInt32 aXCoords, PRInt32 aYCoords, const PRUnichar *aTipText)
{
    METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)
    pThis->m_wndTooltip.SetTipText(CString(aTipText));
    pThis->m_wndTooltip.Show(&pThis->m_wndBrowserView, aXCoords, aYCoords);
}

void CBrowserFrame::BrowserFrameGlueObj::HideTooltip()
{
    METHOD_PROLOGUE(CBrowserFrame, BrowserFrameGlueObj)
    pThis->m_wndTooltip.Hide();
}
