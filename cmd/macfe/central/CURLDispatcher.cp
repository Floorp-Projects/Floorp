/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

//	CURLDispatcher.cp


#include "CURLDispatcher.h"
#include "CNSContext.h"
#include "CBrowserWindow.h"
#include "CDownloadProgressWindow.h"
#include "CWindowMediator.h"
#include "CBrowserContext.h"
#include "CBrowserWindow.h"
#ifdef MOZ_MAIL_NEWS
#include "CMailNewsWindow.h"
#include "CThreadWindow.h"
#include "CMessageWindow.h"
#endif
#include "URobustCreateWindow.h"
#include "uapp.h"
#include "CHTMLClickRecord.h"
#include "CAutoPtr.h"
#include "CAutoPtrXP.h"
#include "cstring.h"

#include "xp.h"
#include "macutil.h"
#include "umimemap.h"
#include "ufilemgr.h"
#include "uprefd.h"
#include "xlate.h"
#include "msv2dsk.h"
#include "msgcom.h"
#include "uerrmgr.h"		// Need for GetCString
#include "resgui.h"

CBrowserWindow* CURLDispatcher::sLastBrowserWindowCreated = NULL;
CAutoPtr<CURLDispatcher> CURLDispatcher::sDispatcher;
CAutoPtr<CBrowserContext> CURLDispatcher::sDispatchContext;

// URL dispatch proc table
// URL types listed in net.h are indices into this table.
// **** NOTE: URL types in net.h start at 1 ****
const Uint32 cNumURLTypes = 39;

static DispatchProcPtr dispatchProcs[] =
{
	CURLDispatcher::DispatchToBrowserWindow		// Unknown URL type        0
,	CURLDispatcher::DispatchToBrowserWindow		// FILE_TYPE_URL           1
,	CURLDispatcher::DispatchToBrowserWindow		// FTP_TYPE_URL            2
,	CURLDispatcher::DispatchToBrowserWindow		// GOPHER_TYPE_URL         3
,	CURLDispatcher::DispatchToBrowserWindow		// HTTP_TYPE_URL           4
,	CURLDispatcher::DispatchToLibNet			// MAILTO_TYPE_URL         5
//,	CURLDispatcher::DispatchToMailNewsWindow	// NEWS_TYPE_URL           6
,	CURLDispatcher::DispatchMailboxURL			// NEWS_TYPE_URL           6 (use mailbox code)
,	NULL		// RLOGIN_TYPE_URL         7
,	CURLDispatcher::DispatchToBrowserWindow		// TELNET_TYPE_URL         8
,	CURLDispatcher::DispatchToBrowserWindow		// TN3270_TYPE_URL         9
,	NULL		// WAIS_TYPE_URL           10
,	CURLDispatcher::DispatchToBrowserWindow		// ABOUT_TYPE_URL          11
,	NULL		// FILE_CACHE_TYPE_URL     12
,	NULL		// MEMORY_CACHE_TYPE_URL   13
,	CURLDispatcher::DispatchToBrowserWindow		// SECURE_HTTP_TYPE_URL    14
,	NULL		// INTERNAL_IMAGE_TYPE_URL 15
,	NULL		// URN_TYPE_URL            16
,	NULL		// POP3_TYPE_URL           17
,	CURLDispatcher::DispatchMailboxURL			// MAILBOX_TYPE_URL        18
,	NULL		// INTERNAL_NEWS_TYPE_URL  19
,	CURLDispatcher::DispatchToBrowserWindow		// SECURITY_TYPE_URL		20
,	CURLDispatcher::DispatchToBrowserWindow		// MOCHA_TYPE_URL			21
,	CURLDispatcher::DispatchToBrowserWindow		// VIEW_SOURCE_TYPE_URL	22
,	NULL		// HTML_DIALOG_HANDLER_TYPE_URL 23
,	NULL		// HTML_PANEL_HANDLER_TYPE_URL 24
,	NULL		// INTERNAL_SECLIB_TYPE_URL 25
,	NULL		// MSG_SEARCH_TYPE_URL     26
,	CURLDispatcher::DispatchMailboxURL			// IMAP_TYPE_URL			27
,	CURLDispatcher::DispatchToLibNet			// LDAP_TYPE_URL			28
,	NULL		// SECURE_LDAP_TYPE_URL	29
,	CURLDispatcher::DispatchToBrowserWindow		// WYSIWYG_TYPE_URL		30
,	CURLDispatcher::DispatchToLibNet			// ADDRESS_BOOK_TYPE_URL	31
,	NULL		// CLASSID_TYPE_URL		32
,	NULL		// JAVA_TYPE_URL			33
,	NULL		// DATA_TYPE_URL			34
,	CURLDispatcher::DispatchToLibNet		// NETHELP_TYPE_URL		35
,	NULL		// NFS_TYPE_URL		    36
,	CURLDispatcher::DispatchToBrowserWindow		// MARIMBA_TYPE_URL	    37
,	NULL		// INTERNAL_CERTLDAP_TYPE_URL 38
};

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CURLDispatcher* CURLDispatcher::GetURLDispatcher()	// singleton class
{
	if (!sDispatcher.get())
	{
		sDispatcher.reset(new CURLDispatcher);
	}
	
	return sDispatcher.get();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CURLDispatcher::CURLDispatcher()
	:	mDelayedURLs(sizeof(CURLDispatchInfo*))
{
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CURLDispatcher::~CURLDispatcher()
{
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CURLDispatcher::DispatchToStorage(
	URL_Struct*				inURL,
	const FSSpec&			inDestSpec,
	FO_Present_Types		inOutputFormat,
	Boolean					inDelay)
{
	Assert_((inOutputFormat == FO_SAVE_AS) || (inOutputFormat == FO_SAVE_AS_TEXT));
	
	if (!inDelay)
		{
		CURLDispatchInfo* dispatchInfo = 
			new CURLDispatchInfo(inURL, nil, inOutputFormat, inDelay, false, true);
		dispatchInfo->SetFileSpec(inDestSpec);
		if (inOutputFormat == FO_SAVE_AS)
		{
			DispatchToDisk(dispatchInfo);
		}
		else
			DispatchToDiskAsText(dispatchInfo);
		}
	else
		{
		CURLDispatchInfo* theDelay =
			new CURLDispatchInfo(inURL, nil, inOutputFormat, true, false, true);
		theDelay->SetFileSpec(inDestSpec);
		GetURLDispatcher()->PostPendingDispatch(theDelay);
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CURLDispatcher::DispatchToStorage(CURLDispatchInfo* inDispatchInfo)
{
	Assert_((inDispatchInfo->GetOutputFormat() == FO_SAVE_AS) || (inDispatchInfo->GetOutputFormat() == FO_SAVE_AS_TEXT));
	
	if (!inDispatchInfo->GetDelay())
		{
		if (inDispatchInfo->GetOutputFormat() == FO_SAVE_AS)
			DispatchToDisk(inDispatchInfo);
		else
			DispatchToDiskAsText(inDispatchInfo);
		}
	else
		{
		GetURLDispatcher()->PostPendingDispatch(inDispatchInfo);
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CURLDispatcher::SpendTime(const EventRecord& /*inMacEvent*/)
{
	if (mDelayedURLs.GetCount() > 0)
		ProcessPendingDispatch();
	else
		StopIdling();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
		
void CURLDispatcher::ListenToMessage(
	MessageT				inMessage,
	void*					ioParam)
{
	if ((inMessage == msg_BroadcasterDied) && (mDelayedURLs.GetCount() > 0))
		UpdatePendingDispatch((CNSContext*)ioParam);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


void CURLDispatcher::DispatchToDisk(CURLDispatchInfo* inDispatchInfo)
{
	CBrowserContext* theContext = NULL;
	CDownloadProgressWindow* theProgressWindow = NULL;
	FSSpec& destSpec = inDispatchInfo->GetFileSpec();
	URL_Struct* inURL = inDispatchInfo->GetURLStruct();
	
	CAutoPtr<CURLDispatchInfo> info(inDispatchInfo);

	Assert_(inURL != NULL);

	try
		{
		theContext = new CBrowserContext(MWContextSaveToDisk);
		StSharer theShareLock(theContext);
		
		theProgressWindow = dynamic_cast<CDownloadProgressWindow*>(URobustCreateWindow::CreateWindow(WIND_DownloadProgress, LCommander::GetTopCommander()));
		ThrowIfNULL_(theProgressWindow);
		theProgressWindow->Show();
		
		inURL->fe_data = StructCopy(&destSpec, sizeof(FSSpec));
		theProgressWindow->SetWindowContext(theContext);
		// the window will be shown on the first progress call.
		
		theContext->ImmediateLoadURL(inDispatchInfo->ReleaseURLStruct(), FO_SAVE_AS);
		}
	catch (...)
		{
		delete theProgressWindow;		
		throw;
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


extern "C" void SaveAsCompletionProc( PrintSetup* p );


void CURLDispatcher::DispatchToDiskAsText(CURLDispatchInfo* inDispatchInfo)
{
	
	CNSContext* theContext = NULL;
	CDownloadProgressWindow* theProgressWindow = NULL;
	FSSpec& destSpec = inDispatchInfo->GetFileSpec();
	URL_Struct* inURL = inDispatchInfo->GetURLStruct();
	
	Assert_(inURL != NULL);

	try
	{
		theContext = new CNSContext(MWContextSaveToDisk);
		StSharer theShareLock(theContext);
		
		theProgressWindow = dynamic_cast<CDownloadProgressWindow*>(URobustCreateWindow::CreateWindow(WIND_DownloadProgress, LCommander::GetTopCommander()));
		ThrowIfNULL_(theProgressWindow);
		theProgressWindow->SetWindowContext(theContext);
		
		CMimeMapper *theMapper = CPrefs::sMimeTypes.FindMimeType(CMimeList::HTMLViewer);
		OSType creator = emSignature, docType='TEXT';
		if (theMapper != NULL && CMimeMapper::Launch == theMapper->GetLoadAction())
		{
			creator = theMapper->GetAppSig();
			docType = theMapper->GetDocType();
		}
		
		OSErr theErr = ::FSpCreate(&destSpec, creator, docType, 0);
		if ((theErr != noErr) && (theErr != dupFNErr))
			ThrowIfOSErr_(theErr);
		
		CFileMgr::FileSetComment(destSpec, NET_URLStruct_Address(inURL));
		
		char* thePath = CFileMgr::EncodedPathNameFromFSSpec(destSpec, TRUE);
		ThrowIfNULL_(thePath);
		
		thePath = NET_UnEscape(thePath);
		XP_File theFile = XP_FileOpen(thePath, xpURL, XP_FILE_WRITE);
		XP_FREE(thePath);
		ThrowIfNULL_(theFile);
		
		PrintSetup print;
		XL_InitializeTextSetup(&print);
		print.width = 76;
		print.out = theFile;
		print.completion = (XL_CompletionRoutine) SaveAsCompletionProc;
		print.carg = (void*)(theContext);
		print.filename = nil;
		print.url = inURL;
		inURL->fe_data = theContext;
		
		MWContext* textContext = (MWContext*) XL_TranslateText(*theContext, inURL, &print);
	}
	catch(...)
	{
	}
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CURLDispatcher::PostPendingDispatch(CURLDispatchInfo* inDispatchInfo)
{
	mDelayedURLs.InsertItemsAt(1, LArray::index_Last, &inDispatchInfo, sizeof(CURLDispatchInfo*));
	StartIdling();
	if (inDispatchInfo->GetTargetContext() != NULL)
		inDispatchInfo->GetTargetContext()->AddListener(this);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CURLDispatcher::UpdatePendingDispatch(
	CNSContext*				inForContext)
{
	Assert_(inForContext != NULL);
	
	CURLDispatchInfo* theInfo;
	LArrayIterator theIter(mDelayedURLs, LArrayIterator::from_Start);
	while (theIter.Next(&theInfo))
		{
		if (theInfo->GetTargetContext() == inForContext)
			mDelayedURLs.RemoveItemsAt(1, theIter.GetCurrentIndex());
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CURLDispatcher::ProcessPendingDispatch(void)
{
	if (CFrontApp::GetApplication()->HasProperlyStartedUp())
	{
		CURLDispatchInfo* theInfo;
		mDelayedURLs.FetchItemAt(LArray::index_First, &theInfo);
		// 97-06-10 pkc -- Hack to workaround trying to dispatch URL's on image
		// anchors while mocha is loading image
		CBrowserContext* browserContext =
			dynamic_cast<CBrowserContext*>(theInfo->GetTargetContext());
		if (theInfo->GetIsWaitingForMochaImageLoad() &&
			theInfo->GetTargetContext())
		{
			if (browserContext && browserContext->IsMochaLoadingImages())
			{
				// The context is loading images for mocha, don't
				// perform dispatch
				return;
			}
		}
		mDelayedURLs.RemoveItemsAt(1, LArray::index_First);
		
		if (theInfo->GetTargetContext() != NULL)
			theInfo->GetTargetContext()->RemoveListener(this);

		theInfo->ClearDelay();

		if (theInfo->GetIsSaving())
			DispatchToStorage(theInfo);
		else
		{
			// See if this delayed URL was for an ftp drag &drop
			if (theInfo->GetURLStruct()->files_to_post)
			{
				// See if the user really meant to upload
				if (browserContext && !browserContext->Confirm((const char*)GetCString(MAC_UPLOAD_TO_FTP))) /* l10n */
				{
					// Delete the info if not
					delete theInfo;
				}
				else
				{
					// Ship it!
					DispatchURL(theInfo);
				}
			}
			else
			{
				// Plain ordinary delayed URL
				DispatchURL(theInfo);
			}
		}
	}
}


// 97-05-13 pkc
// New URL dispatch mechanism.

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CURLDispatcher::DispatchURL(
	const char*				inURL,
	CNSContext*				inTargetContext,
	Boolean					inDelay,
	Boolean					inForceCreate,
	ResIDT					inWindowResID,
	Boolean					inInitiallyVisible,
	FO_Present_Types		inOutputFormat,
	NET_ReloadMethod		inReloadMethod)
{
	CURLDispatchInfo* dispatchInfo =
		new CURLDispatchInfo(
			inURL,
			inTargetContext,
			inOutputFormat,
			inReloadMethod,
			inDelay,
			inForceCreate,
			false,
			inWindowResID,
			inInitiallyVisible
			);
	DispatchURL(dispatchInfo);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CURLDispatcher::DispatchURL(
	URL_Struct*				inURLStruct,
	CNSContext*				inTargetContext,
	Boolean					inDelay,
	Boolean					inForceCreate,
	ResIDT					inWindowResID,
	Boolean					inInitiallyVisible,
	FO_Present_Types		inOutputFormat,
	Boolean					inWaitingForMochaImageLoad)
{
	CURLDispatchInfo* dispatchInfo =
		new CURLDispatchInfo(
			inURLStruct,
			inTargetContext,
			inOutputFormat,
			inDelay,
			inForceCreate,
			false,
			inWindowResID,
			inInitiallyVisible,
			inWaitingForMochaImageLoad
			);
	DispatchURL(dispatchInfo);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CURLDispatcher::DispatchURL(CURLDispatchInfo* inDispatchInfo)
{
	XP_ASSERT(inDispatchInfo != NULL);

	// FIX ME??? Does this go here?
	sLastBrowserWindowCreated = NULL;
	
	// paranoia
	if (inDispatchInfo)
	{
		if (inDispatchInfo->GetDelay())
		{
			GetURLDispatcher()->PostPendingDispatch(inDispatchInfo);
		}
		// Check to make sure URL type index is within dispatch table bounds
		else if (inDispatchInfo->GetURLType() < cNumURLTypes)
		{ 
			// Get dispatch proc from table
			DispatchProcPtr dispatchProc = dispatchProcs[inDispatchInfo->GetURLType()];
			if (dispatchProc)
			{
				(*dispatchProc)(inDispatchInfo);
			}
		}
	}
}

#pragma mark -- Dispatch Procs --

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CURLDispatcher::DispatchToLibNet(CURLDispatchInfo* inDispatchInfo)
{
	CAutoPtr<CURLDispatchInfo> info(inDispatchInfo);
	// If someone passed in a context, use it.
	if (inDispatchInfo->GetTargetContext())
	{
		inDispatchInfo->GetTargetContext()->ImmediateLoadURL(inDispatchInfo->ReleaseURLStruct(), inDispatchInfo->GetOutputFormat());
	}
	else
	{
		try
		{
			if (!sDispatchContext.get())
			{
				sDispatchContext.reset(new CBrowserContext());
			}
			sDispatchContext->ImmediateLoadURL(inDispatchInfo->ReleaseURLStruct(), inDispatchInfo->GetOutputFormat());
		}
		catch (...)
		{
		}
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CURLDispatcher::DispatchToBrowserWindow(CURLDispatchInfo* inDispatchInfo)
{
	CAutoPtr<CURLDispatchInfo> info(inDispatchInfo);
	if (!inDispatchInfo->GetDelay())
	{
		if (inDispatchInfo->GetForceCreate())
		{
			// Must create a new window
			DispatchToNewBrowserWindow(info.release());
		}
		else if (inDispatchInfo->GetTargetContext())
		{
			// Use target context if passed in
			// 97-09-18 pchen -- use target if it's not "_self"
			// I found "_current" in npglue.c; do we need to filter that also?
			if (inDispatchInfo->GetURLStruct()->window_target &&
				XP_STRCASECMP(inDispatchInfo->GetURLStruct()->window_target, "_self"))
			{
				/* The thinking here is that if the URL specifies a preferred window target,
				   it's not safe to use the given context.  There is a known case where
				   this is so; it involves a link in a subframe which links to an image
				   and contains a "target" tag.  In this case, we use the only context
				   always known to be safe: the one belonging to the window itself.
				   This is precisely correct if the tag is "target = _top".  I feel
				   queasy guaranteeing that it's correct for other values of target
				   as well, but pchen thinks it will always work.  So: */
				CBrowserContext	*topContext;
				topContext = ExtractBrowserContext(*inDispatchInfo->GetTargetContext());
				inDispatchInfo->SetTargetContext(topContext->GetTopContext());
			}
			(inDispatchInfo->GetTargetContext())->SwitchLoadURL(inDispatchInfo->ReleaseURLStruct(), inDispatchInfo->GetOutputFormat());
		}
		else
		{
			// Find topmost "regular" browser window and dispatch into that window
			CWindowMediator* theMediator = CWindowMediator::GetWindowMediator();
			CBrowserWindow* theTopWindow =
				dynamic_cast<CBrowserWindow*>(theMediator->FetchTopWindow(WindowType_Browser, regularLayerType, false));
			if (theTopWindow)
			{
				theTopWindow->Select();
				CNSContext* theCurrentContext = theTopWindow->GetWindowContext();
				theCurrentContext->SwitchLoadURL(inDispatchInfo->ReleaseURLStruct(), inDispatchInfo->GetOutputFormat());
			}
			else
			{
				// No "regular" browser window available, so create one
				DispatchToNewBrowserWindow(info.release());
			}
		}
	}
	else
	{
		GetURLDispatcher()->PostPendingDispatch(info.release());
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CURLDispatcher::DispatchMailboxURL(CURLDispatchInfo* inDispatchInfo)
{
#ifdef MOZ_MAIL_NEWS
	const char* urlAddress = inDispatchInfo->GetURL();
	
	// Test to see if this is an attachment URL
	if (XP_STRSTR(urlAddress, "?part=") || XP_STRSTR(urlAddress, "&part="))
	{
		// This is a mail attachment, dispatch to browser window
  		CURLDispatcher::DispatchToBrowserWindow(inDispatchInfo);
	}
	else if (inDispatchInfo->GetForceCreate())
	{
		CMessageWindow::OpenFromURL (urlAddress);
		/* note: we can't handle an internal link (as in the clause just below), so
		   we don't bother trying. Just load the message and let the user ask again
		   once that's completed, if it's really important to go to an internal link. */
	}
	else if (XP_STRCHR(urlAddress, '#'))
	{
		// 97-06-08 pkc -- handle internal links here
		if (inDispatchInfo->GetTargetContext())
			inDispatchInfo->GetTargetContext()->SwitchLoadURL(
				inDispatchInfo->ReleaseURLStruct(),
				inDispatchInfo->GetOutputFormat());
	}
	else
	{
		// Otherwise, call DispatchToMailNewsWindow
		DispatchToMailNewsWindow(inDispatchInfo);
	}
#endif // MOZ_MAIL_NEWS
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CURLDispatcher::DispatchToMailNewsWindow(CURLDispatchInfo* inDispatchInfo)
{
#ifdef MOZ_MAIL_NEWS
	CAutoPtr<CURLDispatchInfo> info(inDispatchInfo);
	CAutoPtrXP<char> url(XP_STRDUP(inDispatchInfo->GetURL()));
	
	const char* urlAddress = url.get();

	// Otherwise, call CMessageWindow::OpenFromURL
	switch (MSG_PaneTypeForURL(urlAddress))
	{
		case MSG_MAILINGLISTPANE:
			// ? Open a list window to allow editing of this list?  
			// Ask someone.  Phil?  Michelle?
			break;
			
		case MSG_ADDRPANE:
			// Can't happen, MSG_PaneTypeForURL doesn't return this type,  
			// but a future release should, and MSG_NewWindowRequired should
			// then return true.
			break;
			
		case MSG_FOLDERPANE:
			CMailNewsFolderWindow::FindAndShow(true);				
			break;
			
		case MSG_THREADPANE:
			CThreadWindow::OpenFromURL(urlAddress);
			break;
			
		case MSG_MESSAGEPANE:
			CMessageWindow::OpenFromURL((char*)urlAddress);
			break;
			
		case MSG_SUBSCRIBEPANE:
			// Can't happen, MSG_PaneTypeForURL doesn't return this type,  
			// but a future release should, and MSG_NewWindowRequired should
			// then return true.
			// CSubscribePane::FindAndShow();
			break;
		
		case MSG_ANYPANE:	// this gets returned for most URLs
			
		case MSG_COMPOSITIONPANE:
			// presumably, this is from a mailto:, and we handle this already, below.
		case MSG_SEARCHPANE:
			// Already handled below.
		default:
			break;
	}
#endif // MOZ_MAIL_NEWS
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CURLDispatcher::DispatchToNewBrowserWindow(CURLDispatchInfo* inDispatchInfo)
{
	CAutoPtr<CURLDispatchInfo> info(inDispatchInfo);
	CBrowserWindow* theBrowserWindow = NULL;
	CNSContext* theContext = NULL;
	URL_Struct* theURLStruct = inDispatchInfo->GetURLStruct();
	
	XP_ASSERT(inDispatchInfo != NULL);
	if (inDispatchInfo)
	{
		theBrowserWindow = CreateNewBrowserWindow(inDispatchInfo->GetWindowResID(), false);
		if (theBrowserWindow)
		{
			theContext = theBrowserWindow->GetWindowContext();
			if (theURLStruct != nil)
			{
				if (theURLStruct->window_target && theURLStruct->window_target[0] != '_')
					{
					// ¥ do not assign special names
					theContext->SetDescriptor(theURLStruct->window_target);
					}
				if (theURLStruct->window_target)
					theURLStruct->window_target[0] = 0;
				theContext->ImmediateLoadURL(inDispatchInfo->ReleaseURLStruct(), inDispatchInfo->GetOutputFormat());
			}
			if (inDispatchInfo->GetInitiallyVisible()) theBrowserWindow->Show();
		}
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CBrowserWindow* CURLDispatcher::CreateNewBrowserWindow(
	ResIDT inWindowResID,
	Boolean inInitiallyVisible)
{
	CBrowserWindow* theBrowserWindow = NULL;
	CBrowserContext* theContext = NULL;
	
	try
	{
		theContext = new CBrowserContext();
		StSharer theShareLock(theContext);
		
		theBrowserWindow =
			dynamic_cast<CBrowserWindow*>(URobustCreateWindow::CreateWindow(inWindowResID, LCommander::GetTopCommander()));
		ThrowIfNULL_(theBrowserWindow);

		theBrowserWindow->SetWindowContext(theContext);
		sLastBrowserWindowCreated = theBrowserWindow;
		if (inInitiallyVisible)
			theBrowserWindow->Show();
	}
	catch (...)
	{
		delete theBrowserWindow;
		throw;
	}
	
	return theBrowserWindow;
}

#pragma mark -

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CURLDispatchInfo::CURLDispatchInfo()
	:	mURLType(HTTP_TYPE_URL),
		mURLStruct(NULL),
		mTargetContext(NULL),
		mOutputFormat(NET_DONT_RELOAD),
		mDelayDispatch(false),
		mIsSaving(false),
		mForceCreate(false),
		mInitiallyVisible(true),
		mIsWaitingForMochaImageLoad(false),
		mWindowResID(1010)
{
}

CURLDispatchInfo::CURLDispatchInfo(
	const char*				inURL,
	CNSContext*				inTargetContext,
	FO_Present_Types		inOutputFormat,
	NET_ReloadMethod		inReloadMethod,
	Boolean					inDelay,
	Boolean					inForceCreate,
	Boolean					inIsSaving,
	ResIDT					inWindowResID,
	Boolean					inInitiallyVisible)
	:	mTargetContext(NULL),
		mOutputFormat(inOutputFormat),
		mDelayDispatch(inDelay),
		mForceCreate(inForceCreate),
		mIsSaving(inIsSaving),
		mInitiallyVisible(inInitiallyVisible),
		mIsWaitingForMochaImageLoad(false),
		mWindowResID(inWindowResID)
{
	mURLStruct = NET_CreateURLStruct(inURL, inReloadMethod);
	if (inTargetContext)
	{
		cstring theReferer = inTargetContext->GetCurrentURL();
		if (theReferer.length() > 0)
			mURLStruct->referer = XP_STRDUP(theReferer);
		mTargetContext = inTargetContext;
	}
	mURLType = NET_URL_Type(inURL);
}

CURLDispatchInfo::CURLDispatchInfo(
	URL_Struct*				inURLStruct,
	CNSContext*				inTargetContext,
	FO_Present_Types		inOutputFormat,
	Boolean					inDelay,
	Boolean					inForceCreate,
	Boolean					inIsSaving,
	ResIDT					inWindowResID,
	Boolean					inInitiallyVisible,
	Boolean					inWaitingForMochaImageLoad)
	:	mURLStruct(inURLStruct),
		mTargetContext(inTargetContext),
		mOutputFormat(inOutputFormat),
		mDelayDispatch(inDelay),
		mForceCreate(inForceCreate),
		mIsSaving(inIsSaving),
		mInitiallyVisible(inInitiallyVisible),
		mIsWaitingForMochaImageLoad(inWaitingForMochaImageLoad),
		mWindowResID(inWindowResID)
{
	if (inTargetContext && mURLStruct->referer == NULL)
	{
		cstring theReferer = inTargetContext->GetCurrentURL();
		if (theReferer.length() > 0)
			mURLStruct->referer = XP_STRDUP(theReferer);
		mTargetContext = inTargetContext;
	}
	if (inURLStruct)
		mURLType = NET_URL_Type(NET_URLStruct_Address(inURLStruct));
	else
		mURLType = 0;
}

CURLDispatchInfo::~CURLDispatchInfo()
{
	if (mURLStruct)
	{
		NET_FreeURLStruct(mURLStruct);
	}
}

URL_Struct* CURLDispatchInfo::ReleaseURLStruct()
{
	URL_Struct* url = mURLStruct;
	mURLStruct = NULL;
	return url;
}

char* CURLDispatchInfo::GetURL()
{
	if (mURLStruct)
		return NET_URLStruct_Address(mURLStruct);
	else
		return NULL;
}

void CURLDispatchInfo::SetFileSpec(const FSSpec& inFileSpec)
{
	mFileSpec = inFileSpec;
}
