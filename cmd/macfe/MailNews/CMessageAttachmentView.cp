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



// CMessageAttachmentView.cp

#include "CMessageAttachmentView.h"

#include "UDebugging.h"
#include "PascalString.h"
#include "UGraphicGizmos.h"
#include "xp_str.h"
#define WANT_ENUM_STRING_IDS		// why don't we just have a header that does this?
#include "allxpstr.h"
#undef WANT_ENUM_STRING_IDS
#include "UDrawingUtils.h"
#include "CMessageView.h"
#include <LTableMonoGeometry.h>
#include <LTableMultiselector.h>
#include "CSwatchBrokerView.h"
#include "CURLDispatcher.h"
#include "cstring.h"
#include "CBrowserContext.h"
#include "uerrmgr.h"
#include "resgui.h"
#include "cstring.h"
#include <LDragTask.h>
#include "macutil.h"
#include "ufilemgr.h"
#include "MoreDesktopMgr.h"
#include "CDeviceLoop.h"
#include "CContextMenuAttachment.h"
#include "CBevelView.h"
#include "macutil.h"
#include "miconutils.h"

const Uint16 kColWidth = 100;
const Uint16 kRowHeight = 50;
const Uint16 kExpandedHeight = 56;

CMessageAttachmentView::CMessageAttachmentView(LStream* inStream):
	Inherited(inStream), LDragAndDrop(GetMacPort(), this),
	mAttachmentList(NULL),
	mMSGPane(NULL),
	mExpandedHeight(kExpandedHeight),
	mNumberAttachments(0),
	mClickCountToOpen(2),
	mAttachmentIconList(nil)
{
	SetUseDragSelect( false );
	mSendDataUPP = NewDragSendDataProc(LDropArea::HandleDragSendData);
	Assert_(mSendDataUPP != NULL);
	SetUpTableHelpers();
	LWindow* window = LWindow::FetchWindowObject( GetMacPort() );
	if( window )
		mBrokeredView = dynamic_cast<CBrokeredView*>(window->FindPaneByID( 'MAtV' ) );
		
} // CMessageAttachmentView::CMessageAttachmentView

CMessageAttachmentView::~CMessageAttachmentView()
{
	if ( mSendDataUPP != NULL )
		DisposeRoutineDescriptor(mSendDataUPP);

	ClearMessageAttachmentView();
} // CMessageAttachmentView::~CMessageAttachmentView()

void CMessageAttachmentView::FinishCreateSelf()
{	
	
} // CMessageAttachmentView::FinishCreateSelf

void CMessageAttachmentView::SetUpTableHelpers()
{
	SetTableGeometry(new LTableMonoGeometry(this, kColWidth, kRowHeight) );
	SetTableSelector(new LTableMultiSelector(this));
} // CMessageAttachmentView::SetUpTableHelpers

void CMessageAttachmentView::SetMessageAttachmentList( MSG_Pane* pane, int32 numberAttachments )
{
	ClearMessageAttachmentView();
	mMSGPane = pane;
	if (numberAttachments <= 0)
	{
		Hide();
		return;
	}
	mNumberAttachments = numberAttachments;
	Assert_( mMSGPane != NULL );
	MWContext* context = MSG_GetContext( mMSGPane );
	mMessageView = dynamic_cast<CMessageView*>( context->fe.newView );
	Assert_( mMessageView != NULL );

	XP_Bool isAllAttachments;
	MSG_GetViewedAttachments( mMSGPane, &mAttachmentList, &isAllAttachments);
	if (mAttachmentList == nil)
	{
		ClearMessageAttachmentView();
		return;
	}
	CalculateRowsColumns();
	mAttachmentIconList = new CAttachmentIcon*[mNumberAttachments];		//careful!

	MSG_AttachmentData* attachment = &mAttachmentList[0];
	
	for (int i = 0; i < mNumberAttachments; i++, attachment++)
	{
		CAttachmentIcon		*attachIcon = NULL;
		
		if (attachment->x_mac_creator && attachment->x_mac_type)
		{
			OSType fileCreator;
			OSType fileType;
		
			// creator and type are 8-byte hex representations...
			sscanf(attachment->x_mac_creator, "%X", &fileCreator);
			sscanf(attachment->x_mac_type, "%X", &fileType);
			
			attachIcon = new CAttachmentIcon(fileCreator, fileType);
		}
		else
		{
			attachIcon = new CAttachmentIcon(attachment->real_type);
		}
		
		mAttachmentIconList[i] = attachIcon;
	}
} // CMessageAttachmentView::SetMessageAttachmentList



void CMessageAttachmentView::ClearMessageAttachmentView()
{
	if (mAttachmentIconList)
	{
		for (int i = 0; i < mNumberAttachments; i++)
		{
			CAttachmentIcon		*thisAttachmentIcon = mAttachmentIconList[i];
			delete thisAttachmentIcon;
		}
		delete mAttachmentIconList;
		mAttachmentIconList = NULL;
	}

	if (mMSGPane && mAttachmentList)
	{
		MSG_FreeAttachmentList( mMSGPane, mAttachmentList);
	}
	mAttachmentList = NULL;
	mMSGPane = NULL;
	mNumberAttachments = 0;
} // CMessageAttachmentView::ClearMessageAttachmentView

void CMessageAttachmentView::OpenSelection( Int32 action)
{
	STableCell selectedCell( 0,0);
	while ( GetNextSelectedCell(selectedCell) )
	{
		Int32 attachmentIndex = CalculateAttachmentIndex (selectedCell ); 
		const char* url = mAttachmentList[ attachmentIndex ].url;
		URL_Struct* theURL = NET_CreateURLStruct( url, NET_DONT_RELOAD);
		ThrowIfNULL_(theURL);
		HandleURL(theURL, action );
 	}
} // CMessageAttachmentView::OpenSelection()

void CMessageAttachmentView::HandleURL( URL_Struct* inURL, int32 action )
{	
	CBrowserContext* browserContext = mMessageView->GetContext();
	if( browserContext )
	{
		cstring theReferer = browserContext->GetCurrentURL();
		if (theReferer.length() > 0)
			inURL->referer = XP_STRDUP(theReferer);
		inURL->window_target = NULL;
		if( action == FO_SAVE_AS )
		{
			CStr31 fileName;
			fe_FileNameFromContext(*browserContext, inURL->address, fileName);
			StandardFileReply reply;
			::StandardPutFile(GetPString(SAVE_AS_RESID), fileName, &reply);
			if (reply.sfGood)
			{
				CURLDispatcher::DispatchToStorage( inURL, reply.sfFile, FO_SAVE_AS, false);
			}
		}
		else
			mMessageView->DispatchURL( inURL, browserContext, false, false, action);
	}
} //  CMessageAttachmentView::HandleURL

void	CMessageAttachmentView::ResizeFrameBy(
								Int16				inWidthDelta,
								Int16				inHeightDelta,
								Boolean				inRefresh)
{
	LTableView::ResizeFrameBy( inWidthDelta, inHeightDelta, inRefresh);
	CalculateRowsColumns();
} // CMessageAttachmentView::ResizeFrameBy

void 	CMessageAttachmentView::ToggleVisibility()
{
	if ( IsVisible() )
		Hide();
	else
		Show();
} // CMessageAttachmentView::ToggleVisibility

void 	CMessageAttachmentView::Hide()
{
	if( mBrokeredView )
	{
		if( mBrokeredView->IsVisible() )
			Remove();
	}
} // CMessageAttachmentView::Hide

void	CMessageAttachmentView::Show()
{
	
		if ( mBrokeredView )
		{
			if( !mBrokeredView->IsVisible() && mNumberAttachments )
			{
				mBrokeredView->Show();
				mBrokeredView->MoveBy( 0, -mExpandedHeight, true );	
				mBrokeredView->ResizeFrameBy(0, mExpandedHeight, true );
				CBevelView::SubPanesChanged(this, true);
				Rect portRect;	
				if ( CalcPortFrameRect( portRect ) ) 
					::EraseRect( &portRect );
				
			}
		}
} // CMessageAttachmentView::Show()

void CMessageAttachmentView::Remove()
{ 
	if ( mBrokeredView )
	{
		if ( mBrokeredView->IsVisible() )
		{
			mBrokeredView->ResizeFrameBy(0, -mExpandedHeight, true);
			mBrokeredView->MoveBy( 0, mExpandedHeight, false );
			mBrokeredView->Hide();		
		}
	}
} // CMessageAttachmentView::Remove

void CMessageAttachmentView::FindCommandStatus(
	CommandT			inCommand,
	Boolean				&outEnabled,
	Boolean				&outUsesMark,
	Char16				&outMark,
	Str255				outName)
{
	switch (inCommand)
	{
		//case cmd_GetInfo:
		//	outEnabled = GetSelectedRowCount() == 1;
		//	break;
		case cmd_SaveAs:
			STableCell dummyCell( 0,0 );
			outEnabled = GetNextSelectedCell( dummyCell);
			break;
		case cmd_SelectAll:
			outEnabled = true;
			break;
		default:
			LCommander::FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
			break;
	}
} // CCMessageAttachmentView::FindCommandStatus


Boolean CMessageAttachmentView::ObeyCommand(
	CommandT	inCommand,
	void		*ioParam)
{
	Boolean	result = false;
	switch(inCommand)
	{
		case cmd_SaveAs:
				OpenSelection( FO_SAVE_AS );
			return true;
		
		case cmd_Open:
			OpenSelection( FO_CACHE_AND_PRESENT );
			return true;
		
		default:
			return mMessageView->ObeyCommand( inCommand, ioParam ); 
	}
} // CMessageAttachmentView::ObeyCommand

void CMessageAttachmentView::HiliteCell(
								const STableCell		&inCell,
								Boolean					/* inHilite */ )

{
	Rect cellRect;
	if ( GetLocalCellRect( inCell, cellRect) )
	{
		FocusDraw();
		DrawCell( inCell, cellRect );
	}
} // CMessageAttachmentView::HiliteCell

void CMessageAttachmentView::HiliteSelection(
								Boolean					/* inActively */,
								Boolean					inHilite )
{
	STableCell	theCell;
	
	while (GetNextSelectedCell(theCell))
	{
		if( !inHilite )
			UnselectCell( theCell );
		HiliteCell( theCell, inHilite );
	}	
}

Boolean	CMessageAttachmentView::ClickSelect(
	const STableCell		&inCell,
	const SMouseDownEvent	&inMouseDown)
{

	// select the cell	
	if (LTableView::ClickSelect(inCell, inMouseDown))
	{
		// Handle it ourselves if the popup attachment doesn't want it.
		CContextMenuAttachment::SExecuteParams params;
		params.inMouseDown = &inMouseDown;
		if (ExecuteAttachments(CContextMenuAttachment::msg_ContextMenu, (void*)&params))
		{
			// drag ? - don't become target
			if ( 	 ::WaitMouseMoved(inMouseDown.macEvent.where))
				DragSelection(inCell, inMouseDown);
			else
			{
				// become target
				if (!IsTarget())
					SwitchTarget(this);
				// open selection if we've got the right click count
				if (GetClickCount() == mClickCountToOpen)
					OpenSelection( FO_CACHE_AND_PRESENT );
			}
		}		
		return true;
	}
	return false;
} // CMessageAttachmentView::ClickSelect



void CMessageAttachmentView::DragSelection(
	const STableCell& 		/*inCell*/, 
	const SMouseDownEvent&	inMouseDown	)
{

	DragReference dragRef = 0;
	
	if (!FocusDraw()) return;
	
	try
	{
		//StRegion	dragRgn;
		OSErr		err;
		
		CBrowserDragTask attachmentDrag( inMouseDown.macEvent );
		
		AddSelectionToDrag( attachmentDrag.GetDragReference() , attachmentDrag.GetDragRegion() );
		err = ::SetDragSendProc( attachmentDrag.GetDragReference() , mSendDataUPP, (LDragAndDrop*)this);
		attachmentDrag.DoDrag();
	}
	catch( ... ) {}
} //CMessageAttachmentView::DragSelection

void CMessageAttachmentView::AddSelectionToDrag(DragReference /* inDragRef */, RgnHandle inDragRgn)
{
	StRegion	tempRgn;
	STableCell	cell;
	Rect		cellRect;
	
	::SetEmptyRgn(inDragRgn);
	
	cell.col = 1;
	cell.row = 0;
	while (GetNextSelectedCell(cell))
	{
		if (GetLocalCellRect(cell, cellRect))
		{
			Int32 	attachmentIndex = CalculateAttachmentIndex( cell );
			
			/*
			::LocalToGlobal(&(topLeft(cellRect)));
			::LocalToGlobal(&(botRight(cellRect)));
			::RectRgn(tempRgn, &cellRect);	
			::UnionRgn(tempRgn, inDragRgn, inDragRgn);
			*/
			CAttachmentIcon		*attachmentIcon = mAttachmentIconList[attachmentIndex];
			
			if (attachmentIcon)
			{
				::SetEmptyRgn(tempRgn);
				
				attachmentIcon->AddIconOutlineToRegion(tempRgn, CAttachmentIcon::kIconSizeLarge);
				
				int16 horizontalOffset = ( cellRect.right - cellRect.left - 32 ) / 2;
				Rect	localRect = cellRect;
				::LocalToGlobal(&(topLeft(cellRect)));
				
				OffsetRgn(tempRgn, horizontalOffset + localRect.left + cellRect.left - localRect.left, localRect.top + (cellRect.top - localRect.top));
				
				::UnionRgn(tempRgn, inDragRgn, inDragRgn);
			}
		}
	}
	
	::CopyRgn(inDragRgn, tempRgn);	
	::InsetRgn(tempRgn, 1, 1);
	::DiffRgn(inDragRgn, tempRgn, inDragRgn);
}

void CMessageAttachmentView::DoDragSendData(FlavorType inFlavor,
								ItemReference inItemRef,
								DragReference inDragRef)
{
	OSErr			theErr;
	cstring			theUrl;
	STableCell selectedCell( 0,0);
	switch( inFlavor )
	{
		case 'TEXT':
			while ( GetNextSelectedCell(selectedCell) )
			{
				Int32 attachmentIndex = CalculateAttachmentIndex (selectedCell ); 
				const char* url = mAttachmentList[ attachmentIndex ].url;
				theErr = ::SetDragItemFlavorData(inDragRef, inItemRef, inFlavor, url, strlen(url), 0);
				ThrowIfOSErr_ (theErr);
			}
			break;
		
		case emBookmarkFileDrag:
			// Get the target drop location
			AEDesc dropLocation;
			
			theErr = ::GetDropLocation(inDragRef, &dropLocation);
			if (theErr != noErr)
				return;
			
				// Get the directory ID and volume reference number from the drop location
			SInt16 	volume;
			SInt32 	directory;
			
			theErr = GetDropLocationDirectory(&dropLocation, &directory, &volume);
		
			// Ok, this is a hack, and here's why:  This flavor type is sent with the FlavorFlag 'flavorSenderTranslated' which
			// means that this send data routine will get called whenever someone accepts this flavor.  The problem is that 
			// it is also called whenever someone calls GetFlavorDataSize().  This routine assumes that the drop location is
			// something HFS related, but it's perfectly valid for something to query the data size, and not be a HFS
			// derrivative (like the text widget for example).
			// So, if the coercion to HFS thingy fails, then we just punt to the textual representation.
			if (theErr == errAECoercionFail)
			{
				theErr = ::SetDragItemFlavorData(inDragRef, inItemRef, inFlavor, theUrl, strlen(theUrl), 0);
				return;
			}

			if (theErr != noErr)
				return;

			// Combine with the unique name to make an FSSpec to the new file
			FSSpec		prototypeFilespec;
			FSSpec		locationSpec;
			prototypeFilespec.vRefNum = volume;
			prototypeFilespec.parID = directory;

			// Save the selection
			while ( GetNextSelectedCell( selectedCell ) )
			{
				MSG_AttachmentData* attachment = &mAttachmentList[ CalculateAttachmentIndex( selectedCell ) ];
				char* fileName =  CFileMgr::MacPathFromUnixPath(attachment->real_name );
				if ( fileName )
				{
					theErr = CFileMgr::UniqueFileSpec( prototypeFilespec, fileName , locationSpec );
					if (theErr && theErr != fnfErr) // need a unique name, so we want fnfErr!
						ThrowIfOSErr_(theErr);
					Int32 attachmentIndex = CalculateAttachmentIndex (selectedCell ); 
					const char* url = mAttachmentList[ attachmentIndex ].url;
					URL_Struct* theURL = NET_CreateURLStruct( url, NET_DONT_RELOAD );
					theErr = ::SetDragItemFlavorData( inDragRef, inItemRef, inFlavor, &locationSpec, sizeof(FSSpec), 0 );
					ThrowIfOSErr_(theErr);
					ThrowIfNULL_(theURL);
					CURLDispatcher::DispatchToStorage( theURL, locationSpec, FO_SAVE_AS, true );
					XP_FREE( fileName );
				}
			}	
		break;
	}
} //CMessageAttachmentView::AddSelectionToDrag


void CMessageAttachmentView::DrawSelf()
{
	// This function is similar to what we had when the "Erase On Update"
	// LWindow attribute was set in Constructor. This flag has been removed
	// because it created a lot of flickers when browsing mails.
	// The other objects in the Thread window continued to behave correctly
	// but the CThreadView showed some update problems. Instead of fixing
	// them as we are supposed to (ie. by invalidating and erasing only what
	// needs to be redrawn), I prefered to emulate the way it used to work
	// when "Erase On Update" was set. My apologies for this easy solution
	// but we have something to ship next week.


	// erase everything
	ApplyForeAndBackColors();
	Rect	frame;
	CalcLocalFrameRect(frame);
	::EraseRect(&frame);

	// redraw everything
	Inherited::DrawSelf();
}// CMessageAttachmentView::DrawSelf()


void CMessageAttachmentView::DrawCell( const STableCell &inCell, const Rect &inLocalRect )
{
	const Int32 kIconSize = 32;
	Int32 attachmentIndex = CalculateAttachmentIndex( inCell );
	if( attachmentIndex < 0 || attachmentIndex >= mNumberAttachments )
		return ; 
	
	MSG_AttachmentData& attachment = mAttachmentList[attachmentIndex];
	
	char* attachmentName = NULL;
	
	if (attachment.real_name != NULL)
		attachmentName = CFileMgr::MacPathFromUnixPath(attachment.real_name );
	else
		attachmentName = XP_STRDUP( XP_GetString(XP_MSG_NONE));
		
	Uint32 nameLength;
	if( attachmentName )
		nameLength= XP_STRLEN(attachmentName);
	
	// file icon
	Rect	iconRect;
	int16 horizontalOffset = ( inLocalRect.right - inLocalRect.left - kIconSize )/2;
	iconRect.left = inLocalRect.left + horizontalOffset;
	iconRect.right = iconRect.left + kIconSize;
	iconRect.top = inLocalRect.top + 2;
	iconRect.bottom = iconRect.top + kIconSize;
	IconTransformType transformType = CellIsSelected(inCell) ? kTransformSelected : kTransformNone;
	
	CAttachmentIcon		*attachmentIcon = mAttachmentIconList[attachmentIndex];
	
	if (attachmentIcon != NULL)
		attachmentIcon->PlotIcon(iconRect, kAlignNone, transformType);
	
	// file name and yes, you can have attachments without names
	if( attachmentName != NULL )
	{
		Rect textRect = inLocalRect;
		textRect.left+=4;
		textRect.top = iconRect.bottom;
		FontInfo fontInfo;
		UTextTraits::SetPortTextTraits( 130 );
		::GetFontInfo(&fontInfo);
		 UGraphicGizmos::PlaceTextInRect(
							attachmentName, nameLength, textRect, teCenter, teCenter, &fontInfo, true, truncMiddle);	
		XP_FREE( attachmentName );
	}
} // CMessageAttachmentView::DrawCell

void	CMessageAttachmentView::CalculateRowsColumns()
{
	SDimension16 frameSize;
	GetFrameSize(frameSize);
	if( mNumberAttachments <= 0 )
		return;
	int16 numCols = frameSize.width / kColWidth;
	if ( numCols == 0 )
		numCols = 1; // if the only cell is a fractionally visible cell, use it
	if ( numCols > mNumberAttachments )
		numCols = mNumberAttachments;
	
	int16 numRows = mNumberAttachments / numCols;
	if ( mNumberAttachments % numCols )
		numRows ++;
	// Remove old table geometry
	RemoveRows( mRows,0, false );
	RemoveCols (mCols, 0, false );
	// Insert the new geometry
	InsertCols( numCols, 1, NULL, 0, false);
	InsertRows( numRows, 1, NULL, 0, true );
} //  CMessageAttachmentView::CalculateRowsColumns()

Int32	CMessageAttachmentView::CalculateAttachmentIndex( STableCell inCell)
{
	Int32 attachmentIndex = ( (inCell.row-1)*(mCols) + inCell.col -1 ); // Attachment Lists are zero based
	Assert_( attachmentIndex >= 0  ); 
	return attachmentIndex;
} // CMessageAttachmentView::CalculateAttachmentIndex
