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



// CThreadView.h

#pragma once

// PowerPlant
#include <LTableView.h>
#include <LListener.h>
#include <LPeriodical.h>

#include "msgcom.h"

// Mac Netscape Lib
#include "CMailFlexTable.h"
#include "LTableHeader.h"
#include "NetscapeDragFlavors.h"

// MacFE
#include "CMailNewsContext.h"
#include "CMessageFolder.h"

class CMessageWindow;
class CDeferredUndoTask;
class CPersistentMessageSelection;

//======================================
class CMessage 
//======================================
{
public:
	CMessage(TableIndexT inIndex, MSG_Pane* mMessageList);
	virtual ~CMessage();
	
	// Sometimes the new mMessageList has the same
	// address as the sCacheMessageList.  So, when we are
	// loading a new message folder, call InvalidateCache
	static void 	InvalidateCache();
	
	MessageId		GetThreadID()			const;
	MessageKey		GetMessageKey()			const;
	
	Boolean			HasBeenRead()	 		const;
	Boolean			IsFlagged()		 		const;
	Boolean			HasBeenRepliedTo()		const;
	
	const char*		GetSubject(char* buffer, UInt16 bufSize) const;
	const char*		GetSubject() 			const;
	const char*		GetSender()				const;
	const char*		GetDateString()			const;
	const char*		GetAddresseeString()	const;
	const char*		GetSizeStr()			const;
	const char*		GetPriorityStr()		const;
	const char*		GetStatusStr()			const;
	void			GetPriorityColor(RGBColor&) const;
	static void		GetPriorityColor(MSG_PRIORITY inPriority, RGBColor& outColor);
	static Int16	PriorityToMenuItem(MSG_PRIORITY inPriority);
	static MSG_PRIORITY	MenuItemToPriority(Int16 inMenuItem);
	static const char*	GetSubject(MSG_MessageLine* data, char* buffer, UInt16 bufSize);
		
	inline time_t	GetDate()				const;
	inline UInt32	GetSize()				const;
	MSG_PRIORITY	GetPriority()			const;
	inline UInt32	GetStatus()				const;
	int8			GetThreadLevel()		const;
	uint16			GetNumChildren()		const;
	uint16			GetNumNewChildren()		const;
	
	Boolean			HasAttachments()		const;
	Boolean			IsThread()				const;
	Boolean			IsOpenThread()			const;
	Boolean			IsOffline()				const; // db has offline news or IMAP msg body
	Boolean			IsDeleted()				const;
	Boolean			IsTemplate()			const;

	ResIDT			GetIconID(UInt16 inFolderFlags) const;
	static ResIDT	GetIconID(UInt16 inFolderFlags, UInt32 inMessageFlags);
	ResIDT			GetThreadIconID() const;

	Boolean			UpdateMessageCache() const;

protected:
	
	Boolean	TestXPFlag(UInt32 inMask) const;

	MSG_ViewIndex 	mIndex;
	MSG_Pane*		mMessageList;
	
	
	static MSG_MessageLine	sMessageLineData;
	static MSG_Pane*		sCacheMessageList;
	static MSG_ViewIndex	sCacheIndex;
}; // class CMessage

//======================================
class CThreadView : public CMailFlexTable
//======================================
{
	friend class CThreadMessageController;
	friend class CFolderThreadController;
private:
	typedef CMailFlexTable Inherited;
public:
	enum {class_ID = 'msTb'};
	
	// ------------------------------------------------------------
	// Construction
	// ------------------------------------------------------------
						CThreadView(LStream *inStream);
	virtual				~CThreadView();
	void				LoadMessageFolder(	
							CNSContext* inContext,
							const CMessageFolder& inFolder,
							Boolean loadNow = false);

	void				FileMessagesToSelectedPopupFolder(const char *ioFolderName,
														  Boolean inMoveMessages);//¥¥TSM
	
	MSG_FolderInfo*		GetOwningFolder() const
						{
							return mXPFolder.GetFolderInfo();
						}

	uint32				GetFolderFlags() const
						{
							return mXPFolder.GetFolderFlags();
						}


	// ------------------------------------------------------------
	// Data change notification
	// Callbacks from MSGlib come here.
	// ------------------------------------------------------------
	virtual void ChangeStarting(
		MSG_Pane*		inPane,
		MSG_NOTIFY_CODE	inChangeCode,
		TableIndexT		inStartRow,
		SInt32			inRowCount
		);
	virtual void ChangeFinished(
		MSG_Pane*		inPane,
		MSG_NOTIFY_CODE	inChangeCode,
		TableIndexT		inStartRow,
		SInt32			inRowCount
		);
	virtual void 		PaneChanged(
		MSG_Pane*		inPane,
		MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
		int32 value);

protected:
	void				SetFolder(const CMessageFolder& inFolder);
	void				SetFolder(const MSG_FolderInfo* inFolderLine);
	
	// ------------------------------------------------------------
	// Clicking
	// ------------------------------------------------------------
protected:
	virtual void		ClickCell(	
								const STableCell		&inCell,
								const SMouseDownEvent	&inMouseDown);								 
	virtual Boolean		CellSelects(const STableCell& inCell) const;
	virtual Boolean 	CellWantsClick( const STableCell& inCell ) const;
									
	// ------------------------------------------------------------
	// Drawing
	// ------------------------------------------------------------
protected:

	virtual void		DrawCellContents(	
								const STableCell		&inCell,
								const Rect				&inLocalRect);																
	void				DrawMessageSubject(	
								TableIndexT		inRow, 
								const Rect&		inLocalRect);											
	void				DrawMessageSize(
								const CMessage&	inMessage,
								const Rect&		inLocalRect	);
	void				DrawMessagePriority(
								const CMessage&	inMessage,
								const Rect&		inLocalRect	);
	void				DrawMessageStatus(
								const CMessage&	inMessage,
								const Rect&		inLocalRect	);

	// Specials from CStandardFlexTable
	virtual TableIndexT	GetHiliteColumn() const;
	virtual ResIDT		GetIconID(TableIndexT inRow) const;
	virtual UInt16		GetNestedLevel(TableIndexT inRow) const;
	virtual	void		GetDropFlagRect( const Rect& inLocalCellRect,
										 Rect&		 outFlagRect) const;
	virtual void		GetMainRowText(
							TableIndexT			inRow,
							char*				outText,
							UInt16				inMaxBufferLength) const;
	virtual Boolean		GetHiliteTextRect(
							const TableIndexT	inRow,
							Rect&				outRect) const;
	virtual void		ApplyTextStyle(TableIndexT inRow) const;
	virtual void		ApplyTextColor(TableIndexT inRow) const;
	virtual void		DrawIconsSelf(
							const STableCell&	inCell,
							IconTransformType	inTransformType,
							const Rect&			inIconRect) const;
										
	// ------------------------------------------------------------
	// Mail and news
	// ------------------------------------------------------------
public:
	void				ExpectNewMail() { mExpectingNewMail = true; mGotNewMail = false; }
	void				DontExpectNewMail() { mExpectingNewMail = false; mGotNewMail = false; }
	Boolean				GotNewMail() { return mExpectingNewMail && mGotNewMail; }
	void				SaveSelectedMessage();
	Boolean				RestoreSelectedMessage();
	
	// ------------------------------------------------------------
	// Drag and Drop
	// ------------------------------------------------------------
public:
	URL_Struct*			CreateURLForProxyDrag(char* outTitle);

protected:
	virtual Boolean		ItemIsAcceptable(DragReference	inDragRef, ItemReference inItemRef);
	//virtual void		InsideDropArea(DragReference inDragRef);
	virtual void		EnterDropArea(DragReference inDragRef, Boolean inDragHasLeftSender);
	virtual void		LeaveDropArea(DragReference	inDragRef);
	virtual void		ReceiveDragItem(
							DragReference	inDragRef
						,	DragAttributes	inDragAttrs
						,	ItemReference	inItemRef
						,	Rect&			inItemBounds);	
	Boolean				GetDragCopyStatus(
							DragReference			inDragRef
						,	const CMailSelection&	inSelection
						,	Boolean&				outCopy);

	// ------------------------------------------------------------
	// Row Expansion/Collapsing
	// ------------------------------------------------------------
protected:
	virtual void		SetCellExpansion(const STableCell &inCell, Boolean inExpanded);
	virtual Boolean		CellHasDropFlag( 
									const STableCell&	inCell,
									Boolean&			outExpanded) const;
	virtual Boolean		CellInitiatesDrag(const STableCell& inCell) const;
	virtual TableIndexT CountExtraRowsControlledByCell(const STableCell& inCell) const;

	//-----------------------------------
	// Commands
	//-----------------------------------
public:
						enum {cmd_UnselectAllCells = 'UnSl'};

	virtual Boolean		ObeyCommand(
							CommandT	inCommand,
							void		*ioParam);

	void				ObeyCommandWhenReady(CommandT inCommand);

protected:
	virtual void		FindCommandStatus(
							CommandT	inCommand,
							Boolean		&outEnabled,
							Boolean		&outUsesMark,
							Char16		&outMark,
							Str255		outName);
	void				FindSortCommandStatus(CommandT inCommand, Int16& inOutMark);
	Boolean				ObeySortCommand(CommandT inCommand);
	Boolean				ObeyMotionCommand(MSG_MotionType inCommand);
	Boolean				ObeyMarkReadByDateCommand();
	
	virtual void		DeleteSelection();
	virtual void		ListenToMessage(
							MessageT	inCommand,
							void		*ioParam);

	// ------------------------------------------------------------
	// Miscellany - PP
	// ------------------------------------------------------------
public:
	virtual void		ActivateSelf();

protected:
			void		NoteSortByThreadColumn(Boolean isThreaded) const;
	virtual void		OpenRow(TableIndexT inRow);

	// ------------------------------------------------------------
	// Miscellany - Selection management
	// ------------------------------------------------------------
public:
	Boolean				ScrollToGoodPlace(); // as, to first unread message
	virtual void		SelectionChanged(); // overridden to scroll into view

	void				SelectMessageWhenReady(MessageKey inKey);
	void				SetSelectAfterDelete(Boolean inDoSelect) { mSelectAfterDelete = inDoSelect; }
	void				SelectAfterDelete(TableIndexT inRow);

protected:
	void				SelectMessage(MessageKey inKey);
	virtual void		ResizeFrameBy(
							Int16		inWidthDelta,
							Int16		inHeightDelta,
							Boolean		inRefresh);
	virtual void		SetRowCount();

	// ------------------------------------------------------------
	// Miscellany - i18n support
	// ------------------------------------------------------------
public:
	void				SetDefaultCSID(Int16 default_csid); 
	virtual Int16		DefaultCSIDForNewWindow();
	
	// The following two should really go into utility class
	static void			DrawUTF8TextString(	const char*		inText, 
								const FontInfo*	inFontInfo,
								SInt16			inMargin,
								const Rect&		inBounds,
								SInt16			inJustification = teFlushLeft,
								Boolean			doTruncate = true,
								TruncCode		truncWhere = truncMiddle);
	static void 		PlaceUTF8TextInRect(
							const char* 	inText,
							Uint32			inTextLength,
							const Rect 		&inRect,
							Int16			inHorizJustType = teCenter,
							Int16			inVertJustType = teCenter,
							const FontInfo*	inFontInfo = NULL,
							Boolean			inDoTruncate = false,
							TruncCode		inTruncWhere = truncMiddle);	
protected:
	void 				ResetTextTraits();

protected:
	virtual void		ChangeSort(const LTableHeader::SortChange* inSortChange);
			void		SyncSortToBackend();
			void		UpdateSortMenuCommands() const;
	Boolean				RelocateViewToFolder(const CMessageFolder& inFolder);
							// Retarget the view to the specified BE folder.
	void				UpdateHistoryEntry(); // For bookmark support.
	void				DoSelectThread(TableIndexT inSelectedRow);
	void				DoSelectFlaggedMessages();

	// ------------------------------------------------------------
	// QA Partner support
	// ------------------------------------------------------------
#if defined(QAP_BUILD)		
public:
	virtual void		GetQapRowText(TableIndexT inRow, char* outText, UInt16 inMaxBufferLength) const;
#endif
	
	// ------------------------------------------------------------
	// Data
	// ------------------------------------------------------------

protected:
	CMessageFolder		mXPFolder; // The unique id of the folder we are viewing.  Owned by backend.

	CPersistentMessageSelection* mSavedSelection; // Used while the window is being rearranged.

	Boolean				mExpectingNewMail;
	Boolean				mGotNewMail;
	Boolean				mIsIdling;
	Boolean				mSelectAfterDelete;	// This was a demand from a big customer
	TableIndexT			mRowToSelect; // hack to help coordinate thread/message panes.
	Boolean				mScrollToShowNew;
	MSG_MotionType		mMotionPendingCommand;
	CDeferredUndoTask*	mUndoTask;
	
}; // class CThreadView
