/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Conrad Carlen <conrad@ingress.com>
 */

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsComponentManagerUtils.h"
#include "nsIServiceManager.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsXPIDLString.h"
#include "nsIContentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMHTMLAnchorElement.h"

#include "CBrowserWindow.h"
#include "CBrowserShell.h"
#include "CWebBrowserChrome.h"
#include "CWebBrowserCMAttachment.h"
#include "CThrobber.h"
#include "ApplIDs.h"
#include "UMacUnicode.h"

#include <LEditText.h>
#include <LStaticText.h>
#include <LWindowHeader.h>
#include <LBevelButton.h>
#include <LProgressBar.h>

#include <InternetConfig.h>

// CBrowserWindow:
// A simple browser window that hooks up a CWebShell to a minimal set of controls
// (Back, Forward and Stop buttons + URL field + status bar).


enum
{
	paneID_BackButton		= 'Back',
	paneID_ForwardButton	= 'Forw',
	paneID_StopButton		= 'Stop',
	paneID_URLField		= 'gUrl',
	paneID_WebShellView	= 'WebS',
	paneID_StatusBar		= 'Stat',
	paneID_Throbber	   = 'THRB',
    paneID_ProgressBar   = 'Prog'
};

// CIDs
static NS_DEFINE_IID(kWindowCID, NS_WINDOW_CID);

// ---------------------------------------------------------------------------
//	¥ CBrowserWindow						Default Constructor		  [public]
// ---------------------------------------------------------------------------

CBrowserWindow::CBrowserWindow() :
    mBrowserShell(NULL), mBrowserChrome(NULL),
    mURLField(NULL), mStatusBar(NULL), mThrobber(NULL),
    mBackButton(NULL), mForwardButton(NULL), mStopButton(NULL),
    mProgressBar(NULL), mBusy(false),
    mInitialLoadComplete(false), mShowOnInitialLoad(false),
    mSizeToContent(true),
    mContextMenuContext(nsIContextMenuListener::CONTEXT_NONE)
{
	nsresult rv = CommonConstruct();
	if (NS_FAILED(rv))
		Throw_(NS_ERROR_GET_CODE(rv));
}


// ---------------------------------------------------------------------------
//	¥ CBrowserWindow						Parameterized Constructor [public]
// ---------------------------------------------------------------------------

CBrowserWindow::CBrowserWindow(LCommander*		inSuperCommander,
                               const Rect&		inGlobalBounds,
                               ConstStringPtr	inTitle,
                               SInt16			inProcID,
                               UInt32			inAttributes,
                               WindowPtr		inBehind) :
    LWindow(inSuperCommander, inGlobalBounds, inTitle, inProcID, inAttributes, inBehind),
    mBrowserShell(NULL), mBrowserChrome(NULL),
    mURLField(NULL), mStatusBar(NULL), mThrobber(NULL),
    mBackButton(NULL), mForwardButton(NULL), mStopButton(NULL),
    mProgressBar(NULL), mBusy(false),
    mInitialLoadComplete(false), mShowOnInitialLoad(false),
    mSizeToContent(true),
    mContextMenuContext(nsIContextMenuListener::CONTEXT_NONE)
{
	nsresult rv = CommonConstruct();
	if (NS_FAILED(rv))
		Throw_(NS_ERROR_GET_CODE(rv));
}  


// ---------------------------------------------------------------------------
//	¥ CBrowserWindow						Stream Constructor		  [public]
// ---------------------------------------------------------------------------

CBrowserWindow::CBrowserWindow(LStream*	inStream) :
    LWindow(inStream),
    mBrowserShell(NULL), mBrowserChrome(NULL),
    mURLField(NULL), mStatusBar(NULL), mThrobber(NULL),
    mBackButton(NULL), mForwardButton(NULL), mStopButton(NULL),
    mProgressBar(NULL), mBusy(false),
    mInitialLoadComplete(false), mShowOnInitialLoad(false),
    mSizeToContent(true),
    mContextMenuContext(nsIContextMenuListener::CONTEXT_NONE)
{
	nsresult rv = CommonConstruct();
	if (NS_FAILED(rv))
		Throw_(NS_ERROR_GET_CODE(rv));
}


// ---------------------------------------------------------------------------
//	¥ ~CBrowserWindow						Destructor				  [public]
// ---------------------------------------------------------------------------

CBrowserWindow::~CBrowserWindow()
{
   if (mBrowserShell)
      mBrowserShell->SetTopLevelWindow(nsnull);
   
   if (mBrowserChrome)
   {
      mBrowserChrome->BrowserShell() = nsnull;
      mBrowserChrome->BrowserWindow() = nsnull;
      NS_RELEASE(mBrowserChrome);
   }
}


CBrowserWindow* CBrowserWindow::CreateWindow(PRUint32 chromeFlags, PRInt32 width, PRInt32 height)
{
    const SInt16 kToolBarHeight = 34;
    
    CBrowserWindow	*theWindow;
    
    if (chromeFlags == nsIWebBrowserChrome::CHROME_DEFAULT || chromeFlags == nsIWebBrowserChrome::CHROME_ALL)
    {
        theWindow = dynamic_cast<CBrowserWindow*>(LWindow::CreateWindow(wind_BrowserWindow, LCommander::GetTopCommander()));
        ThrowIfNil_(theWindow);
    }
    else
    {
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
            windowAttrs |= windAttr_Modal;

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
            windowAttrs |= windAttr_Regular;    

            if (chromeFlags & nsIWebBrowserChrome::CHROME_WITH_SIZE)
            {
                windowDefProc = kWindowGrowDocumentProc;
                windowAttrs |= windAttr_Resizable;
            }
            else
                windowDefProc = kWindowDocumentProc;            
            
            if (chromeFlags & nsIWebBrowserChrome::CHROME_WINDOW_CLOSE)
                windowAttrs |= windAttr_CloseBox;
        }

        theWindow = new CBrowserWindow(LCommander::GetTopCommander(), globalBounds, "\p", windowDefProc, windowAttrs, window_InFront);
        ThrowIfNil_(theWindow);
        theWindow->SetUserCon(wind_BrowserWindow);
        
        SPaneInfo aPaneInfo;
        SViewInfo aViewInfo;
        
        aPaneInfo.paneID = paneID_WebShellView;
        aPaneInfo.width = globalBounds.right - globalBounds.left;
        aPaneInfo.height = globalBounds.bottom - globalBounds.top;
        if (!(chromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_DIALOG) && chromeFlags & nsIWebBrowserChrome::CHROME_WITH_SIZE)
            aPaneInfo.height -= 15;
        if (chromeFlags & nsIWebBrowserChrome::CHROME_TOOLBAR)
            aPaneInfo.height -= kToolBarHeight;
        aPaneInfo.visible = true;
        aPaneInfo.enabled = true;
        aPaneInfo.bindings.left = true;
        aPaneInfo.bindings.top = true;
        aPaneInfo.bindings.right = true;
        aPaneInfo.bindings.bottom = true;
        aPaneInfo.left = 0;
        aPaneInfo.top = (chromeFlags & nsIWebBrowserChrome::CHROME_TOOLBAR) ? kToolBarHeight - 1 : 0;
        aPaneInfo.userCon = 0;
        aPaneInfo.superView = theWindow;
        
        aViewInfo.imageSize.width = 0;
        aViewInfo.imageSize.height = 0;
        aViewInfo.scrollPos.h = aViewInfo.scrollPos.v = 0;
        aViewInfo.scrollUnit.h = aViewInfo.scrollUnit.v = 1;
        aViewInfo.reconcileOverhang = 0;
        
        CBrowserShell *aShell = new CBrowserShell(aPaneInfo, aViewInfo);
        ThrowIfNil_(aShell);
        aShell->PutInside(theWindow, false);
        
        // Now the window is constructed...
        theWindow->FinishCreate();        
    }

	Rect	theBounds;
	theWindow->GetGlobalBounds(theBounds);
    if (width == -1)
        width = theBounds.right - theBounds.left;
    if (height == -1)
        height = theBounds.bottom - theBounds.top;
        
    theWindow->ResizeWindowTo(width, height);

    return theWindow;
}


NS_IMETHODIMP CBrowserWindow::CommonConstruct()
{
   nsresult rv;
   
   // Make the base widget
   mWindow = do_CreateInstance(kWindowCID, &rv);
   NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
   
   // Make our CWebBrowserChrome
   mBrowserChrome = new CWebBrowserChrome;
   NS_ENSURE_TRUE(mBrowserChrome, NS_ERROR_OUT_OF_MEMORY);
   NS_ADDREF(mBrowserChrome);
   mBrowserChrome->BrowserWindow() = this;
   
   return NS_OK;
}


void CBrowserWindow::FinishCreate()
{
   // Initialize the top level widget
   // This needs to be done AFTER the subviews are constructed
   // but BEFORE the subviews do FinishCreateSelf.
   
	Rect portRect = GetMacPort()->portRect;	
	nsRect r(0, 0, portRect.right - portRect.left, portRect.bottom - portRect.top);
	
	nsresult rv = mWindow->Create((nsNativeWidget)GetMacPort(), r, nsnull, nsnull, nsnull, nsnull, nsnull);
	if (NS_FAILED(rv))
	   Throw_(NS_ERROR_GET_CODE(rv));

   Inherited::FinishCreate();   
}


// ---------------------------------------------------------------------------
//	¥ FinishCreateSelf
// ---------------------------------------------------------------------------

void CBrowserWindow::FinishCreateSelf()
{
	mBrowserShell = dynamic_cast<CBrowserShell*>(FindPaneByID(paneID_WebShellView));
	ThrowIfNULL_(mBrowserShell);  // Curtains if we don't have this
	SetLatentSub(mBrowserShell);
	
	// Tell our CBrowserShell about the chrome 
	mBrowserShell->SetTopLevelWindow(mBrowserChrome);
	// Tell our chrome about the CBrowserShell 
	mBrowserChrome->BrowserShell() = mBrowserShell;
	
	// Find our subviews - When we have a way of creating this
	// window with various chrome flags, we may or may not have
	// all of these subviews so don't fail if they don't exist
	mURLField = dynamic_cast<LEditText*>(FindPaneByID(paneID_URLField));
	mStatusBar = dynamic_cast<LStaticText*>(FindPaneByID(paneID_StatusBar));
	mThrobber = dynamic_cast<CThrobber*>(FindPaneByID(paneID_Throbber));
	mProgressBar = dynamic_cast<LProgressBar*>(FindPaneByID(paneID_ProgressBar));
	if (mProgressBar)
	   mProgressBar->Hide();
	
	mBackButton = dynamic_cast<LBevelButton*>(FindPaneByID(paneID_BackButton));
	if (mBackButton)
		mBackButton->Disable();
	mForwardButton = dynamic_cast<LBevelButton*>(FindPaneByID(paneID_ForwardButton));
	if (mForwardButton)
		mForwardButton->Disable();
	mStopButton = dynamic_cast<LBevelButton*>(FindPaneByID(paneID_StopButton));
	if (mStopButton)
		mStopButton->Disable();

	UReanimator::LinkListenerToControls(this, this, mUserCon);
	StartListening();
	StartBroadcasting();	
}


void CBrowserWindow::ResizeFrameBy(SInt16		inWidthDelta,
                				   SInt16		inHeightDelta,
                				   Boolean	    inRefresh)
{
	// Resize the widget BEFORE subviews get resized
	Rect portRect = GetMacPort()->portRect;	
	mWindow->Resize(portRect.right - portRect.left, portRect.bottom - portRect.top, inRefresh);

	Inherited::ResizeFrameBy(inWidthDelta, inHeightDelta, inRefresh);	
}


void CBrowserWindow::ShowSelf()
{
   Inherited::ShowSelf();
   mWindow->Show(PR_TRUE);
}


// ---------------------------------------------------------------------------
//	¥ ListenToMessage
// ---------------------------------------------------------------------------

void CBrowserWindow::ListenToMessage(MessageT		inMessage,
									 void*			ioParam)
{
	ProcessCommand(inMessage, ioParam);
}

// ---------------------------------------------------------------------------
//	¥ ObeyCommand
// ---------------------------------------------------------------------------

Boolean CBrowserWindow::ObeyCommand(CommandT			inCommand,
									void				*ioParam)
{
#pragma unused(ioParam)

	Boolean	cmdHandled = true;
	
	switch (inCommand)
	{
	    case cmd_OpenLinkInNewWindow:
	        {
	            nsresult rv;
	            
	            // Get the URL from the link
	            ThrowIfNil_(mContextMenuDOMNode);
	            nsCOMPtr<nsIDOMHTMLAnchorElement> linkElement(do_QueryInterface(mContextMenuDOMNode, &rv));
	            ThrowIfError_(rv);
	            
	            nsAutoString href;
	            rv = linkElement->GetHref(href);
	            ThrowIfError_(rv);
	            
	            nsCAutoString urlSpec;
	            CopyUCS2toASCII(href, urlSpec);
	            
	            // Send an AppleEvent to ourselves to open a new window with the given URL
	            
	            // IMPORTANT: We need to make our target address using a ProcessSerialNumber
	            // from GetCurrentProcess. This will cause our AppleEvent to be handled from
	            // the event loop. Creating and showing a new window before the context menu
	            // click is done being processed is fatal. If we make the target address with a
	            // ProcessSerialNumber in which highLongOfPSN == 0 && lowLongOfPSN == kCurrentProcess,
	            // the event will be dispatched to us directly and we die.
	            
	            OSErr err;
	            ProcessSerialNumber	currProcess;
	            StAEDescriptor selfAddrDesc;
	            
	            err = ::GetCurrentProcess(&currProcess);
	            ThrowIfOSErr_(err);
	            err = ::AECreateDesc(typeProcessSerialNumber, (Ptr) &currProcess,
							         sizeof(currProcess), selfAddrDesc);
				ThrowIfOSErr_(err);

        		AppleEvent	getURLEvent;
        		err = ::AECreateAppleEvent(kInternetEventClass, kAEGetURL,
										selfAddrDesc,
										kAutoGenerateReturnID,
										kAnyTransactionID,
										&getURLEvent);
                ThrowIfOSErr_(err);
        		
        		StAEDescriptor urlDesc(typeChar, urlSpec.GetBuffer(), urlSpec.Length());
        		
        		err = ::AEPutParamDesc(&getURLEvent, keyDirectObject, urlDesc);
        		if (err) {
        		    ::AEDisposeDesc(&getURLEvent);
        		    Throw_(err);
        		}
        		UAppleEventsMgr::SendAppleEvent(getURLEvent);
	        }
            break;
            
		case paneID_BackButton:
			mBrowserShell->Back();
			break;

		case paneID_ForwardButton:
			mBrowserShell->Forward();
			break;

		case paneID_StopButton:
			mBrowserShell->Stop();
			break;

		case paneID_URLField:
		  	{
			SInt32    urlTextLen;
			mURLField->GetText(nil, 0, &urlTextLen);
        	StPointerBlock  urlTextPtr(urlTextLen, true, false);
			mURLField->GetText(urlTextPtr.Get(), urlTextLen, &urlTextLen);
			mBrowserShell->LoadURL(urlTextPtr.Get(), urlTextLen);
			}
			break;
			
	    default:
	        cmdHandled = false;
	        break;
	}

	if (!cmdHandled)
		cmdHandled = LWindow::ObeyCommand(inCommand, ioParam);
	return cmdHandled;
}


// ---------------------------------------------------------------------------
//		¥ FindCommandStatus
// ---------------------------------------------------------------------------
//	This function enables menu commands.

void
CBrowserWindow::FindCommandStatus(
	PP_PowerPlant::CommandT	inCommand,
	Boolean					&outEnabled,
	Boolean					&outUsesMark,
	PP_PowerPlant::Char16	&outMark,
	Str255					outName)
{

	switch (inCommand)
	{
	    case cmd_OpenLinkInNewWindow:
            outEnabled = (mContextMenuContext & nsIContextMenuListener::CONTEXT_LINK) != 0;
			break;

        case cmd_Back:
            outEnabled = mBrowserShell->CanGoBack();
            break;
 
        case cmd_Forward:
            outEnabled = mBrowserShell->CanGoForward();
            break;
            
        case cmd_Stop:
            outEnabled = mBusy;
            break;
            
        case cmd_Reload:
            outEnabled = true;
            break;
            
        case cmd_ViewPageSource:
            outEnabled = true;
            break;
            
        case cmd_ViewImage:
            outEnabled = (mContextMenuContext & nsIContextMenuListener::CONTEXT_IMAGE) != 0;
            break;
            
        case cmd_CopyLinkLocation:
            outEnabled = (mContextMenuContext & nsIContextMenuListener::CONTEXT_LINK) != 0;
            break;

        case cmd_CopyImageLocation:
            outEnabled = (mContextMenuContext & nsIContextMenuListener::CONTEXT_IMAGE) != 0;
            break;
           	        
		default:
			LWindow::FindCommandStatus(inCommand, outEnabled,
									   outUsesMark, outMark, outName);
			break;
	}
}


NS_METHOD CBrowserWindow::GetWidget(nsIWidget** aWidget)
{
   NS_ENSURE_ARG_POINTER(aWidget);

   *aWidget = mWindow;
   NS_IF_ADDREF(*aWidget);
   
   return NS_OK;
}


NS_METHOD CBrowserWindow::SizeToContent()
{
  nsCOMPtr<nsIContentViewer> aContentViewer;
  mBrowserShell->GetContentViewer(getter_AddRefs(aContentViewer));
  NS_ENSURE_TRUE(aContentViewer, NS_ERROR_FAILURE);
  nsCOMPtr<nsIMarkupDocumentViewer> markupViewer(do_QueryInterface(aContentViewer));
  NS_ENSURE_TRUE(markupViewer, NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(markupViewer->SizeToContent(), NS_ERROR_FAILURE);

  return NS_OK;
}

NS_METHOD CBrowserWindow::Stop()
{
    return mBrowserShell->Stop();
}


//*****************************************************************************
//***    Chrome Interraction
//*****************************************************************************

NS_METHOD CBrowserWindow::SetStatus(const PRUnichar* aStatus)
{
   if (mStatusBar)
   {
		nsCAutoString cStr;
		
        CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(aStatus), cStr);
		mStatusBar->SetText(const_cast<char *>(cStr.GetBuffer()), cStr.Length());
   }
   return NS_OK;
}

NS_METHOD CBrowserWindow::SetLocation(const nsString& aLocation)
{
	if (mURLField)
	{
		nsCAutoString cStr;
		
        CPlatformUCSConversion::GetInstance()->UCSToPlatform(aLocation, cStr);
		mURLField->SetText(const_cast<char *>(cStr.GetBuffer()), cStr.Length());
	}
   return NS_OK;
}

NS_METHOD CBrowserWindow::OnStatusNetStart(nsIWebProgress *progress, nsIRequest *request,
                                           PRInt32 progressStateFlags, PRUint32 status)
{
	if (mProgressBar) {
	   mProgressBar->Show();
	   mProgressBar->SetIndeterminateFlag(true, true);
   }
   
	if (mThrobber)
		mThrobber->Start();

	if (mStopButton)
		mStopButton->Enable();

    mBusy = true;
    
	// Inform any other interested parties
	// Actually, all of the above stuff should done through
	// broadcasters and listeners. But for demo's sake this
	// better shows what's happening.
	LBroadcaster::BroadcastMessage(msg_OnStartLoadDocument, 0);
   
   return NS_OK;
}

NS_METHOD CBrowserWindow::OnStatusNetStop(nsIWebProgress *progress, nsIRequest *request,
                                          PRInt32 progressStateFlags, PRUint32 status)
{
	if (mThrobber)
		mThrobber->Stop();
		
	if (mProgressBar) {
	   if (mProgressBar->IsIndeterminate())
	      mProgressBar->Stop();
	   mProgressBar->Hide();
	}
	
	// Enable back, forward, stop
	if (mBackButton)
		mBrowserShell->CanGoBack() ? mBackButton->Enable() : mBackButton->Disable();
	if (mForwardButton)
		mBrowserShell->CanGoForward() ? mForwardButton->Enable() : mForwardButton->Disable();
	if (mStopButton)
		mStopButton->Disable();
		
    // If this is our first load, check to see if we need to be sized/shown.    
    if (!mInitialLoadComplete) {
    	if (mSizeToContent) {
    	    SizeToContent();
    	    mSizeToContent = false;
        }
        if (mShowOnInitialLoad) {
            Show();
            Select();
            mShowOnInitialLoad = false;
        }
	    mInitialLoadComplete = true;
    }
	
	mBusy = false;
		
	// Inform any other interested parties
	// Actually, all of the above stuff should done through
	// broadcasters and listeners. But for demo's sake this
	// better shows what's happening.
	LBroadcaster::BroadcastMessage(msg_OnEndLoadDocument, 0);

   return NS_OK;
}


NS_METHOD CBrowserWindow::OnProgressChange(nsIWebProgress *progress, nsIRequest *request,
                                           PRInt32 curSelfProgress, PRInt32 maxSelfProgress, 
                                           PRInt32 curTotalProgress, PRInt32 maxTotalProgress)
{
   if (mProgressBar) {
      if (maxTotalProgress != -1 && mProgressBar->IsIndeterminate())
         mProgressBar->SetIndeterminateFlag(false, false);
      else if (maxTotalProgress == -1 && !mProgressBar->IsIndeterminate())
         mProgressBar->SetIndeterminateFlag(true, true);
      
      if (!mProgressBar->IsIndeterminate()) {
         PRInt32 aMax = max(0, maxTotalProgress);
         PRInt32 aVal = min(aMax, max(0, curTotalProgress));
         mProgressBar->SetMaxValue(aMax);
         mProgressBar->SetValue(aVal);
      }
   }
   return NS_OK;
}
  
NS_METHOD CBrowserWindow::SetVisibility(PRBool aVisibility)
{
    // If we are waiting for content to load in order to size ourself,
    // defer making ourselves visible until the load completes.
    
    if (aVisibility) { 
        if (!mSizeToContent || mInitialLoadComplete) {
            Show();
            Select();
        }
        else
            mShowOnInitialLoad = true;
    }
    else
        Hide();
        
    return NS_OK;
}


NS_METHOD CBrowserWindow::OnShowContextMenu(PRUint32 aContextFlags, nsIDOMEvent *aEvent, nsIDOMNode *aNode)
{
    // Find our CWebBrowserCMAttachment, if any
    CWebBrowserCMAttachment *aCMAttachment = nsnull;
	const TArray<LAttachment*>*	theAttachments = GetAttachmentsList();
    
	if (theAttachments) {
		TArrayIterator<LAttachment*> iterate(*theAttachments);
		
		LAttachment*	theAttach;
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
    
    Boolean bHandleClick = true;
    SInt16  cmdListResID;
    
    if (aContextFlags & nsIContextMenuListener::CONTEXT_LINK) {
        cmdListResID = mcmd_ContextLinkCmds;
    }
    else if (aContextFlags & nsIContextMenuListener::CONTEXT_IMAGE) {
        cmdListResID = mcmd_ContextImageCmds;
    }
    else if (aContextFlags & nsIContextMenuListener::CONTEXT_DOCUMENT) {
        cmdListResID = mcmd_ContextDocumentCmds;
    }
    else if (aContextFlags & nsIContextMenuListener::CONTEXT_TEXT) {
        cmdListResID = mcmd_ContextTextCmds;
    }
    else {
        bHandleClick = false;
        mContextMenuContext = nsIContextMenuListener::CONTEXT_NONE;
        mContextMenuDOMNode = nsnull;
    }    
    

    if (bHandleClick) {
        
        // Call OSEventAvail with an event mask of zero which will return a
        // null event but will fill in the mouse location and modifier keys.
        
        EventRecord macEvent;
        ::OSEventAvail(0, &macEvent);
        
        mContextMenuContext = aContextFlags;
        mContextMenuDOMNode = aNode;
        aCMAttachment->SetCommandList(cmdListResID);
        aCMAttachment->DoContextMenuClick(macEvent);
    }

    return NS_OK;
}
