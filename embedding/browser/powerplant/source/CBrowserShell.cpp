/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
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

#ifndef __CBrowserShell__
#include "CBrowserShell.h"
#endif

#include "nsCWebBrowser.h"
#include "nsIComponentManager.h"
#include "nsRepeater.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsIWebBrowserChrome.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWebProgressListener.h"
#include "nsIServiceManager.h"
#include "nsIClipboardCommands.h"
#include "nsIWalletService.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMLocation.h"
#include "nsIWebBrowserFind.h"
#include "nsIWebBrowserFocus.h"
#include "nsIWebBrowserPersist.h"
#include "nsIURI.h"
#include "nsWeakPtr.h"
#include "nsRect.h"
#include "nsReadableUtils.h"
#include "nsILocalFile.h"
#include "nsILocalFileMac.h"
#include "nsWeakReference.h"
#include "nsIChannel.h"
#include "nsIHistoryEntry.h"
#include "nsISHEntry.h"
#include "nsISHistory.h"
#include "nsIWebBrowserPrint.h"
#include "nsIMacTextInputEventSink.h"
#include "nsCRT.h"
#include "nsNetUtil.h"
#include "gfxIImageFrame.h"
#include "nsIImage.h"

// Local
#include "ApplIDs.h"
#include "CBrowserMsgDefs.h"
#include "CBrowserChrome.h"
#include "CWebBrowserCMAttachment.h"
#include "UMacUnicode.h"
#include "CHeaderSniffer.h"
#include "UCustomNavServicesDialogs.h"

// PowerPlant
#include <UModalDialogs.h>
#include <LStream.h>
#include <UNavServicesDialogs.h>
#include <LEditText.h>
#include <LCheckBox.h>
#include <UEventMgr.h>

// ToolBox
#include <InternetConfig.h>

const nsCString CBrowserShell::kEmptyCString;
nsCOMPtr<nsIDragHelperService> CBrowserShell::sDragHelper;

//*****************************************************************************
//***    CBrowserShellProgressListener
//*****************************************************************************

class CBrowserShellProgressListener : public nsIWebProgressListener,
                                      public nsSupportsWeakReference
{
  public:
                                CBrowserShellProgressListener(CBrowserShell* itsOwner);
                                
    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBPROGRESSLISTENER
    
    void                        SetOwner(CBrowserShell* itsOwner)
                                { mpOwner = itsOwner; }
                                
    PRBool                      GetIsLoading()
                                { return mLoading; }
                                
  protected:
    virtual                     ~CBrowserShellProgressListener();
    
  protected:
    CBrowserShell               *mpOwner;
    PRBool                      mLoading;
    PRBool                      mUseRealProgFlag;
    PRInt32                     mFinishedRequests, mTotalRequests;                       
};

NS_IMPL_ISUPPORTS2(CBrowserShellProgressListener, nsIWebProgressListener, nsISupportsWeakReference)

CBrowserShellProgressListener::CBrowserShellProgressListener(CBrowserShell* itsOwner) :
    mpOwner(itsOwner),
    mLoading(PR_FALSE), mUseRealProgFlag(PR_FALSE),
    mFinishedRequests(0), mTotalRequests(0)
{
}

CBrowserShellProgressListener::~CBrowserShellProgressListener()
{
}

NS_IMETHODIMP CBrowserShellProgressListener::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, nsresult aStatus)
{
    NS_ENSURE_TRUE(mpOwner, NS_ERROR_NULL_POINTER);

    if (aStateFlags & STATE_START)
    {
        if (aStateFlags & STATE_IS_NETWORK)
        {
            MsgNetStartInfo startInfo(mpOwner);
            mpOwner->BroadcastMessage(msg_OnNetStartChange, &startInfo);            
            mLoading = true;
            
            // Init progress vars
            mUseRealProgFlag = false;
            mTotalRequests = 0;
            mFinishedRequests = 0;
        }
        if (aStateFlags & STATE_IS_REQUEST)
            mTotalRequests++;
    }
    else if (aStateFlags & STATE_STOP)
    {
        if (aStateFlags & STATE_IS_REQUEST)
        {
            mFinishedRequests += 1;
        
            if (!mUseRealProgFlag)
            {
                MsgOnProgressChangeInfo progInfo(mpOwner, mFinishedRequests, mTotalRequests);    
                mpOwner->BroadcastMessage(msg_OnProgressChange, &progInfo);
            }
        }
        if (aStateFlags & STATE_IS_NETWORK)
        {
            MsgNetStopInfo stopInfo(mpOwner);
            mpOwner->BroadcastMessage(msg_OnNetStopChange, &stopInfo);
            mLoading = false;
        }
    }
    else if (aStateFlags & STATE_TRANSFERRING)
    {

        if (aStateFlags & STATE_IS_DOCUMENT)
        {
            nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
            NS_ENSURE_TRUE(channel, NS_ERROR_FAILURE);
            nsCAutoString contentType;
            channel->GetContentType(contentType);
            if (contentType.Equals(NS_LITERAL_CSTRING("text/html")))
                mUseRealProgFlag = true;
        }
        
        if (aStateFlags & STATE_IS_REQUEST)
        {
            if (!mUseRealProgFlag)
            {
                MsgOnProgressChangeInfo progInfo(mpOwner, mFinishedRequests, mTotalRequests);    
                mpOwner->BroadcastMessage(msg_OnProgressChange, &progInfo);
            }
        }
    }
    return NS_OK;
}

NS_IMETHODIMP CBrowserShellProgressListener::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
    NS_ENSURE_TRUE(mpOwner, NS_ERROR_NULL_POINTER);

    if (!mUseRealProgFlag)
        return NS_OK;
    
    MsgOnProgressChangeInfo progInfo(mpOwner, aCurTotalProgress, aMaxTotalProgress);    
    mpOwner->BroadcastMessage(msg_OnProgressChange, &progInfo);
    
    return NS_OK;
}

NS_IMETHODIMP CBrowserShellProgressListener::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location)
{
    NS_ENSURE_TRUE(mpOwner, NS_ERROR_NULL_POINTER);
    
    nsCAutoString spec;
    
	if (location)
		location->GetSpec(spec);
		
	MsgLocationChangeInfo info(mpOwner, spec.get());
    mpOwner->BroadcastMessage(msg_OnLocationChange, &info);
    
    return NS_OK;
}

NS_IMETHODIMP CBrowserShellProgressListener::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
    NS_ENSURE_TRUE(mpOwner, NS_ERROR_NULL_POINTER);

    MsgStatusChangeInfo info(mpOwner, aStatus, aMessage);
    mpOwner->BroadcastMessage(msg_OnStatusChange, &info);

    return NS_OK;
}

NS_IMETHODIMP CBrowserShellProgressListener::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
    NS_ENSURE_TRUE(mpOwner, NS_ERROR_NULL_POINTER);

    MsgSecurityChangeInfo info(mpOwner, state);
    mpOwner->BroadcastMessage(msg_OnSecurityChange, &info);
    
    return NS_OK;
}


//*****************************************************************************
//***    CBrowserShell: constructors/destructor
//*****************************************************************************

CBrowserShell::CBrowserShell() :
    LDropArea(GetMacWindow()),
    mChromeFlags(nsIWebBrowserChrome::CHROME_DEFAULT), mIsMainContent(true),
    mContextMenuFlags(nsIContextMenuListener2::CONTEXT_NONE)
{
	nsresult rv = CommonConstruct();
	if (rv != NS_OK)
	   Throw_Err(NS_ERROR_GET_CODE(rv));   // If this fails, there's no reason to live anymore :(
}


CBrowserShell::CBrowserShell(const SPaneInfo	&inPaneInfo,
						  	 const SViewInfo	&inViewInfo,
						  	 const UInt32       inChromeFlags,
						  	 const Boolean      inIsMainContent) :
    LView(inPaneInfo, inViewInfo), LDropArea(GetMacWindow()),
    mChromeFlags(inChromeFlags), mIsMainContent(inIsMainContent),
    mContextMenuFlags(nsIContextMenuListener2::CONTEXT_NONE)
{
	nsresult rv = CommonConstruct();
	if (rv != NS_OK)
	   Throw_Err(NS_ERROR_GET_CODE(rv));   // If this fails, there's no reason to live anymore :(
}


CBrowserShell::CBrowserShell(LStream*	inStream) :
	LView(inStream), LDropArea(GetMacWindow()),
    mContextMenuFlags(nsIContextMenuListener2::CONTEXT_NONE)
{
	*inStream >> mChromeFlags;
	*inStream >> mIsMainContent;
	
	nsresult rv = CommonConstruct();
	if (rv != NS_OK)
	   Throw_Err(NS_ERROR_GET_CODE(rv));   // If this fails, there's no reason to live anymore :(
}


CBrowserShell::~CBrowserShell()
{
    if (mWebBrowser)
        mWebBrowser->SetContainerWindow(nsnull);
    
    if (mChrome)
    {
        mChrome->SetBrowserShell(nsnull);
        NS_RELEASE(mChrome);
    }
    if (mProgressListener)
    {
        mProgressListener->SetOwner(nsnull);
        NS_RELEASE(mProgressListener);
    }
}


NS_IMETHODIMP CBrowserShell::CommonConstruct()
{
    nsresult  rv;

    mWebBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(mWebBrowser));
    NS_ENSURE_TRUE(baseWin, NS_ERROR_FAILURE);
    mWebBrowserAsBaseWin = baseWin;

    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mWebBrowser));
    NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);
    mWebBrowserAsWebNav = webNav;
    
    mChrome = new CBrowserChrome(this, mChromeFlags, mIsMainContent);
    NS_ENSURE_TRUE(mChrome, NS_ERROR_FAILURE);
    NS_ADDREF(mChrome);
    AddListener(mChrome);
    mWebBrowser->SetContainerWindow(mChrome);
        
    mProgressListener = new CBrowserShellProgressListener(this);
    NS_ENSURE_TRUE(mProgressListener, NS_ERROR_FAILURE);
    NS_ADDREF(mProgressListener);

    return NS_OK;
}


//*****************************************************************************
//***    CBrowserShell: LPane overrides
//*****************************************************************************

void CBrowserShell::FinishCreateSelf()
{
	FocusDraw();

	Rect portFrame;
	CalcPortFrameRect(portFrame);
	nsRect   r(portFrame.left, portFrame.top, portFrame.right - portFrame.left, portFrame.bottom - portFrame.top);
	
	nsresult rv;
	
    mWebBrowserAsBaseWin->InitWindow(GetMacWindow(), nsnull, r.x, r.y, r.width, r.height);
    mWebBrowserAsBaseWin->Create();
    mEventSink = do_GetInterface(mWebBrowser);
    ThrowIfNil_(mEventSink);
        
    // Hook up our progress listener
    nsWeakPtr weakling(do_GetWeakReference((nsIWebProgressListener *)mProgressListener));
    rv = mWebBrowser->AddWebBrowserListener(weakling, NS_GET_IID(nsIWebProgressListener));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Call to AddWebBrowserListener failed");
      
    AdjustFrame();   
	StartRepeating();
}


void CBrowserShell::ResizeFrameBy(SInt16	inWidthDelta,
                				  SInt16	inHeightDelta,
                				  Boolean	inRefresh)
{
	LView::ResizeFrameBy(inWidthDelta, inHeightDelta, inRefresh);
	AdjustFrame();
}


void CBrowserShell::MoveBy(SInt32	inHorizDelta,
				           SInt32	inVertDelta,
						   Boolean	inRefresh)
{
	LView::MoveBy(inHorizDelta, inVertDelta, inRefresh);
	AdjustFrame();
}


void CBrowserShell::ActivateSelf()
{
    EventRecord osEvent;
    osEvent.what = activateEvt;
    osEvent.modifiers = activeFlag;
    PRBool handled = PR_FALSE;
    mEventSink->DispatchEvent(&osEvent, &handled);
}

void CBrowserShell::DeactivateSelf()
{
    EventRecord osEvent;
    osEvent.what = activateEvt;
    osEvent.modifiers = 0;
    PRBool handled = PR_FALSE;
    mEventSink->DispatchEvent(&osEvent, &handled);
}

void CBrowserShell::ShowSelf()
{
    mWebBrowserAsBaseWin->SetVisibility(PR_TRUE);
}


void CBrowserShell::DrawSelf()
{
    EventRecord osEvent;
    osEvent.what = updateEvt;
    PRBool handled = PR_FALSE;
    mEventSink->DispatchEvent(&osEvent, &handled);
}

	
void CBrowserShell::ClickSelf(const SMouseDownEvent	&inMouseDown)
{
	if (!IsTarget())
		SwitchTarget(this);

	FocusDraw();
  PRBool handled = PR_FALSE;
	mEventSink->DispatchEvent(&const_cast<EventRecord&>(inMouseDown.macEvent), &handled);
}


void CBrowserShell::EventMouseUp(const EventRecord	&inMacEvent)
{
    FocusDraw();
    PRBool handled = PR_FALSE;
    mEventSink->DispatchEvent(&const_cast<EventRecord&>(inMacEvent), &handled);
    
    LEventDispatcher *dispatcher = LEventDispatcher::GetCurrentEventDispatcher();
    if (dispatcher)
        dispatcher->UpdateMenus();
}

void CBrowserShell::AdjustMouseSelf(Point				inPortPt,
                                    const EventRecord&	inMacEvent,
                                    RgnHandle			outMouseRgn)
{
    Rect cursorRect = { inPortPt.h, inPortPt.v, inPortPt.h + 1, inPortPt.v + 1 };
    ::RectRgn(outMouseRgn, &cursorRect);
}

//*****************************************************************************
//***    CBrowserShell: LCommander overrides
//*****************************************************************************

void CBrowserShell::BeTarget()
{
    nsCOMPtr<nsIWebBrowserFocus>  focus(do_GetInterface(mWebBrowser));
    if (focus)
        focus->Activate();
}

void CBrowserShell::DontBeTarget()
{
    nsCOMPtr<nsIWebBrowserFocus>  focus(do_GetInterface(mWebBrowser));
    if (focus)
        focus->Deactivate();
}

Boolean CBrowserShell::HandleKeyPress(const EventRecord	&inKeyEvent)
{
	// set the QuickDraw origin
	FocusDraw();

	// dispatch the event
  PRBool handled = PR_FALSE;
	Boolean keyHandled = mEventSink->DispatchEvent(&const_cast<EventRecord&>(inKeyEvent), &handled);

	return keyHandled;
}

Boolean CBrowserShell::ObeyCommand(PP_PowerPlant::CommandT inCommand, void* ioParam)
{
	Boolean		cmdHandled = true;

    nsresult rv;
    nsCOMPtr<nsIClipboardCommands> clipCmd;

    switch (inCommand)
    {
        case cmd_Back:
            Back();
            break;
 
        case cmd_Forward:
            Forward();
            break;
            
        case cmd_Stop:
            Stop();
            break;
            
        case cmd_Reload:
            Reload();
            break;

        case cmd_SaveAs:
            rv = SaveDocument(eSaveFormatHTML);
            ThrowIfError_(rv);
            break;
                        
        case cmd_Cut:
            rv = GetClipboardHandler(getter_AddRefs(clipCmd));
            if (NS_SUCCEEDED(rv))
                clipCmd->CutSelection();
            break;

        case cmd_Copy:
            rv = GetClipboardHandler(getter_AddRefs(clipCmd));
            if (NS_SUCCEEDED(rv))
                clipCmd->CopySelection();
            break;

        case cmd_Paste:
            rv = GetClipboardHandler(getter_AddRefs(clipCmd));
            if (NS_SUCCEEDED(rv))
                clipCmd->Paste();
            break;

        case cmd_SelectAll:
            rv = GetClipboardHandler(getter_AddRefs(clipCmd));
            if (NS_SUCCEEDED(rv))
                clipCmd->SelectAll();
            break;

		case cmd_Find:
			Find();
			break;

		case cmd_FindNext:
			FindNext();
			break;

        case cmd_OpenLinkInNewWindow:
        case cmd_CopyLinkLocation:
            {               
                // Get the URL from the link
                ThrowIfNil_(mContextMenuInfo);
                nsAutoString temp;
                rv = mContextMenuInfo->GetAssociatedLink(temp);
                ThrowIfError_(rv);
                nsCAutoString urlSpec = NS_ConvertUCS2toUTF8(temp);

                if (inCommand == cmd_OpenLinkInNewWindow) {
                    nsCAutoString referrer;
                    rv = GetFocusedWindowURL(temp);
                    if (NS_SUCCEEDED(rv))
                        referrer = NS_ConvertUCS2toUTF8(temp);
                    PostOpenURLEvent(urlSpec, referrer);
                }
                else
                    UScrap::SetData(kScrapFlavorTypeText, urlSpec.get(), urlSpec.Length());
            }
            break;
            
		case cmd_SaveFormData:
            {
                nsCOMPtr<nsIDOMWindow> domWindow;
                nsCOMPtr<nsIDOMWindowInternal> domWindowInternal;

                nsCOMPtr<nsIWalletService> walletService = 
                         do_GetService(NS_WALLETSERVICE_CONTRACTID, &rv);
                ThrowIfError_(rv);
                rv = mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
                ThrowIfError_(rv);
                domWindowInternal = do_QueryInterface(domWindow, &rv);
                ThrowIfError_(rv);
                PRUint32 retval;
                rv = walletService->WALLET_RequestToCapture(domWindowInternal, &retval);
            }
            break;
		  
		case cmd_PrefillForm:
            {
                nsCOMPtr<nsIDOMWindow> domWindow;
                nsCOMPtr<nsIDOMWindowInternal> domWindowInternal;

                nsCOMPtr<nsIWalletService> walletService = 
                         do_GetService(NS_WALLETSERVICE_CONTRACTID, &rv);
                ThrowIfError_(rv);
                rv = mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
                ThrowIfError_(rv);
                domWindowInternal = do_QueryInterface(domWindow, &rv);
                ThrowIfError_(rv);
                PRBool retval;
                // Don't check the result - A result of NS_ERROR_FAILURE means not to show preview dialog
                rv = walletService->WALLET_Prefill(true, domWindowInternal, &retval);
            }
            break;

        case cmd_ViewPageSource:
            {
                nsCAutoString currentURL;
                rv = GetCurrentURL(currentURL);
                ThrowIfError_(rv);
                currentURL.Insert("view-source:", 0);
                PostOpenURLEvent(currentURL, nsCString());
            }
            break;

        case cmd_SaveLinkTarget:
            {
                // Get the URL from the link
                ThrowIfNil_(mContextMenuInfo);
                nsAutoString linkText;
                rv = mContextMenuInfo->GetAssociatedLink(linkText);
                ThrowIfError_(rv);
                
                nsCOMPtr<nsIURI> linkURI;
                rv = NS_NewURI(getter_AddRefs(linkURI), NS_ConvertUCS2toUTF8(linkText));
                ThrowIfError_(rv);

                SaveLink(linkURI);
            }
            break;
            
        case cmd_SaveImage:
            {
                ThrowIfNil_(mContextMenuInfo);
                nsCOMPtr<nsIURI> imgURI;
                mContextMenuInfo->GetImageSrc(getter_AddRefs(imgURI));
                ThrowIfNil_(imgURI);
                
                SaveLink(imgURI);
            }
            break;
            
        case cmd_ViewImage:
        case cmd_CopyImageLocation:
            {
                ThrowIfNil_(mContextMenuInfo);
                nsCOMPtr<nsIURI> imgURI;
                mContextMenuInfo->GetImageSrc(getter_AddRefs(imgURI));
                ThrowIfNil_(imgURI);
                nsCAutoString temp; 
                rv = imgURI->GetSpec(temp);
                ThrowIfError_(rv);
                if (inCommand == cmd_ViewImage)
                    PostOpenURLEvent(temp, nsCString());
                else
                    UScrap::SetData(kScrapFlavorTypeText, temp.get(), temp.Length());
            }
            break;
        
        case cmd_ViewBackgroundImage:
            {
                ThrowIfNil_(mContextMenuInfo);
                nsCOMPtr<nsIURI> uri;
                rv = mContextMenuInfo->GetBackgroundImageSrc(getter_AddRefs(uri));
                ThrowIfNil_(uri);
                nsCAutoString temp;
                rv = uri->GetSpec(temp);
                ThrowIfError_(rv);
                PostOpenURLEvent(temp, nsCString());                
            }
            break;
        
        case cmd_CopyImage:
            {
                GetClipboardHandler(getter_AddRefs(clipCmd));
                if (clipCmd)
                    clipCmd->CopyImageContents();
            }
            break;
                                        
        default:
            cmdHandled = LCommander::ObeyCommand(inCommand, ioParam);
            break;
    }
    return cmdHandled;
}


void CBrowserShell::FindCommandStatus(PP_PowerPlant::CommandT inCommand,
            		                  Boolean &outEnabled, Boolean &outUsesMark,
            					      UInt16 &outMark, Str255 outName)
{
    nsresult rv;
    nsCOMPtr<nsIClipboardCommands> clipCmd;
    PRBool haveContent, canDo;
    nsCOMPtr<nsIURI> currURI;

    rv = mWebBrowserAsWebNav->GetCurrentURI(getter_AddRefs(currURI));
    haveContent = NS_SUCCEEDED(rv) && currURI;
    
    switch (inCommand)
    {
        case cmd_Back:
            outEnabled = CanGoBack();
            break;
 
        case cmd_Forward:
            outEnabled = CanGoForward();
            break;
            
        case cmd_Stop:
            outEnabled = IsBusy();
            break;
            
        case cmd_Reload:
            outEnabled = haveContent;
            break;

        case cmd_SaveAs:
            outEnabled = haveContent;
            break;
            
        case cmd_Cut:
            if (haveContent) {
                rv = GetClipboardHandler(getter_AddRefs(clipCmd));
                if (NS_SUCCEEDED(rv)) {
                    rv = clipCmd->CanCutSelection(&canDo);
                    outEnabled = NS_SUCCEEDED(rv) && canDo;
                }
            }
            break;

        case cmd_Copy:
            if (haveContent) {
                rv = GetClipboardHandler(getter_AddRefs(clipCmd));
                if (NS_SUCCEEDED(rv)) {
                    rv = clipCmd->CanCopySelection(&canDo);
                    outEnabled = NS_SUCCEEDED(rv) && canDo;
                }
            }
            break;

        case cmd_Paste:
            if (haveContent) {
                rv = GetClipboardHandler(getter_AddRefs(clipCmd));
                if (NS_SUCCEEDED(rv)) {
                    rv = clipCmd->CanPaste(&canDo);
                    outEnabled = NS_SUCCEEDED(rv) && canDo;
                }
            }
            break;

        case cmd_SelectAll:
            outEnabled = haveContent;
            break;

		case cmd_Find:
			outEnabled = haveContent;
			break;

		case cmd_FindNext:
			outEnabled = haveContent && CanFindNext();
			break;

        case cmd_OpenLinkInNewWindow:
            outEnabled = haveContent && mContextMenuInfo && ((mContextMenuFlags & nsIContextMenuListener2::CONTEXT_LINK) != 0);
            break;
            
        case cmd_ViewPageSource:
            outEnabled = haveContent && ((mChromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME) == 0);
            break;
            
        case cmd_ViewImage:
            outEnabled = haveContent && mContextMenuInfo && ((mContextMenuFlags & nsIContextMenuListener2::CONTEXT_IMAGE) != 0);
            break;

        case cmd_ViewBackgroundImage:
            outEnabled = haveContent && mContextMenuInfo && ((mContextMenuFlags & nsIContextMenuListener2::CONTEXT_BACKGROUND_IMAGE) != 0);
            break;

        case cmd_CopyImage:
            if (haveContent) {
                rv = GetClipboardHandler(getter_AddRefs(clipCmd));
                if (NS_SUCCEEDED(rv)) {
                    rv = clipCmd->CanCopyImageContents(&canDo);
                    outEnabled = NS_SUCCEEDED(rv) && canDo;
                }
            }
            break;
            
        case cmd_SaveLinkTarget:
            outEnabled = haveContent && mContextMenuInfo && ((mContextMenuFlags & nsIContextMenuListener2::CONTEXT_LINK) != 0);
            break;
        
        case cmd_SaveImage:
            outEnabled = haveContent && mContextMenuInfo && ((mContextMenuFlags & nsIContextMenuListener2::CONTEXT_IMAGE) != 0);
            break;
            
        case cmd_CopyLinkLocation:
            outEnabled = haveContent && mContextMenuInfo && ((mContextMenuFlags & nsIContextMenuListener2::CONTEXT_LINK) != 0);
            break;
        
        case cmd_CopyImageLocation:
            outEnabled = haveContent && mContextMenuInfo && ((mContextMenuFlags & nsIContextMenuListener2::CONTEXT_IMAGE) != 0);
            break;
        
		case cmd_SaveFormData:
		    outEnabled = haveContent && HasFormElements();
		    break;

		case cmd_PrefillForm:
		    outEnabled = haveContent && HasFormElements();
		    break;

        default:
            LCommander::FindCommandStatus(inCommand, outEnabled,
        					              outUsesMark, outMark, outName);
            break;
    }
}


//*****************************************************************************
//***    CBrowserShell: LPeriodical overrides
//*****************************************************************************

void CBrowserShell::SpendTime(const EventRecord&		inMacEvent)
{
    switch (inMacEvent.what)
    {
        case nullEvent:
            HandleMouseMoved(inMacEvent);
            break;
        
        case osEvt:
            {
                // The event sink will not set the cursor if we are in the background - which is right.
                // We have to feed it suspendResumeMessages for it to know

                unsigned char eventType = ((inMacEvent.message >> 24) & 0x00ff);
                if (eventType == suspendResumeMessage) {
                    PRBool handled = PR_FALSE;
                    mEventSink->DispatchEvent(&const_cast<EventRecord&>(inMacEvent), &handled);
                }
                else if (eventType == mouseMovedMessage)
                    HandleMouseMoved(inMacEvent);
            }
            break;
    }
}


//*****************************************************************************
//***    CBrowserShell:
//*****************************************************************************

void CBrowserShell::AddAttachments()
{
    // Only add a context menu attachment for full browser windows -
    // not view-source and chrome dialogs.
    if ((mChromeFlags & (nsIWebBrowserChrome::CHROME_TOOLBAR |
                         nsIWebBrowserChrome::CHROME_STATUSBAR)) != 0)
    {
        CWebBrowserCMAttachment *cmAttachment = new CWebBrowserCMAttachment(this);
        ThrowIfNil_(cmAttachment);
        cmAttachment->SetCommandList(mcmd_BrowserShellContextMenuCmds);
        AddAttachment(cmAttachment);
    }
}

NS_METHOD CBrowserShell::GetWebBrowser(nsIWebBrowser** aBrowser)
{
    NS_ENSURE_ARG_POINTER(aBrowser);

    *aBrowser = mWebBrowser;
    NS_IF_ADDREF(*aBrowser);
    return NS_OK;
}


NS_METHOD CBrowserShell::SetWebBrowser(nsIWebBrowser* aBrowser)
{
    NS_ENSURE_ARG(aBrowser);

    FocusDraw();

    mWebBrowser = aBrowser;

    nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(mWebBrowser));
    NS_ENSURE_TRUE(baseWin, NS_ERROR_FAILURE);
    mWebBrowserAsBaseWin = baseWin;

    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mWebBrowser));
    NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);
    mWebBrowserAsWebNav = webNav;

    Rect portFrame;
    CalcPortFrameRect(portFrame);
    nsRect   r(portFrame.left, portFrame.top, portFrame.right - portFrame.left, portFrame.bottom - portFrame.top);
    	
    mWebBrowserAsBaseWin->InitWindow(GetMacWindow(), nsnull, r.x, r.y, r.width, r.height);
    mWebBrowserAsBaseWin->Create();
    mEventSink = do_GetInterface(mWebBrowser);
    ThrowIfNil_(mEventSink);

    AdjustFrame();   

    return NS_OK;
}

NS_METHOD CBrowserShell::GetWebBrowserChrome(nsIWebBrowserChrome** aChrome)
{
    NS_ENSURE_ARG_POINTER(aChrome);
    return mChrome->QueryInterface(NS_GET_IID(nsIWebBrowserChrome), (void **)aChrome);
}

NS_METHOD CBrowserShell::GetContentViewer(nsIContentViewer** aViewer)
{
    nsCOMPtr<nsIDocShell> ourDocShell(do_GetInterface(mWebBrowser));
    NS_ENSURE_TRUE(ourDocShell, NS_ERROR_FAILURE);
    return ourDocShell->GetContentViewer(aViewer);
}

NS_METHOD CBrowserShell::GetFocusedWindowURL(nsAString& outURL)
{
    nsCOMPtr<nsIWebBrowserFocus> wbf(do_GetInterface(mWebBrowser));
    if (!wbf)
        return NS_ERROR_FAILURE;
    nsCOMPtr<nsIDOMWindow> domWindow;
    wbf->GetFocusedWindow(getter_AddRefs(domWindow));
    if (!domWindow)
        mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    if (!domWindow)
        return NS_ERROR_FAILURE;
    nsCOMPtr<nsIDOMDocument> domDocument;
    domWindow->GetDocument(getter_AddRefs(domDocument));
    if (!domDocument)
        return NS_ERROR_FAILURE;
    nsCOMPtr<nsIDOMNSDocument> nsDoc(do_QueryInterface(domDocument));
    if (!nsDoc)
        return NS_ERROR_FAILURE;
    nsCOMPtr<nsIDOMLocation> location;
    nsDoc->GetLocation(getter_AddRefs(location));
    if (!location)
        return NS_ERROR_FAILURE;
    return location->GetHref(outURL);
}

NS_METHOD CBrowserShell::GetPrintSettings(nsIPrintSettings** aSettings)
{
    NS_ENSURE_ARG_POINTER(aSettings);
    *aSettings = nsnull;
    
    if (!mPrintSettings) {
        // If we don't have print settings yet, make new ones.
        nsCOMPtr<nsIWebBrowserPrint> wbPrint(do_GetInterface(mWebBrowser));
        NS_ENSURE_TRUE(wbPrint, NS_ERROR_NO_INTERFACE);
        wbPrint->GetGlobalPrintSettings(getter_AddRefs(mPrintSettings));
    }
    if (mPrintSettings) {
        *aSettings = mPrintSettings;
        NS_ADDREF(*aSettings);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

//*****************************************************************************
//***    CBrowserShell: Navigation
//*****************************************************************************

Boolean CBrowserShell::IsBusy()
{
    return mProgressListener->GetIsLoading();
}


Boolean CBrowserShell::CanGoBack()
{
    PRBool      canDo;
    nsresult    rv;

    rv = mWebBrowserAsWebNav->GetCanGoBack(&canDo);
    return (NS_SUCCEEDED(rv) && canDo);
}


Boolean CBrowserShell::CanGoForward()
{
    PRBool      canDo;
    nsresult    rv;

    rv = mWebBrowserAsWebNav->GetCanGoForward(&canDo);
    return (NS_SUCCEEDED(rv) && canDo);
}


NS_METHOD CBrowserShell::Back()
{
   nsresult rv;
    
    if (CanGoBack())
      rv = mWebBrowserAsWebNav->GoBack();
   else
   {
      ::SysBeep(5);
      rv = NS_ERROR_FAILURE;
   }
   return rv;
}

NS_METHOD CBrowserShell::Forward()
{
   nsresult rv;

    if (CanGoForward())
      rv = mWebBrowserAsWebNav->GoForward();
   else
   {
      ::SysBeep(5);
      rv = NS_ERROR_FAILURE;
   }
   return rv;
}

NS_METHOD CBrowserShell::Stop()
{
   return mWebBrowserAsWebNav->Stop(nsIWebNavigation::STOP_ALL);
}

NS_METHOD CBrowserShell::Reload()
{
    return mWebBrowserAsWebNav->Reload(nsIWebNavigation::LOAD_FLAGS_NONE);
}

//*****************************************************************************
//***    CBrowserShell: URL Loading
//*****************************************************************************


NS_METHOD CBrowserShell::LoadURL(const nsACString& urlText, const nsACString& referrer)
{    
    nsCOMPtr<nsIURI> referrerURI;
    if (referrer.Length()) {
        nsresult rv = NS_NewURI(getter_AddRefs(referrerURI), referrer);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to make URI for referrer.");
    }
    return mWebBrowserAsWebNav->LoadURI(NS_ConvertUTF8toUCS2(urlText).get(),
                                        nsIWebNavigation::LOAD_FLAGS_NONE,
                                        referrerURI,
                                        nsnull,
                                        nsnull);
}

NS_METHOD CBrowserShell::GetCurrentURL(nsACString& urlText)
{
    nsresult rv;
    nsCOMPtr<nsIURI> currentURI;
    rv = mWebBrowserAsWebNav->GetCurrentURI(getter_AddRefs(currentURI));
    if (NS_FAILED(rv)) return rv;
    rv = currentURI->GetSpec(urlText);
    if (NS_FAILED(rv)) return rv;
    
    return NS_OK;
}

//*****************************************************************************
//***    CBrowserShell: URI Saving
//*****************************************************************************

// Save All As from menus
NS_METHOD CBrowserShell::SaveDocument(ESaveFormat inSaveFormat)
{
    nsresult    rv;
    
    nsCOMPtr<nsIDOMDocument> domDocument;
    rv = mWebBrowserAsWebNav->GetDocument(getter_AddRefs(domDocument));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDOMNSDocument> nsDoc(do_QueryInterface(domDocument));
    if (!nsDoc)
        return NS_ERROR_FAILURE;
    nsCOMPtr<nsIDOMLocation> location;
    nsDoc->GetLocation(getter_AddRefs(location));
    if (!location)
        return NS_ERROR_FAILURE;
    
    nsAutoString docLocation;
    rv = location->GetHref(docLocation);
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIURI> documentURI;
    rv = NS_NewURI(getter_AddRefs(documentURI), NS_ConvertUCS2toUTF8(docLocation));
    if (NS_FAILED(rv))
      return rv;

    return SaveInternal(documentURI, domDocument, EmptyString(), false, inSaveFormat);     // don't bypass cache
}

// Save link target
NS_METHOD CBrowserShell::SaveLink(nsIURI* inURI)
{
    return SaveInternal(inURI, nsnull, EmptyString(), true, eSaveFormatHTML);     // bypass cache
}

const char* const kPersistContractID = "@mozilla.org/embedding/browser/nsWebBrowserPersist;1";

NS_METHOD CBrowserShell::SaveInternal(nsIURI* inURI, nsIDOMDocument* inDocument,
  const nsAString& inSuggestedFilename, Boolean inBypassCache, ESaveFormat inSaveFormat)
{
    nsresult rv;
    // Create our web browser persist object.  This is the object that knows
    // how to actually perform the saving of the page (and of the images
    // on the page).
    nsCOMPtr<nsIWebBrowserPersist> webPersist(do_CreateInstance(kPersistContractID, &rv));
    if (!webPersist)
        return rv;
    
    // Make a temporary file object that we can save to.
    nsCOMPtr<nsIProperties> dirService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
    if (!dirService)
        return rv;
    nsCOMPtr<nsIFile> tmpFile;
    dirService->Get("TmpD", NS_GET_IID(nsIFile), getter_AddRefs(tmpFile));
    static short unsigned int tmpRandom = 0;
    nsAutoString tmpNo; tmpNo.AppendInt(tmpRandom++);
    nsAutoString saveFile(NS_LITERAL_STRING("-sav"));
    saveFile += tmpNo;
    saveFile += NS_LITERAL_STRING("tmp");
    tmpFile->Append(saveFile); 
    
    // Get the post data if we're an HTML doc.
    nsCOMPtr<nsIInputStream> postData;
    if (inDocument) {
      nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mWebBrowser));
      nsCOMPtr<nsISHistory> sessionHistory;
      webNav->GetSessionHistory(getter_AddRefs(sessionHistory));
      nsCOMPtr<nsIHistoryEntry> entry;
      PRInt32 sindex;
      sessionHistory->GetIndex(&sindex);
      sessionHistory->GetEntryAtIndex(sindex, PR_FALSE, getter_AddRefs(entry));
      nsCOMPtr<nsISHEntry> shEntry(do_QueryInterface(entry));
      if (shEntry)
          shEntry->GetPostData(getter_AddRefs(postData));
    }

    // when saving, we first fire off a save with a nsHeaderSniffer as a progress
    // listener. This allows us to look for the content-disposition header, which
    // can supply a filename, and maybe has something to do with CGI-generated
    // content (?)
    CHeaderSniffer* sniffer = new CHeaderSniffer(webPersist, tmpFile, inURI, inDocument, postData,
                                        inSuggestedFilename, inBypassCache, inSaveFormat);
    if (!sniffer)
        return NS_ERROR_OUT_OF_MEMORY;

    webPersist->SetProgressListener(sniffer);  // owned
    
    return webPersist->SaveURI(inURI, nsnull, nsnull, nsnull, nsnull, tmpFile);
}


//*****************************************************************************
//***    CBrowserShell: Text Searching
//*****************************************************************************

Boolean CBrowserShell::Find()
{
    nsresult rv;
    nsXPIDLString stringBuf;
    PRBool  findBackwards;
    PRBool  wrapFind;
    PRBool  entireWord;
    PRBool  matchCase;

    nsCOMPtr<nsIWebBrowserFind> finder(do_GetInterface(mWebBrowser));
    if (!finder) return FALSE;
    finder->GetSearchString(getter_Copies(stringBuf));
    finder->GetFindBackwards(&findBackwards);    
    finder->GetWrapFind(&wrapFind);    
    finder->GetEntireWord(&entireWord);    
    finder->GetMatchCase(&matchCase);    

    Boolean     result = FALSE;
    nsString    searchString(stringBuf.get());

    if (DoFindDialog(searchString, findBackwards, wrapFind, entireWord, matchCase))
    {
        PRBool  didFind;

        finder->SetSearchString(searchString.get());
        finder->SetFindBackwards(findBackwards);    
        finder->SetWrapFind(wrapFind);    
        finder->SetEntireWord(entireWord);    
        finder->SetMatchCase(matchCase);    
        rv = finder->FindNext(&didFind);
        result = (NS_SUCCEEDED(rv) && didFind);
        if (!result)
          ::SysBeep(1);
    }
    return result;
}

Boolean CBrowserShell::Find(const nsAString& searchString,
                            Boolean findBackwards,
                            Boolean wrapFind,
                            Boolean entireWord,
                            Boolean matchCase)
{
    nsresult    rv;
    Boolean     result;
    PRBool      didFind;

    nsCOMPtr<nsIWebBrowserFind> finder(do_GetInterface(mWebBrowser));
    if (!finder) return FALSE;

    finder->SetSearchString(PromiseFlatString(searchString).get());
    finder->SetFindBackwards(findBackwards);    
    finder->SetWrapFind(wrapFind);    
    finder->SetEntireWord(entireWord);    
    finder->SetMatchCase(matchCase);    

    rv = finder->FindNext(&didFind);
    result = (NS_SUCCEEDED(rv) && didFind);
    if (!result)
        ::SysBeep(1);

    return result;
}

Boolean CBrowserShell::CanFindNext()
{
    nsresult    rv;

    nsCOMPtr<nsIWebBrowserFind> finder(do_GetInterface(mWebBrowser));
    if (!finder) return FALSE;
    
    nsXPIDLString searchStr;
    rv = finder->GetSearchString(getter_Copies(searchStr));
    return (NS_SUCCEEDED(rv) && !searchStr.IsEmpty());
}


Boolean CBrowserShell::FindNext()
{
    nsresult    rv;
    Boolean     result;
    PRBool      didFind;

    nsCOMPtr<nsIWebBrowserFind> finder(do_GetInterface(mWebBrowser));
    if (!finder) return FALSE;
    rv = finder->FindNext(&didFind);
    result = (NS_SUCCEEDED(rv) && didFind);
    if (!result)
        ::SysBeep(1);

    return result;
}


NS_IMETHODIMP CBrowserShell::OnShowContextMenu(PRUint32 aContextFlags,
                                               nsIContextMenuInfo *aInfo)
{
    // Find our CWebBrowserCMAttachment, if any
    CWebBrowserCMAttachment *aCMAttachment = nsnull;
    const TArray<LAttachment*>* theAttachments = GetAttachmentsList();
    
    if (theAttachments) {
        TArrayIterator<LAttachment*> iterate(*theAttachments);
        
        LAttachment*    theAttach;
        while (iterate.Next(theAttach)) {
            aCMAttachment = dynamic_cast<CWebBrowserCMAttachment*>(theAttach);
            if (aCMAttachment != nil)
                break;
        }
    }
    if (!aCMAttachment) {
        NS_ASSERTION(PR_FALSE, "No CWebBrowserCMAttachment");
        return NS_OK;
    }
                
    EventRecord macEvent;        
    UEventMgr::GetMouseAndModifiers(macEvent);
    
    mContextMenuFlags = aContextFlags;
    mContextMenuInfo = aInfo;
    aCMAttachment->DoContextMenuClick(macEvent);
    mContextMenuFlags = 0;
    mContextMenuInfo = nsnull;

    return NS_OK;
}
                                          
NS_IMETHODIMP CBrowserShell::OnShowTooltip(PRInt32 aXCoords,
                                           PRInt32 aYCoords,
                                           const PRUnichar *aTipText)
{
    nsresult rv = NS_ERROR_NOT_IMPLEMENTED;
    
#if TARGET_CARBON
    if ((Ptr)HMDisplayTag != (Ptr)kUnresolvedCFragSymbolAddress &&
        (Ptr)HMHideTag != (Ptr)kUnresolvedCFragSymbolAddress)
    {
        HMHelpContentRec contentRec;
        Point location;

        location.h = aXCoords; location.v = aYCoords;
        FocusDraw();
        LocalToPortPoint(location);
        PortToGlobalPoint(location);
        
        contentRec.version = kMacHelpVersion;
        contentRec.absHotRect.top = contentRec.absHotRect.bottom = location.v;
        contentRec.absHotRect.left = contentRec.absHotRect.right = location.h;
        ::InsetRect(&contentRec.absHotRect, -4, -4);
        contentRec.tagSide = kHMOutsideBottomScriptAligned;
        contentRec.content[kHMMinimumContentIndex].contentType = kHMCFStringContent;
        contentRec.content[kHMMinimumContentIndex].u.tagCFString = 
            ::CFStringCreateWithCharactersNoCopy(NULL, (const UniChar *)aTipText, nsCRT::strlen(aTipText), kCFAllocatorNull);
        contentRec.content[kHMMaximumContentIndex].contentType = kHMNoContent;
        
        ::HMDisplayTag(&contentRec);
        rv = NS_OK;
    }
#endif

    return rv;
}

NS_IMETHODIMP CBrowserShell::OnHideTooltip()
{
    nsresult rv = NS_ERROR_NOT_IMPLEMENTED;
    
#if TARGET_CARBON
    if ((Ptr)HMHideTag != (Ptr)kUnresolvedCFragSymbolAddress)
    {
        ::HMHideTag();
        rv = NS_OK;
    }
#endif

    return rv;
}


void CBrowserShell::HandleMouseMoved(const EventRecord& inMacEvent)
{
    static Point	lastWhere = {0, 0};

    if (IsActive() && (*(long*)&lastWhere != *(long*)&inMacEvent.where))
    {
        FocusDraw();
        PRBool handled = PR_FALSE;
        mEventSink->DispatchEvent(&const_cast<EventRecord&>(inMacEvent), &handled);
        lastWhere = inMacEvent.where;
    }
}


void CBrowserShell::AdjustFrame()
{
    FocusDraw();
    	
    Rect portFrame;
    CalcPortFrameRect(portFrame);
    nsRect r(portFrame.left, portFrame.top, portFrame.right - portFrame.left, portFrame.bottom - portFrame.top);
    		
    mWebBrowserAsBaseWin->SetPositionAndSize(r.x, r.y, r.width, r.height, PR_TRUE);
}


Boolean CBrowserShell::DoFindDialog(nsAString& searchText,
                                     PRBool& findBackwards,
                                     PRBool& wrapFind,
                                     PRBool& entireWord,
                                     PRBool& caseSensitive)
{
    enum
    {
        kSearchTextEdit       = FOUR_CHAR_CODE('Text'),
        kCaseSensitiveCheck   = FOUR_CHAR_CODE('Case'),
        kWrapAroundCheck      = FOUR_CHAR_CODE('Wrap'),
        kSearchBackwardsCheck = FOUR_CHAR_CODE('Back'),
        kEntireWordCheck      = FOUR_CHAR_CODE('Entr')
    };

    Boolean     result;

    try
    {
        // Create stack-based object for handling the dialog box

        StDialogHandler	theHandler(dlog_FindDialog, this);
        LWindow			*theDialog = theHandler.GetDialog();
        Str255          aStr;

        // Set initial text for string in dialog box

        CPlatformUCSConversion::GetInstance()->UCSToPlatform(searchText, aStr);

        LEditText	*editField = dynamic_cast<LEditText*>(theDialog->FindPaneByID(kSearchTextEdit));
        editField->SetDescriptor(aStr);
        theDialog->SetLatentSub(editField);

        LCheckBox   *caseSensCheck, *entireWordCheck, *wrapAroundCheck, *backwardsCheck;

        caseSensCheck = dynamic_cast<LCheckBox*>(theDialog->FindPaneByID(kCaseSensitiveCheck));
        ThrowIfNot_(caseSensCheck);
        caseSensCheck->SetValue(caseSensitive ? 1 : 0);
        entireWordCheck = dynamic_cast<LCheckBox*>(theDialog->FindPaneByID(kEntireWordCheck));
        ThrowIfNot_(entireWordCheck);
        entireWordCheck->SetValue(entireWord ? 1 : 0);
        wrapAroundCheck = dynamic_cast<LCheckBox*>(theDialog->FindPaneByID(kWrapAroundCheck));
        ThrowIfNot_(wrapAroundCheck);
        wrapAroundCheck->SetValue(wrapFind ? 1 : 0);
        backwardsCheck = dynamic_cast<LCheckBox*>(theDialog->FindPaneByID(kSearchBackwardsCheck));
        ThrowIfNot_(backwardsCheck);
        backwardsCheck->SetValue(findBackwards ? 1 : 0);

        theDialog->Show();

        while (true)  // This is our modal dialog event loop
        {				
            MessageT	hitMessage = theHandler.DoDialog();

            if (hitMessage == msg_Cancel)
            {
                result = FALSE;
                break;
   			
            }
            else if (hitMessage == msg_OK)
            {
                editField->GetDescriptor(aStr);
                CPlatformUCSConversion::GetInstance()->PlatformToUCS(aStr, searchText);

                caseSensitive = caseSensCheck->GetValue() ? TRUE : FALSE;
                entireWord = entireWordCheck->GetValue() ? TRUE : FALSE;
                wrapFind = wrapAroundCheck->GetValue() ? TRUE : FALSE;
                findBackwards = backwardsCheck->GetValue() ? TRUE : FALSE;

                result = TRUE;
                break;
            }
        }
    }
	catch (...)
	{
	   result = FALSE;
	   // Don't propagate the error.
	}
	
	return result;
}

nsresult CBrowserShell::GetClipboardHandler(nsIClipboardCommands **aCommand)
{
    NS_ENSURE_ARG_POINTER(aCommand);

    nsCOMPtr<nsIClipboardCommands> clipCmd(do_GetInterface(mWebBrowser));
    NS_ENSURE_TRUE(clipCmd, NS_ERROR_FAILURE);

    *aCommand = clipCmd;
    NS_ADDREF(*aCommand);
    return NS_OK;
}

Boolean CBrowserShell::HasFormElements()
{
    nsresult rv;
    
    nsCOMPtr<nsIDOMWindow> domWindow;
    rv = mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIDOMDocument> domDoc;
        rv = domWindow->GetDocument(getter_AddRefs(domDoc));
        if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(domDoc);
            if (htmlDoc) {
                nsCOMPtr<nsIDOMHTMLCollection> forms;
                htmlDoc->GetForms(getter_AddRefs(forms));
                if (forms) {
                    PRUint32 numForms;
                    forms->GetLength(&numForms);
                    return numForms > 0;
                }
            }
        }
    }
    return false;
}

void CBrowserShell::PostOpenURLEvent(const nsACString& url, const nsACString& referrer)
{    
    // Send an AppleEvent to ourselves to open a new window with the given URL
    
    // IMPORTANT: We need to make our target address using a ProcessSerialNumber
    // from GetCurrentProcess. This will cause our AppleEvent to be handled from
    // the event loop. Creating and showing a new window before the context menu
    // click is done being processed is fatal. If we make the target address with a
    // ProcessSerialNumber in which highLongOfPSN == 0 && lowLongOfPSN == kCurrentProcess,
    // the event will be dispatched to us directly and we die.
    
    OSErr err;
    ProcessSerialNumber currProcess;
    StAEDescriptor selfAddrDesc;
    
    err = ::GetCurrentProcess(&currProcess);
    ThrowIfOSErr_(err);
    err = ::AECreateDesc(typeProcessSerialNumber, (Ptr) &currProcess,
                         sizeof(currProcess), selfAddrDesc);
    ThrowIfOSErr_(err);

    AppleEvent  getURLEvent;
    err = ::AECreateAppleEvent(kInternetEventClass, kAEGetURL,
                            selfAddrDesc,
                            kAutoGenerateReturnID,
                            kAnyTransactionID,
                            &getURLEvent);
    ThrowIfOSErr_(err);

    const nsPromiseFlatCString& flatURL = PromiseFlatCString(url);  
    StAEDescriptor urlDesc(typeChar, flatURL.get(), flatURL.Length());
    
    err = ::AEPutParamDesc(&getURLEvent, keyDirectObject, urlDesc);
    if (err) {
        ::AEDisposeDesc(&getURLEvent);
        Throw_(err);
    }
    if (referrer.Length() != 0) {
        const nsPromiseFlatCString& flatReferrer = PromiseFlatCString(referrer);  
        StAEDescriptor referrerDesc(typeChar, flatReferrer.get(), flatReferrer.Length());
        err = ::AEPutParamDesc(&getURLEvent, keyGetURLReferrer, referrerDesc);
        if (err) {
            ::AEDisposeDesc(&getURLEvent);
            Throw_(err);
        }
    }
    UAppleEventsMgr::SendAppleEvent(getURLEvent);
}


void
CBrowserShell::InsideDropArea(DragReference inDragRef)
{
  if ( sDragHelper ) {
    PRBool dropAllowed = PR_FALSE;
    sDragHelper->Tracking ( inDragRef, mEventSink, &dropAllowed );
  }
}


void
CBrowserShell::EnterDropArea( DragReference inDragRef, Boolean inDragHasLeftSender)
{
  sDragHelper = do_GetService ( "@mozilla.org/widget/draghelperservice;1" );
  NS_ASSERTION ( sDragHelper, "Couldn't get a drag service, we're in biiig trouble" );
  if ( sDragHelper )
    sDragHelper->Enter ( inDragRef, mEventSink );
}


void
CBrowserShell::LeaveDropArea( DragReference inDragRef )
{
  if ( sDragHelper ) {
    sDragHelper->Leave ( inDragRef, mEventSink );
    sDragHelper = nsnull;      
  }
}


Boolean
CBrowserShell::PointInDropArea(Point inGlobalPt)
{
  return true;
}

Boolean
CBrowserShell::DragIsAcceptable( DragReference inDragRef )
{
  return true;
}

void
CBrowserShell::DoDragReceive( DragReference inDragRef )
{
  if ( sDragHelper ) {
    PRBool dragAccepted = PR_FALSE;
    sDragHelper->Drop ( inDragRef, mEventSink, &dragAccepted );
  }
}

#pragma mark -

OSStatus CBrowserShell::HandleUpdateActiveInputArea(
  const nsAString& text, 
  PRInt16 script,  
  PRInt16 language, 
  PRInt32 fixLen, 
  const TextRangeArray * hiliteRng
)
{
  nsCOMPtr<nsIMacTextInputEventSink> tieSink;
  tieSink = do_QueryInterface(mEventSink);
  if (!tieSink)
    return eventNotHandledErr;
    
  OSStatus err = noErr;
  nsresult res = tieSink->HandleUpdateActiveInputArea( text,
                                                       script, language, 
                                                       fixLen, (void*)hiliteRng, 
                                                       &err);
  if (noErr != err)
    return err;
  if (NS_FAILED(res))
    return eventNotHandledErr;
  return noErr;
}

OSStatus CBrowserShell::HandleUnicodeForKeyEvent(
  const nsAString& text, 
  PRInt16 script,
  PRInt16 language, 
  const EventRecord * keyboardEvent)
{
  nsCOMPtr<nsIMacTextInputEventSink> tieSink;
  tieSink = do_QueryInterface(mEventSink);
  if (!tieSink)
    return eventNotHandledErr;
    
  OSStatus err = noErr;
  nsresult res = tieSink->HandleUnicodeForKeyEvent( text,
                                                    script, language, 
                                                    (void*)keyboardEvent, 
                                                    &err);
  if (noErr != err)
      return err;
  if (NS_FAILED(res))
      return eventNotHandledErr;
  return noErr;
}

OSStatus CBrowserShell::HandleOffsetToPos(
  PRInt32 offset, 
  PRInt16 *pointX, 
  PRInt16 *pointY)
{
  nsCOMPtr<nsIMacTextInputEventSink> tieSink;
  tieSink = do_QueryInterface(mEventSink);
  if (!tieSink)
    return eventNotHandledErr;
    
  OSStatus err = noErr;
  nsresult res = tieSink->HandleOffsetToPos( offset, pointX, pointY, &err);

  if (noErr != err)
      return err;
  if (NS_FAILED(res))
      return eventNotHandledErr;
  return noErr;
}

OSStatus CBrowserShell::HandlePosToOffset(
  PRInt16 currentPointX, 
  PRInt16 currentPointY, 
  PRInt32 *offset,
  PRInt16 *regionClass
)
{
  nsCOMPtr<nsIMacTextInputEventSink> tieSink;
  tieSink = do_QueryInterface(mEventSink);
  if (!tieSink)
    return eventNotHandledErr;
    
  OSStatus err = noErr;
  nsresult res = tieSink->HandlePosToOffset( currentPointX, currentPointY, 
                                             offset, regionClass, &err);
  if (noErr != err)
      return err;
  if (NS_FAILED(res))
      return eventNotHandledErr;
  return noErr;
}

OSStatus CBrowserShell::HandleGetSelectedText(nsAString& selectedText)
{
  nsCOMPtr<nsIMacTextInputEventSink> tieSink;
  tieSink = do_QueryInterface(mEventSink);
  if (!tieSink)
    return eventNotHandledErr;
    
  OSStatus err = noErr;
  nsresult res = tieSink->HandleGetSelectedText( selectedText, &err);

  if (noErr != err)
      return err;
  if (NS_FAILED(res))
      return eventNotHandledErr;
  return noErr;
}

