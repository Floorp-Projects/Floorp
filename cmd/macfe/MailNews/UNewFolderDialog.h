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



// UNewFolderDialog.h

#pragma once

class CMessageFolder;
class CMailCallbackListener;
class CStr255;
struct MSG_Pane;
struct MSG_FolderInfo;

//======================================
class UFolderDialogs
//======================================
{
public:
	static Boolean	ConductNewFolderDialog(
							const CMessageFolder& inParentFolder,
							CMailCallbackListener* inListener = nil);
								// Can listen for MSG_PaneNotifySelectNewFolder
	enum FolderKind { drafts, templates, mail_fcc, news_fcc, num_kinds };
	static CMessageFolder
					ConductSpecialFolderDialog(
							FolderKind inKind, // e.g. Sent
							const CMessageFolder& inCurFolder  // if pref is not saved yet
						);
						// Returns nil if user cancels, chosen folder.

	static void		BuildPrefName(
							FolderKind inKind,
							CStr255& outPrefName,
							Boolean& outSaveAsAlias);
	static Boolean	FindLocalMailHost(
							CMessageFolder& outFolder);
	static Boolean	GetDefaultFolderName(
							FolderKind inKind,
							CStr255& outFolderName);
	static Boolean	GetFolderAndServerNames(
							const char* inPrefName,
							CMessageFolder& outFolder,
							CStr255& outFolderName,
							CMessageFolder& outServer,
							Boolean* outMatchesDefault = nil);
	static Boolean	GetFolderAndServerNames(
							const CMessageFolder& inFolder,
							FolderKind inKind,
							CStr255& outFolderName,
							CMessageFolder& outServer,
							Boolean* outMatchesDefault = nil);
}; // class UFolderDialogs
