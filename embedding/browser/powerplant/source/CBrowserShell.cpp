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
#include "nsWidgetsCID.h"
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
#include "nsIWebBrowserFind.h"
#include "nsIWebBrowserFocus.h"
#include "nsIWebBrowserPersist.h"
#include "nsIURI.h"
#include "nsWeakPtr.h"
#include "nsRect.h"
#include "nsReadableUtils.h"
#include "nsILocalFile.h"
#include "nsILocalFileMac.h"

#include <UModalDialogs.h>
#include <LStream.h>

#include "ApplIDs.h"
#include "CBrowserWindow.h"
#include "UMacUnicode.h"

// PowerPlant
#ifndef _H_LEditText
#include "LEditText.h"
#endif

#ifndef _H_LCheckBox
#include "LCheckBox.h"
#endif

#if PP_Target_Carbon
#include <UNavServicesDialogs.h>
#else
#include <UConditionalDialogs.h>
#endif

static NS_DEFINE_IID(kWindowCID, NS_WINDOW_CID);

// CBrowserShell static variables
nsMacMessageSink	CBrowserShell::mMessageSink;


//*****************************************************************************
//***    CBrowserShell: constructors/destructor
//*****************************************************************************

CBrowserShell::CBrowserShell()
{
	nsresult rv = CommonConstruct();
	if (rv != NS_OK)
	   Throw_Err(NS_ERROR_GET_CODE(rv));   // If this fails, there's no reason to live anymore :(
}


CBrowserShell::CBrowserShell(const SPaneInfo	&inPaneInfo,
						  	 const SViewInfo	&inViewInfo) :
    LView(inPaneInfo, inViewInfo)
{
	nsresult rv = CommonConstruct();
	if (rv != NS_OK)
	   Throw_Err(NS_ERROR_GET_CODE(rv));   // If this fails, there's no reason to live anymore :(
}


CBrowserShell::CBrowserShell(LStream*	inStream) :
	LView(inStream)
{
	*inStream >> (StringPtr) mInitURL;
	
	nsresult rv = CommonConstruct();
	if (rv != NS_OK)
	   Throw_Err(NS_ERROR_GET_CODE(rv));   // If this fails, there's no reason to live anymore :(
}


CBrowserShell::~CBrowserShell()
{
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
	
	CBrowserWindow *ourWindow = dynamic_cast<CBrowserWindow*>(LWindow::FetchWindowObject(Compat_GetMacWindow()));
	ThrowIfNil_(ourWindow);
	ourWindow->AddListener(this);
	
	nsCOMPtr<nsIWidget>  aWidget;
	ourWindow->GetWidget(getter_AddRefs(aWidget));
	ThrowIfNil_(aWidget);

	Rect portFrame;
	CalcPortFrameRect(portFrame);
	nsRect   r(portFrame.left, portFrame.top, portFrame.right - portFrame.left, portFrame.bottom - portFrame.top);
	
	nsresult rv;
	
	nsCOMPtr<nsIWebBrowserChrome> ourChrome;
	ourWindow->GetIWebBrowserChrome(getter_AddRefs(ourChrome));
	ThrowIfNil_(ourChrome);

    mWebBrowser->SetContainerWindow(ourChrome);  		
    mWebBrowserAsBaseWin->InitWindow(aWidget->GetNativeData(NS_NATIVE_WIDGET), nsnull, r.x, r.y, r.width, r.height);
    mWebBrowserAsBaseWin->Create();
    
    nsWeakPtr weakling(dont_AddRef(NS_GetWeakReference(ourChrome)));
    rv = mWebBrowser->AddWebBrowserListener(weakling, NS_GET_IID(nsIWebProgressListener));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Call to AddWebBrowserListener failed");
      
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
    mMessageSink.DispatchOSEvent(osEvent, Compat_GetMacWindow());
}

	
void CBrowserShell::ClickSelf(const SMouseDownEvent	&inMouseDown)
{
	if (!IsTarget())
		SwitchTarget(this);

	FocusDraw();
	mMessageSink.DispatchOSEvent((EventRecord&)inMouseDown.macEvent, Compat_GetMacWindow());
}


void CBrowserShell::EventMouseUp(const EventRecord	&inMacEvent)
{
    FocusDraw();
    mMessageSink.DispatchOSEvent((EventRecord&)inMacEvent, Compat_GetMacWindow());
    
    LEventDispatcher *dispatcher = LEventDispatcher::GetCurrentEventDispatcher();
    if (dispatcher)
        dispatcher->UpdateMenus();
}

#if __PowerPlant__ >= 0x02200000
void CBrowserShell::AdjustMouseSelf(Point				inPortPt,
                                    const EventRecord&	inMacEvent,
                                    RgnHandle			outMouseRgn)
{
    static Point	lastWhere = {0, 0};

    if ((*(long*)&lastWhere != *(long*)&inMacEvent.where))
    {
        HandleMouseMoved(inMacEvent);
        lastWhere = inMacEvent.where;
    }
    Rect cursorRect = { inPortPt.h, inPortPt.v, inPortPt.h + 1, inPortPt.v + 1 };
    ::RectRgn(outMouseRgn, &cursorRect);
}

#else

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
#endif

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
	Boolean keyHandled = mMessageSink.DispatchOSEvent((EventRecord&)inKeyEvent, Compat_GetMacWindow());

	return keyHandled;
}

Boolean CBrowserShell::ObeyCommand(PP_PowerPlant::CommandT inCommand, void* ioParam)
{
	Boolean		cmdHandled = true;

    nsresult rv;
    nsCOMPtr<nsIClipboardCommands> clipCmd;

    switch (inCommand)
    {
        case cmd_SaveAs:
            rv = SaveCurrentURI();
            ThrowIfError_(rv);
            break;
            
        case cmd_SaveAllAs:
            rv = SaveDocument();
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
    PRBool canDo;
    nsCOMPtr<nsIURI> currURI;

    switch (inCommand)
    {
        case cmd_SaveAs:
        case cmd_SaveAllAs:
            rv = mWebBrowserAsWebNav->GetCurrentURI(getter_AddRefs(currURI));
            outEnabled = NS_SUCCEEDED(rv);
            break;
            
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
                rv = clipCmd->CanPaste(&canDo);
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
            mMessageSink.DispatchOSEvent(const_cast<EventRecord&>(inMacEvent), Compat_GetMacWindow());
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
    return mWebBrowser->SetContainerWindow(aTopLevelWindow);
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

    CBrowserWindow *ourWindow = dynamic_cast<CBrowserWindow*>(LWindow::FetchWindowObject(Compat_GetMacWindow()));
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
   return mWebBrowserAsWebNav->Stop(nsIWebNavigation::STOP_ALL);
}

NS_METHOD CBrowserShell::Reload()
{
    return mWebBrowserAsWebNav->Stop(nsIWebNavigation::LOAD_FLAGS_NONE);
}

//*****************************************************************************
//***    CBrowserShell: URL Loading
//*****************************************************************************


NS_METHOD CBrowserShell::LoadURL(const nsACString& urlText)
{
   nsAutoString unicodeURL;
   CopyASCIItoUCS2(urlText, unicodeURL);
   return mWebBrowserAsWebNav->LoadURI(unicodeURL.get(), nsIWebNavigation::LOAD_FLAGS_NONE);
}

NS_METHOD CBrowserShell::GetCurrentURL(nsACString& urlText)
{
    nsresult rv;
    nsCOMPtr<nsIURI> currentURI;
    rv = mWebBrowserAsWebNav->GetCurrentURI(getter_AddRefs(currentURI));
    if (NS_FAILED(rv)) return rv;
    nsXPIDLCString urlPath;
    rv = currentURI->GetSpec(getter_Copies(urlPath));
    if (NS_FAILED(rv)) return rv;
    urlText.Assign(urlPath.get());
    
    return NS_OK;
}

//*****************************************************************************
//***    CBrowserShell: URI Saving
//*****************************************************************************

NS_METHOD CBrowserShell::SaveDocument()
{
    FSSpec      fileSpec;
    Boolean     isReplacing;
    nsresult    rv = NS_OK;
    
    if (DoSaveFileDialog(fileSpec, isReplacing)) {
        if (isReplacing) {
            OSErr err = ::FSpDelete(&fileSpec);
            if (err) return NS_ERROR_FAILURE;
        }
        rv = SaveDocument(fileSpec);
    }
    return rv;
}

NS_METHOD CBrowserShell::SaveCurrentURI()
{
    FSSpec      fileSpec;
    Boolean     isReplacing;
    nsresult    rv = NS_OK;
    
    if (DoSaveFileDialog(fileSpec, isReplacing)) {
        if (isReplacing) {
            OSErr err = ::FSpDelete(&fileSpec);
            if (err) return NS_ERROR_FAILURE;
        }
        rv = SaveCurrentURI(fileSpec);
    }
    return rv;
}

NS_METHOD CBrowserShell::SaveDocument(const FSSpec& outSpec)
{
    nsresult    rv;
    
    nsCOMPtr<nsIWebBrowserPersist> wbPersist(do_GetInterface(mWebBrowser, &rv));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIDOMDocument> domDoc;
    rv = mWebBrowserAsWebNav->GetDocument(getter_AddRefs(domDoc));
    if (NS_FAILED(rv)) return rv;

    FSSpec nonConstOutSpec = outSpec;    
    nsCOMPtr<nsILocalFileMac> localMacFile;
    rv = NS_NewLocalFileWithFSSpec(&nonConstOutSpec, PR_FALSE, getter_AddRefs(localMacFile));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsILocalFile> localFile(do_QueryInterface(localMacFile, &rv));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIFile> parentDir;
    rv = localFile->GetParent(getter_AddRefs(parentDir));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsILocalFile> parentDirAsLocal(do_QueryInterface(parentDir, &rv));
    if (NS_FAILED(rv)) return rv;

    rv = wbPersist->SaveDocument(domDoc, localFile, parentDirAsLocal);
    
    return rv;
}

NS_METHOD CBrowserShell::SaveCurrentURI(const FSSpec& outSpec)
{
    nsresult    rv;
    
    nsCOMPtr<nsIWebBrowserPersist> wbPersist(do_GetInterface(mWebBrowser, &rv));
    if (NS_FAILED(rv)) return rv;

    FSSpec nonConstOutSpec = outSpec;    
    nsCOMPtr<nsILocalFileMac> localMacFile;
    rv = NS_NewLocalFileWithFSSpec(&nonConstOutSpec, PR_FALSE, getter_AddRefs(localMacFile));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsILocalFile> localFile(do_QueryInterface(localMacFile, &rv));
    if (NS_FAILED(rv)) return rv;

    rv = wbPersist->SaveURI(nsnull, nsnull, localFile);
    
    return rv;
}

Boolean CBrowserShell::DoSaveFileDialog(FSSpec& outSpec, Boolean& outIsReplacing)
{
#if PP_Target_Carbon
	UNavServicesDialogs::LFileDesignator	designator;
#else
	UConditionalDialogs::LFileDesignator	designator;
#endif

    nsresult rv;
    nsAutoString docTitle;
    Str255       defaultName;

    nsCOMPtr<nsIDOMDocument> domDoc;
    rv = mWebBrowserAsWebNav->GetDocument(getter_AddRefs(domDoc));
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIDOMHTMLDocument> htmlDoc(do_QueryInterface(domDoc, &rv));
        if (NS_SUCCEEDED(rv))
            htmlDoc->GetTitle(docTitle);
    }
    
    // For now, we'll assume that we're saving HTML
    NS_NAMED_LITERAL_STRING(htmlSuffix, ".html");
    if (docTitle.IsEmpty())
        docTitle.AssignWithConversion("untitled");
    else {
        if (docTitle.Length() > 31 - htmlSuffix.Length())
            docTitle.Truncate(31 - htmlSuffix.Length());
    }
    docTitle.Append(htmlSuffix);
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(docTitle, defaultName);

    Boolean     result;
    
    result = designator.AskDesignateFile(defaultName);
    if (result) {
        designator.GetFileSpec(outSpec);
        outIsReplacing = designator.IsReplacing();
    }
    
    return result;
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


Boolean CBrowserShell::Find(const nsString& searchString,
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

    finder->SetSearchString(searchString.get());
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
    return (NS_SUCCEEDED(rv) && nsCRT::strlen(searchStr) != 0);
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


void CBrowserShell::HandleMouseMoved(const EventRecord& inMacEvent)
{
    if (IsActive())
    {
        FocusDraw();
        mMessageSink.DispatchOSEvent(const_cast<EventRecord&>(inMacEvent), Compat_GetMacWindow());
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


Boolean CBrowserShell::DoFindDialog(nsString& searchText,
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
