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



// MailNewsgroupWindow_Defines.h

#pragma once

// ID space
//
// All resources and cmd#s should fall within this range
//
// Mail/Newsgroups 10500-10999


// Menu ResIDs
enum
{
	kMailNewsViewMenuID		=	10500
,	kMailNewsMessageMenuID	=	10501
,	kMailNewsFolderMenuID	=	10502
,	kMailMenuID				=	10503
};


//-----------------------------------
//		COMMAND #s
// There are more command numbers in MailNewsGUI.h
//-----------------------------------
enum
{
	// these require messages to be selected
//	cmd_OpenMailMessage 		= 	cmd_Open
	cmd_OpenMailMessage 		= 	10500
//, cmd_DeleteMessage			=	10500 // NO.  Use cmd_Clear
,	cmd_ReplyToSender			=	10510
,	cmd_ReplyToAll				=	10511
,	cmd_ForwardMessage			=	10512
,	cmd_ForwardMessageQuoted	=	10513
,	cmd_AddSenderToAddressBook	=	10514
,	cmd_AddAllToAddressBook		=	10515
,	cmd_MarkRead				=	10516
,	cmd_MarkUnread				=	10517
, 	cmd_ForwardMessageAttachment = 	10518
,	cmd_ForwardMessageInline	= 	10519

//	 these require folders to be selected
//,	 cmd_DeleteFolder			=	10501 // No. Use cmd_Clear
,	cmd_RenameFolder			=	10520


	// these do not require a selection
//,	cmd_NewMailMessage		=	cmd_New	
,	cmd_NewMailMessage		=	10530	

,	cmd_EmptyTrash			=	10531
,	cmd_MailFilters			=	10532	
,	cmd_NextMessage			=	10534
,	cmd_NextUnreadMessage	=	10535
,	cmd_PreviousMessage		=	10536
	
	
,	cmd_FirstSortCommand	=	10537
,		cmd_SortByDate			=	10537
,		cmd_SortBySubject		=	10538
,		cmd_SortBySender		=	10539
,		cmd_SortByThread		=	10540
,		cmd_SortByPriority		=	10541
,		cmd_SortBySize			=	10542
,		cmd_SortByStatus		=	10543
,		cmd_SortByLocation		=	10544
,		cmd_SortByFlagged		=	10545
,		cmd_SortByReadness		=	10546
,		cmd_SortByOrderReceived = 	10547	// note new item here, and changed IDs below
,		cmd_SortAscending		=	10548
,		cmd_SortDescending		=	10549
,	cmd_LastSortCommand		=	cmd_SortByReadness

,	cmd_MarkReadByDate		=	10550
,	cmd_MarkReadForLater	=	10551
			
,	cmd_NewsGroups			=	10701	// Show folder window with news selected
,	cmd_ToolbarMode			=	0		// еее FIX ME: common!! advanced <-> novice
,	cmd_SendOutboxMail		=	10702	// еее Message Menu
	
,	cmd_CloseAll			=	10703

,	cmd_NextUnreadGroup		=	10704
,	cmd_NextFolder			=	10705
,	cmd_FirstFlagged		=	10706
,	cmd_PreviousFlagged		=	10707
,	cmd_NextFlagged			=	10708
,	cmd_NextUnreadThread	=	10709

,	cmd_SendMessage			=	10610
,	cmd_QuoteMessage		=	10611
,	cmd_SendMessageLater	=	10612
,	cmd_SaveDraft			=	10613

,	cmd_CopyMailMessages	=	10614	//ioParam = (char *), BE name of filing folder
,	cmd_MoveMailMessages	=	10615	//ioParam = (char *), BE name of filing folder

,	cmd_ViewAllThreads		=	10616
,	cmd_ViewKilledThreads	=	10617
,	cmd_ViewThreadsWithNew	=	10618
,	cmd_ViewWatchedThreadsWithNew=	10619
,	cmd_ViewNewOnly			=	10620

,	cmd_ToggleThreadWatched =	10621
,	cmd_ToggleThreadKilled	=	10622

,	cmd_AttachmentsInline	=	10623
,	cmd_AttachmentsAsLinks	=	10624

,	cmd_SearchAddresses		=	10627	// LDAP address search

,	cmd_ToggleOffline			=	10628	// Go Offline/Go Online
,	cmd_SelectForOffline		=	10629
,	cmd_FlaggedForOffline		=	10630
,	cmd_SynchronizeForOffline	=	10631

,	cmd_ToggleFolderPane		=	10635
,	cmd_SaveTemplate			=	10636

,	cmd_RelocateViewToFolder=	'MfPM'	// same as the class ID of the button that does it.

,	cmd_NewsFirst			=	14000	// Subscribe window
,	cmd_NewsToggleSubscribe	=	cmd_NewsFirst+0
,	cmd_NewsExpandGroup		=	cmd_NewsFirst+1
,	cmd_NewsExpandAll		=	cmd_NewsFirst+2
,	cmd_NewsCollapseGroup	=	cmd_NewsFirst+3
,	cmd_NewsCollapseAll		=	cmd_NewsFirst+4
,	cmd_NewsGetGroups		=	cmd_NewsFirst+5
,	cmd_NewsSearch			=	cmd_NewsFirst+6
,	cmd_NewsGetNew			=	cmd_NewsFirst+7
,	cmd_NewsClearNew		=	cmd_NewsFirst+8
,	cmd_NewsHostChanged		=	cmd_NewsFirst+9
,	cmd_NewsSetSubscribe	=	cmd_NewsFirst+10
,	cmd_NewsClearSubscribe	=	cmd_NewsFirst+11
};

// String stuff for menus that change
enum
{
	kMailNewsMenuStrings	= 10500
,	kOpenFolderStrID		= 1
,	kOpenDiscussionStrID
,	kOpenMessageStrID
,	kOpenMailServerStrID
,	kOpenNewsServerStrID
,	kNextChunkMessagesStrID
};




//
// Window PPob IDs
//
enum
{
	kMailNewsWindowPPob		=	10500
,	kMailMessageWindowPPob 	=	10506
,	kNewsgroupWindowPPob	=	10507

,	kRenameFolderDialogPPob	=	10510
,	kNewFolderDialogPPob	=	10511

,	kMailComposeWindowPPob	=	10610
};


// MailNews window pane ID's
enum
{
	kMailNewsTabSwitcherPaneID	=	'TbSw'
,	kMailNewsTabContainerPaneID	=	'Cont'
,	kMailNewsStatusPaneID	    =	'Stat'
,	kOfflineButtonPaneID	    =	'SBof'
};

//
// Tab IDs
//
// The message ID's associated with each tab.
//
// Independent of tab-order.
//
// These also match the ppob id's of each view
//

enum
{
	kInboxTabID			=	10501
,	kDraftsTabID		=	10502
,	kOutboxTabID		=	10503
,	kFiledMailTabID		=	10504
,	kNewsgroupsTabID	=	10505
};
	
//
// Table Column IDs
//
// The column header panes must have these id's
//
enum
{
	/* CThreadView */
	kMarkedReadMessageColumn	=	'Read'
,	kThreadMessageColumn		=	'Thrd' // also used for Icon column in dir. search results
,	kSubjectMessageColumn		=	'Subj'
,	kSenderMessageColumn		=	'Sndr'
,	kAddresseeMessageColumn		=	'SdTo'
,	kDateMessageColumn			=	'Date'
,	kPriorityMessageColumn		=	'Prio'
,	kSizeMessageColumn			=	'Size'
,	kUnreadMessageColumn		=	'Unrd'
,	kStatusMessageColumn		=	'Stus'
,	kTotalMessageColumn			=	'Totl'
,	kFlagMessageColumn			=	'Flag'
,	kHiddenOrderReceivedColumn	= 	'Rcvd'
	
	/* CMessageFolderView */
,	kFolderNameColumn			=	'FNam'
,	kFolderNumUnreadColumn		=	'NumU'
,	kFolderNumTotalColumn		=	'NumT'
	
	/* COfflinePickerView */
,	kSelectFolderColumn			= 	'SelF'
	
	/* CSubscribeView */
,	kNewsgroupNameColumn		=	'NuNm'
,	kNewsgroupSubscribedColumn	=	'NuSb'
,	kNewsgroupPostingsColumn	=	'NuPo'


	/* CSearchTableView */
,	kStatusMessageLocation		=	'Loca'
,	kNameEntryColumn			=	'Name'
,	kEmailEntryColumn			=	'Emal'
,	kCompanyEntryColumn			=	'Comp'
,	kCityEntryColumn			=	'City'
,	kPhoneEntryColumn			=	'Phon'
};

//
// ID#s of 'Cols' resources, for saving the column 
// positions of the various table views...
//
enum
{
	kSavedInboxColumnStateID			=	10501,
	kSavedDraftsColumnStateID			=	10502,
	kSavedOutboxColumnStateID			=	10503,
	kSavedFiledMailColumnStateID		=	10504,
	kSavedFiledMailFolderColumnStateID	=	10506,
	
	kSavedNewsgroupColumnStateID		=	10505
};
