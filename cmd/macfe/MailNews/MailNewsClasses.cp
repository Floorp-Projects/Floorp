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



// MailNewsClasses.cp

#include "MailNewsClasses.h"

//#define REGISTER_(letter,root) \
//	RegisterClass_(letter##root::class_ID, \
//		(ClassCreatorFunc)letter##root::Create##root##Stream);

#define REGISTER_(letter,root) \
	RegisterClass_(letter##root);
		
#define REGISTERC(root) REGISTER_(C,root)
#define REGISTERL(root) REGISTER_(L,root)

// ### General Purpose UI Classes
	#include "CSimpleTextView.h"
		
// ### MailNews Specific UI Classes
	#include "CThreadView.h"
	#include "CSimpleFolderView.h"
	#include "CMessageFolderView.h"
	#include "CMessageWindow.h"
	#include "CMailNewsWindow.h"
	#include "CSubscribeView.h"
	#include "COfflinePicker.h"
	#include "CThreadWindow.h"
	#include "CMessageWindow.h"
	#include "CMessageView.h"
	#include "CMailComposeWindow.h"
	#include "CBiffButtonAttachment.h"
	#include "CSubscribeWindow.h"
	#include "CGAStatusBar.h"
	#include "LGABox_fixes.h"
	#include "SearchHelpers.h"
	#include "CSizeBox.h"
	
	#include "CExpandoDivider.h"

	#include "MailNewsSearch.h"
	#include "MailNewsFilters.h"

	#include "CMailFolderButtonPopup.h"
	#include "StGetInfoHandler.h"
	#include "CMailProgressWindow.h"
	#include "CMessageAttachmentView.h"
//-----------------------------------
void RegisterAllMailNewsClasses(void)
//-----------------------------------
{
	// ### General Purpose UI Classes
	REGISTERC(SimpleTextView)

	// stuff that was being registered twice (usually once in address book, the other time
	// in a search window). Since the new PP complains when you do this, do it once and for
	// all when we init mail/news
	RegisterClass_(CGAStatusBar);
	RegisterClass_(LGABox_fixes);
	RegisterClass_(CSearchEditField);
	RegisterClass_(CSizeBox);
	RegisterClass_(CSearchCaption);
	RegisterClass_(CSearchTabGroup);

	// ### MailNews Specific UI Classes
	REGISTERC(MailNewsFolderWindow)
	REGISTERC(ThreadWindow)
	REGISTERC(ThreadView)
	REGISTERC(SimpleFolderView)
	REGISTERC(MessageFolderView)
	REGISTERC(SubscribeView)
	REGISTERC(OfflinePickerView)
	REGISTERC(OfflinePickerWindow)
	REGISTERC(ExpandoDivider)
	REGISTERC(MessageWindow)
	REGISTERC(MessageView)
	REGISTERC(MailFolderButtonPopup)
	REGISTERC(MailFolderPatternTextPopup)
	REGISTERC(MailFolderGAPopup)
	REGISTERC(AttachmentView)
	REGISTERC(BiffButtonAttachment);
	REGISTERC(MailProgressWindow)
	REGISTERC(MessageAttachmentView)
	
	UComposeUtilities::RegisterComposeClasses();
	USubscribeUtilities::RegisterSubscribeClasses();
	CSearchWindowManager::RegisterSearchClasses();
	CFiltersWindowManager::RegisterFiltersClasses();
	UGetInfo::RegisterGetInfoClasses();
} // RegisterAllMailNewsClasses
