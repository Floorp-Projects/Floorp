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

#include "mattach.h"

// PowerPlant
#include <LWindow.h>

// macfe
#include "ufilemgr.h"
#include "macutil.h"
#include "resgui.h"
#include "uapp.h"
#include "msgcom.h"
#include "uerrmgr.h"
#include "CBrowserWindow.h"
#include "CBrowserContext.h"
#include "UModalDialogs.h"
#include "LGAEditField.h"
// For TEXT_PLAIN
#include "net.h"


#define msg_SelectionChanged	'sele'

CMailAttachment::CMailAttachment(const FSSpec & spec)
:	fDesiredType(nil)
,	fRealType(nil)
,	fSelected(false)
,	fRealName(nil)
,	fDescription(nil)
{
	fURL = CFileMgr::GetURLFromFileSpec(spec);	
	FInfo fndrInfo;
	FSpGetFInfo( &spec, &fndrInfo );
	fFileType = fndrInfo.fdType;
	fFileCreator = fndrInfo.fdCreator;
	ComposeDescription();
}

CMailAttachment::CMailAttachment(const MSG_AttachmentData * data)
{
	memset(this, 0, sizeof(CMailAttachment));
	fURL = data->url ? XP_STRDUP(data->url) : nil;
	fDesiredType = data->desired_type ? XP_STRDUP(data->desired_type) : nil;
	fRealType = data->real_type ? XP_STRDUP(data->real_type) : nil;
	fRealName = data->real_name ? XP_STRDUP(data->real_name) : nil;

	// this designates that we can enable the Source/Text radio buttons
	// for this item -> we really need more help to make the decision 
	fFileType = 'TEXT';		
	fFileCreator = '????';

	fDescription = data->description ? XP_STRDUP(data->description) : nil;
}


void CMailAttachment::FreeMembers()
{
	if (fURL)			XP_FREE(fURL);			fURL 			= nil;
	if (fDesiredType)	XP_FREE(fDesiredType);	fDesiredType 	= nil;
	if (fRealType)		XP_FREE(fRealType);		fRealType 		= nil;
	if (fRealName)		XP_FREE(fRealName);		fRealName		= nil;
	if (fDescription)	XP_FREE(fDescription);	fDescription 	= nil;
}

CStr255 & CMailAttachment::UrlText()
{
	CStr255 text;
	if ( fURL && NET_IsLocalFileURL(fURL) )
	{
		cstring tmp = CFileMgr::FileNameFromURL ( fURL );
		NET_UnEscape(tmp);
		text = tmp;
	}
	else
	{
		text = fURL;	// Default
		if (fURL)		// Messages get special cased
			if ((XP_STRNCASECMP(fURL, "snews:",6) == 0) ||
				(XP_STRNCASECMP(fURL, "news:",5) == 0))
				text = GetPString(ATTACH_NEWS_MESSAGE);
			else if (XP_STRNCASECMP(fURL, "mailbox:",8) == 0)
				text = GetPString(ATTACH_MAIL_MESSAGE);
	}
	return text;
}


void
CMailAttachment::ComposeDescription()
{
	if (fFileCreator == '????' || fDescription != nil)
		return;
	
	OSErr 		err;
	DTPBRec		pb;
	Str255		appName;
	char	*	suffix;
	UInt32		suffixLen;
	UInt32		descriptionLen;
		
	Try_
	{
		for (short vRefNum = -1; /**/; vRefNum --)
		{
			pb.ioVRefNum = vRefNum;
			pb.ioNamePtr = nil;
			err = ::PBDTGetPath(&pb);	// sets dt ref num in pb
			if (err != noErr)
				break;

			pb.ioCompletion	=	nil;
			pb.ioNamePtr	=	appName;
			pb.ioIndex		= 	0;
			pb.ioFileCreator=	fFileCreator;
			err = ::PBDTGetAPPLSync(&pb);
			if (err == noErr)
				break;
		}
		ThrowIfOSErr_(err);

		suffix = (fFileType == 'APPL') ? nil : suffix = GetCString(DOCUMENT_SUFFIX);
		suffixLen = (suffix != nil) ? strlen(suffix) + 1 : 0;	// + 1 for space and 
		
		// + 1 for terminator, space is taken care of in suffixLen
		descriptionLen = appName[0] + 1;
		if (suffix != nil) descriptionLen += suffixLen;

		fDescription = (char *) XP_ALLOC(descriptionLen);
		ThrowIfNULL_(fDescription);
		
		if (suffix != nil)
		{
			p2cstr(appName);
			sprintf(fDescription, "%s %s", (char *) appName, suffix);
		}
		else
		{
			::BlockMoveData(&appName[1], fDescription, appName[0]);
			fDescription[appName[0]] = '\0';
		}
	}	
	Catch_(errNum)
	{
		XP_FREEIF(fDescription);
		fDescription = nil;
	}		
	EndCatch_
}

//
// class CAttachmentList
//

CAttachmentList::CAttachmentList() : LArray(sizeof(CMailAttachment))
{
}

CAttachmentList::~CAttachmentList()
{
	CMailAttachment attach;
	LArrayIterator iter(*this);
	while (iter.Next(&attach))
		attach.FreeMembers();
}

// Clones the list, and its elements
CAttachmentList * CAttachmentList::Clone(CAttachmentList * clone)
{
	CAttachmentList *  newList = new CAttachmentList;
	ThrowIfNil_(newList);
	if (clone)	// Duplicate all the members of a list
	{
		LArrayIterator iter(*clone);
		
		CMailAttachment attach;
		while (iter.Next(&attach))
		{
			CMailAttachment newAttach;
			newAttach.fURL = attach.fURL ? XP_STRDUP(attach.fURL) : nil;
			newAttach.fDesiredType = attach.fDesiredType ? XP_STRDUP(attach.fDesiredType) : nil;
			newAttach.fFileType = attach.fFileType;
			newAttach.fFileCreator = attach.fFileCreator;
			newList->InsertItemsAt(1, 10000, &newAttach);
		}
	}
	return newList;
}

void CAttachmentList::RemoveItemsAt(Uint32	inCount, Int32	inAtIndex)
{
	CMailAttachment attach;
	for (int i =0; i<inCount; i++)
	{
		if (FetchItemAt(i+inAtIndex, &attach))
			attach.FreeMembers();
	}
	LArray::RemoveItemsAt(inCount, inAtIndex);
}



// Creates an attachment list to be passed into MSG_SetAttachmentList
// Note: we do not duplicate url/MIME strings, since msg library creates its own copy
MSG_AttachmentData* CAttachmentList::NewXPAttachmentList()
{
	if (GetCount() == 0)
		return nil;
	
	Size desiredSize = (GetCount()+1) * sizeof(MSG_AttachmentData);
	
	MSG_AttachmentData* newList = (MSG_AttachmentData*)XP_ALLOC(desiredSize);
	ThrowIfNil_(newList);

	memset(newList, 0, desiredSize);
	

	CMailAttachment attach;
	int i = 0;	// Watch out, list starts at 1, C arrays at 0
	while (FetchItemAt(i+1, &attach))
	{
		newList[i].url = attach.fURL;
		newList[i].desired_type = attach.fDesiredType;
		newList[i].real_type = attach.fRealType;
		newList[i].real_name = attach.fRealName;
		if (NET_IsLocalFileURL(attach.fURL))
		{
			char *macType = (char*) XP_ALLOC(9);	ThrowIfNil_(macType);
			sprintf(macType, "%X", attach.fFileType);
			newList[i].x_mac_type = macType;
			
			char *macCreator = (char*) XP_ALLOC(9);	ThrowIfNil_(macCreator);
			sprintf(macCreator, "%X", attach.fFileCreator);
			newList[i].x_mac_creator = macCreator;

			if (attach.fDescription)
				newList[i].description = XP_STRDUP(attach.fDescription);		
		}
		i++;
	}
	return newList;
}

// Deletes the list created by NewXPAttachmentList
void CAttachmentList::DeleteXPAttachmentList(MSG_AttachmentData* list)
{
	if (list == nil)
		return;
	int i=0;
	
	while (list[i].url != nil)
	{
		if (list[i].x_mac_type != nil)			XP_FREE(list[i].x_mac_type);
		if (list[i].x_mac_creator != nil)		XP_FREE(list[i].x_mac_creator);
		if (list[i].description != nil)		XP_FREE(list[i].description);		
		i++;
	}
	XP_FREE(list);
}

void CAttachmentList::InitializeFromXPAttachmentList(const MSG_AttachmentData* list)
{
	// Empty the list
	RemoveItemsAt(mItemCount, 1);

	// Initialize from scratch
	int i=0;
	
	while (list[i].url != nil)
	{
		CMailAttachment attach(&list[i]);
		InsertItemsAt(1, mItemCount+1, &attach);
		i++;
	}
}

// 
// class CAttachmentView
//

CAttachmentView::CAttachmentView( LStream * inStream) :
	LFinderView(inStream),
	mAttachList(nil),
	mCurrentlyAddedToDropList(true), // cuz LDragAndDrop constructor adds us.
	mSelectedItemCount(0),
	mNotifyAttachmentChanges(true)
{
	fNumberColumns 		= 1;
	fHighlightRow 		= false;
	fColumnIDs[0] 		= HIER_COLUMN;
	fCellHeight 		= 18;
}

void CAttachmentView::FinishCreateSelf()
{
	LFinderView::FinishCreateSelf();
}

void CAttachmentView::SelectItem( UInt32 cell, Boolean refresh )
{
	EventRecord		dummy;
	
	memset(&dummy, 0, sizeof(dummy));
	this->SelectItem( dummy, cell, refresh );
}

void CAttachmentView::SetAttachList(CAttachmentList * list)	
{
	mAttachList = list;
	SyncDisplay(mAttachList ? mAttachList->GetCount() : 0);
	
	// Select the first item in the list
	if (mAttachList != nil)
		SelectItem(1, TRUE);	// tj
}

Boolean	CAttachmentView::GetSelectedAttachment(CMailAttachment &attach)
{
	UInt32 cell = FirstSelectedCell();
	if (cell > 0)
	{
		mAttachList->FetchItemAt(cell, &attach);
		return TRUE;
	}
	else
		return false;
}


void CAttachmentView::AddMailAttachment(CMailAttachment * attach, Boolean refresh)
{
	XP_ASSERT( mAttachList);

	// Insert after the first selected cell,
	// if none, the end of the list
	UInt32 cell = FirstSelectedCell();
	if (cell == 0)
		cell =  mAttachList->GetCount() + 1;
	else
		cell++;
	
	mAttachList->InsertItemsAt(1, cell, attach);
	SelectItem( cell, TRUE );
	if (refresh)
		Refresh();
	SyncDisplay(mAttachList->GetCount());
	
	if (mNotifyAttachmentChanges)
		BroadcastMessage(msg_AttachmentsAdded, this);
}

void CAttachmentView::ModifyMailAttachment(Int32 cell, CMailAttachment * attach, Boolean refresh)
{
	CMailAttachment dummy;
	if (cell == SELECTED_CELL)
		cell = FirstSelectedCell();

	XP_ASSERT(mAttachList->FetchItemAt(cell, &attach));

	mAttachList->AssignItemsAt(1, cell, &attach);
	if (refresh)
		Refresh();
}

void CAttachmentView::SetMimeType(char * type)
{
	Boolean needRefresh = false;
	LArrayIterator iter(*mAttachList);
	CMailAttachment attach;
	
	for (int i=1; i<= mAttachList->GetCount(); i++)
	{
		if (mAttachList->FetchItemAt(i, &attach))
		{
			if ( attach.fSelected )
			{
				if (attach.fDesiredType != nil)
					XP_FREE(attach.fDesiredType);
			
				if (type)
					attach.fDesiredType = XP_STRDUP(type);
				else
					attach.fDesiredType = nil;
			
				mAttachList->AssignItemsAt(1, i, &attach);	
				
				needRefresh = true;			
			}
		}
	}
	
	if (needRefresh)
		Refresh();
}

void CAttachmentView::AddFile(const FSSpec& spec)
{
	if (CmdPeriod())	// If we drop the whole HD, allow bailout
		return;
	
	if (CFileMgr::IsFolder(spec))
	{
		FSSpec newSpec;
		FInfo 	dummy;
		Boolean dummy2;
		CFileIter	iter(spec);
		while (iter.Next(newSpec, dummy, dummy2))
			AddFile(newSpec);
	}
	else
	{
		CMailAttachment attach(spec);
		AddMailAttachment(&attach, false);
	}
	Refresh();
}

void CAttachmentView::DeleteSelection(Boolean refresh)
{
	CMailAttachment attach;
	LArrayIterator iter(*mAttachList);
	Boolean needRefresh = false;
	Int32 itemToSelect = 0;
	
	while (iter.Next(&attach))
	{
		if (attach.fSelected)
		{
			needRefresh = TRUE;
			itemToSelect = mAttachList->FetchIndexOf(&attach);
			mAttachList->Remove(&attach);
		}
	}
	if (itemToSelect != 0)
		SelectItem(itemToSelect, TRUE);
	if (refresh && needRefresh)
		Refresh();
	if (needRefresh)
		BroadcastMessage(msg_SelectionChanged, nil);
	if (needRefresh)
		SyncDisplay(mAttachList->GetCount());
		
	
	if (mNotifyAttachmentChanges)
		BroadcastMessage(msg_AttachmentsRemoved, this);	
}

Boolean CAttachmentView::SyncDisplay( UInt32 visCells )
{
	return LFinderView::ResizeTo( fCellHeight * visCells, mImageSize.width );
}

void CAttachmentView::DrawCellColumn( UInt32 cell, UInt16 /*column*/ )
{
	DrawHierarchy( cell );
}

Boolean CAttachmentView::IsCellSelected( Int32 cell )
{
//	XP_ASSERT( mAttachList);
	if (mAttachList)
	{
		CMailAttachment attach;
		if (mAttachList->FetchItemAt(cell, &attach))
			return attach.fSelected;
		else
			return false;
	}
	else
		return false;
}

void CAttachmentView::CellText( Int32 cell, CStr255& text )
{
//	XP_ASSERT( mAttachList);
	if (mAttachList)
	{
		CMailAttachment attach;
		if (mAttachList->FetchItemAt(cell, &attach))
			text = attach.UrlText();		
		else
	#ifdef DEBUG
			text = "What's up";
	#else
			text = CStr255::sEmptyString;
	#endif
	}
}

void CAttachmentView::GetTextRect( UInt32 cell, Rect& where ) 
{
	where.top = ( ( cell - 1 ) * fCellHeight ) +
		mPortOrigin.v + mImageLocation.v;
	where.bottom = where.top + fCellHeight;
	where.left = ( this->CellIndentLevel( cell ) * LEVEL_OFFSET ) + 
		mPortOrigin.h + mImageLocation.h + ICON_WIDTH;
	
	CStr255	text;
	
	CellText(cell, text);
	where.right = where.left + StringWidth(text) + 4;
	// where.right = where.left + mImageSize.width;
}

void CAttachmentView::GetIconRect( UInt32 cell, Rect& where )
{
	where.top = ( ( cell - 1 ) * fCellHeight ) + mPortOrigin.v + mImageLocation.v;
	where.bottom = where.top + ICON_HEIGHT;
	where.left = ( this->CellIndentLevel( cell ) * LEVEL_OFFSET ) +
		+ mPortOrigin.h + mImageLocation.h;
	where.right = where.left + ICON_WIDTH;
}

void CAttachmentView::GetWiglyRect( UInt32 /*cell*/, Rect& rect )
{
	rect.top = rect.bottom = rect.left = rect.right = 0;
}

void CAttachmentView::SelectItem( const EventRecord& event, UInt32 cell, Boolean refresh )
{
	Boolean		shift = ( event.modifiers & shiftKey ) != 0;

	if (  !shift )
		ClearSelection();

	CMailAttachment attach;
	if (mAttachList->FetchItemAt(cell, &attach))
	{
		if ( !shift )
			attach.fSelected = TRUE;
		else
			attach.fSelected = !attach.fSelected;
			
		if (attach.fSelected)
			mSelectedItemCount++;
		else
			mSelectedItemCount--;
		
		mAttachList->AssignItemsAt(1, cell, &attach);
	}

	BroadcastMessage(msg_SelectionChanged, nil);

	if (refresh)
		Refresh();
}

void CAttachmentView::ClearSelection()
{
//	XP_ASSERT(mAttachList);
	if (mAttachList)
	{
		CMailAttachment attach;
		int i=1;
		while (mAttachList->FetchItemAt(i, &attach))
		{
			if (attach.fSelected)
			{
				attach.fSelected = false;
				mAttachList->AssignItemsAt(1, i, &attach);
				
				RefreshCells(i, i, false);
			}
			
			i++;
		}
		
	}
	mSelectedItemCount = 0;
}

UInt32 CAttachmentView::GetVisibleCount()
{
//	XP_ASSERT(mAttachList != nil);
	if (mAttachList)
		return mAttachList->GetCount();
	else
		return 0;
}

UInt32 CAttachmentView::FirstSelectedCell()
{
	CMailAttachment attach;
	if (!mAttachList)
		return 0;

	LArrayIterator iter(*mAttachList);
	int i = 0;
	while (iter.Next(&attach))
	{
		i++;
		if (attach.fSelected)
			return i;
	}
	return 0;
}

Boolean CAttachmentView::HandleKeyPress( const EventRecord& inEvent )
{
	Char16		key = inEvent.message;
	Char16		character = key & charCodeMask;
	switch ( character )
	{
		case char_Backspace:
		case char_FwdDelete:
			DeleteSelection();
			return TRUE;
		default:
			return LFinderView::HandleKeyPress( inEvent );
	}
	return false;
}

Boolean CAttachmentView::ItemIsAcceptable(	DragReference	inDragRef,
											ItemReference	inItemRef	)
{
	FlavorFlags flavorFlags;
	
	if (::GetFlavorFlags( inDragRef, inItemRef, flavorTypeHFS, &flavorFlags) == noErr)
		return true;

		
	return (::GetFlavorFlags(inDragRef, inItemRef, 'TEXT', &flavorFlags) == noErr);
}	

void	
CAttachmentView::DoDragReceive(DragReference inDragRef)
{
	Boolean	saveFlag;
	Boolean success = false; // so we don't broadcast if it fails
	saveFlag = mNotifyAttachmentChanges; 
	mNotifyAttachmentChanges = false;
	Try_
	{
		// Put this in a try block, because this is called
		// from the Finder!  So it fixes bug with nasty hang attaching certain files.
		LFinderView::DoDragReceive(inDragRef);
		success = true;
	}
	Catch_(err)
	{
	}
	EndCatch_
	mNotifyAttachmentChanges = saveFlag;
	
	if (success & saveFlag)
		BroadcastMessage(msg_AttachmentsAdded, this);
}


void CAttachmentView::ReceiveDragItem(	DragReference 	inDragRef, 
										DragAttributes	/*flags*/, 
										ItemReference 	inItemRef, 
										Rect& 			/*itemBounds*/)
{
	OSErr		err;
	HFSFlavor	itemFile;
	Size		itemSize = sizeof(itemFile);
	
	ClearSelection();
		
	// Get the HFS data
	err = ::GetFlavorData( inDragRef, inItemRef, flavorTypeHFS, &itemFile, &itemSize, 0);
	if (err == noErr)
		AddFile(itemFile.fileSpec);	
	else if(err == badDragFlavorErr )
	{
		err = GetHFSFlavorFromPromise (inDragRef, inItemRef, &itemFile, true);
		if( err == noErr )
			AddFile( itemFile.fileSpec );
		else
		{			
			char *url = nil;
			itemSize = 0;
		
			err = GetFlavorDataSize(inDragRef, inItemRef, 'TEXT', &itemSize);
			if (err == noErr)
			{
				char *title;
				
				url = (char *) XP_ALLOC(itemSize+1); ThrowIfNil_(url);
				err = ::GetFlavorData( inDragRef, inItemRef, 'TEXT', url, &itemSize, 0);
				url[itemSize] = '\0';
				
				if (err == noErr)
				{
					title = strchr(url, '\r');
					if (title != nil)
						*title = '\0';
								
					CMailAttachment attach(url, nil, nil, nil);
					AddMailAttachment(&attach, false);
				}
			}
		}		
		// Back to PP
		ThrowIfOSErr_(err);
	}	
}	

void CAttachmentView::TakeOffDuty()
{
	if (mAttachList != nil)
		LFinderView::TakeOffDuty();
}

void CAttachmentView::InsideDropArea( DragReference /*inDragRef*/ )
{
	// don't do the Finder insert highlighting 
}
	

void CAttachmentView::ListenToMessage(MessageT inMessage, void* ioParam)
{
	switch(inMessage)
	{
		case msg_AttachFile:
			StandardFileReply myReply;
			SFTypeList dummy;
			Point myPoint;
			::SetPt(&myPoint,-1,-1); // Center window
			UDesktop::Deactivate();	// Always bracket this
	      	CustomGetFile ( nil, -1, dummy, &myReply, kSFAttachFile, myPoint, 
	      			nil, nil, nil, nil, nil);			
			UDesktop::Activate();
			if (myReply.sfGood)
				AddFile(myReply.sfFile);
			break;
			
		case msg_AttachWeb:
			AddWebPageWithUI((unsigned char *)ioParam);
			break;
			
		case msg_RemoveAttach:
			DeleteSelection();
			break;
		
		default:
			break;
	}
}

void CAttachmentView::AddWebPageWithUI(unsigned char *defaultText)
{
	// Put up dialog
	StDialogHandler handler(10616, nil);
	// Select the "URL" edit field
	LWindow* dialog = handler.GetDialog();
	LGAEditField *urlfield = (LGAEditField*)dialog->FindPaneByID('edit');
	SignalIf_(!urlfield);
	if (!urlfield)
		return;
	urlfield->SetDescriptor(defaultText);
	urlfield->SelectAll();
	dialog->SetLatentSub(urlfield);
	// Run the dialog
	MessageT message;
			
	do {
		message = handler.DoDialog();
	} while (message != msg_OK && message != msg_Cancel);
	
	// Use the result.
	if (message == msg_OK)
	{
		CStr255 result;
		char* url=nil;
		urlfield->GetDescriptor(result);
		if (result.Length() > 0)
		{
			url = (char*)XP_ALLOC(result.Length() + 1);
			ThrowIfNULL_(result);
			::BlockMoveData(&result[1], url, result.Length());
			url[result.Length()] = '\0';
		}
		CMailAttachment attach(url, nil, nil, nil);
		AddMailAttachment(&attach, false);
	}
};

void CAttachmentView::HiliteDropArea(DragReference inDragRef)
{
	LDragAndDrop::HiliteDropArea(inDragRef);
}


Boolean
CAttachmentView::ProcessCommand(CommandT inCommand, void *ioParam)
{
	if (inCommand == msg_TabSelect)
		return (GetVisibleCount() > 0);
	else
		return LFinderView::ProcessCommand(inCommand, ioParam);
}


void
CAttachmentView::BeTarget()
{
	if ( FirstSelectedCell() == 0)
	{
		if ( this->GetVisibleCount() > 0 )
			this->SelectItem( 1, TRUE );
	}
}

void
CAttachmentView::AddDropAreaToWindow(LWindow* inWindow)
{
	if (!mCurrentlyAddedToDropList)
		LDropArea::AddDropArea(this, inWindow->GetMacPort());
	mCurrentlyAddedToDropList = true;
}

void
CAttachmentView::RemoveDropAreaFromWindow(LWindow* inWindow)
{
	if (mCurrentlyAddedToDropList)
		LDropArea::RemoveDropArea(this, inWindow->GetMacPort());
	mCurrentlyAddedToDropList = false;
}
