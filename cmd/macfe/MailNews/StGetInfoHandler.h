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
 * Copyright (C) 1997 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// StGetInfoHandler.h

#pragma once
#include "UStdDialogs.h"

class LCommander;
class CMessageFolder;
class CServerListMediator;
class MSG_Host;


//======================================
class StGetInfoHandler : public StStdDialogHandler
//======================================
{
	public:
							StGetInfoHandler(ResIDT inDialogResID, 
									LCommander *inSuper)
							: StStdDialogHandler(inDialogResID, inSuper)
							,	mDataWritten(false)
							{}
		virtual				~StGetInfoHandler();
		virtual void		ReadDataFields() = 0;
		virtual void		WriteDataFields() = 0;
		virtual Boolean		UserDismissed();
		virtual	MessageT	DoDialog();
		
		virtual void		HideAndCripplePane(PaneIDT paneID);		// crippled panes don't write their prefs out
		virtual void		ShowAndUncripplePane(PaneIDT paneID);
	
	protected:
		Boolean		mDataWritten;
		
}; // class StGetInfoHandler


typedef Boolean	(*ServerNameValidationFunc)(const CStr255& inServerName, Boolean inNewServer, const CServerListMediator* inServerList);


//======================================
class UGetInfo
//======================================
{
	public:
		static UInt32 		CountIMAPServers();
		static void			ConductFolderInfoDialog(CMessageFolder&, LCommander* inSuper);
		enum { kStandAloneDialog = false, kSubDialogOfPrefs = true };
		static Boolean		ConductMailServerInfoDialog(
								CStr255& 					ioName,
								Boolean& 					ioPOP,
								Boolean 					inIsNewServer,
								Boolean						inAllowServerNameEdit = false,
								Boolean						inAllowTypeChange = false,		// whether to allow the user to change POP to IMAP or v.v.
								Boolean						inFromPrefsDialog = false,
								ServerNameValidationFunc	inValidationFunc = nil,
								CServerListMediator*		inValidationServerMediator = nil,
								LCommander* 				inSuper = nil);
		static Boolean		ConductNewsServerInfoDialog(
								MSG_Host* inHost,
								LCommander* inSuper = nil);
		static void			RegisterGetInfoClasses();
		static void			GetDefaultUserName(CStr255&);
};

