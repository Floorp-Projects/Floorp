/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Conrad Carlen <ccarlen@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "CWindowCreator.h"

#include "nsIWindowWatcher.h"
#include "nsIServiceManagerUtils.h"
#include "nsIWebBrowserSetup.h"
#include "nsIPrefBranch.h"

#include "CBrowserShell.h"
#include "CBrowserWindow.h"
#include "CPrintAttachment.h"
#include "ApplIDs.h"

// ---------------------------------------------------------------------------
//  CWindowCreator
// ---------------------------------------------------------------------------

NS_IMPL_ISUPPORTS2(CWindowCreator, nsIWindowCreator, nsIWindowCreator2);

CWindowCreator::CWindowCreator()
{
}

CWindowCreator::~CWindowCreator()
{
}

NS_IMETHODIMP CWindowCreator::CreateChromeWindow(nsIWebBrowserChrome *aParent,
                                                 PRUint32 aChromeFlags,
                                                 nsIWebBrowserChrome **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = 0;

    // we're ignoring aParent,
    // but since windows on the Mac don't have parents anyway...
    try {
        LWindow *theWindow = CreateWindowInternal(aChromeFlags, PR_FALSE, -1, -1);
        CBrowserShell *browser = dynamic_cast<CBrowserShell*>(theWindow->FindPaneByID(CBrowserShell::paneID_MainBrowser));
        ThrowIfNil_(browser);
        browser->GetWebBrowserChrome(_retval);
    } catch(...) {
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

NS_IMETHODIMP CWindowCreator::CreateChromeWindow2(nsIWebBrowserChrome *parent,
                                                  PRUint32 chromeFlags, PRUint32 contextFlags,
                                                  nsIWebBrowserChrome **_retval)
{
    if (contextFlags & nsIWindowCreator2::PARENT_IS_LOADING_OR_RUNNING_TIMEOUT) {
        nsCOMPtr<nsIPrefBranch> prefs(do_GetService("@mozilla.org/preferences-service;1"));
        if (prefs) {
            PRBool showBlocker = PR_TRUE;
            prefs->GetBoolPref("browser.popups.showPopupBlocker", &showBlocker);
            if (showBlocker) {
                short itemHit;
                AlertStdAlertParamRec pb;

                LStr255 msgString(STRx_StdAlertStrings, str_OpeningPopupWindow);
                LStr255 explainString(STRx_StdAlertStrings, str_OpeningPopupWindowExp);
                LStr255 defaultButtonText(STRx_StdButtonTitles, str_DenyAll);
                LStr255 cancelButtonText(STRx_StdButtonTitles, str_Allow);
                
                pb.movable = false;
                pb.helpButton = false;
                pb.filterProc = nil;
                pb.defaultText = defaultButtonText;
                pb.cancelText = cancelButtonText;
                pb.otherText = nil;
                pb.defaultButton = kStdOkItemIndex;
                pb.cancelButton = kStdCancelItemIndex;
                pb.position = kWindowAlertPositionParentWindowScreen;

                ::StandardAlert(kAlertStopAlert, msgString, explainString, &pb, &itemHit);
                // This a one-time (for the life of prefs.js) alert
                prefs->SetBoolPref("browser.popups.showPopupBlocker", PR_FALSE);
                if (itemHit == kAlertStdAlertOKButton) {
                    // Also, if these prefs are set, the DOM itself will prevent future
                    // popups from being opened and our window creator won't even get
                    // called. If you wanted to filter each request, don't set these
                    // prefs. For this purpose, it's what we want, though.
                    prefs->SetBoolPref("dom.disable_open_during_load", PR_TRUE);
                    prefs->SetIntPref("dom.disable_open_click_delay", 1000);
                    return NS_ERROR_FAILURE;
                }
            }
        }
    }
    return CreateChromeWindow(parent, chromeFlags, _retval);
}



/*
   InitializeWindowCreator creates and hands off an object with a callback
   to a window creation function. This will be used by Gecko C++ code
   (never JS) to create new windows when no previous window is handy
   to begin with. This is done in a few exceptional cases, like PSM code.
   Failure to set this callback will only disable the ability to create
   new windows under these circumstances.
*/

nsresult CWindowCreator::Initialize()
{
    // Create a CWindowCreator and give it to the WindowWatcher service
    // The WindowWatcher service will own it so we don't keep a ref.
    CWindowCreator *windowCreator = new CWindowCreator;
    if (!windowCreator) return NS_ERROR_FAILURE;
    
    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
    if (!wwatch) return NS_ERROR_FAILURE;
    return wwatch->SetWindowCreator(windowCreator);
}

LWindow* CWindowCreator::CreateWindowInternal(PRUint32 inChromeFlags,
                                              PRBool enablePrinting,
                                              PRInt32 width, PRInt32 height)
{
    const SInt16 kStatusBarHeight = 16;
    const SDimension16 kMinimumWindowDimension = { 300, 150 };
    const SInt16 kStdSizeVertMargin = 100;
    
    LWindow         *theWindow;
    PRUint32        chromeFlags;
    
    if (inChromeFlags == nsIWebBrowserChrome::CHROME_DEFAULT)
        chromeFlags = nsIWebBrowserChrome::CHROME_WINDOW_RESIZE |
                      nsIWebBrowserChrome::CHROME_WINDOW_CLOSE |
                      nsIWebBrowserChrome::CHROME_TOOLBAR |
                      nsIWebBrowserChrome::CHROME_STATUSBAR;
    else
        chromeFlags = inChromeFlags;
    
    // Bounds - Set to an arbitrary rect - we'll size it after all the subviews are in.
    Rect globalBounds;
    globalBounds.left = 4;
    globalBounds.top = 42;
    globalBounds.right = globalBounds.left + 600;
    globalBounds.bottom = globalBounds.top + 400;
    
    // ProcID and attributes
    short windowDefProc;
    UInt32 windowAttrs = (windAttr_Enabled | windAttr_Targetable);
    if (chromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_DIALOG)
    {
        if (chromeFlags & nsIWebBrowserChrome::CHROME_TITLEBAR)
        {
            windowDefProc = kWindowMovableModalDialogProc;
            windowAttrs |= windAttr_TitleBar;
        }
        else
            windowDefProc = kWindowModalDialogProc;            
    }
    else
    {
        if (chromeFlags & nsIWebBrowserChrome::CHROME_WINDOW_RESIZE)
        {
            windowDefProc = kWindowGrowDocumentProc;
            windowAttrs |= windAttr_Resizable;
            windowAttrs |= windAttr_Zoomable;
        }
        else
            windowDefProc = kWindowDocumentProc;            
        
        if (chromeFlags & nsIWebBrowserChrome::CHROME_WINDOW_CLOSE)
            windowAttrs |= windAttr_CloseBox;
    }

    if (chromeFlags & nsIWebBrowserChrome::CHROME_MODAL)
        windowAttrs |= windAttr_Modal;
    else
        windowAttrs |= windAttr_Regular;    
    
    theWindow = new CBrowserWindow(LCommander::GetTopCommander(), globalBounds, "\p", windowDefProc, windowAttrs, window_InFront);
    ThrowIfNil_(theWindow);
    
    if (windowAttrs & windAttr_Resizable)
    {
        Rect stdBounds, minMaxBounds;
        SDimension16 stdSize;
        
        theWindow->CalcStandardBounds(stdBounds);
        stdSize.width = stdBounds.right - stdBounds.left;
        stdSize.height = stdBounds.bottom - stdBounds.top;
        stdSize.width -= kStdSizeVertMargin; // Leave a vertical strip of desktop exposed
        theWindow->SetStandardSize(stdSize);
        minMaxBounds.left = kMinimumWindowDimension.width;
        minMaxBounds.top = kMinimumWindowDimension.height;
        
        Rect	deskRect;
        ::GetRegionBounds(::GetGrayRgn(), &deskRect);
        minMaxBounds.left = kMinimumWindowDimension.width;
        minMaxBounds.top = kMinimumWindowDimension.height;
        minMaxBounds.right = deskRect.right - deskRect.left;
        minMaxBounds.bottom = deskRect.bottom - deskRect.top;
        theWindow->SetMinMaxSize(minMaxBounds);
    }

    SDimension16 windowSize, toolBarSize;
    theWindow->GetFrameSize(windowSize);

    if (chromeFlags & nsIWebBrowserChrome::CHROME_TOOLBAR)
    {
        LView::SetDefaultView(theWindow);
        LCommander::SetDefaultCommander(theWindow);
        LAttachable::SetDefaultAttachable(nil);

        LView *toolBarView = static_cast<LView*>(UReanimator::ReadObjects(ResType_PPob, 131));
        ThrowIfNil_(toolBarView);
                    
        toolBarView->GetFrameSize(toolBarSize);
        toolBarView->PlaceInSuperFrameAt(0, 0, false);
        toolBarSize.width = windowSize.width;
        toolBarView->ResizeFrameTo(toolBarSize.width, toolBarSize.height, false);
    }

    SPaneInfo aPaneInfo;
    SViewInfo aViewInfo;
    
    aPaneInfo.paneID = CBrowserShell::paneID_MainBrowser;
    aPaneInfo.width = windowSize.width;
    aPaneInfo.height = windowSize.height;
    if (chromeFlags & nsIWebBrowserChrome::CHROME_TOOLBAR)
        aPaneInfo.height -= toolBarSize.height;
    if (chromeFlags & nsIWebBrowserChrome::CHROME_STATUSBAR)
        aPaneInfo.height -= kStatusBarHeight - 1;
    aPaneInfo.visible = true;
    aPaneInfo.enabled = true;
    aPaneInfo.bindings.left = true;
    aPaneInfo.bindings.top = true;
    aPaneInfo.bindings.right = true;
    aPaneInfo.bindings.bottom = true;
    aPaneInfo.left = 0;
    aPaneInfo.top = (chromeFlags & nsIWebBrowserChrome::CHROME_TOOLBAR) ? toolBarSize.height : 0;
    aPaneInfo.userCon = 0;
    aPaneInfo.superView = theWindow;
    
    aViewInfo.imageSize.width = 0;
    aViewInfo.imageSize.height = 0;
    aViewInfo.scrollPos.h = aViewInfo.scrollPos.v = 0;
    aViewInfo.scrollUnit.h = aViewInfo.scrollUnit.v = 1;
    aViewInfo.reconcileOverhang = 0;
    
    CBrowserShell *aShell = new CBrowserShell(aPaneInfo, aViewInfo, chromeFlags, PR_TRUE);
    ThrowIfNil_(aShell);
    aShell->AddAttachments();
    aShell->PutInside(theWindow, false);
    
    if (chromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME)
    {
        nsCOMPtr<nsIWebBrowser> browser;
        aShell->GetWebBrowser(getter_AddRefs(browser));
        nsCOMPtr<nsIWebBrowserSetup> setup(do_QueryInterface(browser));
        if (setup)
          setup->SetProperty(nsIWebBrowserSetup::SETUP_IS_CHROME_WRAPPER, PR_TRUE);
    }
       
    if (chromeFlags & nsIWebBrowserChrome::CHROME_STATUSBAR)
    {
        LView::SetDefaultView(theWindow);
        LCommander::SetDefaultCommander(theWindow);
        LAttachable::SetDefaultAttachable(nil);

        LView *statusView = static_cast<LView*>(UReanimator::ReadObjects(ResType_PPob, 130));
        ThrowIfNil_(statusView);
        
        statusView->PlaceInSuperFrameAt(0, windowSize.height - kStatusBarHeight + 1, false);
        statusView->ResizeFrameTo(windowSize.width - 15, kStatusBarHeight, false);
    }        

    if (enablePrinting)
    {
        CPrintAttachment *printAttachment = new CPrintAttachment(CBrowserShell::paneID_MainBrowser);
        ThrowIfNil_(printAttachment);
        theWindow->AddAttachment(printAttachment);
    }
        
    // Now the window is constructed...
    theWindow->FinishCreate();        

    Rect    theBounds;
    theWindow->GetGlobalBounds(theBounds);
    if (width == -1)
        width = theBounds.right - theBounds.left;
    if (height == -1)
        height = theBounds.bottom - theBounds.top;
        
    theWindow->ResizeWindowTo(width, height);

    return theWindow;
}
