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


//	Created 3/25/96 - Tim Craycroft

#include "CThreadView.h"

// App specific
#include "resgui.h"
#include "macutil.h"
#include "Netscape_Constants.h"
#include "CBrowserContext.h"

#include "libi18n.h"

// Mail/News Specific
#include "CNewsSubscriber.h"
#include "Netscape_Constants.h"
#include "CMailNewsContext.h"
#include "CMessageWindow.h"
#include "CMessageView.h"
#include "MailNewsgroupWindow_Defines.h"
#include "LTableViewHeader.h"
#include "CThreadWindow.h"
#include "UMailFolderMenus.h"
#include "CMailFolderButtonPopup.h"
#include "UMessageLibrary.h"
#include "CMailProgressWindow.h"
#include "CProgressListener.h"
#include "UMailSelection.h"

//	utility
#include "UPropFontSwitcher.h"
#include "UUTF8TextHandler.h"
#include "SearchHelpers.h"
#include "CDrawingState.h"
#include "URobustCreateWindow.h"
#include "CApplicationEventAttachment.h"
// remove next line when we move the PlaceUTF8TextInRect to utility classes
#include "UGraphicGizmos.h"
#include "CTargetedUpdateMenuRegistry.h"
#include "CProxyDragTask.h"
#include "UNewFolderDialog.h"
#include "UDeferredTask.h"

// XP
#include "ntypes.h"
#include "msg_srch.h" // for MSG_GetPriorityName, MSG_GetStatusName

// application
#include "uapp.h"
#include "prefapi.h"
#include "secnav.h"
#include "shist.h"

// PowerPlant
#include <LTableMultiSelector.h>
#include <LTableArrayStorage.h>
#include <URegions.h>
#include <LDropFlag.h>
#include <LSharable.h>
#include <UModalDialogs.h>

// Mac
#include <Drag.h>

// ANSI
#include <string.h>
#include <stdio.h>

#define kPriorityMenuID	10528

enum {
	kThreadedIcon	= 10501,
	kUnthreadedIcon	= 10502
};

static const UInt16 kMaxSubjectLength = 256;

#pragma mark -

MSG_MessageLine	CMessage::sMessageLineData;
MSG_Pane*		CMessage::sCacheMessageList = NULL;
MSG_ViewIndex	CMessage::sCacheIndex	 	= 0;

//----------------------------------------------------------------------------------------
CMessage::CMessage(MSG_ViewIndex inIndex, MSG_Pane* inMessageList)
//----------------------------------------------------------------------------------------
:	mIndex(inIndex-1)
,	mMessageList(inMessageList)
{
}

//----------------------------------------------------------------------------------------
CMessage::~CMessage()
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
Boolean CMessage::UpdateMessageCache() const
//----------------------------------------------------------------------------------------
{
	if (mMessageList && (mIndex != sCacheIndex || mMessageList != sCacheMessageList))
	{
		if (!::MSG_GetThreadLineByIndex(mMessageList, mIndex, 1, &sMessageLineData))
		{
			InvalidateCache();
			return false;
		}
		sCacheIndex = mIndex;
		sCacheMessageList = mMessageList;
	}
	return true;
} // CMessage::UpdateMessageCache

//----------------------------------------------------------------------------------------
void CMessage::InvalidateCache()
//----------------------------------------------------------------------------------------
{
	sCacheMessageList = NULL;
}

//----------------------------------------------------------------------------------------
inline Boolean CMessage::TestXPFlag(UInt32 inMask) const
//----------------------------------------------------------------------------------------
{
	UpdateMessageCache();
	return ((sMessageLineData.flags & inMask) != 0);
}

//----------------------------------------------------------------------------------------
inline Boolean CMessage::IsOffline() const // db has offline news article 
//----------------------------------------------------------------------------------------
{
	return TestXPFlag(MSG_FLAG_OFFLINE); 
}

//----------------------------------------------------------------------------------------
inline Boolean CMessage::IsDeleted() const 
//----------------------------------------------------------------------------------------
{
	return TestXPFlag(MSG_FLAG_IMAP_DELETED); 
}

//----------------------------------------------------------------------------------------
inline Boolean CMessage::IsTemplate() const 
//----------------------------------------------------------------------------------------
{
	return TestXPFlag(MSG_FLAG_TEMPLATE); 
}

//----------------------------------------------------------------------------------------
inline Boolean CMessage::HasBeenRead() const
//----------------------------------------------------------------------------------------
{
	return TestXPFlag(MSG_FLAG_READ);
}

//----------------------------------------------------------------------------------------
inline Boolean CMessage::IsFlagged() const
//----------------------------------------------------------------------------------------
{
	return TestXPFlag(MSG_FLAG_MARKED);
}

//----------------------------------------------------------------------------------------
inline Boolean CMessage::IsThread() const
//----------------------------------------------------------------------------------------
{
	UpdateMessageCache();
	return (sMessageLineData.numChildren > 0);
}

//----------------------------------------------------------------------------------------
inline Boolean CMessage::HasAttachments() const
//----------------------------------------------------------------------------------------
{
	return TestXPFlag(MSG_FLAG_ATTACHMENT); 
}

//----------------------------------------------------------------------------------------
inline Boolean CMessage::IsOpenThread() const
//----------------------------------------------------------------------------------------
{
	Assert_(IsThread()); 	
	return !TestXPFlag(MSG_FLAG_ELIDED); 
}

//----------------------------------------------------------------------------------------
inline Boolean CMessage::HasBeenRepliedTo() const
//----------------------------------------------------------------------------------------
{
	return TestXPFlag(MSG_FLAG_REPLIED);
}

//----------------------------------------------------------------------------------------
inline MessageId CMessage::GetThreadID() const
//----------------------------------------------------------------------------------------
{
	UpdateMessageCache();
	return sMessageLineData.threadId;
}
	
//----------------------------------------------------------------------------------------
MessageKey CMessage::GetMessageKey() const
//----------------------------------------------------------------------------------------
{
	UpdateMessageCache();
	return sMessageLineData.messageKey;
}
	
//----------------------------------------------------------------------------------------
uint16 CMessage::GetNumChildren() const
//----------------------------------------------------------------------------------------
{
	UpdateMessageCache();
	return sMessageLineData.numChildren;
}
	
//----------------------------------------------------------------------------------------
uint16 CMessage::GetNumNewChildren() const
//----------------------------------------------------------------------------------------
{
	UpdateMessageCache();
	return sMessageLineData.numNewChildren;
}
	
//----------------------------------------------------------------------------------------
/* static */ const char* CMessage::GetSubject(
	MSG_MessageLine* data,
	char* buffer,
	UInt16 bufSize)
//----------------------------------------------------------------------------------------
{
	*buffer = 0;
	if ((data->flags & MSG_FLAG_HAS_RE) != 0)
	{
		const CString** sh = (const CString**)GetString(REPLY_FORM_RESID);
		if (sh)
		{
			Assert_(bufSize > (**sh).Length());
			XP_SAFE_SPRINTF(buffer, bufSize - 1, (const char*)(**sh), data->subject);
			::ReleaseResource((Handle)sh);
			return buffer;
		}
	}
	return XP_STRNCPY_SAFE(buffer, data->subject, bufSize);
}

//----------------------------------------------------------------------------------------
const char* CMessage::GetSubject(char* buffer, UInt16 bufSize) const
//----------------------------------------------------------------------------------------
{
	UpdateMessageCache();
	return GetSubject(&sMessageLineData, buffer, bufSize);
}

//----------------------------------------------------------------------------------------
const char* CMessage::GetSubject() const
//----------------------------------------------------------------------------------------
{
	UpdateMessageCache();
	return sMessageLineData.subject;
}

//----------------------------------------------------------------------------------------
inline MSG_PRIORITY CMessage::GetPriority() const
//----------------------------------------------------------------------------------------
{
	UpdateMessageCache();
	return sMessageLineData.priority;
}

//----------------------------------------------------------------------------------------
void CMessage::GetPriorityColor(MSG_PRIORITY priority, RGBColor& priorityColor) /* static */
//----------------------------------------------------------------------------------------
{
	// Initialize to black
	memset(&priorityColor, 0, sizeof(RGBColor));
	switch (priority)
	{
		case MSG_LowestPriority: // green
			priorityColor.green = 0x7FFF;
			break;
		case MSG_LowPriority: // blue
			priorityColor.blue = 0xFFFF;
			break;
		case MSG_HighPriority: // purple
			priorityColor.red = 0x7FFF;
			priorityColor.blue = 0x7FFF;
			break;
		case MSG_HighestPriority: // red
			priorityColor.red = 0xFFFF;
			break;
		case MSG_NormalPriority:
		case MSG_PriorityNotSet:
		case MSG_NoPriority:
		default:
			break;
	}
} // CMessage::GetPriorityColor

//----------------------------------------------------------------------------------------
void CMessage::GetPriorityColor(RGBColor& priorityColor) const
//----------------------------------------------------------------------------------------
{
	// Don't draw deleted messages in color.
	MSG_PRIORITY priority = IsDeleted() ? MSG_NormalPriority : GetPriority();
	GetPriorityColor(priority, priorityColor);
} // CMessage::GetPriorityColor

//----------------------------------------------------------------------------------------
inline Int16 CMessage::PriorityToMenuItem(MSG_PRIORITY inPriority) /* static */
//----------------------------------------------------------------------------------------
{
	return (inPriority > MSG_NoPriority) ? inPriority - 1 : 3;
}

//----------------------------------------------------------------------------------------
inline MSG_PRIORITY CMessage::MenuItemToPriority(Int16 inMenuItem) /* static */
//----------------------------------------------------------------------------------------
{
	return (MSG_PRIORITY)(inMenuItem + 1);
}

//----------------------------------------------------------------------------------------
inline const char* CMessage::GetSender() const
//----------------------------------------------------------------------------------------
{
	UpdateMessageCache();
	return sMessageLineData.author;
}

//----------------------------------------------------------------------------------------
inline const char* CMessage::GetDateString() const
//----------------------------------------------------------------------------------------
{
//	UpdateMessageCache();
	return MSG_FormatDate(mMessageList, GetDate());
}

//----------------------------------------------------------------------------------------
inline const char* CMessage::GetAddresseeString() const
//----------------------------------------------------------------------------------------
{
	UpdateMessageCache();
	return sMessageLineData.author;
}

//----------------------------------------------------------------------------------------
const char * CMessage::GetSizeStr() const
//	¥ FIX ME: Is there an XP call for constructing the size string ?
//----------------------------------------------------------------------------------------
{
	static char sizeString[32];
	UInt32 messageSize = GetSize();
	CStr255 formatString;
	if (messageSize > (1024*1000))
	{
		::GetIndString(formatString, 7099, 4);
		float floatSize = (float)messageSize / (1024*1000);
		sprintf(sizeString, formatString, floatSize);
	}
	else
	{
		if (messageSize > 1024)
		{
			::GetIndString(formatString, 7099, 5);
			messageSize /= 1024;
		}
		else
		{
			::GetIndString(formatString, 7099, 6);
		}
		sprintf(sizeString, formatString, messageSize);
	}
	return sizeString;
}

//----------------------------------------------------------------------------------------
const char * CMessage::GetPriorityStr() const
//----------------------------------------------------------------------------------------
{
	static char buffer[256];
	buffer[0] = '\0';
	MSG_PRIORITY priority = GetPriority();
	if (priority == MSG_PriorityNotSet
		|| priority == MSG_NoPriority
		|| priority == MSG_NormalPriority)
		return buffer;
	MSG_GetPriorityName(priority, buffer, sizeof(buffer)-1);
	return buffer;
}


//----------------------------------------------------------------------------------------
const char * CMessage::GetStatusStr() const
//----------------------------------------------------------------------------------------
{
	static char buffer[256];
	MSG_GetStatusName(GetStatus(), buffer, sizeof(buffer)-1);
	return buffer;
}

//----------------------------------------------------------------------------------------
inline time_t CMessage::GetDate() const
//----------------------------------------------------------------------------------------
{
	UpdateMessageCache();
	return sMessageLineData.date;
}
	
//----------------------------------------------------------------------------------------
inline UInt32 CMessage::GetSize() const
//----------------------------------------------------------------------------------------
{
	UpdateMessageCache();
	return sMessageLineData.messageLines; // bytes for mail, lines for news.  Oh well!
}

//----------------------------------------------------------------------------------------
inline UInt32 CMessage::GetStatus() const
//----------------------------------------------------------------------------------------
{
	UpdateMessageCache();
	return sMessageLineData.flags;
		// actually, not all bits are relevant, but there's no mask in msgcom.h.
}

//----------------------------------------------------------------------------------------
inline int8 CMessage::GetThreadLevel() const
//----------------------------------------------------------------------------------------
{
	UpdateMessageCache();
	return sMessageLineData.level;
}

//======================================
// Icon suite IDs
//======================================
#pragma mark
enum IconTable
{
	kMessageReadIcon	=	15234			// icon to mark a message as read
,	kFlagIcon			=	15236			// icon to mark a message as read

	// Icons for representing mail messages
,	kMailMessageIcon						=	15228
,	kReadMailMessageIcon					=	15229
,	kMailMessageWithAttachmentIcon			=	10511
,	kFlaggedMailMessageIcon					=	10512
,	kFlaggedMailMessageWithAttachmentIcon	=	10513

	// News articles
,	kNewsArticleMessageIcon					=	15231
,	kReadNewsArticleMessageIcon				=	15232
,	kNewsArticleReadFlaggedMessageIcon		=	15232
,	kNewsArticleFlaggedMessageIcon			=	15234
};

static ResIDT gIconIDTable[] = {
//-------------------------------------------------------------------------------------------------------------
//	NAME		UNREAD						READ				NEW						(UNUSED)		
//			LOCAL		ONLINE		LOCAL		ONLINE		LOCAL		ONLINE		LOCAL		ONLINE	
//-------------------------------------------------------------------------------------------------------------
/* POP */	15228	,		0	,	15229	,		0	,	15407	,		0	,		0	,		0	,
/* IMAP */	15228	,	15226	,	15229	,	15226	,	15407	,	15407	,	15523	,	15523	,
/* Art */	15518	,	15233	,	15519	,	15396	,		0	,		0	,		0	,		0	,
/* Drft */	15230	,	15230	,	15230	,	15230	,	15230	,	15230	,		0	,		0	,
};       //----------------------------------------------------------------------------------------------------

enum {
	kOnline			= 1 // offset
,	kRead			= 2	// offset
,	kNew			= 4	// offset
,	kDeleted		= 6	// offset
,	kKindsPerRow	= 8
,	kMessageIx		= 0
,	kIMAPIx			= 1 * kKindsPerRow
,	kArticleIx		= 2 * kKindsPerRow
,	kDraftIx		= 3 * kKindsPerRow
};

//----------------------------------------------------------------------------------------
ResIDT CMessage::GetIconID(UInt16 inFolderFlags) const
//----------------------------------------------------------------------------------------
{
	UpdateMessageCache();
	return GetIconID(inFolderFlags, sMessageLineData.flags);
} // CMessage::GetIconID

//----------------------------------------------------------------------------------------
ResIDT CMessage::GetIconID(UInt16 inFolderFlags, UInt32 inMessageFlags)
//----------------------------------------------------------------------------------------
{
#define IMAP_FOLDER	(MSG_FOLDER_FLAG_IMAPBOX | MSG_FOLDER_FLAG_MAIL)
	short iconIndex = 0;
	// Special case of pop folder because MSG_FLAG_OFFLINE is not correct for pop folders.
	Boolean isPopFolder = (inFolderFlags & IMAP_FOLDER) == MSG_FOLDER_FLAG_MAIL;
	Boolean isNews = (inFolderFlags & MSG_FOLDER_FLAG_NEWSGROUP) == MSG_FOLDER_FLAG_NEWSGROUP;

	// First, what kind of folder are we in (determines the basic icon type, the row in
	// the table above)
	if ((inFolderFlags & IMAP_FOLDER) == IMAP_FOLDER)
		iconIndex = kIMAPIx;
	else if (inFolderFlags & MSG_FOLDER_FLAG_MAIL)
		iconIndex = kMessageIx; // FIXME: How do we tell whether it has attachments?
	else if (inFolderFlags & MSG_FOLDER_FLAG_NEWSGROUP)
		iconIndex = kArticleIx;
	else if (inFolderFlags & MSG_FOLDER_FLAG_DRAFTS)
		iconIndex = kDraftIx;

	// Next, which of the four major columns are we in.
	if (!isNews && !isPopFolder && (inMessageFlags & MSG_FLAG_IMAP_DELETED) != 0)
		iconIndex += kDeleted;
	else if (!isNews && (inMessageFlags & MSG_FLAG_NEW) != 0)
		iconIndex += kNew;
	else if ((inMessageFlags & MSG_FLAG_READ) != 0)
		iconIndex += kRead;

	// Finally, choose the local/offline column.
	// Note: pop does not support the "offline" flag bit, it should always be set
	// (but isn't)
	if (!isPopFolder && (inMessageFlags & MSG_FLAG_OFFLINE) == 0)
		iconIndex += kOnline;

	return gIconIDTable[iconIndex];
} // CMessage::GetIconID

//======================================
// Thread Icon IDs
//======================================
enum ThreadIconTable
{
	// Icons for representing threaded messages
	kThreadIcon								=	15399
,	kNewThreadIcon							=	15401
,	kWatchedThreadIcon						=	15403
,	kWatchedNewThreadIcon					=	15404
,	kIgnoredThreadIcon						=	15429

};

static ResIDT gThreadIconIDTable[] = {
//-----------------------------------------------------------
//	NAME			UNWATCHED					WATCHED		
//				OLD			UNREAD			OLD			UNREAD
//-----------------------------------------------------------
/* Thread */	15399	,	15401	,	15403	,	15404	
};       //--------------------------------------------------

enum {
	kUnread			= 1 // offset
,	kWatched		= 2	// offset
};

//----------------------------------------------------------------------------------------
ResIDT CMessage::GetThreadIconID() const
//----------------------------------------------------------------------------------------
{
	UpdateMessageCache();
	short iconIndex = 0;
	if ((sMessageLineData.flags & MSG_FLAG_IGNORED) != 0)
		return kIgnoredThreadIcon;
	if (GetNumNewChildren() > 0)
		iconIndex += kUnread;
	if ((sMessageLineData.flags & MSG_FLAG_WATCHED) != 0)
		iconIndex += kWatched;
	return gThreadIconIDTable[iconIndex];
} // CMessage::GetIconID

#pragma mark -

//========================================================================================
class CDeferredLoadFolderTask
//========================================================================================
:	public CDeferredTask
{
	public:
								CDeferredLoadFolderTask(
									CThreadView*		inView,
									CNSContext*	inContext,
									CMessageFolder		inFolder);
	protected:
		virtual ExecuteResult	ExecuteSelf();
// data
	protected:
		CThreadView*			mThreadView;
		CNSContext*				mContextToLoadAtIdle;
		CMessageFolder			mFolderToLoadAtIdle;
		UInt32					mEarliestExecuteTime;
}; // class CDeferredLoadFolderTask

//----------------------------------------------------------------------------------------
CDeferredLoadFolderTask::CDeferredLoadFolderTask(
	CThreadView*		inView,
	CNSContext*	inContext,
	CMessageFolder		inFolder)
//----------------------------------------------------------------------------------------
:	mThreadView(inView)
,	mContextToLoadAtIdle(inContext)
,	mFolderToLoadAtIdle(inFolder)
,	mEarliestExecuteTime(::TickCount() + GetDblTime())
{
}

//----------------------------------------------------------------------------------------
CDeferredTask::ExecuteResult CDeferredLoadFolderTask::ExecuteSelf()
//----------------------------------------------------------------------------------------
{
	// Wait for the earliest execute time.  It's expensive to load a folder, and if the
	// user is nervously clicking on multiple folders, we only want to load the last one.
	if (::TickCount() < mEarliestExecuteTime)
		return eWaitStayFront;
	if (CMailProgressWindow::GetModal())
		return eWaitStayFront; // wait until other modal tasks are done.
	mThreadView->LoadMessageFolder(mContextToLoadAtIdle, mFolderToLoadAtIdle, true);
	return eDoneDelete;
} // CDeferredLoadFolderTask::ExecuteSelf

#pragma mark -

//========================================================================================
class CDeferredUndoTask
//========================================================================================
:	public CDeferredTask
{
	public:
								CDeferredUndoTask(
									CThreadView*		inView);
	void						SetUndoStatus(UndoStatus inStatus)
								{
									mUndoStatus = inStatus;
								}
	protected:
		virtual ExecuteResult	ExecuteSelf();
// data
	protected:
		CThreadView*			mThreadView;
		UndoStatus				mUndoStatus;
}; // class CDeferredUndoTask

//----------------------------------------------------------------------------------------
CDeferredUndoTask::CDeferredUndoTask(
	CThreadView*		inView)
//----------------------------------------------------------------------------------------
:	mThreadView(inView)
,	mUndoStatus(UndoInProgress)
{
}

//----------------------------------------------------------------------------------------
CDeferredTask::ExecuteResult CDeferredUndoTask::ExecuteSelf()
//----------------------------------------------------------------------------------------
{
	if (mUndoStatus == UndoComplete)
		return eDoneDelete; // remove from queue
	else if (mUndoStatus == UndoInProgress)
	{
		mUndoStatus = MSG_GetUndoStatus(mThreadView->GetMessagePane());
		switch (mUndoStatus)
		{
			case UndoComplete:
				MSG_FolderInfo* finfo;
				MessageKey key;
				MSG_Pane* pane = mThreadView->GetMessagePane();
				if (MSG_GetUndoMessageKey(pane, &finfo, &key))
				{
					if (finfo && finfo != mThreadView->GetOwningFolder())
					{
						mThreadView->LoadMessageFolder(mThreadView->GetContext(), finfo, false);
					}
					mThreadView->SelectMessageWhenReady(key);
				}
				break;
			case UndoInProgress:
				return eWaitStayFront; // block, leave in queue
			default:
				return eDoneDelete;
		} // switch
	}
	return eDoneDelete; // remove from queue
} // CDeferredUndoTask::ExecuteSelf

#pragma mark -

//========================================================================================
class CDeferredSelectRowTask
//========================================================================================
:	public CDeferredTask
{
	public:
									CDeferredSelectRowTask(
										CThreadView*		inView,
										TableIndexT			inRow);
	protected:
		virtual ExecuteResult		ExecuteSelf();
// data
	protected:
		CThreadView*				mThreadView;
		TableIndexT					mRow;
}; // class CDeferredSelectRowTask

//----------------------------------------------------------------------------------------
CDeferredSelectRowTask::CDeferredSelectRowTask(
	CThreadView*		inView,
	TableIndexT			inRow)
//----------------------------------------------------------------------------------------
:	mThreadView(inView)
,	mRow(inRow)
{
}

//----------------------------------------------------------------------------------------
CDeferredTask::ExecuteResult CDeferredSelectRowTask::ExecuteSelf()
//----------------------------------------------------------------------------------------
{
	mThreadView->SelectAfterDelete(mRow);
	return eDoneDelete; // remove from queue
} // CDeferredSelectRowTask::ExecuteSelf

#pragma mark -

//========================================================================================
class CScrollToNewMailTask
//========================================================================================
:	public CDeferredTask
{
	public:
									CScrollToNewMailTask(
										CThreadView*		inView);
	protected:
		virtual ExecuteResult		ExecuteSelf();
// data
	protected:
		CThreadView*				mThreadView;
}; // class CScrollToNewMailTask

//----------------------------------------------------------------------------------------
CScrollToNewMailTask::CScrollToNewMailTask(
	CThreadView*		inView)
//----------------------------------------------------------------------------------------
:	mThreadView(inView)
{
}

//----------------------------------------------------------------------------------------
CDeferredTask::ExecuteResult CScrollToNewMailTask::ExecuteSelf()
//----------------------------------------------------------------------------------------
{
	mThreadView->ScrollToGoodPlace();
	return eDoneDelete; // remove from queue
} // CScrollToNewMailTask::ExecuteSelf

#pragma mark -

//========================================================================================
class CDeferredSelectKeyTask
//========================================================================================
:	public CDeferredTask
{
	public:
								CDeferredSelectKeyTask(
									CThreadView*		inView,
									MessageKey			inKey);
	protected:
		virtual ExecuteResult	ExecuteSelf();
// data
	protected:
		CThreadView*			mThreadView;
	MessageKey					mPendingMessageKeyToSelect;	// Hack to support ShowSearchMessage()
}; // class CDeferredSelectKeyTask

//----------------------------------------------------------------------------------------
CDeferredSelectKeyTask::CDeferredSelectKeyTask(
	CThreadView*		inView,
	MessageKey			inKey)
//----------------------------------------------------------------------------------------
:	mThreadView(inView)
,	mPendingMessageKeyToSelect(inKey)
{
}

//----------------------------------------------------------------------------------------
CDeferredTask::ExecuteResult CDeferredSelectKeyTask::ExecuteSelf()
//----------------------------------------------------------------------------------------
{
	mThreadView->SelectMessageWhenReady(mPendingMessageKeyToSelect);
	return eDoneDelete;
} // CDeferredSelectKeyTask::ExecuteSelf

#pragma mark -

//========================================================================================
class CDeferredThreadViewCommand
//========================================================================================
:	public CDeferredCommand
{
	private:
		typedef CDeferredCommand Inherited;
	public:
								CDeferredThreadViewCommand(
									CThreadView*		inView,
									CommandT			inCommand,
									void*				ioParam);
	protected:
		virtual ExecuteResult	ExecuteSelf();
// data
	protected:
		CThreadView*			mThreadView;
}; // class CDeferredThreadViewCommand

//----------------------------------------------------------------------------------------
CDeferredThreadViewCommand::CDeferredThreadViewCommand(
	CThreadView*		inView,
	CommandT			inCommand,
	void*				ioParam)
//----------------------------------------------------------------------------------------
:	CDeferredCommand(inView, inCommand, ioParam)
,	mThreadView(inView)
{
}

//----------------------------------------------------------------------------------------
CDeferredTask::ExecuteResult CDeferredThreadViewCommand::ExecuteSelf()
//----------------------------------------------------------------------------------------
{
	if (mCommand == cmd_GetNewMail)
	{
		if (mThreadView->IsStillLoading())
			return eWaitStayFront; // don't stop idling
//		&& ((mXPFolder.GetFolderFlags() & MSG_FOLDER_FLAG_INBOX) != 0) && 	// if this is an inbox
//		((mXPFolder.GetFolderFlags() & MSG_FOLDER_FLAG_IMAPBOX) == 0)		// and our server is pop
	}
	return Inherited::ExecuteSelf();
} // CDeferredThreadViewCommand::ExecuteSelf

#pragma mark -

//----------------------------------------------------------------------------------------
CThreadView::CThreadView(LStream *inStream)
//----------------------------------------------------------------------------------------
:	Inherited(inStream)
,	mXPFolder(NULL)
,	mSavedSelection(nil)
,	mExpectingNewMail(false)
,	mGotNewMail(false)
,	mRowToSelect(0)
,	mSelectAfterDelete(false) 	// This new "hidden" pref was a demand from a big customer
,	mMotionPendingCommand((MSG_MotionType)-1)
,	mUndoTask(nil)
,	mScrollToShowNew(false)
{
	XP_Bool noNextRowSelection;
	if (PREF_GetBoolPref("mail.no_select_after_delete", &noNextRowSelection) == PREF_NOERROR
		&& noNextRowSelection)
		SetSelectAfterDelete(false);
}

//----------------------------------------------------------------------------------------
CThreadView::~CThreadView()
//----------------------------------------------------------------------------------------
{
	SaveSelectedMessage();
	delete mSavedSelection;
}

//----------------------------------------------------------------------------------------
void CThreadView::ApplyTextStyle(TableIndexT inRow) const
//----------------------------------------------------------------------------------------
{
	CMessage message(inRow, GetMessagePane());
	// Set bold if new
	if (!message.HasBeenRead())
		::TextFace(bold);
	else if (message.IsThread() && !message.IsOpenThread() && message.GetNumNewChildren() > 0)
		::TextFace(underline);
	else
		::TextFace(normal);
	::TextMode(message.IsDeleted() ? grayishTextOr : srcOr);
} // CMessageFolderView::ApplyTextStyle

//----------------------------------------------------------------------------------------
void CThreadView::ApplyTextColor(TableIndexT inRow) const
//----------------------------------------------------------------------------------------
{
	CMessage message(inRow, GetMessagePane());
	// Set up the text color based on priority
	RGBColor priorityColor;
	message.GetPriorityColor(priorityColor);
	::RGBForeColor(&priorityColor);
} // CMessageFolderView::ApplyTextStyle

//----------------------------------------------------------------------------------------
void CThreadView::DrawCellContents(
	const STableCell &inCell,
	const Rect &inLocalRect)
//----------------------------------------------------------------------------------------
{
	ApplyTextColor(inCell.row);
	
	CMessage message(inCell.row, GetMessagePane());
	PaneIDT cellType = GetCellDataType(inCell);
	switch (cellType)
	{
		case kThreadMessageColumn:
		{
			// draw a "thread dragger" /*if we're sorted by thread and*/ this
			// message is a thread header... or if we are watched/ignored
			//if (GetSortedColumn() == kThreadMessageColumn && message.IsThread())
			
				ResIDT iconID = message.GetThreadIconID();
				if( /*GetSortedColumn() == kThreadMessageColumn &&*/ ( message.IsThread()  || iconID >= kWatchedThreadIcon ) )
					DrawIconFamily( iconID, 16, 16, 0, inLocalRect);
				break;
			
		}
		case kMarkedReadMessageColumn:
		{
			if (!message.HasBeenRead())
				DrawIconFamily(kMessageReadIcon, 16, 16,  0, inLocalRect);
			break;
		}				
		case kFlagMessageColumn:
			if (message.IsFlagged())
				DrawIconFamily(kFlagIcon, 16, 16,  0, inLocalRect);
			break;
		case kSubjectMessageColumn:
			SInt16 newLeft = DrawIcons(inCell, inLocalRect);
			Rect subjectRect = inLocalRect;
			subjectRect.left = newLeft;
			DrawMessageSubject(inCell.row, subjectRect);
			break;
		case kSenderMessageColumn:
			{
				const char* raw = message.GetSender();
				char* conv = IntlDecodeMimePartIIStr(raw, mContext->GetWinCSID(), FALSE);

				if(mContext->GetWinCSID() == CS_UTF8)
					DrawUTF8TextString((conv ? conv : raw), &mTextFontInfo, 2, inLocalRect);
				else
					DrawTextString((conv ? conv : raw), &mTextFontInfo, 2, inLocalRect);

				if(conv)
					XP_FREE(conv);
			}
			break;	
		case kDateMessageColumn:
			DrawTextString(message.GetDateString(), &mTextFontInfo, 2, inLocalRect, teFlushLeft, true, truncEnd);
			break;
		case kPriorityMessageColumn:
			DrawMessagePriority(message, inLocalRect);
			break;	
		case kSizeMessageColumn:
			DrawMessageSize(message, inLocalRect);
			break;
		case kStatusMessageColumn:
			DrawMessageStatus(message, inLocalRect);
			break;
		case kAddresseeMessageColumn:
			{
				const char* raw = message.GetAddresseeString();
				char* conv = IntlDecodeMimePartIIStr(raw, mContext->GetWinCSID(), FALSE);
				if(mContext->GetWinCSID() == CS_UTF8)
					DrawUTF8TextString((conv ? conv : raw), &mTextFontInfo, 2, inLocalRect);
				else
					DrawTextString((conv ? conv : raw), &mTextFontInfo, 2, inLocalRect);
				DrawTextString((conv ? conv : raw), &mTextFontInfo, 2, inLocalRect);
				if(conv)
					XP_FREE(conv);
			}		
			break;
		case kTotalMessageColumn:
			if (GetSortedColumn() == kThreadMessageColumn && message.IsThread())
				DrawCountCell(message.GetNumChildren(), inLocalRect);
			break;
		case kUnreadMessageColumn:
			if (GetSortedColumn() == kThreadMessageColumn && message.IsThread())
				DrawCountCell(message.GetNumNewChildren(), inLocalRect);
			break;
		default:
			Assert_(false);
			break;	
	}
	// The selection of a new message after one is removed is handled by a timer, and is keyed
	// off mRowToSelect.  This data member is set when we are notified that a row has been removed.
	// We used to start this timer immediately on initiating the delete operation (in DeleteSelection)
	// but this ended up firing before the refresh occurred, resulting in two rows selected, which
	// was ugly.  So we turn on the idler here, now, to make sure that the row isn't selected until
	// after the refresh is done. 98/01/14
	if (mRowToSelect)
	{
		CDeferredTaskManager::Post1(new CDeferredSelectRowTask(this, mRowToSelect), this);
		mRowToSelect = LArray::index_Bad;
	}
	if (mScrollToShowNew)
	{
		CDeferredTaskManager::Post1(new CScrollToNewMailTask(this), this);
		mScrollToShowNew = false;
	}
} // CThreadView::DrawCellContents

//----------------------------------------------------------------------------------------
void CThreadView::DrawIconsSelf(
	const STableCell& inCell,
	IconTransformType inTransformType,
	const Rect& inIconRect) const
//----------------------------------------------------------------------------------------
{
	const ResIDT kPaperClipBackLayer = 15555;
	const ResIDT kPaperClipFrontLayer = 15556;
	CMessage message(inCell.row, GetMessagePane());
	Boolean hasAttachments = message.HasAttachments();
	if (hasAttachments)
		DrawIconFamily(kPaperClipBackLayer, 16, 16, inTransformType, inIconRect);
	Inherited::DrawIconsSelf(inCell, inTransformType, inIconRect);
	if (hasAttachments)
		DrawIconFamily(kPaperClipFrontLayer, 16, 16, inTransformType, inIconRect);
} // CThreadView::DrawIconsSelf

//----------------------------------------------------------------------------------------
Boolean CThreadView::GetDragCopyStatus(
	DragReference			inDragRef
,	const CMailSelection&	inSelection
,	Boolean&				outCopy)
//----------------------------------------------------------------------------------------
{
	SInt16 modifiers;
	::GetDragModifiers(inDragRef, NULL, &modifiers, NULL);
	outCopy = ((modifiers & optionKey) != 0); // user wants copy if possible.
	MSG_DragEffect effect = outCopy ? MSG_Require_Copy : MSG_Default_Drag; 
	MSG_FolderInfo* destFolder = GetOwningFolder();
	if (!destFolder)
		return false; // eg, when the pane is blank because a server is selected.
	// Ask the back end for guidance on what is possible:
	effect = ::MSG_DragMessagesIntoFolderStatus(
		inSelection.xpPane,
		inSelection.GetSelectionList(),
		inSelection.selectionSize,
		destFolder,
		effect);
	if (effect == MSG_Drag_Not_Allowed && outCopy)
	{
		// Well, maybe a move is ok
		effect = ::MSG_DragMessagesIntoFolderStatus(
			inSelection.xpPane,
			inSelection.GetSelectionList(),
			inSelection.selectionSize,
			destFolder,
			MSG_Default_Drag);
	}
	outCopy = (effect & MSG_Require_Copy) != 0;
	return (effect != MSG_Drag_Not_Allowed); // this looks ok as a drop operation.
} //  CThreadView::GetSelectionAndCopyStatusFromDrag

//----------------------------------------------------------------------------------------
void CThreadView::EnterDropArea(
	DragReference		inDragRef,
	Boolean				inDragHasLeftSender)
// CStandardFlexTable overrides the LDropArea base method, because it assumes a drop-on-row
// scenario
//----------------------------------------------------------------------------------------
{
	LDragAndDrop::EnterDropArea(inDragRef, inDragHasLeftSender);
}									

//----------------------------------------------------------------------------------------
void CThreadView::LeaveDropArea(DragReference inDragRef)
// CStandardFlexTable overrides the LDropArea base method, because it assumes a drop-on-row
// scenario
//----------------------------------------------------------------------------------------
{
	LDragAndDrop::LeaveDropArea(inDragRef);
	mDropRow = 0;
	mIsDropBetweenRows = false;
	mIsInternalDrop = false;
}

//----------------------------------------------------------------------------------------
Boolean CThreadView::ItemIsAcceptable(
	DragReference	inDragRef, 
	ItemReference 	/*inItemRef*/	)
//----------------------------------------------------------------------------------------
{
	Boolean dropOK = false;
	Boolean doCopy = false;

	CMailSelection selection;
	if (GetSelectionFromDrag(inDragRef, selection)
	&& selection.xpPane != GetMessagePane()
	&& GetDragCopyStatus(inDragRef, selection, doCopy))
		dropOK = true;
	if (dropOK && doCopy)
	{
		CursHandle copyDragCursor = ::GetCursor(curs_CopyDrag);
		if (copyDragCursor != nil)
			::SetCursor(*copyDragCursor);
	}
	else
		::SetCursor(&qd.arrow);
	return dropOK;			
} // CThreadView::ItemIsAcceptable

//----------------------------------------------------------------------------------------
void CThreadView::ReceiveDragItem(
	DragReference		inDragRef,
	DragAttributes		/*inDragAttrs*/,
	ItemReference		inItemRef,
	Rect&				/*inItemBounds*/)
//----------------------------------------------------------------------------------------
{	
	Assert_(inItemRef == CMailFlexTable::eMailNewsSelectionDragItemRefNum);

	// What are the items being dragged?
	CMailSelection selection;
	if (!GetSelectionFromDrag(inDragRef, selection))
		return;
		
	// Don't do anything if the user aborted by dragging back into the same view.
	if (selection.xpPane == GetMessagePane())
		return;

	Boolean copyMessages;
	if (!GetDragCopyStatus(inDragRef, selection, copyMessages))
		return;

	// display modal dialog
	short	stringID = (copyMessages ? 19 : 15); // Copying / Moving Messages
	CStr255 commandString;
	::GetIndString(commandString, 7099, stringID);
	CMailProgressWindow* progressWindow = CMailProgressWindow::CreateModalParasite(
		CMailProgressWindow::res_ID_modal,
		selection.xpPane,
		commandString);
	if (progressWindow)
		progressWindow->SetDelay(0);

	// copy / move messages
	if (! copyMessages)
		::MSG_MoveMessagesIntoFolder(
			selection.xpPane,
			selection.GetSelectionList(), 
			selection.selectionSize,
			mXPFolder.GetFolderInfo()
			);
	else
		::MSG_CopyMessagesIntoFolder(
			selection.xpPane, 
			selection.GetSelectionList(), 
			selection.selectionSize,
			mXPFolder.GetFolderInfo()
			);
	// Close any windows associated with these messages, if the preferences
	// say we should.
	CMessageWindow::NoteSelectionFiledOrDeleted(selection);
	mUndoCommand = cmd_Undo;
} // CThreadView::ReceiveDragItem								

//----------------------------------------------------------------------------------------
void CThreadView::DrawMessageSize(
	const CMessage&	inMessage,
	const Rect&		inLocalRect	)
//----------------------------------------------------------------------------------------
{
	DrawTextString(inMessage.GetSizeStr(),
		&mTextFontInfo, 2, inLocalRect, teFlushLeft, true, truncEnd);
} // CThreadView::DrawMessageSize

//----------------------------------------------------------------------------------------
void CThreadView::DrawMessagePriority(
	const CMessage&	inMessage, 
	const Rect&		inLocalRect)
//----------------------------------------------------------------------------------------
{
	DrawTextString(inMessage.GetPriorityStr(),
		&mTextFontInfo, 0, inLocalRect);
} // CThreadView::DrawMessagePriority

//----------------------------------------------------------------------------------------
void CThreadView::DrawMessageStatus(
	const CMessage&	inMessage, 
	const Rect&		inLocalRect)
//----------------------------------------------------------------------------------------
{
	DrawTextString(inMessage.GetStatusStr(),
		&mTextFontInfo, 0, inLocalRect);
} //CThreadView::DrawMessageStatus

//----------------------------------------------------------------------------------------
ResIDT CThreadView::GetIconID(TableIndexT inRow) const
//----------------------------------------------------------------------------------------
{
	CMessage message(inRow, GetMessagePane());
	return message.GetIconID(mXPFolder.GetFolderFlags());
} // CMessageFolderView::GetIconID

//----------------------------------------------------------------------------------------
UInt16 CThreadView::GetNestedLevel(TableIndexT inRow) const
//----------------------------------------------------------------------------------------
{
	CMessage message(inRow, GetMessagePane());
	return message.GetThreadLevel();
} // CThreadView::GetNestedLevel

//----------------------------------------------------------------------------------------
Boolean CThreadView::GetHiliteTextRect(
	const TableIndexT	inRow,
	Rect&				outRect) const
//----------------------------------------------------------------------------------------
{
	STableCell cell(inRow, GetHiliteColumn());
	if (!GetLocalCellRect(cell, outRect))
		return false;
	Rect iconRect;
	GetIconRect(cell, outRect, iconRect);
	outRect.left = iconRect.right;
	char subject[kMaxSubjectLength];
	GetHiliteText(inRow, subject, sizeof(subject), &outRect);
	return true;
} // CThreadView::GetHiliteRect

//----------------------------------------------------------------------------------------
void CThreadView::GetDropFlagRect(
	const Rect&	inLocalCellRect,
	Rect& outFlagRect	) const
//----------------------------------------------------------------------------------------
{
	Inherited::GetDropFlagRect(inLocalCellRect, outFlagRect);
	if (GetSortedColumn() != kThreadMessageColumn)
	{
		// Return a rectangle of zero width.
		outFlagRect.left 	= inLocalCellRect.left;
		outFlagRect.right 	= outFlagRect.left;
	}
} // CThreadView::GetDropFlagRect

//----------------------------------------------------------------------------------------
void CThreadView::GetMainRowText(
	TableIndexT			inRow,
	char*				outText,
	UInt16				inMaxBufferLength) const
// Calculate the text and (if ioRect is not passed in as null) a rectangle that fits it.
// Return result indicates if any of the text is visible in the cell
//----------------------------------------------------------------------------------------
{
	if (!outText || inMaxBufferLength < 2)
		return;
	CMessage message(inRow, GetMessagePane());
	const char* raw = message.GetSubject(outText, inMaxBufferLength - 1);
	char* conv = IntlDecodeMimePartIIStr(raw, mContext->GetWinCSID(), FALSE);
	if (conv)
	{
		XP_STRNCPY_SAFE(outText, conv, inMaxBufferLength);
		XP_FREE(conv);
	}
} // CThreadView::GetMainRowText

//----------------------------------------------------------------------------------------
void CThreadView::DrawMessageSubject(
	TableIndexT		inRow, 
	const Rect&		inLocalRect)
//	Draw the message drop flag, icon and subject
//----------------------------------------------------------------------------------------
{
	// draw a drop flag if we're sorted by thread and this
	// message is a thread header...
	char subject[kMaxSubjectLength];
	Rect textRect = inLocalRect;
	if (GetHiliteText(inRow, subject, sizeof(subject), &textRect))
	{
		if (mContext->GetWinCSID() == CS_UTF8)
		{
			textRect.right = inLocalRect.right; // calculation will have been wrong.
			DrawUTF8TextString(subject, &mTextFontInfo, 0, textRect);
		}
		else
			DrawTextString(subject, &mTextFontInfo, 0, textRect);
//		if (inRow == mDropRow)
//				::InvertRect(&textRect);				
	}
} // CThreadView::DrawMessageSubject

//----------------------------------------------------------------------------------------
Boolean CThreadView::CellSelects(const STableCell& inCell) const
//----------------------------------------------------------------------------------------
{
	PaneIDT	cellType = GetCellDataType(inCell);
	switch (cellType)
	{
		case kThreadMessageColumn:
		{
			CMessage message(inCell.row, GetMessagePane());
			return (message.IsThread());
		}
		case kMarkedReadMessageColumn:
		case kFlagMessageColumn:
			return false;
		default:
			return true;
	}
} // CThreadView::CellSelects

//-----------------------------------
void CThreadView::SetRowCount()
// Queries the back end pane and sets the number of rows.
// Overridden to call MSG_GetThreadLineByIndex, which forces
// authentication if the preference specifies it.  A result of false
// means that the folder is password protected.
//-----------------------------------
{
	MSG_MessageLine ignored;
	MSG_Pane* pane = GetMessagePane();
	if (!pane || !::MSG_GetThreadLineByIndex(pane, 0, 1, &ignored))
	{
		// Access denied!
		TableIndexT rows, cols;
		GetTableSize(rows, cols);	
		if (rows > 0)
			RemoveRows(rows, 1, false);
		return;
	}
	Inherited::SetRowCount();
} // CThreadView::SetRowCount()


//----------------------------------------------------------------------------------------
Boolean CThreadView::CellWantsClick ( const STableCell& inCell ) const
// Overloaded to ensure that certain columns get the mouse click
//----------------------------------------------------------------------------------------
{
	PaneIDT	cellType = GetCellDataType(inCell);
	switch (cellType)
	{
//		case kThreadMessageColumn:
		case kMarkedReadMessageColumn:
		case kFlagMessageColumn:
		case kPriorityMessageColumn:
			return true;
			
		default:
			return false;
	}
	
} // CThreadView::CellWantsClick


//----------------------------------------------------------------------------------------
void CThreadView::ClickCell(
	const STableCell		&inCell,
	const SMouseDownEvent	&inMouseDown)
//	Handles marking message read/unread
//----------------------------------------------------------------------------------------
{
	CMailProgressWindow::RemoveModalParasite();
	PaneIDT	cellType = GetCellDataType(inCell);
	switch (cellType)
	{
		case kThreadMessageColumn:
		{
			if (GetSortedColumn() == kThreadMessageColumn)
			{
				CMessage message(inCell.row, GetMessagePane());
				if (message.IsThread())
				{
					if (! message.IsOpenThread())
						ToggleExpandAction(inCell.row);
					DoSelectThread(inCell.row);
					CMessage::InvalidateCache(); // Because BE has changed the message state
				}
			}
			break;
		}
		// toggle the message's read/unread state
		case kMarkedReadMessageColumn:
		{
			MSG_ViewIndex index = inCell.row - 1;
			MSG_Command(GetMessagePane(), MSG_ToggleMessageRead, &index, 1);
			CMessage::InvalidateCache(); // Because BE has changed the message state
			RefreshCell(inCell);
			break;
		}		
		case kFlagMessageColumn:
		{
			CMessage message(inCell.row, GetMessagePane());
			MSG_CommandType cmd = message.IsFlagged() ? MSG_UnmarkMessages : MSG_MarkMessages;
			MSG_ViewIndex index = inCell.row - 1;
			MSG_Command(GetMessagePane(), cmd, &index, 1);
			CMessage::InvalidateCache(); // Because BE has changed the message state
			RefreshCell(inCell);
			break;
		}		
		case kPriorityMessageColumn:
		{
			if ((mXPFolder.GetFolderFlags() & MSG_FOLDER_FLAG_NEWSGROUP) != 0)
				break; // no priority for news.

			// calculate popup location
			FocusDraw();
			Point where = inMouseDown.wherePort;	
			PortToGlobalPoint(where);
			Rect cellRect;
			if (GetLocalCellRect(inCell, cellRect))
			{
				where = topLeft(cellRect);
				::LocalToGlobal(&where);
			}

			// get popup default value
			CMessage message(inCell.row, GetMessagePane());
			MSG_PRIORITY oldPriority = message.GetPriority();			
			Int16 defaultChoice = CMessage::PriorityToMenuItem(oldPriority);

			// display popup
			Int16 menuItem = 0;
			LMenu * thePopup = new LMenu(kPriorityMenuID);

			{	// copied from HandlePopupMenuSelect()

				MenuHandle menuH = thePopup->GetMacMenuH();

// we decided that all the context menus should look the same and
// this meant drawing them in the system font (at least for 4.0)
// to re-enable drawing in another font just take out my #ifdef's
// and #endif's
#ifdef USE_SOME_OTHER_FONT_FOR_THE_PRIORITY_MENU
				Int16 saveFont = ::LMGetSysFontFam();
				Int16 saveSize = ::LMGetSysFontSize();
				
				StMercutioMDEFTextState theMercutioMDEFTextState;
#endif
				try {
#ifdef USE_SOME_OTHER_FONT_FOR_THE_PRIORITY_MENU

					// Reconfigure the system font so that the menu will be drawn in our desired 
					// font and size
					if ( mTextTraitsID ) {				
						FocusDraw();
						TextTraitsH traitsH = UTextTraits::LoadTextTraits(mTextTraitsID);
						if ( traitsH ) {
							// Bug #64133
							// If setting to application font, get the application font for current script
							if((**traitsH).fontNumber == 1)
								::LMSetSysFontFam ( ::GetScriptVariable(::FontToScript(1), smScriptAppFond) );
							else
								::LMSetSysFontFam ( (**traitsH).fontNumber );
							::LMSetSysFontSize((**traitsH).size);
							::LMSetLastSPExtra(-1L);
						}
					}
#endif
					// Handle the actual insertion into the hierarchical menubar
					::InsertMenu(menuH, hierMenu);
					
					// Call PopupMenuSelect and wait for it to return
					Int32 result = ::PopUpMenuSelect(menuH, where.v, where.h, defaultChoice);
					
					// Extract the values from the returned result.
					menuItem = LoWord(result);
				}
				catch(...) {
					// Ignore errors
				}

// this is the last one of these that you'd want to take out in order
// to re-enable the ability to use some font (other than the system font)
// to draw the priority context menu
#ifdef USE_SOME_OTHER_FONT_FOR_THE_PRIORITY_MENU
				// Restore the system font
				::LMSetSysFontFam(saveFont);
				::LMSetSysFontSize(saveSize);
				::LMSetLastSPExtra(-1L);
#endif
				// Finally get the menu removed
				::DeleteMenu(kPriorityMenuID);
			}

			if (menuItem)
			{
				MSG_PRIORITY newPriority = CMessage::MenuItemToPriority(menuItem);
				if (newPriority != oldPriority)
				{
					// BE bug?  We're not getting told that a change happened.  So
					// do it ourselves.  If this gets fixed later, we can remove these
					// change calls.
					ChangeStarting(GetMessagePane(), MSG_NotifyChanged, inCell.row, 1);
					MSG_SetPriority(GetMessagePane(), message.GetMessageKey(), newPriority);
					ChangeFinished(GetMessagePane(), MSG_NotifyChanged, inCell.row, 1);
					CMessage::InvalidateCache(); // Because BE has changed the message state
				}
			}
			break;
		}
		default:
			break;
	}
} // CThreadView::ClickCell

//----------------------------------------------------------------------------------------
TableIndexT CThreadView::GetHiliteColumn() const
//----------------------------------------------------------------------------------------
{
	return mTableHeader->ColumnFromID(kSubjectMessageColumn);
} // CThreadView::GetHiliteColumn

//----------------------------------------------------------------------------------------
void CThreadView::SetCellExpansion(
	const STableCell& inCell,
	Boolean inExpanded)
//----------------------------------------------------------------------------------------
{
	Boolean currentlyExpanded;
	if (!CellHasDropFlag(inCell, currentlyExpanded) || (inExpanded == currentlyExpanded))
		return;
	ToggleExpandAction(inCell.row);
	CMessage::InvalidateCache(); // Because BE has changed the message state
} // CThreadView::SetCellExpansion

//----------------------------------------------------------------------------------------
Boolean CThreadView::CellInitiatesDrag(const STableCell& inCell) const
//----------------------------------------------------------------------------------------
{
	PaneIDT cellType = GetCellDataType(inCell);
	return (cellType == kSubjectMessageColumn
	 || (cellType == kThreadMessageColumn && GetSortedColumn() == kThreadMessageColumn));
} // CThreadView::CellInitiatesDrag

//----------------------------------------------------------------------------------------
Boolean CThreadView::CellHasDropFlag(
	const STableCell& inCell,
	Boolean& outExpanded) const
//----------------------------------------------------------------------------------------
{
	PaneIDT cellType = GetCellDataType(inCell);

	// Only show drop flags when we're sorted by thread
	if (cellType == kSubjectMessageColumn && GetSortedColumn() == kThreadMessageColumn)
	{
		CMessage message(inCell.row, GetMessagePane());
		if (message.IsThread())
		{
			outExpanded = message.IsOpenThread();
			return true;
		}
	}
	return false;
} // CThreadView::CellHasDropFlag

//----------------------------------------------------------------------------------------
TableIndexT CThreadView::CountExtraRowsControlledByCell(const STableCell& inCell) const
//----------------------------------------------------------------------------------------
{
	PaneIDT cellType = GetCellDataType(inCell);
	// Only show drop flags when we're sorted by thread
	if (cellType != kThreadMessageColumn || GetSortedColumn() != kThreadMessageColumn)
		return 0;
	int32 msgExpansionDelta = MSG_ExpansionDelta(GetMessagePane(), inCell.row - 1);
	if (msgExpansionDelta >= 0)
		return 0;
	return -msgExpansionDelta;
} // CThreadView::CountExtraRowsControlledByCell

//----------------------------------------------------------------------------------------
void CThreadView::OpenRow(TableIndexT inRow)
//	Open the message into its own window.
//----------------------------------------------------------------------------------------
{
	Assert_(mContext);
	
	// Check the application heap.
	if (Memory_MemoryIsLow() || ::MaxBlock() < 100*1024)
		throw memFullErr;
	// Check the Mozilla allocator
	void* temp = XP_ALLOC(20*1024);
	if (!temp)
		throw memFullErr;
	XP_FREE(temp);
	
	::SetCursor(*::GetCursor(watchCursor));
	Boolean multipleSelection = GetSelectedRowCount() > 1;
	CMessage message(inRow, GetMessagePane());
	// if a Draft, template, or Queue message open the compose window
	if (
		(mXPFolder.GetFolderFlags()
			& (MSG_FOLDER_FLAG_DRAFTS | MSG_FOLDER_FLAG_QUEUE | MSG_FOLDER_FLAG_TEMPLATES)) 
		|| message.IsTemplate()
	)
	{
		::MSG_OpenDraft(GetMessagePane() , mXPFolder.GetFolderInfo(), message.GetMessageKey());
		return;
	}
	// First see if there's an open window open to this message.  If there is,
	// bring it to the front.
	CMessageWindow* messageWindow = CMessageWindow::FindAndShow(message.GetMessageKey());
	if (messageWindow)
	{
		messageWindow->GetMessageView()->SetDefaultCSID(mContext->GetDefaultCSID());
		// Found it.  Bring it to the front.
		messageWindow->Select();
		return;
	}
	// First, if recycling a window is indicated, ask for any existing one.
	if (mContext->GetCurrentCommand() != cmd_OpenSelectionNewWindow)
	{
		// With a multiple selection, we never recycle!
		XP_Bool prefReuseWindow = 0; // recycle any message window
		PREF_GetBoolPref("mailnews.reuse_message_window", &prefReuseWindow);
		if (!multipleSelection &&
			(CApplicationEventAttachment::CurrentEventHasModifiers(optionKey) ^ prefReuseWindow))
		{
			messageWindow = CMessageWindow::FindAndShow(0);
		}
	}
	// If we couldn't (or shouldn't) recycle one, make a new one.
	if (!messageWindow)
	{
		try
		{
			messageWindow =
					(CMessageWindow*)URobustCreateWindow::CreateWindow(
						CMessageWindow::res_ID,
						LCommander::GetTopCommander());
			CBrowserContext* theContext = new CBrowserContext(MWContextMailMsg);
			StSharer theShareLock(theContext); // if we throw, theContext will have no users & die
			messageWindow->SetWindowContext(theContext);
		}
		catch(...)
		{
			delete messageWindow;
			messageWindow = NULL;
			throw;
		}
	}
	// Whether it's a new one or an old one, load the message now.
	if (messageWindow)
	{
		try
		{
			messageWindow->GetMessageView()->ShowMessage(
				CMailNewsContext::GetMailMaster(),
				mXPFolder.GetFolderInfo(),
				message.GetMessageKey()); // pass flags to tell what sort of message it is
			messageWindow->GetMessageView()->SetDefaultCSID(mContext->GetDefaultCSID());
			messageWindow->Select();
//			messageWindow->Show();
			// The window will now show itself when a successful load occurs
		}
		catch(...)
		{
			delete messageWindow;
			messageWindow = NULL;
			throw;			
		}
	}
	CMessage::InvalidateCache(); // So that the "read" flag gets changed.
} // CThreadView::OpenRow

#define THREE_PANE 1

//----------------------------------------------------------------------------------------
void CThreadView::SaveSelectedMessage()
//----------------------------------------------------------------------------------------
{
	MSG_Pane* curPane = GetMessagePane();
	if (curPane)
	{
		MSG_FolderInfo* curFolder = ::MSG_GetThreadFolder(curPane);
		if (curFolder)
		{
			if (GetSelectedRowCount() == 1)
			{
				TableIndexT curRow = mTableSelector->GetFirstSelectedRow();
				CMessage message(curRow, curPane);
				MessageKey curMessage = message.GetMessageKey();
				::MSG_SetLastMessageLoaded(curFolder, curMessage);
			}
			else
				::MSG_SetLastMessageLoaded(curFolder, MSG_MESSAGEKEYNONE);
		}
	}
} // CThreadView::SaveSelectedMessage

//----------------------------------------------------------------------------------------
Boolean CThreadView::RestoreSelectedMessage()
// Called on FolderLoaded pane notification.  We probably should check the preference.
//----------------------------------------------------------------------------------------
{
	XP_Bool remember = true;
	if (PREF_NOERROR != PREF_GetBoolPref("mailnews.remember_selected_message", &remember))
		return false;
	if (!remember)
		return false;
	MSG_Pane* curPane = GetMessagePane();
	if (curPane)
	{
		MSG_FolderInfo* curFolder = ::MSG_GetThreadFolder(curPane);
		if (curFolder)
		{
			MessageKey savedMessage = ::MSG_GetLastMessageLoaded(curFolder);
			if (savedMessage != MSG_MESSAGEKEYNONE)
			{
				MSG_ViewIndex index = ::MSG_GetMessageIndexForKey(curPane, savedMessage, true);
				if (index != MSG_VIEWINDEXNONE)
				{
					mRowToSelect = index + 1;
					return true;
				}
			}
		}
	}
	return false;
} // CThreadView::RestoreSelectedMessage

//----------------------------------------------------------------------------------------
void CThreadView::LoadMessageFolder(
	CNSContext*	inContext,		// My window's context
	const CMessageFolder& inFolder,
	Boolean	inLoadNow)
//	Load a given message folder into the message table view.
//----------------------------------------------------------------------------------------
{
	Assert_(inContext);
	// Load the folder into the message list.  We'll get a PaneChanged message
	// when loading is complete, and another one to say that the folder info
	// has changed.  When these callbacks occur, we'll call SetFolder().  However,
	// the first time the window is initialized, the title needs to look nice
	// even during the load process.  Hence this call, which will be made twice:
	SetFolder(inFolder);
	if (!inLoadNow)
	{
		CDeferredLoadFolderTask* task
			= new CDeferredLoadFolderTask(this, inContext, inFolder);
		CDeferredTaskManager::Post1(task, this);
		return;
	}
	try
	{
		::SetCursor(*::GetCursor(watchCursor));
		SaveSelectedMessage();
		UnselectAllCells();
			// Else it crashes, because the messageview (if displaying a message), doesn't
			// get cleaned up properly. #81060
		mContext = inContext;
#if !ONECONTEXTPERWINDOW
		SetMessagePane(nil); // release the memory first.
#endif
		// A null inFolder is possible, and the result should be to clear the thread area.
		// This behavior is new in 5.0 with the 3-pane window - 97/10/06
		if (!inFolder.GetFolderInfo())
		{
			RemoveAllRows( true );
			SetMessagePane(nil);
			return;
		}
#if ONECONTEXTPERWINDOW
		if (!GetMessagePane())
#endif
		{
			SetMessagePane(::MSG_CreateThreadPane(*inContext, CMailNewsContext::GetMailMaster()));
			ThrowIfNULL_(GetMessagePane());	
			// plug into the view so we get notified when the data changes
			::MSG_SetFEData(GetMessagePane(), CMailCallbackManager::Get());
		}
		if (inFolder.GetFolderInfo() == MSG_GetThreadFolder(GetMessagePane()))
			return;

#if USING_MODAL_FOLDER_LOAD
		CStr255 commandString;
		::GetIndString(commandString, 7099, 16); // "Loading Folder"
		CMailProgressWindow::CreateModalParasite(
			CMailProgressWindow::res_ID_modal,
			GetMessagePane(),
			commandString);
#endif
		// Close any other windows showing this folder
		CWindowIterator iter(WindowType_MailThread);
		CThreadWindow* thisWindow
			= dynamic_cast<CThreadWindow*>(LWindow::FetchWindowObject(GetMacPort()));

		CThreadWindow* window;
		for (iter.Next(window); window; iter.Next(window))
		{
			window = dynamic_cast<CThreadWindow*>(window);
			if (window && window != thisWindow)
			{
				CMessageView* messageView = window->GetMessageView();
				if (inFolder.GetFolderInfo() == messageView->GetFolderInfo())
					window->AttemptClose();
			}
		}
#if 1
		// IMAP mailboxes do "get new mail" automatically.  So set up the flags to expect new
		// mail.
		if ((inFolder.GetFolderFlags() & MSG_FOLDER_FLAG_IMAPBOX) == MSG_FOLDER_FLAG_IMAPBOX)
			ExpectNewMail();
#else
		// IMAP inbox does "get new mail" automatically.  So set up the flags to expect new
		// mail.
		#define IMAP_INBOX (MSG_FOLDER_FLAG_INBOX | MSG_FOLDER_FLAG_IMAPBOX)
		if ((inFolder.GetFolderFlags() & IMAP_INBOX) == IMAP_INBOX)
			ExpectNewMail();
#endif
		// Now finally, load the folder
		ThrowIfError_(::MSG_LoadFolder(GetMessagePane(), inFolder.GetFolderInfo()));

		#ifndef THREE_PANE
		SwitchTarget(this);
		#endif	
		
		// Change the i18n encoding and font to match the new folder:
		int foldercsid = ::MSG_GetFolderCSID(inFolder.GetFolderInfo());
		if ((CS_UNKNOWN == foldercsid) || (CS_DEFAULT == foldercsid))
		{
			foldercsid = INTL_DefaultDocCharSetID(0);
			::MSG_SetFolderCSID(inFolder.GetFolderInfo(), foldercsid);
		}
		SetDefaultCSID(foldercsid);
		ResetTextTraits();	// May need to change the font when we change folder
	}
	catch(...)
	{
	}
} // CThreadView::LoadMessageFolder

//----------------------------------------------------------------------------------------
Boolean CThreadView::RelocateViewToFolder(const CMessageFolder& inFolder)
// Retarget the view to the specified BE folder.
//----------------------------------------------------------------------------------------
{
	if (!inFolder.GetFolderInfo() || inFolder == mXPFolder)
		return false;

	if (CThreadWindow::FindAndShow(inFolder.GetFolderInfo()) == nil)
	{
		MSG_Master* master = CMailNewsContext::GetMailMaster();
		LoadMessageFolder(mContext, inFolder);
		return true;
	}
	return false;
} // CThreadWindow::RelocateViewToFolder


//----------------------------------------------------------------------------------------
void CThreadView::FileMessagesToSelectedPopupFolder(const char *inFolderName, 
													Boolean inMoveMessages)
// File selected messages to the specified BE folder. If inMoveMessages is true, move
// the messages, otherwise copy them.
//----------------------------------------------------------------------------------------
{
	if (!inFolderName || !*inFolderName)
		return;
	CMailSelection selection;
	if ( !GetSelection(selection) ) {
		Assert_(false);	// Method should not be called in this case!
		return;	// Nothing selected!
	}
	
#ifdef Debug_Signal
	const char *curName = ::MSG_GetFolderNameFromID(GetOwningFolder());
	Assert_(strcasecomp(curName, inFolderName) != 0);	// Specified folder should not be the same as this folder
#endif // Debug_Signal

	CStr255 commandString;
	::GetIndString(commandString, 7099, (inMoveMessages ? 15 : 19)); // "Moving/Copying Messages"
	CMailProgressWindow* progressWindow = CMailProgressWindow::CreateModalParasite(
		CMailProgressWindow::res_ID_modal,
		GetMessagePane(),
		commandString);
	if (progressWindow)
		progressWindow->SetDelay(0);
	if ( inMoveMessages )
		::MSG_MoveMessagesInto(selection.xpPane, selection.GetSelectionList(), selection.selectionSize, 
						   	   inFolderName);
	else
		::MSG_CopyMessagesInto(selection.xpPane, selection.GetSelectionList(), selection.selectionSize, 
						   	   inFolderName);
	mUndoCommand = cmd_Undo;
} // CThreadWindow::FileMessagesToSelectedPopupFolder

//----------------------------------------------------------------------------------------
Boolean CThreadView::ScrollToGoodPlace()
//----------------------------------------------------------------------------------------
{
	if (mRows == 0)
		return false;
	MessageKey id;
	MSG_ViewIndex index;
	MSG_ViewIndex threadIndex;
	MSG_ViewNavigate(GetMessagePane(), MSG_FirstNew, 0, &id, &index, &threadIndex, NULL);
	if (index != MSG_VIEWINDEXNONE)
	{
		ScrollRowIntoFrame(mRows); // show bottom cell...
		ScrollRowIntoFrame(index + 1); // ... then show first unread.
		return true;
	}
	return false;
} // CThreadView::ScrollToGoodPlace

//----------------------------------------------------------------------------------------
void CThreadView::NoteSortByThreadColumn(Boolean isThreaded) const
//----------------------------------------------------------------------------------------
{
	if (isThreaded)
		GetTableHeader()->ChangeIconOfColumn(kThreadMessageColumn, kThreadedIcon);
	else
		GetTableHeader()->ChangeIconOfColumn(kThreadMessageColumn, kUnthreadedIcon);
}

//----------------------------------------------------------------------------------------
void CThreadView::ChangeSort(const LTableHeader::SortChange* inSortChange)
//----------------------------------------------------------------------------------------
{
	CStandardFlexTable::ChangeSort(inSortChange);
	if (GetMessagePane() != NULL)
	{
		MSG_CommandType	sortCommand;
		switch (inSortChange->sortColumnID)
		{
			case kDateMessageColumn:		sortCommand = MSG_SortByDate;		break;
	  		case kSubjectMessageColumn:		sortCommand = MSG_SortBySubject;	break;
			case kThreadMessageColumn:		sortCommand = MSG_SortByThread;		break;
			case kSenderMessageColumn:
			case kAddresseeMessageColumn:	sortCommand = MSG_SortBySender;		break;

			case kMarkedReadMessageColumn:	sortCommand = MSG_SortByUnread;		break;
			case kPriorityMessageColumn:	sortCommand = MSG_SortByPriority;	break;
			case kSizeMessageColumn:		sortCommand = MSG_SortBySize;		break;
			case kStatusMessageColumn:		sortCommand = MSG_SortByStatus;		break;
			case kFlagMessageColumn:		sortCommand = MSG_SortByFlagged;	break;
			case kHiddenOrderReceivedColumn: sortCommand = MSG_SortByMessageNumber;	break;
			//case kUnreadMessageColumn:		sortCommand = 
			//case kTotalMessageColumn:		sortCommand = 
		
			default: sortCommand = (MSG_CommandType) 0;	
		}
		if (sortCommand != 0)
		{	
			CMailProgressWindow::RemoveModalParasite();
			::MSG_Command(GetMessagePane(), sortCommand, NULL, 0);		
			// Make sure the menus and headers reflect the new state of affairs
			SyncSortToBackend();
		}
	}
} // CThreadView::ChangeSort

//----------------------------------------------------------------------------------------
void CThreadView::DeleteSelection()
// Delete all selected messages
//----------------------------------------------------------------------------------------
{
	try
	{
		CMailSelection selection;
		if (GetSelection(selection))
		{		
#if 0
			// Don't do this any more! Interrupting IMAP will cause a crash if the timing
			// is just right.   98/03/31
			CThreadWindow* threadWindow
				= dynamic_cast<CThreadWindow*>(LWindow::FetchWindowObject(GetMacPort()));
			XP_ASSERT(threadWindow);
			CMessageView* messageView
				= dynamic_cast<CMessageView *>(threadWindow->
					FindPaneByID(CMessageView::class_ID));
			XP_ASSERT(messageView);
			messageView->ObeyCommand(cmd_Stop, NULL);
#endif
			// Try to close any open windows on these messages
			const MSG_ViewIndex* index = selection.GetSelectionList();
			for (int i = 0; i < selection.selectionSize; i++,index++)
			{
				CMessage message((*index + 1), GetMessagePane());
					// (Add one to convert to TableIndexT)
				CMessageWindow::CloseAll(message.GetMessageKey());
			}
			MSG_CommandType cmd =
				((mXPFolder.GetFolderFlags() & MSG_FOLDER_FLAG_NEWSGROUP) == 0)
				? UMessageLibrary::GetMSGCommand(cmd_Clear)
				: MSG_CancelMessage; // cancel my posting, if possible.
			CMailProgressWindow::RemoveModalParasite();
			MSG_Command(
				GetMessagePane(),
				cmd,
				(MSG_ViewIndex*)selection.GetSelectionList(),
				selection.selectionSize);
			UnselectAllCells();
		}
	// The selection of a new message after one is removed is handled by a deferred task.
	// We used to start this timer immediately on initiating the delete operation (here)
	// but this ended up firing before the refresh occurred, resulting in two rows
	// selected, which was ugly.  So we moved the posting call to DrawCellContents,
	// to make sure that the row isn't selected until after the refresh is done. 98/01/14
	}
	catch(...)	{}
} // CThreadView::DeleteSelection()	
				
//----------------------------------------------------------------------------------------
void CThreadView::ChangeStarting(
	MSG_Pane*		inPane,
	MSG_NOTIFY_CODE	inChangeCode,
	TableIndexT		inStartRow,
	SInt32			inRowCount)
//----------------------------------------------------------------------------------------
{
	if (mMysticPlane <= kMysticUpdateThreshHold
		&& (inChangeCode == MSG_NotifyScramble || inChangeCode == MSG_NotifyAll))
	{
		try
		{
			// Get the selection, convert it to non-volatile MSG_FolderInfo...
			CMailSelection selection;
			if (GetSelection(selection))
			{
				delete mSavedSelection;
				mSavedSelection = new CPersistentMessageSelection(selection);
			}
		}
		catch (...) {}
	}
	Inherited::ChangeStarting(inPane, inChangeCode, inStartRow, inRowCount);
} // CThreadView::ChangeStarting

//----------------------------------------------------------------------------------------
void CThreadView::ChangeFinished(
	MSG_Pane*		inPane,
	MSG_NOTIFY_CODE	inChangeCode,
	TableIndexT		inStartRow,
	SInt32			inRowCount)
//----------------------------------------------------------------------------------------
{
	Inherited::ChangeFinished(inPane, inChangeCode, inStartRow, inRowCount);
	if (mMysticPlane <= kMysticUpdateThreshHold)
	{
		switch (inChangeCode)
		{
			case MSG_NotifyScramble:
			case MSG_NotifyAll:
			{
				SetNotifyOnSelectionChange(false);
				try
				{
					UnselectAllCells();
					if (mSavedSelection)
					{			
						const MSG_ViewIndex* index = mSavedSelection->GetSelectionList();
						MSG_ViewIndex count = mSavedSelection->selectionSize;
						for (MSG_ViewIndex i = 0; i < count; i++, index++)
						{
							if (*index != MSG_VIEWINDEXNONE)
								SelectRow(1 + *index);
						}
					}
				}
				catch (...) {}
				SetNotifyOnSelectionChange(true);
				SelectionChanged();
				delete mSavedSelection;
				mSavedSelection = NULL;
			}
			break;
			case MSG_NotifyInsertOrDelete:
				if (CMailProgressWindow::GettingMail()) // could happen from the folder pane etc.
					mExpectingNewMail = true;
				if (mExpectingNewMail || inRowCount > 0)
					mGotNewMail = true;
				// If we're deleting the only selected row, we may need to select
				// something later.  Don't do this as a result of "get new mail", though.
				// The intention here is that this behavior is only to happen as a result
				// of an explicit user delete, on the current machine (Note that you may
				// log on to another machine and delete the message that is currently
				// selected.   Note also that, during folder load, filters can cause a
				// "row-deleted" message here.)
				if (inRowCount == -1			// one row was deleted
				&& mSelectAfterDelete			// the preference to do this is on
				&& GetSelectedRowCount() == 0	// there is nothing selected now
				&& inStartRow <= mRows + 1		// range check (+1 if deleting last row)
				&& !mExpectingNewMail)			// this is not a folder load, or get mail
					mRowToSelect = inStartRow;
			break;
		}
		CMessage::InvalidateCache(); // most changes will make the index invalid.
	}
} // CThreadView::ChangeFinished

//----------------------------------------------------------------------------------------
void CThreadView::SetFolder(const CMessageFolder& inFolder)
// Set the window title to the folder name.
//----------------------------------------------------------------------------------------
{
	mXPFolder = inFolder;	

	Boolean isNewsgroup = ((mXPFolder.GetFolderFlags() & MSG_FOLDER_FLAG_NEWSGROUP) != 0);

	char newName[255];
	const char* nameToUse = (inFolder.GetPrettyName() ?
		inFolder.GetPrettyName() : inFolder.GetName());
	if (nameToUse)
	{
		strcpy(newName, nameToUse);
		NET_UnEscape(newName);
	}
	else
		*newName = '\0';

	CThreadWindow* threadWindow
		= dynamic_cast<CThreadWindow*>(LWindow::FetchWindowObject(GetMacPort()));
	if (threadWindow)
		threadWindow->SetFolderName(newName, isNewsgroup);
} // CThreadView::SetFolderName

// -------------------------
void CThreadView::SyncSortToBackend()
//---------------------------
{
	// This is ugly it would be better if we did this in FindCommandStatus.
	// since the BE could provide us the needed info. The difficutly would be in keeping
	// the headers in sync. If done properly would help simplify FindCommandStatus
	// and eliminate all this extra stuff 
	// Something to think about for 5.0
	MSG_Pane* inPane = GetMessagePane();
	XP_Bool selectable;
	MSG_COMMAND_CHECK_STATE checkedState;
	const char* display_string = nil;
	MSG_CommandType cmd;
	XP_Bool plural;
	for (cmd = MSG_SortByDate;
		cmd <= MSG_SortByUnread;
		cmd = MSG_CommandType( int(cmd) + 1))
	{
		::MSG_CommandStatus(
			inPane, cmd, nil, 0, &selectable, &checkedState, &display_string, &plural );
		if (checkedState == MSG_Checked)
			break;
	}
	PaneIDT pane = 0;
	switch ( cmd )
	{
		case MSG_SortByDate: 			pane = kDateMessageColumn;		break;
  		case MSG_SortBySubject:			pane = kSubjectMessageColumn;	break;
		case MSG_SortBySender:			pane = kSenderMessageColumn;	break;
		case MSG_SortByUnread:			pane = kMarkedReadMessageColumn;break;
		case MSG_SortByPriority:		pane = kPriorityMessageColumn;	break;
		case MSG_SortBySize:			pane = kSizeMessageColumn;		break;
		case MSG_SortByStatus:			pane = kStatusMessageColumn;	break;
		case MSG_SortByFlagged:			pane = kFlagMessageColumn;		break;
		case MSG_SortByMessageNumber:	pane = kHiddenOrderReceivedColumn; break;
		default:
		case MSG_SortByThread:			pane = kThreadMessageColumn;	break;
	}
	NoteSortByThreadColumn(cmd == MSG_SortByThread);
	UInt16 col = mTableHeader->ColumnFromID( pane );
	::MSG_CommandStatus(
			inPane, MSG_SortBackward, nil, 0, &selectable, &checkedState,
			&display_string, &plural);
	Boolean backwards = checkedState == MSG_Checked;
	mTableHeader->SetWithoutSort( col, backwards, true );
	// Make sure the sort submenus on the menu bar are showing the correct state.
	UpdateSortMenuCommands();
} // CThreadView::SyncSortToBackend()

//----------------------------------------------------------------------------------------
void CThreadView::PaneChanged(
	MSG_Pane*		inPane,
	MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
	int32 value)
//----------------------------------------------------------------------------------------
{
	switch (inNotifyCode)
	{
		case MSG_PaneNotifyFolderLoaded:			
			if (value > 0)
			{
				LWindow* win = LWindow::FetchWindowObject(GetMacPort());
				if (win)
					win->Show();
				SetFolder(CMessageFolder((MSG_FolderInfo*)value));
				CMessage::InvalidateCache();
				SetRowCount();
				Refresh();
			}
			// This is the first opportunity to do stuff that needs the folder etc.
			// Update history entry and load an empty message into the message view.
			// Calling SelectionChanged will do these things.
			
			SelectionChanged();
			
			EnableStopButton(false);
			if (mMotionPendingCommand != (MSG_MotionType)-1)
			{
				ObeyMotionCommand( mMotionPendingCommand );
				mMotionPendingCommand = (MSG_MotionType)-1;
			}
#ifdef PREFERENCE_IMPLEMENTED
			if (!RestoreSelectedMessage())
				ScrollToGoodPlace();
#else
			RestoreSelectedMessage(); // select the last selected message AND
			mScrollToShowNew = true;
			//ScrollToGoodPlace(); // scroll to show anything new.
#endif
			DontExpectNewMail();
			// break; NO!  Fall through, because we need to update the rowcount and sort order.
		case MSG_PaneNotifyFolderInfoChanged:
			if (mXPFolder == (MSG_FolderInfo*)value)
			{
				SyncSortToBackend();
				CMessage::InvalidateCache();
			}
			mXPFolder.FolderInfoChanged();
			break;
		case MSG_PaneProgressDone: // comes from progress window
			if ((MessageT)value == msg_NSCAllConnectionsComplete)
			{
				if (GotNewMail()) // ie in THIS folder in THIS operation
				{
					CMessage::InvalidateCache();
					SetRowCount();
					Refresh();
					ScrollToGoodPlace();
				}
				if (CMailProgressWindow::GettingMail())
					DontExpectNewMail();
			}
			break;
		case MSG_PaneNotifyFolderDeleted:
			if ((MSG_FolderInfo*)value == mXPFolder.GetFolderInfo())
			{
				RemoveAllRows( false );
				::MSG_LoadFolder(GetMessagePane(), nil);
				mXPFolder.SetFolderInfo(nil); // prevent crashes
			}
			break;
		default:
			Inherited::PaneChanged(inPane, inNotifyCode, value);
	}
} // CThreadView::PaneChanged

//----------------------------------------------------------------------------------------
void CThreadView::ResizeFrameBy(
	Int16		inWidthDelta,
	Int16		inHeightDelta,
	Boolean		inRefresh)
//----------------------------------------------------------------------------------------
{
	Inherited::ResizeFrameBy(inWidthDelta, inHeightDelta, inRefresh);
	ScrollSelectionIntoFrame();
} // CThreadView::ResizeFrameBy

//----------------------------------------------------------------------------------------
void CThreadView::FindSortCommandStatus(CommandT inCommand, Int16& outMark)
//----------------------------------------------------------------------------------------
{
	PaneIDT paneID = GetSortedColumn();	
	outMark = 0;
	switch (inCommand)
	{
	case cmd_SortByDate:
		if (paneID == kDateMessageColumn)
			outMark = checkMark;
		break;		
	case cmd_SortBySubject:
		if (paneID == kSubjectMessageColumn)
			outMark = checkMark;
		break;		
	case cmd_SortBySender:
		if (paneID == kSenderMessageColumn || paneID == kAddresseeMessageColumn)
			outMark = checkMark;
		break;		
	case cmd_SortByThread:
		if (paneID == kThreadMessageColumn)
			outMark = checkMark;
		break;		
	case cmd_SortByPriority:
		if (paneID == kPriorityMessageColumn)
			outMark = checkMark;
		break;		
	case cmd_SortBySize:
		if (paneID == kSizeMessageColumn)
			outMark = checkMark;
		break;		
	case cmd_SortByReadness:
		if (paneID == kMarkedReadMessageColumn)
			outMark = checkMark;
		break;
	case cmd_SortByStatus:
		if (paneID == kStatusMessageColumn)
			outMark = checkMark;
		break;
	case cmd_SortByFlagged:
		if (paneID == kFlagMessageColumn)
			outMark = checkMark;
		break;
	case cmd_SortByOrderReceived:
		if (paneID == kHiddenOrderReceivedColumn)
			outMark = checkMark;
		break;
	case cmd_SortAscending:
	case cmd_SortDescending:
		outMark =
			((inCommand == cmd_SortDescending) == IsSortedBackwards()) ?
				checkMark : 0;
		break;		
	}
} // CThreadView::FindSortCommandStatus

// -------------------------
void CThreadView::UpdateSortMenuCommands() const
//---------------------------
{
	list<CommandT> commandsToUpdate;
	commandsToUpdate.push_front(cmd_SortByDate);
	commandsToUpdate.push_front(cmd_SortBySubject);
	commandsToUpdate.push_front(cmd_SortBySender);
	commandsToUpdate.push_front(cmd_SortByThread);
	commandsToUpdate.push_front(cmd_SortByPriority);
	commandsToUpdate.push_front(cmd_SortBySize);
	commandsToUpdate.push_front(cmd_SortByReadness);
	commandsToUpdate.push_front(cmd_SortByStatus);
	commandsToUpdate.push_front(cmd_SortByFlagged);
	commandsToUpdate.push_front(cmd_SortByOrderReceived);
	commandsToUpdate.push_front(cmd_SortAscending);
	commandsToUpdate.push_front(cmd_SortDescending);
	CTargetedUpdateMenuRegistry::SetCommands(commandsToUpdate);
	CTargetedUpdateMenuRegistry::UpdateMenus();
} // CThreadView::UpdateSortMenuCommands

//----------------------------------------------------------------------------------------
Boolean CThreadView::ObeySortCommand(CommandT inCommand)
//----------------------------------------------------------------------------------------
{
	PaneIDT oldColumnID = GetSortedColumn();	
	Boolean oldReverse = IsSortedBackwards();
	PaneIDT newColumnID;
	Boolean newReverse = oldReverse;
	switch (inCommand)
	{
	case cmd_SortByDate:
		newColumnID = kDateMessageColumn;
		break;
	case cmd_SortBySubject:
		newColumnID = kSubjectMessageColumn;
		break;
	case cmd_SortBySender:
		newColumnID = kSenderMessageColumn;
		break;
	case cmd_SortByThread:
		newColumnID = kThreadMessageColumn;
		break;
	case cmd_SortByPriority:
		newColumnID = kPriorityMessageColumn;
		break;
	case cmd_SortBySize:
		newColumnID = kSizeMessageColumn;
		break;
	case cmd_SortByReadness:
		newColumnID = kMarkedReadMessageColumn;
		break;
	case cmd_SortByStatus:
		newColumnID = kStatusMessageColumn;
		break;
	case cmd_SortByFlagged:
		newColumnID = kFlagMessageColumn;
		break;
	case cmd_SortByOrderReceived:
		newColumnID = kHiddenOrderReceivedColumn;
		break;
	case cmd_SortAscending:
	case cmd_SortDescending:
		newColumnID = GetSortedColumn();
		newReverse = (cmd_SortDescending == inCommand);
		break;
	default:
		return false;
	}
	if (oldColumnID != newColumnID || oldReverse != newReverse)
	{
		mTableHeader->SetSortedColumn(mTableHeader->ColumnFromID(newColumnID), newReverse, true);
	}
	return true;
} // CThreadView::ObeySortCommand

//----------------------------------------------------------------------------------------
void CThreadView::FindCommandStatus(
	CommandT			inCommand,
	Boolean				&outEnabled,
	Boolean				&outUsesMark,
	Char16				&outMark,
	Str255				outName)
//----------------------------------------------------------------------------------------
{
	outEnabled =false; // This is probably unnecessary, I think PP does it before calling.
	if (::StillDown())
	{
		// Assume this is a call from the context menu attachment.
		Point whereLocal;
		::GetMouse(&whereLocal);
		SPoint32 imagePoint;
		LocalToImagePoint(whereLocal, imagePoint);
		STableCell	hitCell;
		if (GetCellHitBy(imagePoint, hitCell))
		{
			PaneIDT	cellType = GetCellDataType(hitCell);
			if (cellType == kPriorityMessageColumn)
				return; // disable all commands, because we do it ourselves.
			// Fix me: there's no reason why the priority popup can't be done
			// using the context menu mechanism now.
		}
	}
	if (inCommand == cmd_Stop && mStillLoading)
	{
		outEnabled = true; // stop button on, nothing else.
		return;
		// ... otherwise, fall through and pass it up to the window
	}
	// Note: for cmd_Stop, the window may also be able to handle the command, if
	// a message pane exists.
	CMailSelection selection;
	GetSelection(selection);
	switch (inCommand)
	{
		// Single-selection items
		case cmd_SelectThread:
			outEnabled = selection.selectionSize == 1;
			return;
		// Items always available
		case cmd_SubscribeNewsgroups:
			outEnabled = true;
			return;
		case cmd_AddToBookmarks:
			outEnabled = (mXPFolder.GetFolderFlags() & MSG_FOLDER_FLAG_IMAPBOX) == 0;
			return;
		case cmd_SelectMarkedMessages:
		case cmd_MarkReadByDate:
		case cmd_NewFolder:
			outEnabled = true;
			outUsesMark = false;
			return;
		case cmd_GetMoreMessages:
			CStr255 cmdStr;
			CStr255 numStr;
			::GetIndString(cmdStr, kMailNewsMenuStrings, kNextChunkMessagesStrID);
			::NumToString(CPrefs::GetLong(CPrefs::NewsArticlesMax), numStr);
			cmdStr += numStr;
			memcpy(outName, (StringPtr)cmdStr, cmdStr.Length() + 1);
			break;
	}
	// Now check for message library commands. Remember (###), some of these are
	// handled by CMessageView(), and that might be our super commander, so
	// return here only if msglib enables the item for the thread pane.
	// Also don't want the message pane to only deal with cases where 1 message is selected
	if (FindMessageLibraryCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName)
		&& ( outEnabled || selection.selectionSize != 1) )
		return;
	MSG_MotionType cmd = UMessageLibrary::GetMotionType(inCommand);
	if (UMessageLibrary::IsValidMotion(cmd))
	{
		XP_Bool selectable;
		outEnabled = (
			GetMessagePane()
			&& MSG_NavigateStatus(
				GetMessagePane(),
				cmd,
				selection.GetSelectionList() ? *selection.GetSelectionList() : MSG_VIEWINDEXNONE,
				&selectable,
				NULL) == 0
			&& selectable
			);
		outUsesMark = false;
		if (outEnabled) // ONLY in this case. See the previous comment (###).
			return;
	}
	if ( (inCommand == cmd_MoveMailMessages) || (inCommand == cmd_CopyMailMessages) ||
		 CMailFolderSubmenu::IsMailFolderCommand(&inCommand) ) {
		// Mail folder commands
		outEnabled = (GetSelectedRowCount() > 0);
		return;
	}
	// Default for if and switch: fall through into second switch
	switch (inCommand)
	{
		case cmd_GetInfo:
			outEnabled = false; // FIXME: implement it.
			break;
		case cmd_SortByDate:
		case cmd_SortBySubject:
		case cmd_SortBySender:
		case cmd_SortByThread:
		case cmd_SortByPriority:
		case cmd_SortBySize:
//		case cmd_SortByTotal:
		case cmd_SortByReadness:
		case cmd_SortByStatus:
		case cmd_SortByFlagged:
		case cmd_SortByOrderReceived:
		case cmd_SortAscending:
		case cmd_SortDescending:
			if (!mStillLoading)
			{
				outUsesMark = true;
				outEnabled = true;
				FindSortCommandStatus(inCommand, outMark);
			}
			break;
		default:
			if (inCommand >= ENCODING_BASE && inCommand < ENCODING_CEILING)
			{
				if (mContext)
				{
					outUsesMark = true;
					outEnabled = true;

					int16 csid = CPrefs::CmdNumToDocCsid( inCommand );
					outMark = (csid == mContext->GetDefaultCSID()) ? 0x12 : ' ';
				}
			}
			else
			{
				// if (inCommand == cmd_OpenSelection)
				//	outEnabled = false;
				//  else
					Inherited::FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
			}
	}
} // CThreadView::FindCommandStatus

//----------------------------------------------------------------------------------------
void CThreadView::SelectAfterDelete(TableIndexT row)
// Called from ObeyCommand after command completion.
// In "expanded mode", the user probably wants a new message to
// be loaded into the message pane after a deletion.
// So detect the case when a message was showing and the message has
// been moved from the folder.  We want to display a new message.
//----------------------------------------------------------------------------------------
{
	SetUpdateCommandStatus(true);	// Always do this.
	if (row > 0)
		SelectRow(row > mRows ? mRows : row);
} // CThreadView::SelectAfterDelete

//----------------------------------------------------------------------------------------
Boolean CThreadView::ObeyMotionCommand(MSG_MotionType cmd)
//----------------------------------------------------------------------------------------
{
	if (!GetMessagePane()) return false;
	Assert_(UMessageLibrary::IsValidMotion(cmd));
	try
	{
		CMailSelection selection;
		GetSelection(selection);
		MSG_ViewIndex resultIndex;
		MessageKey resultKey = MSG_MESSAGEKEYNONE;
		MSG_FolderInfo* finfo;
		if (MSG_ViewNavigate(
				GetMessagePane(),
				cmd,
				selection.GetSelectionList() ? *selection.GetSelectionList() : MSG_VIEWINDEXNONE,
				&resultKey,
				&resultIndex,
				NULL,
				&finfo) == 0)
		{
			if (resultKey != MSG_MESSAGEKEYNONE)
			{
				SetNotifyOnSelectionChange(false);
				UnselectAllCells();
				SetNotifyOnSelectionChange(true);
				if (finfo && finfo != GetOwningFolder())
					RelocateViewToFolder(finfo);
				SelectMessageWhenReady(resultKey);
			}
			else if (finfo)
			{
				switch( cmd )
				{
					case MSG_NextFolder:
					case MSG_NextMessage:
						 mMotionPendingCommand = MSG_FirstMessage;
						break;
					case MSG_NextUnreadMessage:
					case MSG_NextUnreadThread:
					case MSG_NextUnreadGroup:
					case MSG_LaterMessage:
					case (MSG_MotionType)MSG_ToggleThreadKilled:
						mMotionPendingCommand = MSG_NextUnreadMessage;
						break;
					default:
						break;
				}
				RelocateViewToFolder( finfo );
			}
			return true;
		} 
		mUndoCommand = cmd_Undo;
	}
	catch (...)
	{
		SysBeep(1);
	}
	return false;
} // CThreadView::ObeyMotionCommand

//----------------------------------------------------------------------------------------
void CThreadView::SelectionChanged()
//----------------------------------------------------------------------------------------
{
	Inherited::SelectionChanged();
} // CThreadView::SelectionChanged

//----------------------------------------------------------------------------------------
void CThreadView::SelectMessageWhenReady(MessageKey inKey)
//----------------------------------------------------------------------------------------
{
	if (mStillLoading || (!GetMessagePane()))
		CDeferredTaskManager::Post1(new CDeferredSelectKeyTask(this, inKey), this);
	else
		SelectMessage(inKey);
} // CThreadView::SelectMessageWhenReady

//----------------------------------------------------------------------------------------
void CThreadView::SelectMessage(MessageKey inKey)
//----------------------------------------------------------------------------------------
{
	Assert_(GetMessagePane());
	if (GetMessagePane())
	{
		MSG_ViewIndex index = ::MSG_GetMessageIndexForKey(GetMessagePane(), inKey, true);
		if (index != MSG_VIEWINDEXNONE)
			SelectRow(index + 1);
	}
} // CThreadView::SelectMessage

//----------------------------------------------------------------------------------------
void CThreadView::UpdateHistoryEntry()
//----------------------------------------------------------------------------------------
{
	TableIndexT rowCount = GetSelectedRowCount();
	MSG_Pane* pane = GetMessagePane();
	if (!pane)
		return; // e.g., during window creation
	URL_Struct* url = nil;
	char entryName[64];
	entryName[0] = '\0';
	if (rowCount == 1)
	{
#if 0	// Auto-scroll should not be done here (+ it was redundant)
		if (!CApplicationEventAttachment::CurrentEventHasModifiers(cmdKey) &&
			!CApplicationEventAttachment::CurrentEventHasModifiers(shiftKey))
		{
			STableCell cell = GetFirstSelectedCell();
			if (IsValidCell(cell))
				ScrollCellIntoFrame(cell);
		}
#endif
		CMailSelection selection;
		GetSelection( selection );
		CMessage message(*selection.GetSelectionList() + 1, selection.xpPane);
		MessageKey id = message.GetMessageKey();
		url = ::MSG_ConstructUrlForMessage(pane, id);
		message.GetSubject(entryName, sizeof(entryName));
	}
	else
	{
		// zero selection or multiple selection - use folder for history entry
		url = CreateURLForProxyDrag(entryName);
	}
	if (url && *entryName)
	{
		LO_DiscardDocument(*mContext);
		History_entry* theNewEntry = ::SHIST_CreateHistoryEntry(
			url,
			entryName);
		::SHIST_AddDocument(*mContext, theNewEntry);
	}
	XP_FREEIF(url);
} // CThreadView::UpdateHistoryEntry

//----------------------------------------------------------------------------------------
Boolean CThreadView::ObeyMarkReadByDateCommand()
//----------------------------------------------------------------------------------------
{
	StDialogHandler handler(10525, NULL);
	LWindow* dlog = handler.GetDialog();
	if (! dlog)
		return true;

	CSearchDateField* dateField = dynamic_cast<CSearchDateField*>
					(dlog->FindPaneByID(CSearchDateField::class_ID));
	if (dateField)
	{
		dateField->SetToToday();
		MessageT message;
		do
		{
			message = handler.DoDialog();
		} while (message != msg_OK && message != msg_Cancel);
		if (message == msg_OK)
		{
			Int16 outYear;	UInt8 outMonth, outDay;
			dateField->GetDate(&outYear, &outMonth, &outDay);

			tm time;
			time.tm_sec		= 1;
			time.tm_min		= 1;
			time.tm_hour	= 1;
			time.tm_mday	= outDay;
			time.tm_mon		= outMonth - 1;
			time.tm_year	= outYear - 1900;
			time.tm_wday	= -1;
			time.tm_yday	= -1;
			time.tm_isdst	= -1;

			time_t endDate = ::mktime(&time);
			MSG_MarkReadByDate(GetMessagePane(), 0, endDate);
		}
	}
	return true;
} // CThreadView::ObeyMarkReadByDateCommand

//----------------------------------------------------------------------------------------
void CThreadView::DoSelectThread(TableIndexT inSelectedRow)
// Select all messages belonging to the same thread as this row.
//----------------------------------------------------------------------------------------
{
	CMessage message(inSelectedRow, GetMessagePane());
	MSG_ViewIndex threadIndex = MSG_ThreadIndexOfMsg(GetMessagePane(), message.GetMessageKey());
	if (threadIndex == MSG_VIEWINDEXNONE)
		return;
	TableIndexT first = 1 + threadIndex;
	STableCell firstCell(first, 1);
	TableIndexT last = first + CountExtraRowsControlledByCell(firstCell);
	Assert_(first < last || first == inSelectedRow);
	if (LTableRowSelector * selector = dynamic_cast<LTableRowSelector*>(mTableSelector))
	{
		// Powerplant's ClickSelect has already been called, so use the sense of the
		// clicked cell and turn all other cells in the thread on or off to match.
		STableCell clickedCell(inSelectedRow, 1);
		Boolean doSelect = selector->CellIsSelected(clickedCell);
		SetNotifyOnSelectionChange(false);
		try
		{
			if (!CApplicationEventAttachment::CurrentEventHasModifiers(shiftKey|cmdKey))
				UnselectAllCells();
			for (TableIndexT i = first; i <= last; i++)
				selector->DoSelect(i, doSelect, true, false);
		}
		catch (...) {}
		SetNotifyOnSelectionChange(true);
		SelectionChanged();
	}
} // CThreadView::DoSelectThread

//----------------------------------------------------------------------------------------
void CThreadView::DoSelectFlaggedMessages()
//----------------------------------------------------------------------------------------
{
	SetNotifyOnSelectionChange(false);
	try
	{
		if (!CApplicationEventAttachment::CurrentEventHasModifiers(shiftKey|cmdKey))
			UnselectAllCells();
		for (TableIndexT i = 1; i <= mRows; i++)
		{
			CMessage message(i, GetMessagePane());
			if (message.IsFlagged())
				SelectCell(STableCell(i, 1));
		}
	}
	catch (...) {}
	SetNotifyOnSelectionChange(true);
	SelectionChanged();
} // CThreadView::DoSelectFlaggedMessages

//----------------------------------------------------------------------------------------
Boolean CThreadView::ObeyCommand(
	CommandT	inCommand,
	void		*ioParam)
//----------------------------------------------------------------------------------------
{
	if (!mContext)
		return false;
		
	if (inCommand == cmd_UnselectAllCells)
	{
		UnselectAllCells();
		return true;
	}
	
	if (inCommand != cmd_Stop && XP_IsContextBusy((MWContext*)(*mContext)))
		return LCommander::GetTopCommander()->ObeyCommand(inCommand, ioParam); // global commands OK.
	if (inCommand == msg_TabSelect)
		return (GetOwningFolder() != nil); // Allow selection only if a folder is loaded.

	Boolean commandHandled = false;
	CommandT originalCommand = mContext->GetCurrentCommand();
	CNSContext* originalContext = mContext; // in case we close the window & delete it!
	mContext->SetCurrentCommand(inCommand);
	if (inCommand == cmd_GetNewMail || inCommand == cmd_GetMoreMessages)
	{
		ExpectNewMail();

		// getting new messages: slow down the status bar
		// to reduce flickers and improve performance
		CThreadWindow* threadWindow
			= dynamic_cast<CThreadWindow*>(LWindow::FetchWindowObject(GetMacPort()));
		if (threadWindow)
			threadWindow->GetProgressListener()->SetLaziness(
				CProgressListener::lazy_VeryButForThisCommandOnly);
	}
	// Don't let msgLib do the deletion since we want(?) to close open mail windows
	if ( inCommand == cmd_Clear )
	{
		DeleteSelection();
		return true;
	}
	if ( inCommand == cmd_NewFolder )
	{
		UFolderDialogs::ConductNewFolderDialog(GetOwningFolder());
		return true;
	}
	
	// If you're reading a news group, and you want to compose a new message, the new message
	// is, by default, addressed to that news group... this test and reassignment are necessary
	// to make this happen
	if ( (inCommand == cmd_NewMailMessage) && (GetFolderFlags() & MSG_FOLDER_FLAG_NEWSGROUP) ) {
			inCommand = cmd_PostNew;
	}
	
	// For msglib commands, we have to be careful to check whether the command
	// can be handled for THIS pane, because the message pane might have
	// enabled the menu item.  Failing to check again here leads to a nasty
	// crash.
	Boolean enabled; Boolean usesMark; Char16 mark; Str255 name;
	if (FindMessageLibraryCommandStatus(inCommand, enabled, usesMark, mark, name)
		&& enabled)
		{
			commandHandled = ObeyMessageLibraryCommand(inCommand, ioParam);
			if (inCommand == cmd_Undo || inCommand == cmd_Redo)
			{
				UnselectAllCells();
				mUndoTask = new CDeferredUndoTask(this);
				CDeferredTaskManager::Post1(mUndoTask, this);
			}
			else if (mUndoTask)
				CDeferredTaskManager::Remove(mUndoTask, this);
		}
	if (!commandHandled)
	{
		MSG_MotionType mcmd = UMessageLibrary::GetMotionType(inCommand);
		if (UMessageLibrary::IsValidMotion(mcmd))
			commandHandled = ObeyMotionCommand(mcmd);
		if (!commandHandled)
		{
			// Mail folder commands.  We come here either from a button (broadcast) or
			// from a menu command (synthetic).
			const char* folderPath = nil;
			if (inCommand == cmd_MoveMailMessages || inCommand == cmd_CopyMailMessages)
			{
				// Button case
				if ( ioParam ) // The BE dereferences this ASAP
				folderPath = ::MSG_GetFolderNameFromID((MSG_FolderInfo*)ioParam);
			}
			else if (!CMailFolderSubmenu::IsMailFolderCommand(&inCommand, &folderPath)) // menu case
			{
				folderPath = nil; // any other case.
			}
			if (folderPath && *folderPath)
			{
				FileMessagesToSelectedPopupFolder(
					folderPath, 
					inCommand == cmd_MoveMailMessages);
				commandHandled = true;
			}
		}
	}
	if (!commandHandled) switch(inCommand)
	{
		case cmd_SubscribeNewsgroups:
			MSG_Host* selectedHost =  MSG_GetHostForFolder(GetOwningFolder());
			CNewsSubscriber::DoSubscribeNewsGroup(selectedHost);
			break;
		case cmd_MarkReadByDate:
			ObeyMarkReadByDateCommand();
			commandHandled = true;
			break;
#if 0
		case cmd_RelocateViewToFolder:
			// Don't handle this in the three-pane view.  The folder pane has to change, too,
			// so pass it up to CThreadWindow and let it delegate.
			commandHandled = RelocateViewToFolder((MSG_FolderInfo*)ioParam);
			break;
#endif // 0
		case cmd_SortByDate:
		case cmd_SortBySubject:
		case cmd_SortBySender:
		case cmd_SortByThread:
		case cmd_SortByPriority:
		case cmd_SortBySize:
		case cmd_SortByStatus:
		case cmd_SortByFlagged:
		case cmd_SortByOrderReceived:
		case cmd_SortByReadness:
		case cmd_SortAscending:
		case cmd_SortDescending:
			if (ObeySortCommand(inCommand))
				commandHandled = true;
			break;
		case cmd_SelectThread:
			CMailSelection selection;
			GetSelection( selection );
			TableIndexT selectedRow = *selection.GetSelectionList() + 1;
			DoSelectThread(selectedRow);
			break;
		case cmd_SelectMarkedMessages:
			DoSelectFlaggedMessages();
			break;
	} // switch
	if (!commandHandled && inCommand > ENCODING_BASE && inCommand < ENCODING_CEILING)
	{
		Int16 default_csid = CPrefs::CmdNumToDocCsid(inCommand);
		SetDefaultCSID(default_csid);
		LCommander::SetUpdateCommandStatus(true); // bug #80474
		commandHandled = true;
	}
	if (!commandHandled)
		commandHandled = Inherited::ObeyCommand(inCommand, ioParam);
	//-----------------------------------	
	// Cleanup
	//-----------------------------------		
	// The following test against originalContext protects against the cases (quit, close)
	// when the object has been deleted.  The test against cmdHandled protects against
	// re-entrant calls to ListenToMessage.
	if (mContext == originalContext)
		if (commandHandled)
				mContext->SetCurrentCommand(cmd_Nothing); // watch out for re-entrant broadcast msgs.
		else
		{
			// It wasn't a command, so restore damage done if we're processing a broadcast.
			mContext->SetCurrentCommand(originalCommand);
		}
	return commandHandled;
} // CThreadView::ObeyCommand

//----------------------------------------------------------------------------------------
void CThreadView::ResetTextTraits()
//----------------------------------------------------------------------------------------
{
	Int16 wincsid = mContext->GetWinCSID();
	ResIDT newTextTraitsID;
	//	## Begin Hacky Code copy from Akbar (v3.0) mnews.cp
    // This will make everyone happy.
    // If the window is MAC_ROMAN, it will use the default mnews font.
    // Otherwise, get it from the Font preference
    if ( wincsid == INTL_CharSetNameToID(INTL_ResourceCharSet()) )	
    	newTextTraitsID = 130;
    else
		newTextTraitsID = CPrefs::GetTextFieldTextResIDs(wincsid);
	//	## End of Hacky Code copy from Akbar (v3.0) mnews.cp
           
    if (newTextTraitsID != mTextTraitsID )
	{
		SetTextTraits(newTextTraitsID);
		Refresh();
	}
} // CThreadView::ResetTextTraits

//----------------------------------------------------------------------------------------
Int16 CThreadView::DefaultCSIDForNewWindow(void)
//----------------------------------------------------------------------------------------
{
	if (mContext)
		return mContext->GetDefaultCSID();
	return 0;
} // CThreadView::DefaultCSIDForNewWindow

//----------------------------------------------------------------------------------------
void CThreadView::SetDefaultCSID(Int16 default_csid)
//----------------------------------------------------------------------------------------
{
	// Set the csid in the context
	mContext->SetDefaultCSID(default_csid);
	mContext->SetWinCSID(INTL_DocToWinCharSetID(default_csid));
	
	// Set the csid for the folder		
	MSG_SetFolderCSID(GetOwningFolder(), default_csid);
	
	// We need to set the csid for the view with the window
	// Ask the CThreadWindow to do it.
	CThreadWindow* threadWindow
		= dynamic_cast<CThreadWindow*>(LWindow::FetchWindowObject(GetMacPort()));
	if (threadWindow)
		threadWindow->SetDefaultCSID(default_csid);
		
	// We need to change the text info and redraw all the header 
	ResetTextTraits();
	
	// If we have Message Window appear, we need to set the csid for it.	
	try {
		// Find the all the MessageWindow which view the some folder	
		// and set the default csid for its view.
			
		CWindowIterator iter(WindowType_Message);
		CMessageWindow* window;
		for (iter.Next(window); window; iter.Next(window))
		{
			window = dynamic_cast<CMessageWindow*>(window);
			if (window)
			{
				CMessageView* messageView = window->GetMessageView();
				if (GetOwningFolder() == messageView->GetFolderInfo())
				{
					messageView->SetDefaultCSID(default_csid);
				}
			}
		}
	}	
	catch( ... )
	{
	}

} // CThreadView::SetDefaultCSID

// ¥¥ÊFix me, this should go into some utility classes
//----------------------------------------------------------------------------------------
void CThreadView::DrawUTF8TextString(
	const char*		inText, 
	const FontInfo*	inFontInfo,
	SInt16			inMargin,
	const Rect&		inBounds,
	SInt16			inJustification,
	Boolean			inDoTruncate,
	TruncCode		inTruncWhere)
//----------------------------------------------------------------------------------------
{
	Rect r = inBounds;
	
	r.left  += inMargin;
	r.right -= inMargin;
	
	PlaceUTF8TextInRect(inText, 
									strlen(inText),
									r,
									inJustification,
									teCenter,
									inFontInfo,
									inDoTruncate,
									inTruncWhere );			
}

// ¥¥ÊFix me, this should go into some utility classes
//----------------------------------------------------------------------------------------
void CThreadView::PlaceUTF8TextInRect(
	const char* 	inText,
	Uint32			inTextLength,
	const Rect 		&inRect,
	Int16			inHorizJustType,
	Int16			inVertJustType,
	const FontInfo*	/*inFontInfo*/,
	Boolean			inDoTruncate,
	TruncCode		/*inTruncWhere*/)
//----------------------------------------------------------------------------------------
{
	FontInfo 			utf8FontInfo;
	UFontSwitcher *fs;
	UMultiFontTextHandler *th;
	th = UUTF8TextHandler::Instance();
	fs = UPropFontSwitcher::Instance();
	th->GetFontInfo(fs, &utf8FontInfo);
	
	const char* text = inText;
	short length = inTextLength;
	if (inDoTruncate)
	{
		// ¥¥ Fix ME: Don't know how to do text truncation for UTF8 now.
	}
	Point thePoint = UGraphicGizmos::CalcStringPosition(
		 	inRect,
		 	th->TextWidth(fs, (char*)text, length),
		 	inHorizJustType,
		 	inVertJustType,
		 	&utf8FontInfo);
	::MoveTo(thePoint.h, thePoint.v);
	th->DrawText(fs, (char*)text ,length);	
} // CThreadView::PlaceUTF8TextInRect

//----------------------------------------------------------------------------------------
void CThreadView::ListenToMessage(
	MessageT	inCommand,
	void		*ioParam)
//----------------------------------------------------------------------------------------
{
	// Check ObeyCommand first, for a button message, but ONLY IF WE'RE ON DUTY.
	if (!IsOnDuty() || !ObeyCommand((CommandT)inCommand, ioParam)) // button message?
	{
		Inherited::ListenToMessage(inCommand, ioParam);
		switch (inCommand)
		{
			case msg_NSCAllConnectionsComplete:
			{
//				if (mPendingCommand)
//				{
//					CommandT cmd = mPendingCommand;
//					mPendingCommand = 0;
//					ObeyCommand(cmd, nil);
//					
//				}
//				else
#ifdef REMOVED_870427
				if (GotNewMail())
				{
					ScrollToGoodPlace();
					DontExpectNewMail();
				}
				break;
#endif
			if (GetContext()->GetCurrentCommand() == cmd_GetNewMail)
				DontExpectNewMail();
			}
		} // switch
	}
} // CThreadView::ListenToMessage

//----------------------------------------------------------------------------------------
void CThreadView::ActivateSelf()
//----------------------------------------------------------------------------------------
{
	Inherited::ActivateSelf();
} // CThreadView::ActivateSelf

//----------------------------------------------------------------------------------------
void CThreadView::ObeyCommandWhenReady(CommandT inCommand)
//----------------------------------------------------------------------------------------
{
	if (inCommand != cmd_Nothing)
		CDeferredTaskManager::Post(new CDeferredThreadViewCommand(this, inCommand, nil), this);
}

//----------------------------------------------------------------------------------------
URL_Struct* CThreadView::CreateURLForProxyDrag(char* outTitle)
//----------------------------------------------------------------------------------------
{
	MSG_Pane* pane = GetMessagePane();
	if (!pane)
		return nil;
	MSG_FolderInfo* folderInfo = GetOwningFolder();
	if (!folderInfo)
		return nil;
	CMessageFolder folder(folderInfo);
	if (outTitle)
	{
		strcpy(outTitle, folder.GetName());
		NET_UnEscape(outTitle);
	}
	return ::MSG_ConstructUrlForFolder(pane, folderInfo);
} // CThreadView::CreateURLForProxyDrag

#if defined(QAP_BUILD)		
//----------------------------------------------------------------------------------------
void CThreadView::GetQapRowText(
	TableIndexT			inRow,
	char*				outText,
	UInt16				inMaxBufferLength) const
// Calculate the text and (if ioRect is not passed in as null) a rectangle that fits it.
// Return result indicates if any of the text is visible in the cell
//----------------------------------------------------------------------------------------
{
	if (!outText || inMaxBufferLength == 0)
		return;

	cstring rowText("");
	short colCount = mTableHeader->CountVisibleColumns();
	CMessage message(inRow, GetMessagePane());

	CMailNewsWindow * myWindow = dynamic_cast<CMailNewsWindow*>(LWindow::FetchWindowObject(GetMacPort()));
	if (!myWindow) return;

	for (short col = 1; col <= colCount; col ++)
	{
		STableCell	aCell(inRow, col);
		LTableHeader::SColumnData * colData = mTableHeader->GetColumnData(col);
		if (!colData) break;
		LPane * colPane = myWindow->FindPaneByID(colData->paneID);
		if (!colPane) break;

		// get column name
		CStr255 descriptor;
		switch (GetCellDataType(aCell))
		{
			case kThreadMessageColumn:		descriptor = "Thread";		break;
			case kMarkedReadMessageColumn:	descriptor = "MarkRead";	break;
			case kFlagMessageColumn:		descriptor = "Flag";		break;
			default:
				colPane->GetDescriptor(descriptor);
				break;
		}
		rowText += descriptor;
		rowText += "=\042";

		// add cell text
		switch (PaneIDT dataType = GetCellDataType(aCell))
		{
			case kThreadMessageColumn:
				if (message.IsThread())
				{
					if (message.IsOpenThread())
						rowText += "-";
					else
						rowText += "+";
				}
				else
					rowText += " ";
				break;
				
			// note: no intl conversions (yet?) for subject, sender and addressee
			case kMarkedReadMessageColumn:	rowText += (message.HasBeenRead() ? "+" : " ");	break;
			case kFlagMessageColumn:		rowText += (message.IsFlagged() ? "+" : " ");	break;
			case kSubjectMessageColumn:		rowText += message.GetSubject();				break;
			case kSenderMessageColumn:		rowText += message.GetSender();					break;	
			case kDateMessageColumn:		rowText += message.GetDateString();				break;
			case kPriorityMessageColumn:	rowText += message.GetPriorityStr();			break;	
			case kSizeMessageColumn:		rowText += message.GetSizeStr();				break;
			case kStatusMessageColumn:		rowText += message.GetStatusStr();				break;
			case kAddresseeMessageColumn:	rowText += message.GetAddresseeString();		break;

			case kTotalMessageColumn:
			case kUnreadMessageColumn:
				if (GetSortedColumn() == kThreadMessageColumn && message.IsThread())
				{
					int theNum = (dataType == kTotalMessageColumn
										? message.GetNumChildren()
										: message.GetNumNewChildren());
					char tempStr[32];
					sprintf(tempStr, "%d", theNum);
					rowText += tempStr;
				}
				break;
		}
		if (col < colCount)
			rowText += "\042 | ";
		else
			rowText += "\042\r";
	}
	strncpy(outText, (char*)rowText, inMaxBufferLength);
	outText[inMaxBufferLength - 1] = '\0';
} // CThreadView::GetQapRowText
#endif //QAP_BUILD
