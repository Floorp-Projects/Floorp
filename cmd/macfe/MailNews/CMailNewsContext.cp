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
 * Copyright (C) 1996 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// CMailNewsContext.cp

#include "CMailNewsContext.h"

#include "ufilemgr.h"

#include "CCheckMailContext.h"
#include "UStdDialogs.h"
#include "CMessageFolderView.h"

// Command numbers
#include "resgui.h"
#include "MailNewsgroupWindow_Defines.h"

#include "uprefd.h"
#include "prefapi.h"
#include "shist.h"
#include "macutil.h"
#include "macgui.h"
#include "uerrmgr.h"

// get string constants
#define WANT_ENUM_STRING_IDS
#include "allxpstr.h"
#undef WANT_ENUM_STRING_IDS

#include "mimages.h"
#include "layers.h"
#include "CDrawable.h"
#include "CBrowserContext.h"

#include "CNSContextCallbacks.h"
#include "CMailProgressWindow.h"

// The message master from which all life springs...
MSG_Master* CMailNewsContext::sMailMaster = NULL;
Int32 CMailNewsContext::sMailMasterRefCount = 0;
MSG_Prefs* CMailNewsContext::sMailPrefs = NULL;

//----------------------------------------------------------------------------------------
CMailNewsContext::CMailNewsContext(MWContextType inContextType)
//----------------------------------------------------------------------------------------
:	Inherited(inContextType)
,	mWasPromptOkay( false )
{
	ThrowUnlessPrefsSet(inContextType);
	GetMailMaster(); // make sure it's there.
	sMailMasterRefCount++;
	SHIST_InitSession(&mContext);
#if 0
	// Bug: hang when replying to a thread with attachment icons
	// from the collapsed thread window.

	// The following is left here as a caution to young programmers who might need a lesson
	// learned the hard way by a foolish forerunner - me.  The easy solution
	// is to set the output_function pointer in mimemoz.c to a stub, so that no
	// HTML is generated.
	
	// Adding an image context avoids the hang by handling the attachment icon images.
	CreateImageContext( &mContext );

	// Adding a compositor to avoid the HTML dialog complaining about window.document.layers
	// having "no properties"  mime converter was adding javascript that referred to them.
	CRouterDrawable* onscreenDrawable = new CRouterDrawable();
	CL_Drawable * theDrawable = CL_NewDrawable(100, 100, CL_WINDOW,
		 &mfe_drawable_vtable, (void *) onscreenDrawable);
	CL_Compositor* c = CL_NewCompositor(theDrawable, nil, 0, 0, 100, 100, 10);
	CSharableCompositor* compositor = new CSharableCompositor(c);
	compositor->AddUser(this); // shared by context and view. - NEVER REMOVED/  TEST/
	mContext.compositor = *compositor;
	CL_SetCompositorEnabled(*compositor, PR_TRUE);
#endif
} // CMailNewsContext::CMailNewsContext

//----------------------------------------------------------------------------------------
CMailNewsContext::~CMailNewsContext()
//----------------------------------------------------------------------------------------
{
	XP_InterruptContext(*this); // This must happen before the master is deleted!
	#if 1 //CNSContext::NoMoreUsers is responsible for disposing the history sessions
			// yes, but 97/05/22 CNSContext::NoMoreUsers now has "NoMoreUsers". 
	LO_DiscardDocument(&mContext);
	SHIST_EndSession(&mContext);
	MimeDestroyContextData(&mContext);
	// No java
	// No layers
	// No mocha
	#endif //0
	if (sMailMasterRefCount == 1)
	{
		// If we are about to delete the last mail-news context...
		// (WARNING: there's a backend bug: MSG_CleanupNeeded will continue to return
		//	true even after the cleanup is finished.  So be sure not to get into an
		//	infinite loop.)
		if (mContext.type != MWContextMailNewsProgress && MSG_CleanupNeeded(sMailMaster))
		{
			try
			{
				CMailProgressWindow::CleanUpFolders();
			}
			catch(...)
			{
				// Don't throw here, we're trying to quit.
			}
		}
	}
	Assert_(sMailMaster);
	Assert_(sMailMasterRefCount);
	sMailMasterRefCount--;
	if (sMailMasterRefCount == 0)
	{
		MSG_DestroyMaster(sMailMaster);
		sMailMaster = NULL;
	}
} // CMailNewsContext::~CMailNewsContext

//----------------------------------------------------------------------------------------
Boolean CMailNewsContext::IsPrefSet(const char* inPrefKey)
//----------------------------------------------------------------------------------------
{
	char buffer[512]; int bufferLength = sizeof(buffer);
	PREF_GetCharPref(inPrefKey, buffer, &bufferLength);
	if (*buffer)
		return true;
	return false;
} // CMailNewsContext::IsPrefSet

//----------------------------------------------------------------------------------------
Boolean CMailNewsContext::ThrowUnlessPrefSet(
	const char* inPrefKey,
	PREF_Enum inPrefPaneSelector)
//----------------------------------------------------------------------------------------
{
	if (IsPrefSet(inPrefKey))
		return true;
	AlertPrefAndThrow(inPrefPaneSelector);
	return false; // dead, but the compiler needs this, see?
} // CMailNewsContext::ThrowUnlessPrefSet

//----------------------------------------------------------------------------------------
void CMailNewsContext::AlertPrefAndThrow(PREF_Enum inPrefPaneSelector)
//----------------------------------------------------------------------------------------
{
	// OK. We're going to throw, but first, we prompt and send them to the prefs pane.
	// Ask "Would you like to set the preference?"
	short	strID;
	switch (inPrefPaneSelector)
	{
		case PREF_EmailAddress:	strID = 23;	break;	// I don't know why but all string IDs from
		case PREF_Pop3ID:		strID = 21;	break;	// STR# 7099 are hard-coded in other parts
		case PREF_SMTPHost:		strID = 21;	break;	// of the code: let's continue!
		case PREF_PopHost:		strID = 21;	break;
		case PREF_NewsHost:		strID = 22;	break;
		default:				strID = 24;	break;
	}
	CStr255	whereString;
	::GetIndString(whereString, 7099, strID);

	CStr255 alertString;
	::GetIndString(alertString, 7099, 7);

	::StringParamText(alertString, (char*)whereString, nil, nil, nil);

	if (UStdDialogs::AskOkCancel(alertString, nil, nil)) // if "ok"
		FE_EditPreference(inPrefPaneSelector);
	throw (OSErr)userCanceledErr; // we already presented the alert.  This avoids another.
} // CMailNewsContext::AlertPrefAndThrow

//----------------------------------------------------------------------------------------
/* static */ Boolean CMailNewsContext::UserHasNoLocalInbox()
//----------------------------------------------------------------------------------------
{
	// Check whether there is an inbox in their local mail tree.  If not, assume
	// first time setup.
	FSSpec inboxSpec = CPrefs::GetFilePrototype(CPrefs::MailFolder);
//	*(CStr63*)(inboxSpec.name) = XP_GetString(MK_MSG_INBOX_L10N_NAME);
//	if (FSMakeFSSpec(inboxSpec.vRefNum, inboxSpec.parID, inboxSpec.name, &spec2) == fnfErr)
//		return true;
	FSSpec spec2;
	*(CStr63*)(inboxSpec.name) = XP_GetString(MK_MSG_TRASH_L10N_NAME);
	if (FSMakeFSSpec(inboxSpec.vRefNum, inboxSpec.parID, inboxSpec.name, &spec2) == fnfErr)
		return true;
	return false;
} // CMailNewsContext::UserHasNoLocalInbox

//----------------------------------------------------------------------------------------
/* static */ void CMailNewsContext::ThrowIfNoLocalInbox()
//----------------------------------------------------------------------------------------
{
	if (UserHasNoLocalInbox())
		AlertPrefAndThrow(PREF_PopHost);
} // CMailNewsContext::ThrowIfNoLocalInbox

//----------------------------------------------------------------------------------------
void CMailNewsContext::ThrowUnlessPrefsSet(MWContextType inContextType)
//----------------------------------------------------------------------------------------
{
	if (inContextType == MWContextBiff
	|| inContextType == MWContextSearch
	|| inContextType == MWContextAddressBook
	|| inContextType == MWContextMailFilters)
		return; // no prefs required for biff.

#ifdef HAVE_SEPARATE_SMTP_USERNAME_PREF
	//if (we are using POP)
	//	ThrowUnlessPrefSet("mail.pop_name",PREF_Pop3ID);
	
	ThrowUnlessPrefSet("mail.smtp_name", PREF_Pop3ID);
#else
	// temporary code while the backend is still using pop_name for smtp_name
	if ( !IsPrefSet("mail.pop_name") )
	{
		char		*smtpName = NULL;
		
		if (PREF_CopyCharPref("mail.smtp_name", &smtpName) == PREF_NOERROR)
		{
			int		prefError = PREF_SetCharPref("mail.pop_name", smtpName);
			Assert_(prefError == PREF_NOERROR || prefError == PREF_VALUECHANGED);
		}
		
		XP_FREEIF(smtpName);
	}
#endif

	if (!IsPrefSet("network.hosts.pop_server") && !IsPrefSet("network.hosts.imap_servers"))
		AlertPrefAndThrow(PREF_PopHost);
	// OK, she has the basic stuff.  Now for the fancy stuff.  I know it's plain wrong
	// to have a switch like this in an object-oriented world, but time is of the essence.
	switch (inContextType)
	{
  		case MWContextNews:					// A news reader window
		case MWContextNewsMsg:				// A window to display a news msg
			ThrowUnlessPrefSet(
				"network.hosts.nntp_server",
				PREF_NewsHost);
			break;
		case MWContextMessageComposition:	// A news-or-mail message editing window
			ThrowUnlessPrefSet(
				"network.hosts.smtp_server",
				PREF_SMTPHost);
			ThrowUnlessPrefSet(
				"mail.identity.useremail",
				PREF_EmailAddress);
			break;
	}
} //  CMailNewsContext::ThrowUnlessPrefsSet

//----------------------------------------------------------------------------------------
/* static */ MSG_Master* CMailNewsContext::GetMailMaster()
//----------------------------------------------------------------------------------------
{
	if (!sMailMaster)
	{
		sMailMaster = MSG_InitializeMail(GetMailPrefs());
		ThrowIfNULL_(sMailMaster);
	}
	return sMailMaster;
}

/*
//----------------------------------------------------------------------------------------
void CMailNewsContext::DoProgress(const char* message, int level)
//----------------------------------------------------------------------------------------
{
	StatusInfo info; // constructor zeroes fields
	info.message = message;
	info.level = level;
	BroadcastMessage(CProgressBroadcaster::msg_StatusText, &info);
}

//----------------------------------------------------------------------------------------
void CMailNewsContext::DoSetProgressBarPercent(int32 percent)
//----------------------------------------------------------------------------------------
{
	StatusInfo info; // constructor zeroes fields
	info.percent = percent;
	BroadcastMessage(CProgressBroadcaster::msg_StatusPercent, &info);
}
//----------------------------------------------------------------------------------------
void CMailNewsContext::AllConnectionsComplete()
//----------------------------------------------------------------------------------------
{
	CNSContext::AllConnectionsComplete();
	StatusInfo info; // constructor zeroes fields
	BroadcastMessage(CProgressBroadcaster::msg_StatusText, &info);
	BroadcastMessage(CProgressBroadcaster::msg_StatusPercent, &info);
	BroadcastMessage(CProgressBroadcaster::msg_StatusComplete, &info);
}
*/

//----------------------------------------------------------------------------------------
/* static */ MSG_Prefs* CMailNewsContext::GetMailPrefs()	
//----------------------------------------------------------------------------------------
{
	if (!sMailPrefs)
	{
		sMailPrefs = MSG_CreatePrefs();
		ThrowIfNULL_(sMailPrefs);
		FSSpec mailFolder = CPrefs::GetFolderSpec(CPrefs::MailFolder);
		char* mailFolderPath = CFileMgr::EncodedPathNameFromFSSpec(mailFolder, true);
		
		MSG_SetFolderDirectory(sMailPrefs, mailFolderPath);
		XP_FREE(mailFolderPath);	
	}
   	return sMailPrefs;
}							

//----------------------------------------------------------------------------------------
char* CMailNewsContext::PromptWithCaption(
	const char*				inTitleBarText,
	const char* 			inMessage,
	const char*				inDefaultText)
//----------------------------------------------------------------------------------------
{
	char* result = NULL;
	CStr255 mesg(inMessage), ioString(inDefaultText);
	mesg = NET_UnEscape(mesg);

	uint8 maxLength;
	switch (GetCurrentCommand())
	{
		case cmd_RenameFolder:
		case cmd_NewFolder:
			maxLength = 27;
			break;
		default:
			maxLength = 255;
			break;
	}
	
	mWasPromptOkay = UStdDialogs::AskStandardTextPrompt(
		inTitleBarText, mesg, ioString, NULL, NULL, maxLength );
		
	if ( mWasPromptOkay )
	{
		if (ioString.Length() > 0)
		{
			if ( GetCurrentCommand() == cmd_NewFolder || 
				 GetCurrentCommand() == cmd_RenameFolder )
			{
				ioString = NET_UnEscape(ioString);				// make sure the path...
				char * temp = NET_Escape(ioString, URL_PATH);	// ...is fully escaped
				if (temp)
				{
					ioString = temp;
					XP_FREE(temp);
				}
			}

			result = (char*)XP_STRDUP((const char*)ioString);
		}
	}
		
	// If result is null then set as canceled
	if ( !result )
	{
		mWasPromptOkay = false;
		SetCurrentCommand(cmd_Nothing);
	}
	return result;	
}

//----------------------------------------------------------------------------------------
void CMailNewsContext::NoMoreUsers()
//----------------------------------------------------------------------------------------
{
	Inherited::NoMoreUsers();
#if 0
	// CNSContext::NoMoreUsers has so much unnecessary stuff in it now, that
	// we have to AVOID calling it.  We don't have mocha, images, layers, or any of that
	// stuff.  All we have is session history.
	MWContext* cx = (MWContext*)*this;
	LSharable::NoMoreUsers(); // which says "delete this".
	cx->fe.newContext = nil; // prevent callbacks (and crashes).
//#if DEBUG
	// Make sure we assert instead of crashing.
	CNSContextCallbacks* theCallbacks = CNSContextCallbacks::GetContextCallbacks();
	Assert_(theCallbacks != NULL);
	cx->funcs = &(theCallbacks->GetInternalCallbacks());
//#endif
#endif	// 0
}

//----------------------------------------------------------------------------------------
void CMailNewsContext::SwitchLoadURL(
	URL_Struct*				inURL,
	FO_Present_Types		inOutputFormat)
//----------------------------------------------------------------------------------------
{
	Inherited::SwitchLoadURL(inURL, inOutputFormat);
}

//----------------------------------------------------------------------------------------
void CMailNewsContext::AllConnectionsComplete()
//----------------------------------------------------------------------------------------
{	
	// Note: since this might lead to deletion of our host window (eg, a progress context),
	// we might lose all our users after broadcasting here.  On the other hand, we may be
	// called from our destructor, in which case making a StSharer here led to
	// double deletion on exit (when StSharer goes out of scope).
	// The solution?  Don't do anything if we know we are in a destruction sequence.
	if (GetUseCount() <= 0)
		return;
	StSharer theShareLock(this);
	Inherited::AllConnectionsComplete();
} // CMailNewsContext::AllConnectionsComplete
