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

#pragma once

#include "mfinder.h"
#include "LArray.h"

class CMailWindow;
class LWindow;
struct MSG_AttachmentData;

/* CMailAttachment
 * holds all the information about a single attachment
 * list of these is kept inside a mail window
 * Has to mirror MSG_AttachmentData struct in mime.h
 */
struct CMailAttachment	{
	char * fURL;
	char * fDesiredType;
	char * fRealType; 	/* The type of the URL if known, otherwise NULL */
	char * fRealName;	/* Real name of the file */
	char * fDescription;
	Boolean fSelected;
	OSType	fFileType;
	OSType	fFileCreator;

	CMailAttachment(char * url, char * mimeType = NULL, char * realType = NULL, char * realName = NULL)	
	{
		memset(this, 0, sizeof(CMailAttachment));
		fURL = url; 
		fDesiredType = mimeType;
		fRealType = realType;
		fRealName = realName;
		fSelected = FALSE;

		// ¥¥¥ FIX ME: LAME
		fFileType = 'TEXT';
		fFileCreator = '????';
	}
	CMailAttachment(const FSSpec & spec);
	CMailAttachment(const MSG_AttachmentData * data);
	CMailAttachment()	
	{
		memset(this, 0, sizeof(CMailAttachment));
	}
	void FreeMembers();
	CStr255& UrlText();
	void ComposeDescription();
};

/**************************************************************************
 * class CAttachmentList
 * holds a list of CMailAttachments
 * When you delete a CMailAttachments item, it disposes of things that it
 * points to.
 * knows how to clone itself
 **************************************************************************/
class CAttachmentList : public LArray	{
public:

// constructors
					CAttachmentList();
	virtual			~CAttachmentList();
	static CAttachmentList * Clone(CAttachmentList * clone);

// LArray overrides
	virtual void	RemoveItemsAt(Uint32	inCount, Int32	inAtIndex);

// ¥¥Êaccess
	MSG_AttachmentData* NewXPAttachmentList();		// Creates an attachment list
	void			DeleteXPAttachmentList(MSG_AttachmentData* list);	// Deletes the XP list
	void			InitializeFromXPAttachmentList(const MSG_AttachmentData* list);
};

/* ************************************************************************
 * CAttachmentView
 * displays a list of attachments
 * By default, our list is fully destroyed on termination
 **************************************************************************/
#define attachWindID		8002
#define SELECTED_CELL		-1

#define kSFAttachFile 146
const MessageT msg_AttachFile			= 'Atch';
const MessageT msg_AttachWeb			= 'AtWb';
const MessageT msg_RemoveAttach			= 'RmAt';

class CAttachmentView : public LFinderView, public LBroadcaster, public LListener	{
public:
	enum { class_ID = 'AtVw', msg_AttachmentsAdded = 'AttA', msg_AttachmentsRemoved = 'AttR' };

// ¥¥ constructors
	CAttachmentView( LStream * inStream);

	virtual void FinishCreateSelf();

// ¥¥ access
	void			SetAttachList(CAttachmentList * list);
	CAttachmentList * GetAttachList() {return mAttachList;}
	Boolean			GetSelectedAttachment(CMailAttachment &attach);
	void			AddMailAttachment(CMailAttachment * attach, Boolean refresh = true);
	void			ModifyMailAttachment(Int32 cell, 	// SELECTED_CELL for current selection
											CMailAttachment * attach, 
											Boolean refresh);
	void			SetMimeType(char * type);
	void 			AddWebPageWithUI(unsigned char *defaultText);
	void			AddFile(const FSSpec& spec);
	void			DeleteSelection(Boolean refresh = TRUE);

// ¥¥ Finder view overrides
	virtual void		DrawCellColumn( UInt32 cell, UInt16 column );
	virtual Boolean		SyncDisplay( UInt32 visCells );
	virtual Boolean		IsCellSelected( Int32 cell );
	virtual ResIDT		CellIcon( Int32 /* cell */ ) { return 133; }
	virtual void		CellText( Int32 cell, CStr255& text );
	virtual void		GetTextRect( UInt32 cell, Rect& where );
	virtual void 		GetIconRect( UInt32 cell, Rect& where );
	virtual void		GetWiglyRect( UInt32 cell, Rect& rect );
	virtual void		SelectItem( const EventRecord& event, UInt32 cell, Boolean refresh );
	virtual void		ClearSelection();
	virtual UInt32		GetVisibleCount();
	virtual UInt32		FirstSelectedCell();

// ¥¥ PP overrides
	virtual Boolean 	HandleKeyPress( const EventRecord& inEvent );
	virtual Boolean		ProcessCommand(CommandT inCommand, void *ioParam);
	virtual void		BeTarget();
	virtual void		TakeOffDuty();
	virtual void		ListenToMessage(MessageT inMessage, void* ioParam);
// ¥¥ drag'n'drop
	virtual	void 		InsideDropArea( DragReference inDragRef );
	virtual void		HiliteDropArea(DragReference inDragRef);

	virtual Boolean 	ItemIsAcceptable(DragReference	inDragRef, ItemReference	inItemRef);
	virtual void 		DoDragReceive(DragReference inDragRef);
	virtual void		ReceiveDragItem(	DragReference 	inDragRef, 
									DragAttributes	flags, 
									ItemReference 	inItemRef, 
									Rect& 			itemBounds);
	// utility functions for new compose window because attach view
	// is inside a tab switcher
	void				AddDropAreaToWindow(LWindow* inWindow);
	void				RemoveDropAreaFromWindow(LWindow* inWindow);

	inline UInt32		CountSelectedItems()	{ return mSelectedItemCount; }									

protected:
	void			SelectItem( UInt32 cell, Boolean refresh );

	CAttachmentList *	mAttachList;
	UInt32				mSelectedItemCount;
	Boolean				mNotifyAttachmentChanges;
	Boolean				mCurrentlyAddedToDropList;
};
