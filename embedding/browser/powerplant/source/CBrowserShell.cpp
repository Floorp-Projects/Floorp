/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Conrad Carlen <ccarlen@netscape.com>
 */

#ifndef __CBrowserShell__
#include "CBrowserShell.h"
#endif

#include "nsCWebBrowser.h"
#include "nsIComponentManager.h"
#include "nsComponentManagerUtils.h"
#include "nsWidgetsCID.h"
#include "nsRepeater.h"
#include "nsString.h"
#include "nsIWebBrowserChrome.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIInterfaceRequestor.h"
#include "nsIWebProgressListener.h"
#include "nsIDocShellHistory.h"
#include "nsIGlobalHistory.h"
#include "nsIServiceManager.h"
#include "nsIClipboardCommands.h"
#include "nsIWalletService.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLCollection.h"

#include <UModalDialogs.h>
#include <LStream.h>

#include "ApplIDs.h"
#include "CBrowserWindow.h"
#include "CFindComponent.h"
#include "UMacUnicode.h"

// PowerPlant
#ifndef _H_LEditText
#include "LEditText.h"
#endif

#ifndef _H_LCheckBox
#include "LCheckBox.h"
#endif

static NS_DEFINE_IID(kWindowCID, NS_WINDOW_CID);

// CBrowserShell static variables
nsMacMessageSink	CBrowserShell::mMessageSink;


//*****************************************************************************
//***    CBrowserShell: constructors/destructor
//*****************************************************************************

CBrowserShell::CBrowserShell() :
   mFinder(nsnull)
{
	nsresult rv = CommonConstruct();
	if (rv != NS_OK)
	   Throw_Err(NS_ERROR_GET_CODE(rv));   // If this fails, there's no reason to live anymore :(
}


CBrowserShell::CBrowserShell(const SPaneInfo	&inPaneInfo,
						  	 const SViewInfo	&inViewInfo) :
    LView(inPaneInfo, inViewInfo),
    mFinder(nsnull)
{
	nsresult rv = CommonConstruct();
	if (rv != NS_OK)
	   Throw_Err(NS_ERROR_GET_CODE(rv));   // If this fails, there's no reason to live anymore :(
}


CBrowserShell::CBrowserShell(LStream*	inStream) :
	LView(inStream),
	mFinder(nsnull)
{
	*inStream >> (StringPtr) mInitURL;
	
	nsresult rv = CommonConstruct();
	if (rv != NS_OK)
	   Throw_Err(NS_ERROR_GET_CODE(rv));   // If this fails, there's no reason to live anymore :(
}


CBrowserShell::~CBrowserShell()
{
    delete mFinder;

    // nsCOMPtr destructors, do your thing
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

    return NS_OK;
}


//*****************************************************************************
//***    CBrowserShell: LPane overrides
//*****************************************************************************

void CBrowserShell::FinishCreateSelf()
{
	FocusDraw();
	
	CBrowserWindow *ourWindow = dynamic_cast<CBrowserWindow*>(LWindow::FetchWindowObject(GetMacPort()));
	ThrowIfNil_(ourWindow);
	ourWindow->AddListener(this);
	
	nsCOMPtr<nsIWidget>  aWidget;
	ourWindow->GetWidget(getter_AddRefs(aWidget));
	ThrowIfNil_(aWidget);

	Rect portFrame;
	CalcPortFrameRect(portFrame);
	nsRect   r(portFrame.left, portFrame.top, portFrame.right - portFrame.left, portFrame.bottom - portFrame.top);
		
    mWebBrowserAsBaseWin->InitWindow(aWidget->GetNativeData(NS_NATIVE_WIDGET), nsnull, r.x, r.y, r.width, r.height);
    mWebBrowserAsBaseWin->Create();
   
    // Set the global history
    nsCOMPtr<nsIDocShell> docShell(do_GetInterface(mWebBrowser));
    ThrowIfNil_(docShell);
    nsCOMPtr<nsIDocShellHistory> dsHistory(do_QueryInterface(docShell));
    if (dsHistory)
    {
        nsresult rv;
        NS_WITH_SERVICE(nsIGlobalHistory, history, NS_GLOBALHISTORY_CONTRACTID, &rv);  
        if (history)
            dsHistory->SetGlobalHistory(history);
    }
   
    AdjustFrame();   
	StartRepeating();
	StartListening();
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


void CBrowserShell::ShowSelf()
{
    mWebBrowserAsBaseWin->SetVisibility(PR_TRUE);
}


void CBrowserShell::DrawSelf()
{
    EventRecord osEvent;
    osEvent.what = updateEvt;
    mMessageSink.DispatchOSEvent(osEvent, GetMacPort());
}

	
void CBrowserShell::ClickSelf(const SMouseDownEvent	&inMouseDown)
{
	if (!IsTarget())
		SwitchTarget(this);

	FocusDraw();
	mMessageSink.DispatchOSEvent((EventRecord&)inMouseDown.macEvent, GetMacPort());
}


void CBrowserShell::EventMouseUp(const EventRecord	&inMacEvent)
{
    FocusDraw();
    mMessageSink.DispatchOSEvent((EventRecord&)inMacEvent, GetMacPort());
    
    LEventDispatcher *dispatcher = LEventDispatcher::GetCurrentEventDispatcher();
    if (dispatcher)
        dispatcher->UpdateMenus();
}


void CBrowserShell::AdjustCursorSelf(Point				/* inPortPt */,
                                     const EventRecord&	inMacEvent)
{
    static Point	lastWhere = {0, 0};

    if ((*(long*)&lastWhere != *(long*)&inMacEvent.where))
    {
        HandleMouseMoved(inMacEvent);
        lastWhere = inMacEvent.where;
    }
}


//*****************************************************************************
//***    CBrowserShell: LCommander overrides
//*****************************************************************************

void CBrowserShell::BeTarget()
{
}

void CBrowserShell::DontBeTarget()
{
}

Boolean CBrowserShell::HandleKeyPress(const EventRecord	&inKeyEvent)
{
	// set the QuickDraw origin
	FocusDraw();

	// dispatch the event
	Boolean keyHandled = mMessageSink.DispatchOSEvent((EventRecord&)inKeyEvent, GetMacPort());

	return keyHandled;
}

Boolean CBrowserShell::ObeyCommand(PP_PowerPlant::CommandT inCommand, void* ioParam)
{
	Boolean		cmdHandled = true;

    nsresult rv;
    nsCOMPtr<nsIClipboardCommands> clipCmd;

    switch (inCommand)
    {
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
                clipCmd->PasteSelection();
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

		case cmd_SaveFormData:
            {
                nsCOMPtr<nsIDOMWindow> domWindow;
                nsCOMPtr<nsIDOMWindowInternal> domWindowInternal;

                NS_WITH_SERVICE(nsIWalletService, walletService, NS_WALLETSERVICE_CONTRACTID, &rv);
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

                NS_WITH_SERVICE(nsIWalletService, walletService, NS_WALLETSERVICE_CONTRACTID, &rv);
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

        default:
            cmdHandled = LCommander::ObeyCommand(inCommand, ioParam);
            break;
    }
    return cmdHandled;
}


void CBrowserShell::FindCommandStatus(PP_PowerPlant::CommandT inCommand,
            		                  Boolean &outEnabled, Boolean &outUsesMark,
            					      PP_PowerPlant::Char16 &outMark, Str255 outName)
{
    nsresult rv;
    nsCOMPtr<nsIClipboardCommands> clipCmd;
    PRBool canDo;

    switch (inCommand)
    {
        case cmd_Cut:
            rv = GetClipboardHandler(getter_AddRefs(clipCmd));
            if (NS_SUCCEEDED(rv)) {
                rv = clipCmd->CanCutSelection(&canDo);
                outEnabled = NS_SUCCEEDED(rv) && canDo;
            }
            break;

        case cmd_Copy:
            rv = GetClipboardHandler(getter_AddRefs(clipCmd));
            if (NS_SUCCEEDED(rv)) {
                rv = clipCmd->CanCopySelection(&canDo);
                outEnabled = NS_SUCCEEDED(rv) && canDo;
            }
            break;

        case cmd_Paste:
            rv = GetClipboardHandler(getter_AddRefs(clipCmd));
            if (NS_SUCCEEDED(rv)) {
                rv = clipCmd->CanPasteSelection(&canDo);
                outEnabled = NS_SUCCEEDED(rv) && canDo;
            }
            break;

        case cmd_SelectAll:
            outEnabled = PR_TRUE;
            break;

		case cmd_Find:
			outEnabled = true;
			break;

		case cmd_FindNext:
			outEnabled = CanFindNext();
			break;
        
		case cmd_SaveFormData:
		    outEnabled = HasFormElements();
		    break;

		case cmd_PrefillForm:
		    outEnabled = HasFormElements();
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
        case osEvt:
        {
            // The MacMessageSink will not set the cursor if we are in the background - which is right.
            // We have to feed it suspendResumeMessages for it to know

            unsigned char eventType = ((inMacEvent.message >> 24) & 0x00ff);
            if (eventType == suspendResumeMessage)
            mMessageSink.DispatchOSEvent(const_cast<EventRecord&>(inMacEvent), GetMacPort());
        }
        break;
    }
}

//*****************************************************************************
//***    CBrowserShell: LListener overrides
//*****************************************************************************

void CBrowserShell::ListenToMessage(MessageT			inMessage,
									void*				ioParam)
{
	switch (inMessage)
	{
		case msg_OnStartLoadDocument:
		case msg_OnEndLoadDocument:
			break;
	}
}


//*****************************************************************************
//***    CBrowserShell:
//*****************************************************************************

NS_IMETHODIMP CBrowserShell::SetTopLevelWindow(nsIWebBrowserChrome * aTopLevelWindow)
{
    mWebBrowser->SetContainerWindow(aTopLevelWindow);  

    /*
    In case we needed to do something with the underlying docshell...   

    nsCOMPtr<nsIDocShell> ourDocShell(do_GetInterface(mWebBrowser));
    NS_ENSURE_TRUE(ourDocShell, NS_ERROR_FAILURE);
    */
     
    return NS_OK;
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

    CBrowserWindow *ourWindow = dynamic_cast<CBrowserWindow*>(LWindow::FetchWindowObject(GetMacPort()));
    NS_ENSURE_TRUE(ourWindow, NS_ERROR_FAILURE);

    nsCOMPtr<nsIWidget>  aWidget;
    ourWindow->GetWidget(getter_AddRefs(aWidget));
    NS_ENSURE_TRUE(aWidget, NS_ERROR_FAILURE);

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
    	
    mWebBrowserAsBaseWin->InitWindow(aWidget->GetNativeData(NS_NATIVE_WIDGET), nsnull, r.x, r.y, r.width, r.height);
    mWebBrowserAsBaseWin->Create();

    AdjustFrame();   

    return NS_OK;
}

NS_METHOD CBrowserShell::GetContentViewer(nsIContentViewer** aViewer)
{
    nsCOMPtr<nsIDocShell> ourDocShell(do_GetInterface(mWebBrowser));
    NS_ENSURE_TRUE(ourDocShell, NS_ERROR_FAILURE);
    return ourDocShell->GetContentViewer(aViewer);
}


//*****************************************************************************
//***    CBrowserShell: Navigation
//*****************************************************************************

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
   return mWebBrowserAsWebNav->Stop();
}

//*****************************************************************************
//***    CBrowserShell: URL Loading
//*****************************************************************************

NS_METHOD CBrowserShell::LoadURL(const char* urlText, SInt32 urlTextLen)
{
    nsAutoString urlString;
    
    if (urlTextLen == -1)
        urlString.AssignWithConversion(urlText);
    else
        urlString.AssignWithConversion(urlText, urlTextLen);
        
    return LoadURL(urlString);
}


NS_METHOD CBrowserShell::LoadURL(const nsString& urlText)
{
   return mWebBrowserAsWebNav->LoadURI(urlText.GetUnicode(), nsIWebNavigation::LOAD_FLAGS_NONE);
}


//*****************************************************************************
//***    CBrowserShell: Text Searching
//*****************************************************************************

Boolean CBrowserShell::Find()
{
    // Make sure we have a finder
    nsresult rv = EnsureFinder();
    if (NS_FAILED(rv))   // Throw an exception??
      return FALSE;

    // Get the current find params from it
    nsString  searchString;
    PRBool   caseSensitive;
    PRBool   wrapSearch;
    PRBool   seachBackwards;
    PRBool   entireWord;

    mFinder->GetLastSearchString(searchString);
    mFinder->GetLastCaseSensitive(caseSensitive);
    mFinder->GetLastWrapSearch(wrapSearch);
    mFinder->GetLastSearchBackwards(seachBackwards);
    mFinder->GetLastEntireWord(entireWord);

    Boolean   result = FALSE;

    if (GetFindParams(searchString, caseSensitive, seachBackwards, wrapSearch, entireWord))
    {
        PRBool  didFind;

        mFinder->Find(searchString, caseSensitive, seachBackwards, wrapSearch, entireWord, didFind);
        result = (NS_SUCCEEDED(rv) && didFind);
        if (!result)
          ::SysBeep(1);
    }
    return result;
}


Boolean CBrowserShell::Find(const nsString& searchString,
                                Boolean caseSensitive,
                                Boolean searchBackwards,
                                Boolean wrapSearch,
                                Boolean entireWord)
{
    // Make sure we have a finder
    nsresult rv = EnsureFinder();
    if (NS_FAILED(rv))   // Throw an exception??
    return FALSE;

    Boolean   result;
    PRBool   didFind;

    rv = mFinder->Find(searchString, caseSensitive, searchBackwards, wrapSearch, entireWord, didFind);
    result = (NS_SUCCEEDED(rv) && didFind);
    if (!result)
        ::SysBeep(1);

    return result;
}

Boolean CBrowserShell::CanFindNext()
{
    if (!mFinder)
        return FALSE;

    nsresult    rv;
    PRBool      canDo;
     
    rv = mFinder->CanFindNext(canDo);
    return (NS_SUCCEEDED(rv) && canDo);
}


Boolean CBrowserShell::FindNext()
{
    if (!mFinder)
    {
        SignalStringLiteral_("This should not be called until Find is called");
        return FALSE;
    }

    Boolean   result;
    PRBool   didFind;

    nsresult rv = mFinder->FindNext(didFind);
    result = (NS_SUCCEEDED(rv) && didFind);
    if (!result)
        ::SysBeep(1);

    return result;
}


void CBrowserShell::HandleMouseMoved(const EventRecord& inMacEvent)
{
    if (IsActive())
    {
        FocusDraw();
        mMessageSink.DispatchOSEvent(const_cast<EventRecord&>(inMacEvent), GetMacPort());
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


NS_METHOD CBrowserShell::EnsureFinder()
{
    nsCOMPtr<nsIDocShell> ourDocShell(do_GetInterface(mWebBrowser));

    if (mFinder)
    {
        // For now, we have to clear the context each time.
        // The finder should be a nsIDocumentLoaderObserver and then
        // it could clear itself when the contents of our docshell changed.

        mFinder->SetContext(nsnull);
        mFinder->SetContext(ourDocShell);
        return NS_OK;
    }
      
    mFinder = new CFindComponent;
    NS_ENSURE_TRUE(mFinder, NS_ERROR_OUT_OF_MEMORY);   
    mFinder->SetContext(ourDocShell);

    return NS_OK;
}


Boolean CBrowserShell::GetFindParams(nsString& searchText,
                                     PRBool& caseSensitive,
                                     PRBool& searchBackwards,
                                     PRBool& wrapSearch,
                                     PRBool& entireWord)
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
      Str255      aStr;

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
        wrapAroundCheck->SetValue(wrapSearch ? 1 : 0);
        backwardsCheck = dynamic_cast<LCheckBox*>(theDialog->FindPaneByID(kSearchBackwardsCheck));
        ThrowIfNot_(backwardsCheck);
        backwardsCheck->SetValue(searchBackwards ? 1 : 0);

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
                wrapSearch = wrapAroundCheck->GetValue() ? TRUE : FALSE;
                searchBackwards = backwardsCheck->GetValue() ? TRUE : FALSE;

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

    nsCOMPtr<nsIDocShell> docShell(do_GetInterface(mWebBrowser));
    NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);
    nsCOMPtr<nsIClipboardCommands> clipCmd(do_QueryInterface(docShell));
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
