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

//	CNSContext.cp


#include "CNSContext.h"
#include "CNSContextCallbacks.h"
#include "CHTMLView.h"
#ifndef NSPR20
#include "CNetwork.h"
#endif
#include "UStdDialogs.h"
#include "CURLEditField.h"
#include "CMochaHacks.h"

/*  #include <LString.h> */

#include "earlmgr.h"
/*  #include "uprefd.h" */
#include "xp.h"
#include "xp_thrmo.h"
#include "shist.h"
#include "glhist.h"
#include "libimg.h"
#include "np.h"
#include "libmime.h"
#include "libi18n.h"
#include "mimages.h"
#include "ufilemgr.h"
#include "java.h"
#include "layers.h"
#include "libevent.h"
#include "uerrmgr.h"
#include "resgui.h"
#include "uapp.h" // for CFrontApp::sHRes and CFrontApp::sVRes
#include "intl_csi.h"

#include "mkhelp.h"

// FIX ME -- write a CopyAlloc like function that takes a CString
#include "macutil.h"

UInt32 CNSContext::sNSCWindowID = 1;		// Unique ID, incremented for each context

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- CONSTRUCTION / DESTRUCTION ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CNSContext::CNSContext(MWContextType inType)
:	mLoadRefCount(0)
,	mCurrentCommand(cmd_Nothing)
{
	::memset(&mContext, 0, sizeof(MWContext));

	mContext.type = inType;
	mContext.convertPixX = 1;
	mContext.convertPixY = 1;
	
		// ¥¥¥ OK, here's the skinny on this preference.
		// apparently, there *was* this preference for changing
		// the way FTP sites are displayed, but no one can remember
		// there being an actual UI for setting and changing it.
		// and so, there *may* be people out there who are seeing FTP sites
		// in different ways.  For Dogbert, we think the right behavior is
		// to show FTP sites 'fancy' always, and not carry the old value into
		// the new version.
		//
		// ideally we'd like to remove all references to this now defunct
		// preference from all the code, but that's a bit too much work for
		// beta 2 and beyond, so simply setting this value to true is the short
		// term solution.
		//
		// deeje 97-02-07
	mContext.fancyFTP = true;		// CPrefs::GetBoolean(CPrefs::UseFancyFTP);
	
	mContext.fancyNews = CPrefs::GetBoolean(CPrefs::UseFancyNews);

	CNSContextCallbacks* theCallbacks = CNSContextCallbacks::GetContextCallbacks();
	Assert_(theCallbacks != NULL);
	mContext.funcs = &(theCallbacks->GetInternalCallbacks());
	
	mContext.fe.realContext = NULL;
	mContext.fe.view = NULL;
	mContext.fe.newContext = this;
	mContext.fe.newView = NULL;

	mContext.pHelpInfo = NULL;

//	mLoadImagesOverride = false;
//	mDelayImages = 	CPrefs::GetBoolean( CPrefs::DelayImages );
//	mIsRepaginating = false;
//	mIsRepaginationPending = false;
	mRequiresClone = false;
	mProgress = NULL;
	
	// Allocate a private i18n record
	mContext.INTL_CSIInfo = INTL_CSICreate();

	// 97-09-17 pchen -- Call XP_InitializeContext function
	XP_InitializeContext(&mContext);
	
	// FIX ME!!! need to add unique identifier
	// So why doesn't it work the way it is? (cth)
	fNSCWindowID = sNSCWindowID++;

	XP_AddContextToList(&mContext);
//	SHIST_InitSession(&mContext);
	
	// 97-05-09 pkc -- initialize new MWContext field
	mContext.fontScalingPercentage = 1.0;
	
	InitDefaultCSID();
	INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(&mContext);
	INTL_SetCSIDocCSID(csi, INTL_DefaultDocCharSetID(&mContext));
	INTL_SetCSIWinCSID(csi, INTL_DocToWinCharSetID(INTL_GetCSIDocCSID(csi)));

	// 97-05-12 pkc -- initialize XpixelsPerPoint and YpixelsPerPoint
	mContext.XpixelsPerPoint = CFrontApp::sHRes / 72.0;
	mContext.YpixelsPerPoint = CFrontApp::sVRes / 72.0;
	
	NET_CheckForTimeBomb(&mContext);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// this is a shallow copy.  child contexts are not duplicated.
CNSContext::CNSContext(const CNSContext& inOriginal)
:	mLoadRefCount(0)
{
	::memset(&mContext, 0, sizeof(MWContext));

	mContext.type = inOriginal.mContext.type;
	mContext.convertPixX = inOriginal.mContext.convertPixX;
	mContext.convertPixY = inOriginal.mContext.convertPixY;
	mContext.fancyFTP = inOriginal.mContext.fancyFTP;
	mContext.fancyNews = inOriginal.mContext.fancyNews;

	CNSContextCallbacks* theCallbacks = CNSContextCallbacks::GetContextCallbacks();
	Assert_(theCallbacks != NULL);
	mContext.funcs = &(theCallbacks->GetInternalCallbacks());

	mContext.fe.realContext = NULL;
	mContext.fe.view = NULL;
	mContext.fe.newContext = this;
	mContext.fe.newView = NULL;

	mContext.pHelpInfo = NULL;

	// Allocate a private i18n record
	mContext.INTL_CSIInfo = INTL_CSICreate();

	// 97-09-17 pchen -- Call XP_InitializeContext function
	XP_InitializeContext(&mContext);

//	mLoadImagesOverride = inOriginal.IsLoadImagesOverride();
//	mDelayImages = 	inOriginal.IsImageLoadingDelayed();
//	mIsRepaginating = inOriginal.IsRepaginating();
//	mIsRepaginationPending = inOriginal.IsRepagintaitonPending();

	mProgress = inOriginal.mProgress;
	if (mProgress != NULL)
		mProgress->AddUser(this);

	mRequiresClone = false;
// FIX ME!!! need to make sure all things inited in the default ctor are done here

	// 97-05-14 pkc -- whoops, forgot to initialize this here.
	mContext.fontScalingPercentage = inOriginal.mContext.fontScalingPercentage;

	// 97-05-12 pkc -- initialize XpixelsPerPoint and YpixelsPerPoint
	mContext.XpixelsPerPoint = inOriginal.mContext.XpixelsPerPoint;
	mContext.YpixelsPerPoint = inOriginal.mContext.YpixelsPerPoint;

	INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(&mContext);
	mDefaultCSID = inOriginal.GetDefaultCSID();
	INTL_SetCSIDocCSID(csi, inOriginal.GetDocCSID());
	INTL_SetCSIWinCSID(csi, inOriginal.GetWinCSID());
	SHIST_InitSession(&mContext);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

static void MochaDone ( CNSContext * context )
{
	delete context;
}

CNSContext::~CNSContext()
{		
	CMochaHacks::RemoveReferenceToMouseOverElementContext(&mContext);
	
	// 97-06-06 mjc - remove the context from the global list in NoMoreUsers before calling mocha.
	//XP_RemoveContextFromList(&mContext);
	if (mContext.name != NULL)
		{
		XP_FREE(mContext.name);
		mContext.name = NULL;
		}
		

	/* EA: Remove any help information associated with this context */	
 	if ((HelpInfoStruct *) mContext.pHelpInfo != NULL) {
 		if (((HelpInfoStruct *) mContext.pHelpInfo)->topicURL != NULL) {
 			XP_FREE(((HelpInfoStruct *) mContext.pHelpInfo)->topicURL);
 			((HelpInfoStruct *) mContext.pHelpInfo)->topicURL = NULL;
 		}
 
  		XP_FREE(mContext.pHelpInfo);
  		mContext.pHelpInfo = NULL;
 	}

	if (mContext.INTL_CSIInfo != NULL)
		XP_FREE(mContext.INTL_CSIInfo);

	if (CFrontApp::GetApplication()->GetNetcasterContext() == &mContext) {
		CFrontApp::GetApplication()->SetNetcasterContext(NULL);
	}	
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CNSContext::NoMoreUsers(void)
{
	// 97-06-13 mjc - Make sure applets are destroyed so they don't continue to post events to the mozilla event queue.
	// This must be called before SHIST_EndSession, so java can find the applet context
	// with which to destroy applets. Placing in the same sequence as on Windows.
	LJ_DiscardEventsForContext(&mContext);
	// We would rather to do this here instead of in the destruction 
	// sequence because this call will probably reuslt in some callbacks.
	XP_InterruptContext(&mContext);
	
	// Do most of the work the destructor used to then call mocha to kill itself
	if (mContext.defaultStatus != NULL)
		XP_FREE(mContext.defaultStatus);
	mContext.defaultStatus = NULL;
	
	LO_DiscardDocument(&mContext);
	DestroyImageContext(&mContext);
	SHIST_EndSession(&mContext);
	
	// 	Yikes.  Before we dispose of the context, MAKE
	//	SURE THAT IT IS SCOURED.  This uncovered a bug
	//	in grids... disposing of grid contexts did not
	//	remove their entry from the colormap associated
	//	with them.  This never allowed the reference count
	//	on the colormap to go to zero.
	
	//	dkc	1/18/96

	FM_SetFlushable(&mContext, FALSE );
#ifdef MOZ_MAIL_NEWS
	MimeDestroyContextData(&mContext);
#endif // MOZ_MAIL_NEWS

#if 0 // was: #ifdef LAYERS but the compositor is partly owned by CHTMLView, and so
		// it's now sharable
	if (mContext.compositor != NULL)
		{
		CL_DestroyCompositor(mContext.compositor);
		mContext.compositor = NULL;
		}		
#endif
	// 97-06-06 mjc - remove ourself from the global context list before calling mocha,
	// or else mocha may try to access a partially destroyed context.
	XP_RemoveContextFromList(&mContext);
	// We'll do the deletion in MochaDone(), called back from here.
	ET_RemoveWindowContext( &mContext, (ETVoidPtrFunc) MochaDone, this );
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	This is another useless banner that takes up space, but this one has text, at least.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CNSContext::EnsureContextProgress()
{
	if (mProgress == NULL)
	{
		mProgress = new CContextProgress;
		mProgress->AddUser(this);
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CContextProgress* CNSContext::GetContextProgress(void)
{
	return mProgress;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CNSContext::SetContextProgress(CContextProgress* inProgress)
{
	Assert_(mProgress == NULL);
	mProgress = inProgress;
	mProgress->AddUser(this);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

cstring CNSContext::GetDescriptor(void) const
{
	cstring theName;
	if (mContext.name != NULL)
		theName = mContext.name;
		
	return theName;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CNSContext::SetDescriptor(const char* inDescriptor)
{
	if (mContext.name != NULL)
		XP_FREE(mContext.name);
		
	mContext.name = XP_STRDUP(inDescriptor);
	ThrowIfNULL_(mContext.name);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CNSContext::SetRequiresClone(Boolean inClone)
{
	mRequiresClone = inClone;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CNSContext::IsCloneRequired(void) const
{
	return mRequiresClone;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CContextProgress* CNSContext::GetCurrentProgressStats(void)
{
	return mProgress;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CNSContext::UpdateCurrentProgressStats(void)
{
	// You shouldn't be calling this when we're not processing something
	Assert_(mProgress != NULL);
	BroadcastMessage(msg_NSCProgressUpdate, mProgress);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- CHARACTER SET ACCESSORS ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
extern Int16 DefaultDocCharSetIDFromFrontWindow();

void CNSContext::InitDefaultCSID(void)
{
	mDefaultCSID = DefaultDocCharSetIDFromFrontWindow();
	if(0 == mDefaultCSID )
		mDefaultCSID = INTL_DefaultDocCharSetID(0);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CNSContext::SetDefaultCSID(Int16 inDefaultCSID)
{
	mDefaultCSID = inDefaultCSID;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Int16 CNSContext::GetDefaultCSID(void) const
{
	return mDefaultCSID;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Int16 CNSContext::GetDocCSID(void) const
{
	INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo((MWContext *)&mContext);
	return INTL_GetCSIDocCSID(csi);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CNSContext::SetWinCSID(Int16 inWinCSID)
{
	INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(&mContext);
	INTL_SetCSIWinCSID(csi, inWinCSID);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CNSContext::SetDocCSID(Int16 inDocCSID)
{
	INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(&mContext);
	INTL_SetCSIDocCSID (csi, inDocCSID);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Int16 CNSContext::GetWinCSID(void) const
{
	INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo((MWContext *)&mContext);
	return INTL_GetCSIWinCSID(csi);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Int16 CNSContext::GetWCSIDFromDocCSID(
	Int16			inDocCSID)
{
	if (inDocCSID == CS_DEFAULT)
		inDocCSID = mDefaultCSID;
		
	return INTL_DocToWinCharSetID(inDocCSID);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- STATUS ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

const char* CNSContext::GetDefaultStatus(void) const
{
	return mContext.defaultStatus;
}

void CNSContext::ClearDefaultStatus()
{
	if (mContext.defaultStatus)
		XP_FREE(mContext.defaultStatus);
	mContext.defaultStatus = NULL;
}

void CNSContext::SetStatus(const char* inStatus)
{
	Progress(inStatus);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- URL MANIPULATION ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

cstring CNSContext::GetURLForReferral(void)
{
	cstring theCurrentURL;
	
	History_entry* theCurrentHist = SHIST_GetCurrent(&mContext.hist);
	if (theCurrentHist != NULL)
	{
		if (theCurrentHist->origin_url != NULL)
			theCurrentURL = theCurrentHist->origin_url;
		else
			theCurrentURL = theCurrentHist->address;
	}

	return theCurrentURL;
}

cstring CNSContext::GetCurrentURL(void)
{
	cstring theCurrentURL;
	
	History_entry* theCurrentHist = SHIST_GetCurrent(&mContext.hist);
	if (theCurrentHist != NULL)
		theCurrentURL = theCurrentHist->address;

	return theCurrentURL;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// PLEASE NOTE THAT THIS ROUTINE FREES THE URL_STRUCT!!!

void CNSContext::SwitchLoadURL(
	URL_Struct*				inURL,
	FO_Present_Types		inOutputFormat)
{
	if ((inURL->address != NULL) && XP_STRCMP(inURL->address, "about:document") != 0 /*&& XP_IsContextBusy( this->fHContext ) */)
		XP_InterruptContext(&mContext);

	// Don't broadcast the notification that a copy of one or more messages
	// will take place: just forward it to the EarlManager... If we broadcast
	// it through the front-end, the Netscape icon doesn't stop spinning
	// because we receive one more GetURL events than AllConnectionsComplete events.
	if (XP_STRCMP(inURL->address, "mailbox:copymessages") == 0)
	{
		EarlManager::StartLoadURL(inURL, &mContext, inOutputFormat);
		return;
	}

	// This is a notification in which all clients are warned that a new URL is
	// going to be loaded.  Any client can nullify the load request by zeroing the ioParam.
	Boolean bAllClientsHappy = true;
	BroadcastMessage(msg_NSCConfirmLoadNewURL, &bAllClientsHappy);
	if (!bAllClientsHappy)
	{
	    NET_FreeURLStruct(inURL);
		return;
	}
		
	// Ok, no clients of this context objected to the load request.  We will now
	// notify them that a new load is about to take place.

	EnsureContextProgress();

	// There are listeners that listen to several contexts (eg, spinning N in mail windows).
	// This works by reference counting, and such listeners assume calls to
	// SwitchLoadURL and AllConnectionsComplete are balanced.  Each context must
	// therefore ensure that they are, even if it is done artificially.
	mLoadRefCount++;

	BroadcastMessage(msg_NSCStartLoadURL, inURL);
	
	// if we are going to named anchor, do not load from the net
	Int32 theXPos, theYPos;
	History_entry* theHist = SHIST_GetCurrent(&mContext.hist);
	if ((theHist != NULL) 													&&
		(SHIST_GetIndex(&mContext.hist, theHist) != inURL->history_num)		&&	// We are not reloading one page
 		(inURL->force_reload == 0)											&&
		((inOutputFormat == FO_PRESENT) || (inOutputFormat == FO_CACHE_AND_PRESENT)) &&	// And only if we want to display the result
		(XP_FindNamedAnchor(&mContext, inURL, &theXPos, &theYPos)))
		{
		// ¥ no double redraw
		SetDocPosition(0, theXPos, theYPos);
		if (inURL->history_num == 0)
			{
			/* Create URL from prev history entry to preserve security, etc. */
			URL_Struct *theURLCopy = SHIST_CreateURLStructFromHistoryEntry(&mContext, theHist);

			/*  Swap addresses. */
			char *temp = inURL->address;
			inURL->address = theURLCopy->address;
			theURLCopy->address = temp;

			/*  Free old URL, and reassign. */
			NET_FreeURLStruct(inURL);
			inURL = theURLCopy;
			
			SHIST_AddDocument(&mContext, SHIST_CreateHistoryEntry(inURL, theHist->title));
			}
		else
			SHIST_SetCurrent(&mContext.hist, inURL->history_num);
			
		GH_UpdateGlobalHistory(inURL);
		NET_FreeURLStruct(inURL);
		// ¥Êupdate global history for the named anchor & refresh them
		XP_RefreshAnchors();

		// Since this was not a net load, we need to signal all clients that the
		// load in complete.
		AllConnectionsComplete();
		}
	else
		{
		// otherwise, go off the net
		EarlManager::StartLoadURL(inURL, &mContext, inOutputFormat);
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CNSContext::ImmediateLoadURL(
	URL_Struct*				inURL,
	FO_Present_Types		inOutputFormat)
{
	EnsureContextProgress();
	BroadcastMessage(msg_NSCStartLoadURL, inURL);
	EarlManager::StartLoadURL(inURL, &mContext, inOutputFormat);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- CALLBACK IMPLEMENTATION ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char* CNSContext::TranslateISOText(											
	int 					/* inCharset */,
	char*					inISOText)
{
	return inISOText;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CNSContext::GraphProgressInit(
	URL_Struct*				/*inURL*/,
	Int32 					inContentLength)
{
	EnsureContextProgress();
	mProgress->mInitCount++;
	
	if (inContentLength <= 0)
		mProgress->mUnknownCount++;
	else
		mProgress->mTotal += inContentLength;

	if (mProgress->mInitCount == 1)
		{
		mProgress->mStartTime = ::TickCount() / 60;
		BroadcastMessage(msg_NSCProgressBegin, mProgress);
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CNSContext::GraphProgress(
	URL_Struct*				/*inURL*/,
	Int32 					inBytesReceived,
	Int32 					inBytesSinceLast,
	Int32 					inContentLength)
{
	EnsureContextProgress();
		
	Uint32 theTotalBytes;
	if ((mProgress->mUnknownCount > 0) || (inBytesReceived > inContentLength))
		theTotalBytes = 0;
	else
		theTotalBytes = mProgress->mTotal;
		
	mProgress->mRead += inBytesSinceLast;

	Uint32 theTime = ::TickCount() / 60;
	const char* theMessage = XP_ProgressText(theTotalBytes, mProgress->mRead, mProgress->mStartTime, theTime);

	if (inContentLength > 0)
		mProgress->mPercent = inBytesReceived / (double)inContentLength * 100;
	else
		mProgress->mPercent = -1; // this signifies and indefinite ammount

	if (theMessage != NULL)
		{
		mProgress->mMessage = theMessage;
		BroadcastMessage(msg_NSCProgressUpdate, mProgress);
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CNSContext::GraphProgressDestroy(
	URL_Struct*				/*inURL*/,
	Int32 					inContentLength,
	Int32 					inTotalRead)
{
	Assert_(mProgress != NULL);
	if (mProgress)
	{
	if (inContentLength <= 0 )
		mProgress->mUnknownCount--;
	else
		mProgress->mTotal -= inContentLength;	

	mProgress->mRead -= inTotalRead;

	mProgress->mInitCount--;
	if (mProgress->mInitCount == 0)
		BroadcastMessage(msg_NSCProgressEnd, mProgress);
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CNSContext::SetProgressBarPercent(
	Int32 					inPercent)
{
	BroadcastMessage(msg_NSCProgressPercentChanged, &inPercent);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CNSContext::Progress(
	const char*				inMessageText)
{
	// We MUST make a copy of this string, because of CStr255's finitely many buffers,
	// one of which is used by the caller (NET_Progress).
	char messageCopy[255];
	if ( inMessageText )
		XP_STRNCPY_SAFE(messageCopy, inMessageText, sizeof(messageCopy));
	else
		messageCopy[0] = 0;
	BroadcastMessage(msg_NSCProgressMessageChanged, messageCopy);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CNSContext::Alert(
	const char*				inAlertText)
{
	CStr255 	pmessage( inAlertText );

	ConvertCRtoLF( pmessage );	// In fact, this function converts LF to CRs and 
								// that's what everybody is using it for. Well...

	StripDoubleCRs( pmessage );
	pmessage=NET_UnEscape(pmessage);
	UStdDialogs::Alert(pmessage, eAlertTypeCaution);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CNSContext::SetCallNetlibAllTheTime(void)
{
#ifndef NSPR20
	extern CNetworkDriver* gNetDriver;
	Assert_(gNetDriver != NULL);
 	gNetDriver->SetCallAllTheTime();
#endif
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CNSContext::ClearCallNetlibAllTheTime(void)
{
#ifndef NSPR20
	extern CNetworkDriver* gNetDriver;
	Assert_(gNetDriver != NULL);
 	gNetDriver->ClearCallAllTheTime();
#endif
}

// A temporary abortion, we need this routine outside of MWContext function table
void XP_ClearCallNetlibAllTheTime(MWContext *)
{
#ifndef NSPR20
	extern CNetworkDriver* gNetDriver;
	Assert_(gNetDriver != NULL);
 	gNetDriver->SetCallAllTheTime();
#endif
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

XP_Bool CNSContext::UseFancyFTP(void)
{
	return mContext.fancyFTP;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

XP_Bool CNSContext::UseFancyNewsgroupListing(void)
{
	return CPrefs::GetBoolean(CPrefs::UseFancyNews);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int CNSContext::FileSortMethod(void)
{
	return CPrefs::GetLong( CPrefs::FileSortMethod );
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

XP_Bool CNSContext::ShowAllNewsArticles(void)
{
	return CPrefs::GetBoolean( CPrefs::ShowAllNews );
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

XP_Bool	CNSContext::Confirm(
	const char* 			inMessage)
{
	CStr255 mesg(inMessage);
	mesg = NET_UnEscape(mesg);
	return UStdDialogs::AskOkCancel(mesg);
}

PRBool XP_Confirm( MWContext * , const char * msg)
{
	CStr255 mesg(msg);
	mesg = NET_UnEscape(mesg);
	if ( UStdDialogs::AskOkCancel(mesg))
		return PR_TRUE;
	else
		return PR_FALSE;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char* CNSContext::Prompt(
	const char* 			inMessage,
	const char*				inDefaultText)
{
	return PromptWithCaption("", inMessage, inDefaultText);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
char* CNSContext::PromptWithCaption(
	const char*				inTitleBarText,
	const char* 			inMessage,
	const char*				inDefaultText)
{
	char* result = NULL;
	CStr255 mesg(inMessage), ioString(inDefaultText);
	mesg = NET_UnEscape(mesg);

	if (UStdDialogs::AskStandardTextPrompt(inTitleBarText, mesg, ioString))
	{
		if (ioString.Length() > 0)
		{
			result = (char*)XP_STRDUP((const char*)ioString);
		}
	}

	return result;	
}

char * XP_Prompt(MWContext * /* pContext */, const char *inMessage, const char * inDefaultText)
{
	char* result = NULL;
	CStr255 mesg(inMessage), ioString(inDefaultText);
	mesg = NET_UnEscape(mesg);

	if (UStdDialogs::AskStandardTextPrompt("", mesg, ioString))
	{
		if (ioString.Length() > 0)
		{
			result = (char*)XP_STRDUP((const char*)ioString);
		}
	}

	return result;	
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

XP_Bool CNSContext::PromptUsernameAndPassword(
	const char*				inMessage,
	char**					outUserName,
	char**					outPassword)
{
	CStr255 mesg(inMessage), username, password;
	if (UStdDialogs::AskForNameAndPassword(mesg, username, password))
	{
		*outUserName = XP_STRDUP((const char*)username);
		*outPassword = XP_STRDUP((const char*)password);
		return true;
	}
	return false;
}

PRBool XP_PromptUsernameAndPassword (MWContext * /* window_id */,
					  const char *  message,
					  char **       outUserName,
					  char **       outPassword)
{
	CStr255 mesg(message), username, password;
	if (UStdDialogs::AskForNameAndPassword(mesg, username, password))
	{
		*outUserName = XP_STRDUP((const char*)username);
		*outPassword = XP_STRDUP((const char*)password);
		return PR_TRUE;
	}
	return PR_FALSE;	
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char* CNSContext::PromptPassword(
	const char* 			inMessage)
{
	CStr255 message(inMessage), password;
	if (UStdDialogs::AskForPassword(message, password))
		return XP_STRDUP((const char*)password);
	return nil;
}

char *XP_PromptPassword(MWContext */* pContext */, const char *pMessage)
{
	CStr255 message(pMessage), password;
	if (UStdDialogs::AskForPassword(message, password))
		return XP_STRDUP((const char*)password);
	return nil;
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CNSContext::AllConnectionsComplete(void)
{
//	mIsRepaginating = false;
	// For certain contexts (i.e. biff context), we don't get
	// a start loading URL call, so mProgress is NULL
	if (mProgress)
	{
		mProgress->RemoveUser(this);
		mProgress = NULL;
	}
	
	XP_RefreshAnchors();
	BroadcastMessage(msg_NSCAllConnectionsComplete);

//	if (mContext.type == MWContextMail)
	{
		// There are listeners that listen to several contexts (eg, in mail windows).
		// This works by reference counting, and such listeners assume calls to
		// SwitchLoadURL and AllConnectionsComplete are balanced.  Each context must
		// therefore ensure that they are, even if it is done artificially.
		for (mLoadRefCount--; mLoadRefCount > 0; mLoadRefCount--)
			BroadcastMessage(msg_NSCAllConnectionsComplete);
				// decrement the ref count on the spinning-N the same number of times
				// as we incremented it.
	}
	mLoadRefCount = 0;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- PROGRESS STATISTICS ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


CContextProgress::CContextProgress()
{
	mTotal = 0;
	mRead = 0;			// How many have been read
	mUnknownCount = 0;	// How many connections of the unknown length do we have
	mPercent = 0;
	mStartTime = 0;
	mInitCount = 0;
}		

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- STUFF ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// SIGH!!!!!
extern int MK_OUT_OF_MEMORY;

void CNSContext::CompleteLoad(URL_Struct* inURL, int inStatus)
{
	if ( inStatus < 0 )
	{
		if ( inURL->error_msg )
		{
// FIX ME!!! If anyone ever redoes progress process apple events this code needs
// to be re-enabled
//			if (HasProgressProcess())
//			{
//				fDidSetProgressPercent = FALSE;
//				SendMakingProgressEvent(nurl->error_msg, 0);
//			}
//			else
				ErrorManager::PlainAlert( inURL->error_msg, NULL, NULL, NULL );
		}
		Progress("");
	}
	if ( inStatus == MK_OUT_OF_MEMORY )
	{
		XP_InterruptContext( *this );
  		ErrorManager::PlainAlert( NO_MEM_LOAD_ERR_RESID );
	}
	
	if (inStatus == MK_CHANGING_CONTEXT)
		Progress((const char*)GetCString(DOWNLD_CONT_IN_NEW_WIND));
}

// Clear fe.newView inside MWContext
// This is so that we can catch XP code trying to layout in a
// window that's been destroyed.
void CNSContext::ClearMWContextViewPtr(void)
{
	mContext.fe.newView = NULL;
}

void CNSContext::WaitWhileBusy()
{
	EventRecord stupidNullEvent = {0};
	while (XP_IsContextBusy(*this))
	{
		ThrowIf_(CmdPeriod());
		TheEarlManager.SpendTime(stupidNullEvent);
	}
} // CNSContext::WaitWhileBusy


void CNSContext::CopyListenersToContext(CNSContext* aSubContext)		// used when spawning grid contexts
{
		// give this grid context all the same listeners as the parent context
		// deeje 97-02-12
	CHTMLView*		theRootView = ExtractHyperView(*this);
	
	LArrayIterator iterator(mListeners);
	LListener	*theListener;
	while (iterator.Next(&theListener))
	{
		CURLEditField* urlField  = dynamic_cast<CURLEditField*>(theListener);
		if (urlField == NULL && theListener != theRootView)
		{
			aSubContext->AddListener(theListener);
		}
	}
}

MWContext* CNSContext::CreateNewDocWindow(URL_Struct*  inURL )
{
	CURLDispatcher::DispatchURL(inURL, nil);
	return NULL;
}

History_entry* CNSContext::GetCurrentHistoryEntry()
{
	return SHIST_GetCurrent(&mContext.hist);
}

Int32 CNSContext::GetHistoryListCount(void)
{
	Int32 count = 0;
	XP_List* historyList = SHIST_GetList(&mContext);
	if (historyList)
		count = XP_ListCount(historyList);
	return count;
}

Int32 CNSContext::GetIndexOfCurrentHistoryEntry(void)
{
	Int32 index = 0;
	History_entry* theCurrentHist = GetCurrentHistoryEntry();
	if (theCurrentHist != NULL)
		index = SHIST_GetIndex(&mContext.hist, theCurrentHist);
	return index;
}

// inIndex is one-based
cstring* CNSContext::GetHistoryEntryTitleByIndex(Int32 inIndex)
{
	cstring* title = NULL;
	History_entry* theCurrentHist = SHIST_GetObjectNum(&mContext.hist, inIndex);
	if (theCurrentHist != NULL)
	{
		try {
			title = new cstring;
			*title = theCurrentHist->title;
		} catch (...) {
		}
	}
	return title;
}

void CNSContext::GetHistoryURLByIndex(cstring& outURL, Int32 inIndex)
{
	History_entry* theEntry = SHIST_GetObjectNum(&mContext.hist, inIndex);
	if (theEntry)
	{
		outURL = theEntry->address;
	}
	else
	{
		throw IndexOutOfRangeException();
	}
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- STUBS ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CNSContext::LayoutNewDocument(
	URL_Struct*			/* inURL */,
	Int32*				/* inWidth */,
	Int32*				/* inHeight */,
	Int32*				/* inMarginWidth */,
	Int32*				/* inMarginHeight */) {}
void CNSContext::SetDocTitle(char* /* inTitle */) {}
void CNSContext::FinishedLayout(void) {}
int CNSContext::GetTextInfo(
	LO_TextStruct*			/* inText */,
	LO_TextInfo*			/* inTextInfo */) {	return 1; }
int CNSContext::MeasureText(
	LO_TextStruct*			/* inText */,
	short*					/* outCharLocs */) { return 0; }
void CNSContext::GetEmbedSize(
	LO_EmbedStruct* /* inEmbedStruct */,
	NET_ReloadMethod /* inReloadMethod */) {}
void CNSContext::GetJavaAppSize(
	LO_JavaAppStruct* /* inJavaAppStruct */,
	NET_ReloadMethod /* inReloadMethod */) {}
void CNSContext::GetFormElementInfo(LO_FormElementStruct* /* inElement */) {}
void CNSContext::GetFormElementValue(
	LO_FormElementStruct* /* inElement */,
	XP_Bool /*inHide */) {}
void CNSContext::ResetFormElement(LO_FormElementStruct* /* inElement */) {}
void CNSContext::SetFormElementToggle(
	LO_FormElementStruct* /* inElement */,
	XP_Bool /* inToggle */) {}
void CNSContext::FreeEmbedElement(LO_EmbedStruct* /* inEmbedStruct */) {}
void CNSContext::CreateEmbedWindow(NPEmbeddedApp* /* inEmbeddedApp */) {}
void CNSContext::SaveEmbedWindow(NPEmbeddedApp* /* inEmbeddedApp */) {}
void CNSContext::RestoreEmbedWindow(NPEmbeddedApp* /* inEmbeddedApp */) {}
void CNSContext::DestroyEmbedWindow(NPEmbeddedApp* /* inEmbeddedApp */) {}
void CNSContext::FreeJavaAppElement(LJAppletData* /* inAppletData */) {}
void CNSContext::HideJavaAppElement(LJAppletData* /* inAppletData */) {}
void CNSContext::FreeEdgeElement(LO_EdgeStruct* /* inEdgeStruct */) {}
void CNSContext::FormTextIsSubmit(LO_FormElementStruct* /* inElement */) {}
void CNSContext::DisplaySubtext(
	int 					/* inLocation */,
	LO_TextStruct*			/* inText */,
	Int32 					/* inStartPos */,
	Int32					/* inEndPos */,
	XP_Bool 				/* inNeedBG */) {}
void CNSContext::DisplayText(
	int 					/* inLocation */,
	LO_TextStruct*			/* inText */,
	XP_Bool 				/* inNeedBG */) {}
void CNSContext::DisplayEmbed(
	int /* inLocation */,
	LO_EmbedStruct* /* inEmbedStruct */) {}
void CNSContext::DisplayJavaApp(
	int /* inLocation */,
	LO_JavaAppStruct* /* inJavaAppStruct */) {}
void CNSContext::DisplayEdge (
	int /* inLocation */,
	LO_EdgeStruct* /* inEdgeStruct */) {}
void CNSContext::DisplayTable(
	int 					/* inLocation */,
	LO_TableStruct*			/* inTableStruct */) {}
void CNSContext::DisplayCell(
	int /* inLocation */,
	LO_CellStruct* /* inCellStruct */) {}
void CNSContext::InvalidateEntireTableOrCell(
	LO_Element*				/* inElement */) {}
void CNSContext::DisplayAddRowOrColBorder(
	XP_Rect*				/* inRect */,
	XP_Bool					/* inDoErase */) {}
void CNSContext::DisplaySubDoc(
	int 					/* inLocation */,
	LO_SubDocStruct*		/* inSubdocStruct */) {}
void CNSContext::DisplayLineFeed(
	int 					/* inLocation */,
	LO_LinefeedStruct*		/* inLinefeedStruct */,
	XP_Bool 				/* inNeedBG */) {}
void CNSContext::DisplayHR(
	int 					/* inLocation */,
	LO_HorizRuleStruct*		/* inRuleStruct */) {}
void CNSContext::DisplayBullet(
	int 					/* inLocation */,
	LO_BullettStruct*		/* inBullettStruct */) {}
void CNSContext::DisplayFormElement(
	int /* inLocation */,
	LO_FormElementStruct* /* inFormElement */) {}
void CNSContext::DisplayBorder(
	int 			/* inLocation */,
	int				/* inX */,
	int				/* inY */,
	int				/* inWidth */,
	int				/* inHeight */,
	int				/* inBW */,
	LO_Color*	 	/* inColor */,
	LO_LineStyle)	/* inStyle */ {}
void CNSContext::DisplayFeedback(
	int 			/* inLocation */,
	LO_Element_struct*	/* inElement */)	/* inStyle */ {}
void CNSContext::UpdateEnableStates() {}
void CNSContext::ClearView(int /* inWhich */) {}
void CNSContext::SetDocDimension(
	int /* inLocation */,
	Int32 /* inWidth */,
	Int32 /* inLength */) {}
void CNSContext::SetDocPosition(
	int /* inLocation */,
	Int32 /* inX */,
	Int32 /* inY */) {}
void CNSContext::GetDocPosition(
	int /* inLocation */,
	Int32* /* outX */,
	Int32* /* outY */) {}
void CNSContext::BeginPreSection(void) {}
void CNSContext::EndPreSection(void) {}
void CNSContext::SetBackgroundColor(
	Uint8 /* inRed */,
	Uint8 /* inGreen */,
	Uint8 /* inBlue */) {}
void CNSContext::EnableClicking(void) {}
void CNSContext::EraseBackground(
	int /* inLocation */,
	Int32 /* inX */,
	Int32 /* inY */,
	Uint32 /* inWidth */,
	Uint32 /* inHieght */,
	LO_Color* /* inColor */) {}
void CNSContext::SetDrawable(CL_Drawable* /* inDrawable */) {}
void CNSContext::GetTextFrame(
	LO_TextStruct* /* inTextStruct */,
	Int32 /* inStartPos */,
	Int32 /* inEndPos */,
	XP_Rect* /* outFrame */) {}
void CNSContext::GetDefaultBackgroundColor(
	LO_Color* /* outColor */) const {}
void CNSContext::DrawJavaApp(
	int /*inLocation*/,
	LO_JavaAppStruct* /*inJavaAppStruct*/) {}
void CNSContext::HandleClippingView(
	struct LJAppletData* /*appletD*/, 
	int /*x*/, 
	int /*y*/, 
	int /*width*/, 
	int /*height*/) {}
	
