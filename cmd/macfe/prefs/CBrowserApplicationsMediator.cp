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
#include "CBrowserApplicationsMediator.h"
#include "LTableView.h"
#include <LTableSingleSelector.h>
#include <LTableMonoGeometry.h>
#include <UModalDialogs.h>
#include <LArrayIterator.h>

#include "UGraphicGizmos.h"
#include "LGADialogBox.h"
#include "LGACheckbox.h"
#include "LGARadioButton.h"
#include "CLargeEditField.h"

#include "umimemap.h"
#include "macutil.h"
#include "resgui.h"
#include "prefAPI.h"
#include "macgui.h"
#include "np.h"
#include "uerrmgr.h"
#include "ufilemgr.h"
#include "miconutils.h"
#include "InternetConfig.h"

enum
{
	eNormalTextTraits = 12003,
	eGrayTextTraits = 12008,
	eDescriptionField = 12507,
	eMIMETypeField,
	eSuffixes,
	eCommunicatorRButton,
	ePluginRButton,
	ePluginPopupMenu,
	eApplicationRButton,
	eApplicationNameCaption,
	eChooseApplicationButton,
	eFileTypePopupMenu,
	eSaveRButton,
	eUnknownRButton,
	eResourceCheckbox,
	eOutgoingCheckbox
};

#pragma mark --CMimeCell--
//------------------------------------------------------------------------------
//	•	CMimeCell
//------------------------------------------------------------------------------

class CMimeCell
{
public:
	CMimeCell(): mMapper(nil) ,mIcon(nil) { };
	~CMimeCell();
	CMimeMapper*	mMapper;
	Handle			mIcon;
	
};

//------------------------------------------------------------------------------
//	•	~CMimeCell
//------------------------------------------------------------------------------
// Frees the mapper and icon
//
CMimeCell::~CMimeCell() 
{
		delete mMapper;
		if (mIcon)
			::DisposeIconSuite(mIcon, true);
}

#pragma mark --CMimeTableView--
//------------------------------------------------------------------------------
//	•	CMimeTableView
//------------------------------------------------------------------------------
class CMimeTableView : public LTableView,  public LBroadcaster
{
private:
	typedef LTableView Inherited;
public:
	enum
	{
		class_ID = 'MmLP',
		msg_SelectionChanged = 'Sel∆',	// ioparam = this
		msg_DoubleClick = '2Clk'		// ioparam = this
	};
	
					CMimeTableView( LStream *inStream);
	virtual			~CMimeTableView();		
	virtual void	SelectionChanged();
			void	GetCellData( const STableCell &inCell, void *outDataPtr, Uint32 &ioDataSize) const;
			void 	AddEntry ( CMimeCell** cellToAdd );
	virtual	void 	RemoveRows( Uint32 inHowMany, TableIndexT inFromRow, Boolean inRefresh = true);
			Boolean MimeTypeExists(CMimeMapper* mapper);

protected:
	virtual void	ClickCell( const STableCell	&inCell, const SMouseDownEvent &inMouseDown);
	virtual	void 	DrawCell( const STableCell& inCell , const Rect& inLocalRect );
	virtual	void 	FinishCreateSelf();
public:
	LArray	mMimeData;
	
};


// ---------------------------------------------------------------------------
//		• CMimeTableView(LStream*)
// ---------------------------------------------------------------------------
//	Construct from data in a Stream
//
CMimeTableView::CMimeTableView(
	LStream		*inStream)
		: LTableView(inStream)
{	
}

// ---------------------------------------------------------------------------
//		• ~CMimeTableView
// ---------------------------------------------------------------------------
//	Destructor
//
CMimeTableView::~CMimeTableView()
{	
	LArrayIterator iterator(mMimeData, LArrayIterator::from_Start);
	CMimeCell* currentCell;
	while (iterator.Next(&currentCell))
	{
		delete currentCell;	
	}		
}

// ---------------------------------------------------------------------------
//		• FinishCreateSelf
// ---------------------------------------------------------------------------
//	Sets up Table geometry and selector then loads the table with the current Mime
//	types.
//
void	CMimeTableView::FinishCreateSelf()
{
	// Initialize Table
	const Int32 colWidth = 321;
	const Int32 rowHeight =	20;
	mRows = 0;
	mCols = 0;
	mTableStorage = nil;
	SetTableGeometry(new LTableMonoGeometry(this, colWidth, rowHeight));
	SetTableSelector(new LTableSingleSelector(this));;
	LTableView::InsertCols(1, 0, nil, 0, Refresh_No);
	mUseDragSelect = false;
	mCustomHilite = false;
	mDeferAdjustment = false;
	mRefreshAllWhenResized = false;
	
	// Load in MimeData
	for (TableIndexT i = 1; i <= CPrefs::sMimeTypes.GetCount(); i++)	// Insert a row for each mime type
	{
		CMimeMapper * mime;
		CPrefs::sMimeTypes.FetchItemAt(i, &mime);
		ThrowIfNil_(mime);
		if (!mime->IsTemporary())
		{
			CMimeMapper * duplicate = new CMimeMapper(*mime);
			InsertRows(1, i);
			CMimeCell* cell = new CMimeCell;
			cell->mMapper = duplicate;
			mMimeData.InsertItemsAt ( 1, i, &cell, sizeof( CMimeCell * ) );
		}
	}
	Refresh();
}

//------------------------------------------------------------------------------
//	•	AddEntry
//------------------------------------------------------------------------------
//	Adds the passed in cell to the end of the table
//
void CMimeTableView::AddEntry ( CMimeCell** cellToAdd )
{
	InsertRows( 1, mRows );
	mMimeData.InsertItemsAt ( 1 , LArray::index_Last,cellToAdd, sizeof (CMimeCell * ));
}

//------------------------------------------------------------------------------
//	•	RemoveRows
//------------------------------------------------------------------------------
//
void CMimeTableView::RemoveRows( Uint32 inHowMany, TableIndexT inFromRow, Boolean /* inRefresh */)
{
	mMimeData.RemoveItemsAt(inHowMany, inFromRow);
	Inherited::RemoveRows(  inHowMany, inFromRow, true );

}
// ---------------------------------------------------------------------------
//		• SelectionChanged
// ---------------------------------------------------------------------------
//	Broadcast message when selected cells change
//
void CMimeTableView::SelectionChanged()
{
	BroadcastMessage( msg_SelectionChanged, (void*) this);
}


// ---------------------------------------------------------------------------
//		• GetCellData
// ---------------------------------------------------------------------------
//	Pass back the data for a particular Cell
//
void CMimeTableView::GetCellData(
	const STableCell	&inCell,
	void				*outDataPtr,
	Uint32				&ioDataSize) const
{
	Assert_( ioDataSize >= sizeof ( CMimeCell* ) );
	ioDataSize = sizeof( CMimeCell* );
	if ( outDataPtr )
		mMimeData.FetchItemAt( inCell.row, outDataPtr);
}

// ---------------------------------------------------------------------------
//		• ClickCell
// ---------------------------------------------------------------------------
//	Broadcast message for a double-click on a cell
//
void CMimeTableView::ClickCell(
	const STableCell&		/* inCell */,
	const SMouseDownEvent&	/* inMouseDown */)
{
	if (GetClickCount() == 2)
		BroadcastMessage(msg_DoubleClick, (void*) this);
}


// ---------------------------------------------------------------------------
//		• DrawCell
// ---------------------------------------------------------------------------
//	Draw the contents of the specified Cell
//
void CMimeTableView::DrawCell(
	const STableCell&	 inCell ,
	const Rect&			 inLocalRect )
{
// Some constants used for drawing
	const uint32 offsetTable	=	7;
	const uint32 offsetTextTop	=	15;
	const uint32 offsetMimeType	=	(offsetTable + 0);
	const uint32 offsetIcon		=	(offsetTable + 166);
	const uint32 offsetHandler	=	(offsetIcon + 24);
	const uint32 widthMimeType	=	(offsetIcon - offsetMimeType - 5);
	Rect cellFrame = inLocalRect;
	
	::PenPat( &(UQDGlobals::GetQDGlobals()->gray) );
	::MoveTo(cellFrame.left, cellFrame.bottom-1);
	::LineTo(cellFrame.right, cellFrame.bottom-1);
	::PenPat( &(UQDGlobals::GetQDGlobals()->black) );
	UTextTraits::SetPortTextTraits(10000);	// HACK
	FontInfo			textFontInfo;
	::GetFontInfo(	&textFontInfo);
	
	CMimeCell* cellInfo = nil;
	
	mMimeData.FetchItemAt( inCell.row, &cellInfo );
	CMimeMapper* mapper = cellInfo->mMapper;
	
	
	
	// Draw mime type
	if (cellInfo->mMapper->IsLocked())
			UTextTraits::SetPortTextTraits(eGrayTextTraits);
	CStr255 description = mapper->GetDescription();
	if (description.IsEmpty())
		description = mapper->GetMimeName();
	
	Rect textRect = cellFrame;
	textRect.left += offsetTable;
	textRect.right = widthMimeType;
	textRect.top += 2;
	textRect.bottom -= 2 ;
	UGraphicGizmos::PlaceTextInRect( 
		(char *)description, description.Length(), 
		textRect, teFlushLeft, teJustCenter, &textFontInfo,true, smTruncMiddle );	
	
	
		
	// Draw the icon
	CMimeMapper::LoadAction loadAction = mapper->GetLoadAction();
	
	/*if (	loadAction == CMimeMapper::Launch || 
			loadAction == CMimeMapper::Plugin || 
			loadAction == CMimeMapper::Internal ) */
	{
		Rect r;
		r.left = offsetIcon;
		r.top = cellFrame.top + 2;
		r.right = r.left + 16;
		r.bottom = r.top + 16;
		
		// Only create the icon when needed. 
		if( cellInfo->mIcon == nil )
		{
			OSType appSig = mapper->GetAppSig();
			OSType docSig = 'APPL';
			
			if( loadAction == CMimeMapper::Save )
				docSig = mapper->GetDocType();
			
			if( loadAction == CMimeMapper::Internal  )
				appSig = 'MOSS';
			
			CIconUtils::GetDesktopIconSuite( appSig , docSig, kSmallIcon, &cellInfo->mIcon );
		}	
		
		if ( cellInfo->mIcon )
			::PlotIconSuite(&r, atHorizontalCenter, ttNone, cellInfo->mIcon);
		
	}
		
	// Draw the handler name

	CStr255 handlerName;
	switch (loadAction)
	{
		case CMimeMapper::Save:
			handlerName = *GetString(SAVE_RESID);
			break;
		case CMimeMapper::Unknown:
			handlerName = *GetString(UNKNOWN_RESID);
			break;
		case CMimeMapper::Internal:
			handlerName = *GetString(INTERNAL_RESID);
			break;
		case CMimeMapper::Launch:
			handlerName = mapper->GetAppName();
			break;
		case CMimeMapper::Plugin:
			handlerName = mapper->GetPluginName();
			break;
	}

	textRect.left = offsetHandler;
	textRect.right = inLocalRect.right;
	if (cellInfo->mMapper->IsLocked())
			UTextTraits::SetPortTextTraits(eGrayTextTraits);
	UGraphicGizmos::PlaceTextInRect( 
		(char *) handlerName, handlerName.Length(), 
		textRect,  teFlushLeft, teJustCenter, &textFontInfo,true, smTruncEnd );	
}


// ---------------------------------------------------------------------------
//		• MimeTypeExists
// ---------------------------------------------------------------------------
//
Boolean CMimeTableView::MimeTypeExists(CMimeMapper* mapper)
{
	STableCell	cell;
	cell.col = 1;
	
	CMimeCell* cellInfo = nil;
	
	for (int i=1; i<= mRows; i++)
	{
		cell.row = i;
		mMimeData.FetchItemAt( cell.row, &cellInfo );
		if (cellInfo->mMapper != mapper &&
			cellInfo->mMapper->GetMimeName() == mapper->GetMimeName())
			return true;
	}
	return false;
}


enum
{
	eEditTypeDialogResID = 12008,
	eHelperScroller = 12501,
	eHelperTable,
	eHelperNewButton,
	eHelperEditButton,
	eHelperDeleteButton,
	eDownloadFilePicker
};




#pragma mark ---CEditMIMEWindow---
// ---------------------------------------------------------------------------
//		• CEditMIMEWindow
// ---------------------------------------------------------------------------
//	

#define msg_LaunchRadio		300		// Launch option changed
#define msg_BrowseApp		301		// Pick a new application
#define msg_FileTypePopup 	302		// New file type picked
//msg_EditField						// User typed in a field
#define msg_NewMimeType		303		// New Mime type
#define msg_NewMimeTypeOK	305		// Sent by newMimeType dialog window
//#define msg_ClearCell		306
#define msg_EditMimeType	307		// Edit Mime type
#define msg_DeleteMimeType	308		// Delete Mime type
#define msg_PluginPopup		309		// Pick a plug-in


class CEditMIMEWindow : public LGADialogBox
{
public:
	enum { class_ID = 'EMMW' };
	
								CEditMIMEWindow(LStream* inStream);
	virtual 					~CEditMIMEWindow();

	virtual void				ListenToMessage(MessageT inMessage, void* ioParam);
			
			void				SetCellInfo(CMimeCell *cell);
			
			Boolean				Modified()							{ return mModified;   }

protected:
	virtual void				FinishCreateSelf();
	
private:
			void				UpdateMapperToUI();
			void				UpdateUIToMapper();
			void				SyncTextControls();
			void				UpdateRadioUItoMapper();
			void				SyncInternalControl();
			void				SyncApplicationControls(Boolean mimeTypeChanged);
			void				SyncPluginControls(Boolean mimeTypeChanged);
			void				BuildPluginMenu();
			void				BuildPluginList();
			void				DeletePluginList();
			void				BuildFileTypeMenu();
			CMimeMapper::LoadAction GetLoadAction();

			Boolean				mInitialized;		// Initialized with fCellInfo yet?
			Boolean				mModified;			// Have any MIMEs been modified?
		
			
			CMimeCell*					mCellInfo;			// Data for selected item
			char**				mPluginList;		// Null-terminated array of plug-in names
			uint32				mPluginCount;		// Number of plug-ins in array

			LGAPopup			*mFileTypePopup;
			LGAPopup			*mPluginPopup;
			CLargeEditField	*mDescriptionEditField;
			CLargeEditField	*mTypeEditField;
			CLargeEditField	*mExtensionEditField;
			LGARadioButton		*mRadioSave;
			LGARadioButton		*mRadioLaunch;
			LGARadioButton		*mRadioInternal;
			LGARadioButton		*mRadioUnknown;
			LGARadioButton		*mRadioPlugin;
			LGACheckbox			*mSendResouceFork;
			LGACheckbox			*mOutgoingCheckbox;
//			CFilePicker			*mAppPicker;
			LCaption			*mAppName;
			LButton				*mAppButton;
//			LCaption			*mAppMenuLabel;
			CStr255 			mAppStr;
			OSType				mAppSig;
			Boolean				mLocked;
}; // class CEditMIMEWindow

//-----------------------------------
CEditMIMEWindow::CEditMIMEWindow(LStream* inStream):
//-----------------------------------
	LGADialogBox(inStream),
	mModified(false),
	mInitialized(false),
	mPluginList(nil),
	mPluginCount(0),
	mLocked( false )
{
}
 
 
//-----------------------------------
CEditMIMEWindow::~CEditMIMEWindow()
//-----------------------------------
{
	DeletePluginList();
}


// ---------------------------------------------------------------------------
// CEditMIMEWindow::FinishCreateSelf:
// Fiddle around with a few of our subviews once they've been created.
// ---------------------------------------------------------------------------
void CEditMIMEWindow::FinishCreateSelf()
{
	// Cache pointers to all the controls
	mFileTypePopup = (LGAPopup *)FindPaneByID(eFileTypePopupMenu);
	XP_ASSERT(mFileTypePopup);

	mPluginPopup = (LGAPopup *)FindPaneByID(ePluginPopupMenu);
	XP_ASSERT(mPluginPopup);

	mDescriptionEditField = (CLargeEditField *)FindPaneByID(eDescriptionField);
	XP_ASSERT(mDescriptionEditField);

	mTypeEditField = (CLargeEditField *)FindPaneByID(eMIMETypeField);
	XP_ASSERT(mTypeEditField);

	mExtensionEditField = (CLargeEditField *)FindPaneByID(eSuffixes);
	XP_ASSERT(mExtensionEditField);

	mRadioSave = (LGARadioButton *)FindPaneByID(eSaveRButton);
	XP_ASSERT(mRadioSave);

	mRadioLaunch = (LGARadioButton *)FindPaneByID(eApplicationRButton);
	XP_ASSERT(mRadioLaunch);

	mRadioInternal = (LGARadioButton *)FindPaneByID(eCommunicatorRButton);
	XP_ASSERT(mRadioInternal);

	mRadioUnknown = (LGARadioButton *)FindPaneByID(eUnknownRButton);
	XP_ASSERT(mRadioUnknown);

	mRadioPlugin = (LGARadioButton *)FindPaneByID(ePluginRButton);
	XP_ASSERT(mRadioPlugin);

//	mAppPicker = (CFilePicker *)FindPaneByID(eApplicationFilePicker);
//	XP_ASSERT(mAppPicker);

	mAppName = (LCaption *)FindPaneByID(eApplicationNameCaption);		
	XP_ASSERT(mAppName);

	mAppButton = (LButton *)FindPaneByID(eChooseApplicationButton); 
	XP_ASSERT(mAppButton);

	mSendResouceFork = (LGACheckbox* ) ( FindPaneByID(eResourceCheckbox) );
	mOutgoingCheckbox = (LGACheckbox* ) ( FindPaneByID(eOutgoingCheckbox) );
	XP_ASSERT( mSendResouceFork );
//	mAppMenuLabel = (LCaption *)FindPaneByID(pref8AppMenuLabel);
//	XP_ASSERT(mAppMenuLabel);

	// Text fields cannot become broadcasters automatically because
	// LinkListenerToControls expects fields to be descendants of LControl
	// C++ vtable gets fucked up
	mDescriptionEditField->AddListener(this);
	mTypeEditField->AddListener(this);
	mExtensionEditField->AddListener(this);

	LGADialogBox::FinishCreateSelf();
}


// ---------------------------------------------------------------------------
// CEditMIMEWindow::SetCellInfo:
// After the CPrefHelpersContain creates us it calls this method to provide
// us with the info for the selected cell, which we copy.  We can then 
// synchronize our controls to the data they display.
// ---------------------------------------------------------------------------
void CEditMIMEWindow::SetCellInfo(CMimeCell* cellInfo)
{
	mCellInfo = cellInfo;
	UpdateUIToMapper();
	mInitialized = true;
}


// ---------------------------------------------------------------------------
// CEditMIMEWindow::DeletePluginList:
// Delete the list of plug-ins.
// ---------------------------------------------------------------------------
void CEditMIMEWindow::DeletePluginList()
{
	if (mPluginList)
	{	
		uint32 index = 0;
		
		while (mPluginList[index++])
		{
			free(mPluginList[index]);
		}
		free(mPluginList);
		mPluginList = nil;
	}
}


// ---------------------------------------------------------------------------
// CEditMIMEWindow::BuildPluginList:
// Build a list of plug-ins for this MIME type.
// ---------------------------------------------------------------------------
void CEditMIMEWindow::BuildPluginList()
{
	// Delete the old list
	DeletePluginList();
	
	// Get the new list from XP code
	mPluginList = NPL_FindPluginsForType(mCellInfo->mMapper->GetMimeName());

	// Count how many are in the list
	mPluginCount = 0;
	if (mPluginList)
	{
		while (mPluginList[mPluginCount])
			mPluginCount++;
	}
}


// ---------------------------------------------------------------------------
// CEditMIMEWindow::BuildPluginMenu:
// Build the plug-in menu from the list of plug-ins available for the current
// MIME type.  We need to redraw the popup if the plug-in list used to be non-
// empty or is non-empty now.  (It's too expensive to try to detect the case
// of the plug-in list getting rebuilt but still containing the same items.)
// ---------------------------------------------------------------------------
void CEditMIMEWindow::BuildPluginMenu()
{
	uint32 oldCount = mPluginCount;
	
	BuildPluginList();
	
	if (oldCount || mPluginCount)
	{
		SetMenuSizeForLGAPopup(mPluginPopup, mPluginCount);

		MenuHandle menuH = mPluginPopup->GetMacMenuH();
		uint32 index = 0;
		uint32 desiredValue = 1;		// Default desired value is first item
		while (mPluginList[index])
		{
			SetMenuItemText(menuH, index+1, CStr255(mPluginList[index]));
			::EnableItem(menuH, index+1);
			if (mCellInfo->mMapper->GetPluginName() == mPluginList[index])
			{
				desiredValue = index + 1;
			}
			index++;
		}
		
		//
		// If the previously-selected plug-in name is in this list,
		// select it; otherwise just select the first item.
		// If we didn't change the control value, make sure it
		// redraws since the contents of the list have changed.
		//
		uint32 previousValue = mPluginPopup->GetValue();
		if (desiredValue != previousValue)
		{
			mPluginPopup->SetValue(desiredValue);
		}
		else
		{
			mPluginPopup->Refresh();
		}
	}
}

//struct BNDLIds
//{	// Utility structure for bundle parsing
//	Int16 localID;
//	Int16 resID;
//};

// ---------------------------------------------------------------------------
// CEditMIMEWindow::BuildFileTypeMenu:
// Build the file-type menu from the list file types in the application info.
// ---------------------------------------------------------------------------
void CEditMIMEWindow::BuildFileTypeMenu()
{

	CApplicationIconInfo*	newInfo = NULL;
	short					refNum;
	Handle					bundle = NULL;
	OSErr					err;
	Handle					appIcon = NULL;
	Boolean					handlesAE = TRUE;
	
	BNDLIds*				iconOffset;
	BNDLIds*				frefOffset;
	short					numOfIcons;
	short					numOfFrefs;	// Really number-1, just like in the resource
	BNDLIds					frefBNDL;
	Handle					iconFamily;
	OSType					iconType;
	Handle					frefRes = NULL;
	
	Try_
	{
		SetResLoad( FALSE );
		FSSpec						appSpec;
		OSErr err = CFileMgr::FindApplication( mAppSig, appSpec );
		refNum = ::FSpOpenResFile( &appSpec,fsRdPerm );
		err = ResError();
		SetResLoad( TRUE );
		ThrowIfOSErr_( err );

		// BNDL
		bundle = ::Get1IndResource( 'BNDL' , 1 );
		ThrowIfNil_( bundle );
		HLock( bundle );
		HNoPurge( bundle );
		// Get the signature
		::BlockMoveData( *bundle, &mAppSig, 4 );

		
		OSType		resType;
		Ptr			bundlePtr = NULL;
		
		bundlePtr = *bundle;
		
		::BlockMoveData( bundlePtr + 8, &resType, 4 );		// Get the first resource type
		if ( resType == 'ICN#' )
		{
			::BlockMoveData( bundlePtr + 12, &numOfIcons, 2 );
			iconOffset = (BNDLIds*)( bundlePtr + 14 );
			//::BlockMove( (Ptr)iconOffset + ( numOfIcons + 1) * sizeof( BNDLIds ), &resType, 4 );	// Just checkin'
			::BlockMoveData( (Ptr)iconOffset + (numOfIcons + 1) * sizeof( BNDLIds ) + 4, &numOfFrefs, 2 );
			frefOffset = (BNDLIds*)( (Ptr)iconOffset + 6 + ( numOfIcons + 1 ) * sizeof( BNDLIds ) );
		}
		else	// FREF
		{
			::BlockMoveData( bundlePtr + 12, &numOfFrefs, 2 );
			frefOffset = (BNDLIds*) (bundlePtr + 14 );
			//::BlockMove( (Ptr)frefOffset + ( numOfFrefs + 1 ) * sizeof( BNDLIds ), &resType, 4 );	// Just checkin'
			::BlockMoveData( (Ptr)frefOffset + ( numOfFrefs + 1 ) * sizeof( BNDLIds ) + 4, &numOfIcons, 2 );
			iconOffset = (BNDLIds*)( (Ptr)frefOffset + 6 + (numOfFrefs + 1) * sizeof(BNDLIds) );			
		}
		// We have the offsets for FREF and ICN# resources
		// Not every FREF has a matching icon, so we read in all 
		// the FREFs and try to find the matching icon for each


		MenuHandle menuH = mFileTypePopup->GetMacMenuH();
		Int16 numMenuItems = ::CountMItems(menuH);
		while ( numMenuItems > 0 )
		{
			::DeleteMenuItem(menuH, numMenuItems);
			numMenuItems--;
		}
		
		
		for ( int i = 0; i <= numOfFrefs; i++ )
		{
			// Algorithm:
			// Get the FREF resource specified in BNDL
			// Find the ICN# resource from BNDL with same local ID
			// Get the icon specified in ICN# resource.
			// If getting of the icon fails in any case, use the default icon
			frefBNDL = frefOffset[i];
			iconFamily = NULL;
			iconType = 'DUMB';
			frefRes = ::Get1Resource( 'FREF', frefBNDL.resID );
			if ( frefRes == NULL )	// Ignore the record  if FREF resource is not found
				continue;
			::BlockMoveData( *frefRes, &iconType, 4 );
			::ReleaseResource( frefRes );
			
			if ( ( iconType == 'fold' ) || 		// folders are not files
				 ( iconType == '****' ) ||		// any file will default to text later
				 ( iconType == 'pref' ) ||		// prefs are also ignored
				 ( iconType == 'PREF' ) )	
				continue;
		
			if ( iconType == 'APPL' )
			{
				//  Don't do anything
			}
			else
			{							// Document icons
				::AppendMenu(menuH, CStr255(iconType));
				Int16 numMenuItems = ::CountMItems(menuH);
				SetMenuItemText(menuH, numMenuItems, CStr255(iconType));
			
			}
		}	// Done parsing all the file types
		
		HUnlock( bundle );
		HPurge( bundle );
		ReleaseResource( bundle );
		
		// Error handling: no file types
		numMenuItems = ::CountMItems(menuH);
		if ( numMenuItems == 0 )	// No icons were read, add 1 dummy one
		{
			::AppendMenu(menuH, CStr255(iconType));
			
			SetMenuItemText(menuH, numMenuItems +1 , CStr255('TEXT'));
		}
	}
	Catch_(inErr)
	{
		
	}
	EndCatch_
	mFileTypePopup->SetPopupMinMaxValues();
	CloseResFile( refNum );
	err = ResError();
}



// ---------------------------------------------------------------------------
// CEditMIMEWindow::SyncTextControls:
// Sync the edit text fields with their actual values (only called when
// the window is first created).
// ---------------------------------------------------------------------------
void CEditMIMEWindow::SyncTextControls()
{
	XP_ASSERT(mCellInfo->mMapper != nil);

	// MIME type
	mAppStr = mCellInfo->mMapper->GetAppName();
	mDescriptionEditField->SetDescriptor(mCellInfo->mMapper->GetDescription()  );

	// MIME type
	mTypeEditField->SetDescriptor(mCellInfo->mMapper->GetMimeName());
	
	// Extensions
	mExtensionEditField->SetDescriptor(mCellInfo->mMapper->GetExtensions());
}


// ---------------------------------------------------------------------------
// CEditMIMEWindow::SyncRadioControlValues:
// Sync the radio buttons with their actual values (only called when
// the window is first created).
// ---------------------------------------------------------------------------
void CEditMIMEWindow::UpdateRadioUItoMapper()
{
	switch (mCellInfo->mMapper->GetLoadAction() )
	{
		case CMimeMapper::Save:
				mRadioSave->SetValue(1);
			break;
		case CMimeMapper::Launch:	
				mRadioLaunch->SetValue(1);
			break;
		case CMimeMapper::Internal:
				mRadioInternal->SetValue(1);
			break;
		case CMimeMapper::Unknown:
				mRadioUnknown->SetValue(1);
			break;
		case CMimeMapper::Plugin:	
				mRadioPlugin->SetValue(1);
			break;
	}
}


// ---------------------------------------------------------------------------
// CEditMIMEWindow::SyncInternalControl:
// Sync up the "Use Netscape" radio button when the MIME type changes.
// ---------------------------------------------------------------------------
void CEditMIMEWindow::SyncInternalControl()
{
	CStr255 newText;
	mTypeEditField->GetDescriptor( newText);
	if (CMimeMapper::NetscapeCanHandle( newText))
	{
		if( !mLocked )
			mRadioInternal->Enable();
	}
	else
	{
		if (mRadioInternal->GetValue() == 1)
		{
			mRadioUnknown->SetValue(1);
		}
		mRadioInternal->Disable();
	}
}


// ---------------------------------------------------------------------------
// CEditMIMEWindow::SyncApplicationControls:
// Sync up the application-related controls when the "Use application" radio
// button changes state, etc.
// ---------------------------------------------------------------------------
void CEditMIMEWindow::SyncApplicationControls(Boolean applicationChanged)
{
	if (applicationChanged)
	{
		BuildFileTypeMenu();
	}
	
	Boolean enableAppControls = ( GetLoadAction() == CMimeMapper::Launch) && !mLocked;

	//
	// Application name
	//
	ResIDT oldTraits = mAppName->GetTextTraitsID();
	if (enableAppControls)
	{
		mAppName->SetTextTraitsID(eNormalTextTraits);			// Normal
		mAppButton->Enable();
	}
	else	
	{
		mAppName->SetTextTraitsID(eGrayTextTraits);			// Dimmed
		mAppButton->Disable();
	}
	if (applicationChanged)
	{
		mAppName->SetDescriptor( mAppStr);
	}
	
	else if (oldTraits != mAppName->GetTextTraitsID())
	{
		mAppName->Refresh();
	}
	
	//
	// Popup enabled state
	//
	MenuHandle menuH = mFileTypePopup->GetMacMenuH();
	Int16 numMenuItems = ::CountMItems(menuH);
	if (enableAppControls  && numMenuItems > 0 )
	{
		mFileTypePopup->Enable();
	}
	else
	{
		mFileTypePopup->Disable();
	}

	//
	// File type
	//
	if (mRadioLaunch->GetValue() == 1)
	{
	#if 0
		uint32 item = mFileTypePopup->GetValue();
		XP_ASSERT(item > 0);
		CFileType* fileType = mCellInfo->mIconInfo->GetFileType(item);
		if (fileType)
		{
			mCellInfo->mMapper->SetDocType(fileType->fIconSig);
		}
	#endif
	}
}


// ---------------------------------------------------------------------------
// CEditMIMEWindow::SyncPluginControls:
// Sync up the plugin-related controls when the "Use plugin" radio
// button changes state or the MIME type changes.
// ---------------------------------------------------------------------------
void CEditMIMEWindow::SyncPluginControls(Boolean mimeTypeChanged)
{
	if (mimeTypeChanged)
	{
		BuildPluginMenu();
	}
	//
	// Plug-in radio button enabled state
	//
	if (mPluginCount > 0)
	{
		mRadioPlugin->Enable();
	}
	else
	{
		mRadioPlugin->Disable();
		if (mRadioPlugin->GetValue() == 1)
		{
			mRadioUnknown->SetValue(1);
		}
	}

	//
	// Plug-in popup control enabled state
	//

	if (GetLoadAction() == CMimeMapper::Plugin && mPluginCount > 0)
	{
		mPluginPopup->Enable();
	}
	else
	{
		mPluginPopup->Disable();
	}	
	
}


CMimeMapper::LoadAction CEditMIMEWindow::GetLoadAction()
{
	if (mRadioSave->GetValue() ) 		return CMimeMapper::Save ;
	if ( mRadioLaunch->GetValue() )		return CMimeMapper::Launch;
	if ( mRadioInternal->GetValue() )	return CMimeMapper::Internal;
	if ( mRadioUnknown->GetValue() )	return CMimeMapper::Unknown;
	if (mRadioPlugin ->GetValue() )		return CMimeMapper::Plugin;
	return CMimeMapper::Unknown;
}

//-----------------------------------
void CEditMIMEWindow::ListenToMessage(MessageT inMessage, void *ioParam)
// Process a message from the MIME types dialog
//-----------------------------------
{
	if (!mInitialized)
	{
		return;
	}	
	if ( ioParam == mRadioLaunch)
		SyncApplicationControls(false);
	if ( ioParam == mRadioPlugin )
		SyncPluginControls(false);
	switch (inMessage)
	{
		// Pick a handler
		case eSaveRButton:
		case eApplicationRButton:	
		case eCommunicatorRButton:
		case eUnknownRButton:
		case ePluginRButton:
		case eResourceCheckbox:
		case eOutgoingCheckbox:
			mModified = TRUE;
			break;
		
		// Pick a new application
		case eChooseApplicationButton:	
		{		
			StandardFileReply reply;
			CFilePicker::DoCustomGetFile(reply, CFilePicker::Applications, FALSE);	// Pick app
			if (reply.sfGood)
			{
				// set mappers properties from the app
				FInfo finderInfo;
				OSErr err = FSpGetFInfo(&reply.sfFile, &finderInfo);	
				mAppSig = finderInfo.fdCreator;
				mAppStr = reply.sfFile.name;
				SyncApplicationControls(true);		// App changed
				mModified = TRUE;
			}
			break;
		}

		// Change the file type
		case eFileTypePopupMenu:	
		{
			SyncApplicationControls(false);
			mModified = TRUE;		
			break;
		}

		// Change the plug-in
		case ePluginPopupMenu:
		{
			SyncPluginControls(false);
			mModified = TRUE;		
			break;
		}

		// Edit some text
		case msg_EditField2:
		{
			CStr255 newText;

			mTypeEditField->GetDescriptor(newText);
			if ( ioParam == mTypeEditField )
			{
				SyncPluginControls(true);		// Plug-in choices depend on MIME type
				SyncInternalControl();			// "Use Netscape" radio depends on MIME type
			}

			mModified = TRUE;
			break;
		}

		case msg_OK:
			UpdateMapperToUI();
			break;
		case msg_Cancel:
			break;
		default:
			break;
	}	
}

//------------------------------------------------------------------------------
//	•	UpdateUIToMapper
//------------------------------------------------------------------------------
// Set up the controls in the dialog box (before it's visible) with the
// information from the currently selected cell. Note that the order of these
//	unction calls is important
//
void CEditMIMEWindow::UpdateUIToMapper()
{
	
	mAppSig = mCellInfo->mMapper->GetAppSig();
	UpdateRadioUItoMapper();
	SyncTextControls();
	SyncInternalControl();
	SyncApplicationControls(true);
	SyncPluginControls(true);
	Int32 fileFlags = mCellInfo->mMapper->GetFileFlags();
	Boolean flag =  ( fileFlags & ICmap_resource_fork_mask )  > 0; 
	mSendResouceFork->SetValue(flag );
	flag = (!(fileFlags & ICmap_not_outgoing_mask) )> 0;
	mOutgoingCheckbox->SetValue( flag );
	mLocked = PREF_PrefIsLocked( CPrefs::Concat(mCellInfo->mMapper->GetBasePref(), ".mimetype") );
	if( mLocked )
	{	
		for( PaneIDT i = eDescriptionField ; i<= eOutgoingCheckbox ; i++ )
		{
			LPane* pane = FindPaneByID( i );
			if( pane )
				pane->Disable();
		}
		// Disable OK
		LPane* view = FindPaneByID( 900 );
		view->Disable();
	}
}

//------------------------------------------------------------------------------
//	•	UpdateMapperToUI
//------------------------------------------------------------------------------
//	Update the UI for the original mapper
//	The mediator is reponsible for actually commiting changes to the prefs
//
void CEditMIMEWindow::UpdateMapperToUI()
{
	CStr255 newText;
	// Description
	mDescriptionEditField->GetDescriptor(newText);
	mCellInfo->mMapper->SetDescription(newText);
	
	// MimeType
	mTypeEditField->GetDescriptor(newText);
	mCellInfo->mMapper->SetMimeType(newText);
	
	// Extension Fields
	mExtensionEditField->GetDescriptor(newText);
	mCellInfo->mMapper->SetExtensions(newText);
	
	// Application Name and Signature
	mCellInfo->mMapper->SetAppSig( mAppSig );
	mCellInfo->mMapper->SetAppName ( mAppStr );
	
	// load action
	mCellInfo->mMapper->SetLoadAction( GetLoadAction() );
	
	// Plug-in name
	if (mRadioPlugin->GetValue() == 1)
	{
		uint32 item = mPluginPopup->GetValue();	
		XP_ASSERT(item > 0);	
		if (mPluginList && item > 0 && item <= mPluginCount)
		{
			char* plugin = mPluginList[item - 1];		// Menu is 1-based, list is 0-based
			if (plugin)
				mCellInfo->mMapper->SetPluginName(plugin);
		}
	}
	else
		mCellInfo->mMapper->SetPluginName( "\p" );
	
	// Since the App has changed, get rid of the old icon
	if ( mCellInfo -> mIcon)
	{
		::DisposeIconSuite( mCellInfo->mIcon , true);
		mCellInfo->mIcon = nil;
	}
	Int32	fileFlags = 0;
	if( mSendResouceFork->GetValue( ) )
		fileFlags = fileFlags  | ICmap_resource_fork_mask ;
	if( !mOutgoingCheckbox->GetValue() )
		fileFlags = fileFlags | ICmap_not_outgoing_mask ;
	mCellInfo->mMapper->SetFileFlags( fileFlags );
}
#pragma mark --CBrowserApplicationsMediator--
//------------------------------------------------------------------------------
//	•	RegisterViewClasses
//------------------------------------------------------------------------------
//
void CBrowserApplicationsMediator::RegisterViewClasses()
{
	RegisterClass_( CEditMIMEWindow);
	RegisterClass_( CMimeTableView );
}

//------------------------------------------------------------------------------
//	•	CBrowserApplicationsMediator
//------------------------------------------------------------------------------
//
CBrowserApplicationsMediator::CBrowserApplicationsMediator(LStream*)
:	CPrefsMediator(class_ID)
,	mModified(false)
,	mMIMETable(nil)
{
}

//------------------------------------------------------------------------------
//	•	EditMimeEntry
//------------------------------------------------------------------------------
//	Brings up a property window for the selected Cell or
//	if isNewEntry is true allows the user to add a new mime type
//
void CBrowserApplicationsMediator::EditMimeEntry( Boolean isNewEntry)
{
	StDialogHandler	theHandler(eEditTypeDialogResID, sWindow->GetSuperCommander());
	CEditMIMEWindow* theDialog = (CEditMIMEWindow*)theHandler.GetDialog();
	XP_ASSERT(theDialog);
	UReanimator::LinkListenerToControls(theDialog, theDialog, eEditTypeDialogResID);
	
	// Get the info that the dialog should display
	CMimeCell* cellInfo = nil;
	if( isNewEntry )
	{
		cellInfo = new CMimeCell;
		CStr255 emptyType = "";
		cellInfo->mMapper = CPrefs::CreateDefaultUnknownMapper(emptyType, FALSE);
	}
	else
	{
		STableCell	cellToEdit = mMIMETable->GetFirstSelectedCell();
		uint32 size = sizeof( cellInfo  );
		mMIMETable->GetCellData(cellToEdit, &cellInfo, size );
	}
	
	// Tell Dialog what to edit
	theDialog->SetCellInfo( cellInfo);

	// Let the user have at it...
	theDialog->Show();
	MessageT theMessage = msg_Nothing;	
	while ((theMessage != msg_Cancel) && (theMessage != msg_OK))
	{
		theMessage = theHandler.DoDialog();
		
		if (theMessage == msg_OK &&
			/* cellInfo->mMapper->GetMimeName() != cellInfo->mMapper->GetMimeName()  && */
			 mMIMETable->MimeTypeExists(cellInfo->mMapper) )
		{
			ErrorManager::PlainAlert(mPREFS_DUPLICATE_MIME);
			theMessage = msg_Nothing;
		}
		// The CEditMIMEWindow will handle all other messages
	}
	
	// Process the munged data as appropriate
	if (theMessage == msg_OK && theDialog->Modified())
	{
		mModified = TRUE;									// Remember that something changed
		if( isNewEntry )
			mMIMETable->AddEntry( &cellInfo );
		mMIMETable->Refresh();								// Let table sync to the new data
		
	} 
	else if ( isNewEntry )
	{
			delete cellInfo;	// Throw away changes						
	}
}


void CBrowserApplicationsMediator::DeleteMimeEntry()
{
	if (ErrorManager::PlainConfirm((const char*) GetCString(DELETE_MIMETYPE)))
	{
		XP_ASSERT(mMIMETable);
		STableCell cellToDelete = mMIMETable->GetFirstSelectedCell();
	
		CMimeCell* cellInfo = nil;
		uint32 size = sizeof( CMimeCell * );
		mMIMETable->GetCellData(cellToDelete, &cellInfo, size);
		
		// Instead of freeing the item, add it to a list of deleted mime types
		// and at commit time, delete the items from xp pref storage.
		mDeletedTypes.LArray::InsertItemsAt(1, LArray::index_Last, &cellInfo);
		
		mModified = TRUE;
		
		mMIMETable->RemoveRows(1, cellToDelete.row);

		// We want to select the cell immediately after the deleted one. It just so
		// happens that its coordinates are now (after deleting), what the cell to
		// delete was before. So we just need to select cellToDelete. However, if
		// there is no cell after the deleted cell (it was the last one), then we
		// just select the last one.
		TableIndexT	rows, columns;
		mMIMETable->GetTableSize(rows, columns);
		cellToDelete.row = cellToDelete.row > rows? rows: cellToDelete.row;
		mMIMETable->ScrollCellIntoFrame(cellToDelete);
		mMIMETable->SelectCell(cellToDelete);
		mMIMETable->Refresh();
	}	
}

void CBrowserApplicationsMediator::ListenToMessage(MessageT inMessage, void *ioParam)
{
	switch (inMessage)
	{
		case eHelperDeleteButton:
			DeleteMimeEntry();
			break;
	
		case eHelperNewButton:
			EditMimeEntry( true );
			break;
			
		case eHelperEditButton:
		case CMimeTableView::msg_DoubleClick:
			EditMimeEntry( false );
			break;
	
		case CMimeTableView::msg_SelectionChanged:
			LButton	*deleteButton = (LButton *)FindPaneByID(eHelperDeleteButton);
			XP_ASSERT(deleteButton);
			LButton	*editButton = (LButton *)FindPaneByID(eHelperEditButton);
			XP_ASSERT(editButton);

			XP_ASSERT(ioParam);
			STableCell	cell =  mMIMETable->GetFirstSelectedCell();
			Boolean inactive;
			Boolean locked = false;
			if (cell.row > 0)
			{
				CMimeCell* cellInfo = nil;
				uint32 size = sizeof( CMimeCell * );
				mMIMETable->GetCellData(cell, &cellInfo, size );
				locked = cellInfo->mMapper->IsLocked();
				inactive = CMimeMapper::NetscapeCanHandle(cellInfo->mMapper->GetMimeName());
				editButton->Enable();
			}
			else
			{
				inactive = true;
				editButton->Disable();
			}
			if (inactive)
				deleteButton->Disable();
			else
			{
				if( locked == false )
					deleteButton->Enable();
			}
			break;
		default:
			CPrefsMediator::ListenToMessage(inMessage, ioParam);
			break;
	}
}

void CBrowserApplicationsMediator::LoadMainPane()
{
	CPrefsMediator::LoadMainPane();
	mMIMETable = (CMimeTableView *)FindPaneByID(eHelperTable);
	XP_ASSERT(mMIMETable);
	mMIMETable->AddListener(this);
	STableCell	currentCell(1, 1);		// This has to be done after
	mMIMETable->SelectCell(currentCell);	// mMIMETable->AddListener(); to set the buttons.
}

void CBrowserApplicationsMediator::WritePrefs()
{
	if (mModified)
	{
		CPrefs::sMimeTypes.DeleteAll(FALSE);
		XP_ASSERT(mMIMETable);
		TableIndexT rows, cols;
		mMIMETable->GetTableSize(rows, cols);
		for (int i = 1; i <= rows; i++)
		{
			CMimeCell* cellInfo = nil;
			uint32 size = sizeof( CMimeCell * );
			STableCell cell( i, 1 );
			mMIMETable->GetCellData( cell , &cellInfo, size);
			
			cellInfo->mMapper->WriteMimePrefs();
			CMimeMapper* mapper = new CMimeMapper(*cellInfo->mMapper);
			CPrefs::sMimeTypes.InsertItemsAt(1, LArray::index_Last, &mapper);
		}
		
		for (Int32 i = 1; i <= mDeletedTypes.GetCount(); i++)
		{
			CMimeCell* mapper;
			mDeletedTypes.FetchItemAt(i, &mapper);
			PREF_DeleteBranch(mapper->mMapper->GetBasePref());
			delete mapper;
		}
	}
}

void	CBrowserApplicationsMediator::UpdateFromIC()
{
	CFilePicker* filePicker = dynamic_cast< CFilePicker*>( FindPaneByID( 12506 ) );
	XP_ASSERT( filePicker );
	if( UseIC() )
	{
		FSSpec specFromIC;
		if (!CInternetConfigInterface::GetFileSpec( kICDownloadFolder , specFromIC ) )
		{	
				filePicker->SetFSSpec( specFromIC,false);
				filePicker->Disable();
		}
	}
	else
		filePicker->Enable();
}