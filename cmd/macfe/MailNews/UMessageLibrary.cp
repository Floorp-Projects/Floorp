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



// UMessageLibrary.cp

#include "UMessageLibrary.h"

// Want a command number?  They hide in several places...
#include "Netscape_Constants.h"
#include "resgui.h"
#include "MailNewsgroupWindow_Defines.h"
#include "CApplicationEventAttachment.h"

// necessary for hack that makes sure we can only have one
// open MSG_GetNewMail command at a time
#include "CMailProgressWindow.h"

#include "macutil.h"
#include "PascalString.h"
#include "UOffline.h"

//======================================
// class UMessageLibrary
// Utility calls
//======================================

//-----------------------------------
/*static*/ Boolean UMessageLibrary::FindMessageLibraryCommandStatus(
	MSG_Pane*			inPane,
	MSG_ViewIndex*		inIndices,
	int32				inNumIndices,
	CommandT			inCommand,
	Boolean				&outEnabled,
	Boolean				&outUsesMark,
	Char16				&outMark,
	Str255				outName)
// returns false if not a msglib command.
// Commands enabled/disabled here are obeyed in CMailFlexTable::ObeyMessageLibraryCommand
//-----------------------------------
{
	MSG_CommandType cmd;
	switch (inCommand)
	{
		case cmd_ToggleOffline:
		case cmd_SynchronizeForOffline:
			UOffline::FindOfflineCommandStatus(inCommand, outEnabled, outName);
			return true;
		case cmd_ToggleThreadKilled:
	// cmd_ToggleThreadKilled: ugh! it's a navigate command, but navigatestatus
	// doesn't support it!  So when testing for enabling, we have to call commandstatus,
	// but in executing, we have to call navigate.  UMessageLibrary now treats this
	// as a navigate command, hence this hack here to override this behavior.
			cmd = MSG_ToggleThreadKilled;
			break;
		case cmd_CompressAllFolders:
			cmd = MSG_CompressFolder; // see hack below.
			break;
		default:
			cmd = UMessageLibrary::GetMSGCommand(inCommand);
			break;
	}
	

	if (!UMessageLibrary::IsValidCommand(cmd))
		return false;
	if (inPane == nil)
		return false;

	XP_Bool selectable = false;
	MSG_COMMAND_CHECK_STATE checkedState;
	const char* display_string = nil;
	XP_Bool plural;
	if (MSG_CommandStatus(
			inPane,
			cmd,
			inIndices,
			inNumIndices,
			&selectable,
			&checkedState,
			&display_string,
			&plural)
		>= 0)
	{
		// The logic is: if the selection permits MSG_CompressFolder, then do that.
		// Otherwise, try for MSG_CompressAllFolders.
		if (cmd == MSG_CompressFolder && !selectable)
		{
			const char* redo_string = nil;
			if (MSG_CommandStatus(
				inPane,
				MSG_CompressAllFolders,
				inIndices,
				inNumIndices,
				&selectable,
				&checkedState,
				&redo_string,
				&plural)
			>= 0 && selectable)
			{
				display_string = redo_string;
			}
		}
		outEnabled = (Boolean)selectable;
		outUsesMark = true;
		if (checkedState == MSG_Checked)
			outMark = checkMark;
		else
			outMark = 0;
		switch (cmd)
		{
			// commands where we don't like the back end's command strings:
			case MSG_AddSender:
			case MSG_AddAll:
 			case MSG_ReplyToSender:
 			case MSG_ReplyToAll:
 			case MSG_PostReply:
 			case MSG_PostAndMailReply:
 			case MSG_DoRenameFolder:
 			case MSG_DeleteMessage:
 			case MSG_DeleteMessageNoTrash:
				break;
			// All other commands, use the back-end string, if any.
			default:
				if (display_string && *display_string)
					*(CStr255*)outName = display_string;
		}		
	}
	
	if ((cmd == MSG_GetNewMail) && selectable)
		if (CMailProgressWindow::GettingMail())
			outEnabled = false;
			
	return true;
}

//-----------------------------------
MSG_CommandType UMessageLibrary::GetMSGCommand(CommandT inCommand)
//-----------------------------------
{
	switch (inCommand)
	{
		case cmd_Undo:						return MSG_Undo;
		case cmd_Redo:						return MSG_Redo;
		case cmd_Clear:
											return (
												CApplicationEventAttachment::CurrentEventHasModifiers(optionKey)
													? MSG_DeleteNoTrash
													: MSG_Delete
													);
		case cmd_NewFolder:					return MSG_NewFolder;
		case cmd_RenameFolder:				return MSG_DoRenameFolder;
		case cmd_EmptyTrash:				return MSG_EmptyTrash;
		case cmd_CompressFolder:			return MSG_CompressFolder;
		case cmd_CompressAllFolders:		return MSG_CompressAllFolders;
		case cmd_ManageMailAccount:			return MSG_ManageMailAccount;
		case cmd_NewNewsgroup:				return MSG_NewNewsgroup;
		case cmd_ModerateNewsgroup:			return MSG_ModerateNewsgroup;

		case cmd_NewMailMessage:			return MSG_MailNew;
		case cmd_ReplyToSender:				return MSG_ReplyToSender;
		case cmd_PostReply:					return MSG_PostReply;
		case cmd_PostAndMailReply:			return MSG_PostAndMailReply;
		case cmd_PostNew:					return MSG_PostNew;
		case cmd_ReplyToAll:				return MSG_ReplyToAll;
		case cmd_ForwardMessage:			return MSG_ForwardMessage;
		case cmd_ForwardMessageQuoted:		return MSG_ForwardMessageQuoted;
		case cmd_ForwardMessageAttachment:  return MSG_ForwardMessageAttachment;
		case cmd_ForwardMessageInline:		return MSG_ForwardMessageInline;
		case cmd_MarkMessage:				return MSG_MarkMessages;		// same cmd
		case cmd_MarkSelectedMessages:		return MSG_MarkMessages;		//  ''  ''
		case cmd_MarkUnread:				return MSG_MarkMessagesUnread;
		case cmd_MarkRead:					return MSG_MarkMessagesRead;
		case cmd_UnmarkSelectedMessages:	return MSG_UnmarkMessages;
		case cmd_MarkThreadRead:			return MSG_MarkThreadRead;
		case cmd_MarkAllRead:				return MSG_MarkAllRead;

		case cmd_GetNewMail:				return MSG_GetNewMail;
		case cmd_SelectForOffline:			return MSG_RetrieveSelectedMessages;
		case cmd_FlaggedForOffline:			return MSG_RetrieveMarkedMessages;

		case cmd_GetMoreMessages:			return MSG_GetNextChunkMessages;
		case cmd_ShowOnlyUnreadMessages:	return MSG_ViewNewOnly;
		case cmd_ShowMicroMessageHeaders:	return MSG_ShowMicroHeaders;
		case cmd_ShowSomeMessageHeaders:	return MSG_ShowSomeHeaders;
		case cmd_ShowAllMessageHeaders:		return MSG_ShowAllHeaders;
		case cmd_DeliverQueuedMessages:		return MSG_DeliverQueuedMessages;
		case cmd_UpdateMessageCount:		return MSG_UpdateMessageCount;
		case cmd_ShowAllMessages:			return MSG_ViewAllThreads;

		case cmd_AttachmentsInline:			return MSG_AttachmentsInline;
		case cmd_AttachmentsAsLinks:		return MSG_AttachmentsAsLinks;
		case cmd_AddSenderToAddressBook:	return MSG_AddSender;
		case cmd_AddAllToAddressBook:		return MSG_AddAll;
		//case cmd_ToggleThreadKilled:		return MSG_ToggleThreadKilled;  // actually a motiontype!
		case cmd_ToggleThreadWatched:		return MSG_ToggleThreadWatched;
		case cmd_ViewAllThreads:			return MSG_ViewAllThreads;
		case cmd_ViewKilledThreads:			return MSG_ViewKilledThreads;
		case cmd_ViewThreadsWithNew:		return MSG_ViewThreadsWithNew;
		case cmd_ViewWatchedThreadsWithNew:	return MSG_ViewWatchedThreadsWithNew;
		case cmd_ViewNewOnly:				return MSG_ViewNewOnly;
		case cmd_EditMessage:				return MSG_OpenMessageAsDraft;
		case cmd_WrapLongLines:				return MSG_WrapLongLines;

//#define MAP_CMD_TO_MSG(verb) case cmd_##verb: return MSG_##verb;

	}
	return (MSG_CommandType)invalid_command;
}

//-----------------------------------
MSG_MotionType UMessageLibrary::GetMotionType(CommandT inCommand)
//-----------------------------------
{
	switch (inCommand)
	{
		case cmd_Back:						return MSG_Back;
		case cmd_GoForward:					return MSG_Forward;
		case cmd_NextMessage:				return MSG_NextMessage;
		case cmd_NextUnreadMessage:			return MSG_NextUnreadMessage;
		case cmd_PreviousUnreadMessage:		return MSG_PreviousUnreadMessage;
		case cmd_PreviousMessage:			return MSG_PreviousMessage;
		case cmd_NextUnreadGroup:			return MSG_NextUnreadGroup;
		case cmd_NextUnreadThread:			return MSG_NextUnreadThread;
		case cmd_MarkReadForLater:			return MSG_LaterMessage;
		case cmd_NextFolder:				return MSG_NextFolder;
		case cmd_FirstFlagged:				return MSG_FirstFlagged;
		case cmd_PreviousFlagged:			return MSG_PreviousFlagged;
		case cmd_NextFlagged:				return MSG_NextFlagged;
		case cmd_ToggleThreadKilled:		return (MSG_MotionType)MSG_ToggleThreadKilled;

	}
	return (MSG_MotionType)invalid_command;
}

