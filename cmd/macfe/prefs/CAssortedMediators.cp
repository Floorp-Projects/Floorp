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

#include "CAssortedMediators.h"


extern "C"
{
#include "xp_help.h"
}

#include "prefapi.h"
#ifdef MOZ_MAIL_NEWS
#include "CMailNewsContext.h"
#endif
#include "CBrowserWindow.h"
#include "CBrowserContext.h"
#include "InternetConfig.h"
#ifdef MOZ_MAIL_NEWS
#include "CMessageFolder.h"
#endif
#include "UGraphicGizmos.h"
#include "CToolTipAttachment.h"
#include "CValidEditField.h"

#include "MPreference.h"
#include "PrefControls.h"

//#include "prefwutil.h"
#include "glhist.h"
#include "proto.h"
//#include "meditdlg.h"
#include "macgui.h"
#include "resgui.h"
#include "ufilemgr.h"
#include "uerrmgr.h"
#include "libi18n.h"
#include "abdefn.h"
#include "addrbook.h"

#include "np.h"
#include "uapp.h"

#include "macutil.h"

#include "CSizePopup.h"

#include <Sound.h>
#include <UModalDialogs.h>
#include <UNewTextDrawing.h>
#include <LTextColumn.h>
#include <UDrawingUtils.h>
#include <UGAColorRamp.h>
#include <LControl.h>


#include <LGADialogBox.h>
#include <LGACheckbox.h>
#include <LGARadioButton.h>

//#include <QAP_Assist.h>


//#include <ICAPI.h>
//#include <ICKeys.h>
#include <UCursor.h>
#ifdef MOZ_MAIL_NEWS
#define DEFAULT_NEWS_SERVER_KLUDGE
#ifdef DEFAULT_NEWS_SERVER_KLUDGE
	#include "CMailNewsContext.h"
	#include "CMailProgressWindow.h"
#endif
#endif // MOZ_MAIL_NEWS

#pragma mark ---CMIMEListPane---
//======================================
class CMIMEListPane : public LTable, public LCommander, public LBroadcaster
//======================================
{
public:

	enum
	{
		class_ID = 'MmLP',
		msg_SelectionChanged = 'Sel∆',	// ioparam = this
		msg_DoubleClick = '2Clk'		// ioparam = this
	};

	class PrefCellInfo
	{
		public:
					PrefCellInfo();
					PrefCellInfo(CMimeMapper* mapper, CApplicationIconInfo* iconInfo); 

		CMimeMapper*	fMapper;			// The mapper from the preference MIME list
		CApplicationIconInfo* fIconInfo;	// Information about icon to draw
	};

	// •• Constructors/destructors/access

						CMIMEListPane(LStream *inStream);
	void				FinishCreateSelf();
	void 				BindCellToApplication(TableIndexT row, CMimeMapper * mapper);
	CApplicationIconInfo* GetAppInfo(CMimeMapper* mapper);
	
	// •• access
//#if defined(QAP_BUILD)
//	virtual void			QapGetContents(PWCINFO pwc, short *pCount, short max);
//	virtual void			QapGetListInfo(PQAPLISTINFO pInfo);
//	virtual short			QapGetListContents(Ptr pBuf, short index);
//#endif	

//	void				SetContainer( CPrefHelpersContain* container) { fContainer = container; }
	void				GetCellInfo(PrefCellInfo& cellInfo, int row);
	void				FreeMappers();
	Boolean				MimeTypeExists(CMimeMapper* mapper);

	// •• Cell selection
	void				DrawCell( const TableCellT& inCell );
	void				SelectCell( const TableCellT& inCell );
	virtual	void		ClickCell(	const TableCellT& inCell,
									const SMouseDownEvent& inMouseDown);

	// Drawing
	virtual void		DrawSelf();
	virtual void		HiliteCell(const TableCellT &inCell);
	virtual void		UnhiliteCell(const TableCellT &inCell);
			void 		ScrollCellIntoFrame(const TableCellT& inCell);
	virtual	void		ActivateSelf();

	// Events
	virtual Boolean		HandleKeyPress(const EventRecord &inKeyEvent);

protected:
	CApplicationList			mApplList;		// List of application and their icons
//	CPrefHelpersContain*		fContainer;		// Containing view
	Handle						mNetscapeIcon;	// Icon for Netscape
	Handle						mPluginIcon;	// Icon for plug-ins
}; // class CMIMEListPane


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

//-----------------------------------
CMIMEListPane::CMIMEListPane(LStream *inStream)
//-----------------------------------
:
//fContainer(nil),
mNetscapeIcon(nil),
mPluginIcon(nil),
LTable(inStream)
{
}

//-----------------------------------
void CMIMEListPane::FinishCreateSelf()
//-----------------------------------
{
	SetCellDataSize(sizeof(PrefCellInfo));
	
	for (TableIndexT i = 1; i <= CPrefs::sMimeTypes.GetCount(); i++)	// Insert a row for each mime type
	{
		CMimeMapper * mime;
		CPrefs::sMimeTypes.FetchItemAt(i, &mime);
		ThrowIfNil_(mime);
		if (!mime->IsTemporary())
		{
			CMimeMapper * duplicate = new CMimeMapper(*mime);
			PrefCellInfo cellInfo;
			InsertRows(1, i, &cellInfo);
			BindCellToApplication(i, duplicate);
		}
	}
	
	// Cache a handle to the Navigator application icon
	CApplicationIconInfo* netscapeInfo = mApplList.GetAppInfo(emSignature);
	ThrowIfNil_(netscapeInfo);
	ThrowIfNil_(netscapeInfo->fDocumentIcons);
	mNetscapeIcon = netscapeInfo->fApplicationIcon;
	
	// Cache a handle to the standard plug-in icon
	LArrayIterator iterator(*(netscapeInfo->fDocumentIcons));
	CFileType* fileInfo;
	while (iterator.Next(&fileInfo))
	{
		if (fileInfo->fIconSig == emPluginFile)	
		{
			mPluginIcon = fileInfo->fIcon;
			break;	
		}
	}

	LTable::FinishCreateSelf();

	// Set latent subcommander of supercommander of supercommander, which is the
	// view that contains the scroller of this table, to table
//	if( GetSuperCommander()->GetSuperCommander() )
//		(GetSuperCommander()->GetSuperCommander())->SetLatentSub(this);
	LCommander::SwitchTarget(this);
}


CApplicationIconInfo* CMIMEListPane::GetAppInfo(CMimeMapper* mapper)
{
	return mApplList.GetAppInfo(mapper->GetAppSig(), mapper);
}

/*
#if defined(QAP_BUILD)
void
CMIMEListPane::QapGetContents(PWCINFO pwc, short *pCount, short max)
{
	QapAddViewItem(pwc, pCount, WT_ASSIST_ITEM, WC_LIST_BOX);
}

void
CMIMEListPane::QapGetListInfo(PQAPLISTINFO pInfo)
{
	TableIndexT		outRows, outCols;
	ControlHandle	ctlHandle;
	LScroller		*scroller;
	LStdControl		*VSBar;
	ControlHandle	ctlhVSBar;
	
	if (pInfo == nil)
		return;
		
	scroller 	= (LScroller *)((this->GetSuperView())->GetSuperView())->FindPaneByID(12501);
	VSBar		= scroller->GetVScrollbar();
	ctlhVSBar	= VSBar->GetMacControl();
		
	GetTableSize(outRows, outCols);
	
	pInfo->itemCount	= (short)outRows;
	pInfo->topIndex 	= (mFrameLocation.v - mImageLocation.v) / mRowHeight;
	pInfo->itemHeight 	= mRowHeight;
	pInfo->visibleCount = 8;
	pInfo->vScroll 		= nil;
	pInfo->isMultiSel 	= false;
	pInfo->isExtendSel 	= false;
	pInfo->hasText 		= true;
}

short
CMIMEListPane::QapGetListContents(Ptr pBuf, short index)
{
	TableCellT		sTblCell, currentCell;
	PrefCellInfo	theData;
	Ptr				pLimit;
	char 			str[256];
	short			count = 0, len = 0;
	Uint32			uiSize;

	pLimit = pBuf + *(long *)pBuf;

	switch (index)
		{
		case QAP_INDEX_ALL:
			sTblCell.row = 1;
			sTblCell.col = 1;
			do
			{
				GetCellData(sTblCell, (void *)&theData);
				
				CStr255 description = theData.fMapper->GetDescription();
				if (description.IsEmpty())
					description = theData.fMapper->GetMimeName();
			
				if (pBuf + 3 >= pLimit)
					break;
				
				count++;

				*(unsigned short *)pBuf = sTblCell.row - 1;

				GetSelectedCell(currentCell);
				
				if (currentCell.row == sTblCell.row)
					*(unsigned short *)pBuf |= 0x8000;

				pBuf += sizeof(short);
		
				strcpy(str, (char *)description);
				len = strlen(str) + 1;
	
				if (pBuf + len >= pLimit)
					break;
		
				strcpy(pBuf, str);
				pBuf += len;
				sTblCell.row++;
			}
			while (sTblCell.row <= mRows);
			
			break;
			
		case QAP_INDEX_SELECTED:
			GetSelectedCell(sTblCell);
			if (sTblCell.row <= mRows)
			{
				GetCellData(sTblCell, (void *)&theData);
			
				CStr255 description = theData.fMapper->GetDescription();
				if (description.IsEmpty())
					description = theData.fMapper->GetMimeName();
			
				if (pBuf + 3 >= pLimit)
					break;
				
				count++;

				*(unsigned short *)pBuf = (sTblCell.row - 1) | 0x8000;

				pBuf += sizeof(short);
		
				strcpy(str, (char *)description);
				len = strlen(str) + 1;
	
				if (pBuf + len >= pLimit)
					break;
		
				strcpy(pBuf, str);
				pBuf += len;
			}
			break;
			
		default:
			sTblCell.row = index + 1;
			sTblCell.col = 1;
			
			if (sTblCell.row <= mRows)
			{
				GetCellData(sTblCell, (void *)&theData);
				
				CStr255 description = theData.fMapper->GetDescription();
				if (description.IsEmpty())
					description = theData.fMapper->GetMimeName();
				
				if (pBuf + 3 >= pLimit)
					break;
					
				count++;

				*(unsigned short *)pBuf = sTblCell.row - 1;


				GetSelectedCell(currentCell);
					
				if (currentCell.row == sTblCell.row)
					*(unsigned short *)pBuf |= 0x8000;


				pBuf += sizeof(short);
		
				strcpy(str, (char *)description);
				len = strlen(str) + 1;
		
				if (pBuf + len >= pLimit)
					break;
			
				strcpy(pBuf, str);
			}
			break;
		}
	return count;
}
#endif
*/

// Sets cell data to values of the mapper
void
CMIMEListPane::BindCellToApplication(TableIndexT row, CMimeMapper* mapper)
{
	CApplicationIconInfo* appInfo = GetAppInfo(mapper);
	
	// Now figure out the extensions from the netlib
	PrefCellInfo cellInfo(mapper, appInfo);
	TableCellT cell;
	cell.row = row;
	cell.col = 1;
	SetCellData(cell, &cellInfo);
}

void
CMIMEListPane::ActivateSelf()
{
	if (triState_On == mVisible)
	{
		LTable::ActivateSelf();
	}
}


void
CMIMEListPane::DrawSelf()
{
	RgnHandle localUpdateRgnH = GetLocalUpdateRgn();
	Rect updateRect = (**localUpdateRgnH).rgnBBox;
	DisposeRgn(localUpdateRgnH);
	
	{
		StColorState saveTheColor;
		RGBColor white = { 0xFFFF, 0xFFFF, 0xFFFF};
		RGBBackColor(&white);
		EraseRect(&updateRect);
	}
	
	LTable::DrawSelf();
}


void CMIMEListPane::HiliteCell(const TableCellT &inCell)
{
	if (triState_On == mVisible)
	{
		StColorState saveTheColor;
		RGBColor white = { 0xFFFF, 0xFFFF, 0xFFFF};
		RGBBackColor(&white);
	
		LTable::HiliteCell(inCell);
	}
}


void CMIMEListPane::UnhiliteCell(const TableCellT &inCell)
{
	if (triState_On == mVisible)
	{
		StColorState saveTheColor;
		RGBColor white = { 0xFFFF, 0xFFFF, 0xFFFF};
		RGBBackColor(&white);
	
		LTable::UnhiliteCell(inCell);
	}
}



#define offsetTable			7
#define offsetTextTop		15
#define offsetMimeType		(offsetTable + 0)
#define offsetIcon			(offsetTable + 166)
#define offsetHandler		(offsetIcon + 24)
#define widthMimeType		(offsetIcon - offsetMimeType - 5)

void CMIMEListPane::DrawCell(const TableCellT	&inCell)
{
	Rect cellFrame;
	
	if (FetchLocalCellFrame(inCell, cellFrame))
	{
		::PenPat( &(UQDGlobals::GetQDGlobals()->gray) );
		::MoveTo(cellFrame.left, cellFrame.bottom-1);
		::LineTo(cellFrame.right, cellFrame.bottom-1);
		::PenPat( &(UQDGlobals::GetQDGlobals()->black) );
		
		PrefCellInfo cellInfo;
		GetCellData(inCell, &cellInfo);
		UTextTraits::SetPortTextTraits(10000);	// HACK
		
		// Draw mime type
		CStr255 description = cellInfo.fMapper->GetDescription();
		if (description.IsEmpty())
			description = cellInfo.fMapper->GetMimeName();
		short result = ::TruncString(widthMimeType, description, smTruncMiddle);
		::MoveTo(offsetMimeType, cellFrame.top+offsetTextTop);
		::DrawString(description);
			
		// Draw the icon
		CMimeMapper::LoadAction loadAction = cellInfo.fMapper->GetLoadAction();
		if (loadAction == CMimeMapper::Launch || loadAction == CMimeMapper::Plugin || loadAction == CMimeMapper::Internal)
		{
			Rect r;
			r.left = offsetIcon;
			r.top = cellFrame.top + 2;
			r.right = r.left + 16;
			r.bottom = r.top + 16;
			
			Handle iconHandle;
			if (loadAction == CMimeMapper::Plugin)
				iconHandle = mPluginIcon;
			else if (loadAction == CMimeMapper::Internal)
				iconHandle = mNetscapeIcon;
			else
				iconHandle = cellInfo.fIconInfo->fApplicationIcon;
			XP_ASSERT(iconHandle);
			
			if (iconHandle)
			{
				if (loadAction == CMimeMapper::Launch && !(cellInfo.fIconInfo->fApplicationFound))
					::PlotIconSuite(&r, atHorizontalCenter, ttDisabled, iconHandle);
				else
					::PlotIconSuite(&r, atHorizontalCenter, ttNone, iconHandle);
			}
		}
			
		// Draw the handler name
		::MoveTo(offsetHandler, cellFrame.top+offsetTextTop);
		switch (loadAction)
		{
			case CMimeMapper::Save:
				::DrawString(*GetString(SAVE_RESID));
				break;
			case CMimeMapper::Unknown:
				::DrawString(*GetString(UNKNOWN_RESID));
				break;
			case CMimeMapper::Internal:
				::DrawString(*GetString(INTERNAL_RESID));
				break;
			case CMimeMapper::Launch:
				::DrawString(cellInfo.fMapper->GetAppName());
				break;
			case CMimeMapper::Plugin:
				::DrawString(cellInfo.fMapper->GetPluginName());
				break;
		}
	}
}


// Returns PrefCellInfo for the given cell
void CMIMEListPane::GetCellInfo(PrefCellInfo& cellInfo, int row)
{
	TableCellT	cell;
	cell.row = row;
	cell.col = 1;
	GetCellData(cell, &cellInfo);
}

void CMIMEListPane::FreeMappers()
{
	TableCellT	cell;
	cell.col = 1;
	PrefCellInfo cellInfo;
	for (int i=1; i<= mRows; i++)
	{
		cell.row = i;
		GetCellData(cell, &cellInfo);
		delete cellInfo.fMapper;
	}
}

Boolean CMIMEListPane::MimeTypeExists(CMimeMapper* mapper)
{
	TableCellT	cell;
	cell.col = 1;
	PrefCellInfo cellInfo;
	for (int i=1; i<= mRows; i++)
	{
		cell.row = i;
		GetCellData(cell, &cellInfo);
		if (cellInfo.fMapper != mapper &&
			cellInfo.fMapper->GetMimeName() == mapper->GetMimeName())
			return true;
	}
	return false;
}

//
// Scroll the view as little as possible to move the specified cell
// entirely within the frame.  Currently we only handle scrolling
// up or down a single line (when the selection moves because of
// and arrow key press).
//
void CMIMEListPane::ScrollCellIntoFrame(const TableCellT& inCell)
{
	// Compute bounds of specified cell in image coordinates
	Int32 cellLeft = 0;
	Int32 cellRight = 1;
	Int32 cellBottom = inCell.row * mRowHeight - 2;
	Int32 cellTop = cellBottom - mRowHeight + 2;

	if (ImagePointIsInFrame(cellLeft, cellTop) &&
		ImagePointIsInFrame(cellRight, cellBottom))
	{
		return;				// Cell is already within Frame
	}
	else
	{
		if (cellTop + mImageLocation.v < mFrameLocation.v)		
			ScrollImageTo(cellLeft, cellTop, true);				// Scroll up					
		else								
			ScrollPinnedImageBy(0, mRowHeight, true);			// Scroll down
	}
}

Boolean	CMIMEListPane::HandleKeyPress(const EventRecord &inKeyEvent)
{
	Char16 theChar = inKeyEvent.message & charCodeMask;
	Boolean handled = false;
	
	TableIndexT tableRows;
	TableIndexT tableCols;
	TableCellT currentCell;
	GetSelectedCell(currentCell);
	GetTableSize(tableRows, tableCols);
	
	switch (theChar)
	{
		case char_UpArrow:
			currentCell.row--;
			if (currentCell.row >= 1)
			{
				SelectCell(currentCell);
				ScrollCellIntoFrame(currentCell);
			}
			handled = true;
			break;

		case char_DownArrow:
			currentCell.row++;
			if (currentCell.row <= tableRows)
			{
				SelectCell(currentCell);
				ScrollCellIntoFrame(currentCell);
			}
			handled = true;
			break;
			
		case char_PageUp:
		case char_PageDown:
		case char_Home:
		case char_End:
		default:
			handled = LCommander::HandleKeyPress(inKeyEvent);
	}
	
	return handled;
}

// called by table
void CMIMEListPane::SelectCell(const TableCellT &inCell)
{
//	fContainer->SelectCell(inCell);
	LTable::SelectCell(inCell);
	BroadcastMessage(msg_SelectionChanged, this);
}

void
CMIMEListPane::ClickCell(const TableCellT &inCell, const SMouseDownEvent& inMouseDown)
{
	LTable::ClickCell(inCell, inMouseDown);
	if (GetClickCount() > 1)
	{
		BroadcastMessage(msg_DoubleClick, this);
	}
}


CMIMEListPane::PrefCellInfo::PrefCellInfo(CMimeMapper * mapper, CApplicationIconInfo * iconInfo)
{
	fMapper = mapper; 
	fIconInfo = iconInfo;
}

CMIMEListPane::PrefCellInfo::PrefCellInfo()
{
	fMapper = NULL;
	fIconInfo = NULL;
}

#pragma mark ---CEditMIMEWindow---
//======================================
class CEditMIMEWindow : public LGADialogBox
//======================================
{
public:
	enum { class_ID = 'EMMW' };
	
								CEditMIMEWindow(LStream* inStream);
	virtual 					~CEditMIMEWindow();

	virtual void				ListenToMessage(MessageT inMessage, void* ioParam);
			
			void				SetCellInfo(CMIMEListPane::PrefCellInfo&);
			void				SetMimeTable(CMIMEListPane* table)		{ mMIMETable = table; }
			CMIMEListPane::PrefCellInfo&
								GetCellInfo()						{ return mCellInfo;   }
			Boolean				Modified()							{ return mModified;   }

protected:
	virtual void				FinishCreateSelf();
	
private:
			void				SyncAllControls();
			void				SyncTextControls();
			void				SyncRadioControlValues();
			void				SyncInternalControl();
			void				SyncApplicationControls(Boolean mimeTypeChanged);
			void				SyncPluginControls(Boolean mimeTypeChanged);
			void				BuildPluginMenu();
			void				BuildPluginList();
			void				DeletePluginList();
			void				BuildFileTypeMenu();

			Boolean				mInitialized;		// Initialized with fCellInfo yet?
			Boolean				mModified;			// Have any MIMEs been modified?
			CMIMEListPane		*mMIMETable;			// Scrolling table of MIME types
			CMIMEListPane::PrefCellInfo
								mCellInfo;			// Data for selected item
			char**				mPluginList;		// Null-terminated array of plug-in names
			uint32				mPluginCount;		// Number of plug-ins in array

			LGAPopup			*mFileTypePopup;
			LGAPopup			*mPluginPopup;
			CGAEditBroadcaster	*mDescriptionEditField;
			CGAEditBroadcaster	*mTypeEditField;
			CGAEditBroadcaster	*mExtensionEditField;
			LGARadioButton		*mRadioSave;
			LGARadioButton		*mRadioLaunch;
			LGARadioButton		*mRadioInternal;
			LGARadioButton		*mRadioUnknown;
			LGARadioButton		*mRadioPlugin;
//			CFilePicker			*mAppPicker;
			LCaption			*mAppName;
			LButton				*mAppButton;
//			LCaption			*mAppMenuLabel;
}; // class CEditMIMEWindow
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
	eUnknownRButton
};

//-----------------------------------
CEditMIMEWindow::CEditMIMEWindow(LStream* inStream):
//-----------------------------------
	LGADialogBox(inStream),
	mMIMETable(nil),
	mModified(false),
	mInitialized(false),
	mPluginList(nil),
	mPluginCount(0)
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

	mDescriptionEditField = (CGAEditBroadcaster *)FindPaneByID(eDescriptionField);
	XP_ASSERT(mDescriptionEditField);

	mTypeEditField = (CGAEditBroadcaster *)FindPaneByID(eMIMETypeField);
	XP_ASSERT(mTypeEditField);

	mExtensionEditField = (CGAEditBroadcaster *)FindPaneByID(eSuffixes);
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

//	mAppMenuLabel = (LCaption *)FindPaneByID(pref8AppMenuLabel);
//	XP_ASSERT(mAppMenuLabel);

	// Text fields cannot become broadcasters automatically because
	// LinkListenerToControls expects fields to be descendants of LControl
	// C++ vtable gets messed up
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
void CEditMIMEWindow::SetCellInfo(CMIMEListPane::PrefCellInfo& cellInfo)
{
	mCellInfo = cellInfo;
	SyncAllControls();
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
	mPluginList = NPL_FindPluginsForType(mCellInfo.fMapper->GetMimeName());

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
			if (mCellInfo.fMapper->GetPluginName() == mPluginList[index])
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


// ---------------------------------------------------------------------------
// CEditMIMEWindow::BuildFileTypeMenu:
// Build the file-type menu from the list file types in the application info.
// ---------------------------------------------------------------------------
void CEditMIMEWindow::BuildFileTypeMenu()
{
	CApplicationIconInfo* appInfo = mCellInfo.fIconInfo;
	uint32 count = appInfo->GetFileTypeArraySize();
	
	if (count)
	{
		SetMenuSizeForLGAPopup(mFileTypePopup, count);

		MenuHandle menuH = mFileTypePopup->GetMacMenuH();
		uint32 index;
		uint32 desiredValue = 1;
		for (index = 1; index <= count; index++)
		{
			CFileType* fileType = appInfo->GetFileType(index);
			SetMenuItemText(menuH, index, CStr255(fileType->fIconSig));
			if (fileType->fIconSig == mCellInfo.fMapper->GetDocType())
			{
				desiredValue = index;
			}
		}
		
		//
		// If the previously-selected file type is in this list,
		// select it; otherwise just select the first item.
		// If we didn't change the control value, make sure it
		// redraws since the contents of the list have changed.
		//
		uint32 previousValue = mFileTypePopup->GetValue();
		if (desiredValue != previousValue)
		{
			mFileTypePopup->SetValue(desiredValue);
		}
		else
		{
			mFileTypePopup->Refresh();
		}
	}
}



// ---------------------------------------------------------------------------
// CEditMIMEWindow::SyncTextControls:
// Sync the edit text fields with their actual values (only called when
// the window is first created).
// ---------------------------------------------------------------------------
void CEditMIMEWindow::SyncTextControls()
{
	XP_ASSERT(mCellInfo.fMapper != nil);

	// MIME type
	CStr255 newValue = mCellInfo.fMapper->GetDescription();
	mDescriptionEditField->SetDescriptor(newValue);

	// MIME type
	newValue = mCellInfo.fMapper->GetMimeName();
	mTypeEditField->SetDescriptor(newValue);
	
	// Extensions
	newValue = mCellInfo.fMapper->GetExtensions();
	mExtensionEditField->SetDescriptor(newValue);
}


// ---------------------------------------------------------------------------
// CEditMIMEWindow::SyncRadioControlValues:
// Sync the radio buttons with their actual values (only called when
// the window is first created).
// ---------------------------------------------------------------------------
void CEditMIMEWindow::SyncRadioControlValues()
{
	switch (mCellInfo.fMapper->GetLoadAction())
	{
		case CMimeMapper::Save:
			if (mRadioSave->GetValue() == 0)
			{
				mRadioSave->SetValue(1);
			}
			break;
		case CMimeMapper::Launch:
			if (mRadioLaunch->GetValue() == 0)
			{
				mRadioLaunch->SetValue(1);
			}
			break;
		case CMimeMapper::Internal:
			if (mRadioInternal->GetValue() == 0)
			{
				mRadioInternal->SetValue(1);
			}
			break;
		case CMimeMapper::Unknown:
			if (mRadioUnknown->GetValue() == 0)
			{
				mRadioUnknown->SetValue(1);
			}
			break;
		case CMimeMapper::Plugin:
			if (mRadioPlugin->GetValue() == 0)
			{
				mRadioPlugin->SetValue(1);
			}
			break;
	}
}


// ---------------------------------------------------------------------------
// CEditMIMEWindow::SyncInternalControl:
// Sync up the "Use Netscape" radio button when the MIME type changes.
// ---------------------------------------------------------------------------
void CEditMIMEWindow::SyncInternalControl()
{
	if (CMimeMapper::NetscapeCanHandle(mCellInfo.fMapper->GetMimeName()))
	{
		mRadioInternal->Enable();
	}
	else
	{
		mRadioInternal->Disable();
		if (mRadioInternal->GetValue() == 1)
		{
			mRadioInternal->SetValue(0);
			mRadioUnknown->SetValue(1);
		}
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
	Boolean enableAppControls = (mCellInfo.fMapper->GetLoadAction() == CMimeMapper::Launch);

	//
	// Application name
	//
	ResIDT oldTraits = mAppName->GetTextTraitsID();
	if (enableAppControls)
	{
		mAppName->SetTextTraitsID(eNormalTextTraits);			// Normal
	}
	else	
	{
		mAppName->SetTextTraitsID(eGrayTextTraits);			// Dimmed
	}
	if (applicationChanged)
	{
		mAppName->SetDescriptor(mCellInfo.fMapper->GetAppName());
	}
	else if (oldTraits != mAppName->GetTextTraitsID())
	{
		mAppName->Refresh();
	}
	//
	// Application pick button
	//
	if (enableAppControls)
	{
		mAppButton->Enable();
	}
	else
	{
		mAppButton->Disable();
	}
	//
	// Popup label
	//
//	oldTraits = mAppMenuLabel->GetTextTraitsID();
//	if (enableAppControls)
//	{
//		mAppMenuLabel->SetTextTraitsID(10000);		// Normal
//	}
//	else
//	{
//		mAppMenuLabel->SetTextTraitsID(10003);		// Dimmed
//	}
//	if (oldTraits != mAppMenuLabel->GetTextTraitsID())
//	{
//		mAppMenuLabel->Refresh();
//	}
	//
	// Popup enabled state
	//
	if (enableAppControls && mCellInfo.fIconInfo->GetFileTypeArraySize() > 0)
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
		uint32 item = mFileTypePopup->GetValue();
		XP_ASSERT(item > 0);
		CFileType* fileType = mCellInfo.fIconInfo->GetFileType(item);
		if (fileType)
		{
			mCellInfo.fMapper->SetDocType(fileType->fIconSig);
		}
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
//			mRadioPlugin->SetValue(0);
			mRadioUnknown->SetValue(1);
			mCellInfo.fMapper->SetLoadAction(CMimeMapper::Unknown);
			mModified = true;
		}
	}

	//
	// Plug-in popup control enabled state
	//
	CMimeMapper* mapper = mCellInfo.fMapper;
	CMimeMapper::LoadAction loadAction = mapper->GetLoadAction();
	if (loadAction == CMimeMapper::Plugin && mPluginCount > 0)
//	if (fCellInfo.fMapper->GetLoadAction() == CMimeMapper::Plugin && fPluginCount > 0)
	{
		mPluginPopup->Enable();
	}
	else
	{
		mPluginPopup->Disable();
	}	
	//
	// Plug-in name
	//
	if (mRadioPlugin->GetValue() == 1)
	{
		uint32 item = mPluginPopup->GetValue();	
		XP_ASSERT(item > 0);	
		if (mPluginList && item > 0 && item <= mPluginCount)
		{
			char* plugin = mPluginList[item - 1];		// Menu is 1-based, list is 0-based
			if (plugin)
				mCellInfo.fMapper->SetPluginName(plugin);
		}
	}
	
}

//-----------------------------------
void CEditMIMEWindow::SyncAllControls()
// Set up the controls in the dialog box (before it's visible) with the
// information from the currently selected cell.
//-----------------------------------
{
	SyncTextControls();
	SyncRadioControlValues();
	SyncInternalControl();
	SyncApplicationControls(true);
	SyncPluginControls(true);
}


//-----------------------------------
void CEditMIMEWindow::ListenToMessage(MessageT inMessage, void */*ioParam*/)
// Process a message from the MIME types dialog
//-----------------------------------
{
	if (!mInitialized)
	{
		return;
	}	
	switch (inMessage)
	{
		// Pick a handler
		case eSaveRButton:
			if (mRadioSave->GetValue() == 1)
			{
				mCellInfo.fMapper->SetLoadAction(CMimeMapper::Save);
			}
			mModified = TRUE;
			break;
		case eApplicationRButton:
			if (mRadioLaunch->GetValue() == 1)
			{
				mCellInfo.fMapper->SetLoadAction(CMimeMapper::Launch);
			}
			SyncApplicationControls(false);
			mModified = TRUE;
			break;
		case eCommunicatorRButton:
			if (mRadioInternal->GetValue() == 1)
			{
				mCellInfo.fMapper->SetLoadAction(CMimeMapper::Internal);
			}
			mModified = TRUE;
			break;
		case eUnknownRButton:
			if (mRadioUnknown->GetValue() == 1)
			{
				mCellInfo.fMapper->SetLoadAction(CMimeMapper::Unknown);
			}
			mModified = TRUE;
			break;
		case ePluginRButton:
			if (mRadioPlugin->GetValue() == 1)
			{
				mCellInfo.fMapper->SetLoadAction(CMimeMapper::Plugin);
			}
			SyncPluginControls(false);
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
				mCellInfo.fMapper->SetAppSig(finderInfo.fdCreator);
				mCellInfo.fMapper->SetAppName(reply.sfFile.name);
				mCellInfo.fIconInfo = mMIMETable->GetAppInfo(mCellInfo.fMapper);
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
		case msg_EditField:
		{
			CStr255 newText;

			// Commit description
			mDescriptionEditField->GetDescriptor(newText);
			mCellInfo.fMapper->SetDescription(newText);
			
			// Commit type
			mTypeEditField->GetDescriptor(newText);
			if (newText != mCellInfo.fMapper->GetMimeName())
			{
				mCellInfo.fMapper->SetMimeType(newText);
				SyncPluginControls(true);		// Plug-in choices depend on MIME type
				SyncInternalControl();			// "Use Netscape" radio depends on MIME type
			}
			
			// Commit the extensions
			mExtensionEditField->GetDescriptor(newText);
			mCellInfo.fMapper->SetExtensions(newText);

			mModified = TRUE;
			break;
		}

		case msg_OK:
			break;
		case msg_Cancel:
			break;
		default:
			break;
	}	
}

#pragma mark ---CEditFieldControl---
//======================================
class CEditFieldControl : public LGAEditField
//======================================
{
	// Note: This is not derived from LControl! It has control in the
	// name because it broadcasts when the user changes its contents.

	public:
		enum
		{
			class_ID = 'edtC',
			msg_ChangedText = 'TxtC'
		};
		virtual				~CEditFieldControl() {}
							CEditFieldControl(LStream *inStream) :
											  LGAEditField(inStream)
											  {}
		virtual void		UserChangedText();
}; // class CEditFieldControl

//-----------------------------------
void CEditFieldControl::UserChangedText()
//-----------------------------------
{
	BroadcastMessage(msg_ChangedText, this);
}

//======================================
#pragma mark --CAppearanceMainMediator---
//======================================

enum
{
	eBrowserBox = 12001,
	eMailBox,
	eNewsBox,
	eEditorBox,
	eConferenceBox,
	eCalendarBox,
	eNetcasterBox,
	ePicturesAndTextRButton,
	eShowToolTipsBox,
	ePicturesOnlyRButton,
	eTextOnlyRButton,
	eDesktopPatternBox
};

//-----------------------------------
CAppearanceMainMediator::CAppearanceMainMediator(LStream*)
//-----------------------------------
:	CPrefsMediator(class_ID)
{
}

//-----------------------------------
void CAppearanceMainMediator::LoadPrefs()
//-----------------------------------
{
	const OSType kConferenceAppSig = 'Ncqπ';
	FSSpec fspec;
	if (CFileMgr::FindApplication(kConferenceAppSig, fspec) != noErr)
	{
		LGACheckbox* checkbox = (LGACheckbox *)FindPaneByID(eConferenceBox);
		XP_ASSERT(checkbox);
		checkbox->SetValue(false);
		// disable the control
		checkbox->Disable();	
	}

	const OSType kCalendarAppSig = 'NScl';
	if (CFileMgr::FindApplication(kCalendarAppSig, fspec) != noErr)
	{
		LGACheckbox* checkbox = (LGACheckbox *)FindPaneByID(eCalendarBox);
		XP_ASSERT(checkbox);
		checkbox->SetValue(false);
		// disable the control
		checkbox->Disable();	
	}

	if (!FE_IsNetcasterInstalled())
	{
		LGACheckbox* checkbox = (LGACheckbox *)FindPaneByID(eNetcasterBox);
		XP_ASSERT(checkbox);
		checkbox->SetValue(false);
		// disable the control
		checkbox->Disable();
	}
}

void CAppearanceMainMediator::WritePrefs()
{
	// this pref will not take effect immediately unless we tell the CToolTipAttachment
	// class to make it so
	LGACheckbox	*theBox = (LGACheckbox *)FindPaneByID(eShowToolTipsBox);
	XP_ASSERT(theBox);
	CToolTipAttachment::Enable(theBox->GetValue());
}

enum
{
	eCharSetMenu = 12101,
	ePropFontMenu,
	ePropSizeMenu,
	eFixedFontMenu,
	eFixedSizeMenu,
	eAlwaysOverrideRButton,
	eQuickOverrideRButton,
	eNeverOverrideRButton
};


//-----------------------------------
CAppearanceFontsMediator::CAppearanceFontsMediator(LStream*)
//-----------------------------------
:	CPrefsMediator(class_ID)
,	mEncodings(nil)
{
}

int
CAppearanceFontsMediator::GetSelectEncMenuItem()
{
	LGAPopup *encMenu = (LGAPopup *)FindPaneByID(eCharSetMenu);
	XP_ASSERT(encMenu);
	return encMenu->GetValue();
}

void
CAppearanceFontsMediator::UpdateEncoding(PaneIDT changedMenuID)
{
	Int32	selectedEncMenuItem = GetSelectEncMenuItem();
	XP_ASSERT(selectedEncMenuItem <= mEncodingsCount);

	if (changedMenuID == ePropFontMenu || changedMenuID == eFixedFontMenu)
	{
		LGAPopup *changedMenu = (LGAPopup *)FindPaneByID(changedMenuID);
		XP_ASSERT(changedMenu);
		int	changedMenuValue = changedMenu->GetValue();
		CStr255	itemString;
		::GetMenuItemText(changedMenu->GetMacMenuH(), changedMenuValue, itemString);
		if (changedMenuID == ePropFontMenu)
		{
			mEncodings[selectedEncMenuItem - 1].mPropFont = itemString;
		}
		else
		{
			mEncodings[selectedEncMenuItem - 1].mFixedFont = itemString;
		}
	}
	else if (changedMenuID == ePropSizeMenu || changedMenuID == eFixedSizeMenu)
	{
		CSizePopup *sizeMenu = (CSizePopup *)FindPaneByID(changedMenuID);
		XP_ASSERT(sizeMenu);
		if (changedMenuID == ePropSizeMenu)
		{
			mEncodings[selectedEncMenuItem - 1].mPropFontSize = sizeMenu->GetFontSize();
		}
		else
		{
			mEncodings[selectedEncMenuItem - 1].mFixedFontSize = sizeMenu->GetFontSize();
		}
	}
}


void
CAppearanceFontsMediator::UpdateMenus()
{
	Int32	selectedEncMenuItem = GetSelectEncMenuItem();
	XP_ASSERT(selectedEncMenuItem <= mEncodingsCount);

	CSizePopup *propSizeMenu = (CSizePopup *)FindPaneByID(ePropSizeMenu);
	XP_ASSERT(propSizeMenu);
	propSizeMenu->SetFontSize(mEncodings[selectedEncMenuItem - 1].mPropFontSize);
	if (mEncodings[selectedEncMenuItem - 1].mPropFontSizeLocked)
	{
		propSizeMenu->Disable();
	}
	else
	{
		propSizeMenu->Enable();
	}
	CSizePopup *fixedSizeMenu = (CSizePopup *)FindPaneByID(eFixedSizeMenu);
	XP_ASSERT(fixedSizeMenu);
	fixedSizeMenu->SetFontSize(mEncodings[selectedEncMenuItem - 1].mFixedFontSize);
	if (mEncodings[selectedEncMenuItem - 1].mFixedFontSizeLocked)
	{
		fixedSizeMenu->Disable();
	}
	else
	{
		fixedSizeMenu->Enable();
	}

	Str255	fontName;
	LGAPopup *propFontMenu = (LGAPopup *)FindPaneByID(ePropFontMenu);
	XP_ASSERT(propFontMenu);
	if (!SetMenuToNamedItem(propFontMenu, propFontMenu->GetMacMenuH(), mEncodings[selectedEncMenuItem - 1].mPropFont))
	{
		GetFontName(applFont, fontName);
		if (!SetMenuToNamedItem(propFontMenu, propFontMenu->GetMacMenuH(), fontName))
		{
			propFontMenu->SetValue(1);
		}
	}
	propSizeMenu->MarkRealFontSizes(propFontMenu);
	if (mEncodings[selectedEncMenuItem - 1].mPropFontLocked)
	{
		propFontMenu->Disable();
	}
	else
	{
		propFontMenu->Enable();
	}

	LGAPopup *fixedFontMenu = (LGAPopup *)FindPaneByID(eFixedFontMenu);
	XP_ASSERT(fixedFontMenu);
	if (!SetMenuToNamedItem(fixedFontMenu, fixedFontMenu->GetMacMenuH(), mEncodings[selectedEncMenuItem - 1].mFixedFont))
	{
		GetFontName(applFont, fontName);
		if (!SetMenuToNamedItem(fixedFontMenu, fixedFontMenu->GetMacMenuH(), fontName))
		{
			fixedFontMenu->SetValue(1);
		}
	}
	fixedSizeMenu->MarkRealFontSizes(fixedFontMenu);
	if (mEncodings[selectedEncMenuItem - 1].mFixedFontLocked)
	{
		fixedFontMenu->Disable();
	}
	else
	{
		fixedFontMenu->Enable();
	}
}


void
CAppearanceFontsMediator::SetEncodingWithPref(Encoding& enc)
{
	char *propFontTemplate = "intl.font%hu.prop_font";
	char *fixedFontTemplate = "intl.font%hu.fixed_font";
	char *propFontSizeTemplate = "intl.font%hu.prop_size";
	char *fixedFontSizeTemplate = "intl.font%hu.fixed_size";
	char prefBuffer[255];
	char fontNameBuffer[32];
	int	bufferLength;
	int32	fontSize;

	sprintf(prefBuffer, propFontTemplate, enc.mCSID);
	bufferLength = 32;
	PREF_GetCharPref(prefBuffer, fontNameBuffer, &bufferLength);
	enc.mPropFont = fontNameBuffer;
	enc.mPropFontLocked = PREF_PrefIsLocked(prefBuffer);

	sprintf(prefBuffer, fixedFontTemplate, enc.mCSID);
	bufferLength = 32;
	PREF_GetCharPref(prefBuffer, fontNameBuffer, &bufferLength);
	enc.mFixedFont = fontNameBuffer;
	enc.mFixedFontLocked = PREF_PrefIsLocked(prefBuffer);

	sprintf(prefBuffer, propFontSizeTemplate, enc.mCSID);
	PREF_GetIntPref(prefBuffer, &fontSize);
	enc.mPropFontSize = fontSize;
	enc.mPropFontSizeLocked = PREF_PrefIsLocked(prefBuffer);

	sprintf(prefBuffer, fixedFontSizeTemplate, enc.mCSID);
	PREF_GetIntPref(prefBuffer, &fontSize);
	enc.mFixedFontSize = fontSize;
	enc.mFixedFontSizeLocked = PREF_PrefIsLocked(prefBuffer);
}

void
CAppearanceFontsMediator::ReadEncodings(Handle hndl)
{
	unsigned short	numberEncodings;
	
	LHandleStream	stream(hndl);
				
	stream.ReadData(&numberEncodings, sizeof(unsigned short));
	XP_ASSERT(numberEncodings > 0);
	mEncodingsCount = numberEncodings;
	mEncodings	= new Encoding[numberEncodings];
	XP_ASSERT(mEncodings != nil);
	for(int i = 0; i < numberEncodings; i++)
	{
		CStr31			EncodingName;
		CStr31			PropFont;
		CStr31			FixedFont;
		unsigned short	PropFontSize;
		unsigned short	FixedFontSize;
		unsigned short	CSID;
		unsigned short  FallbackFontScriptID;
		unsigned short	TxtrButtonResID;
		unsigned short	TxtrTextFieldResID;	
		
		stream.ReadData(&EncodingName,	sizeof( CStr31));
		stream.ReadData(&PropFont, 		sizeof( CStr31));
		stream.ReadData(&FixedFont, 		sizeof( CStr31));
		stream.ReadData(&PropFontSize, 	sizeof( unsigned short));
		stream.ReadData(&FixedFontSize, 	sizeof( unsigned short));
		stream.ReadData(&CSID, 			sizeof( unsigned short));
		stream.ReadData(&FallbackFontScriptID, sizeof( unsigned short));
		stream.ReadData(&TxtrButtonResID, sizeof( unsigned short));
		stream.ReadData(&TxtrTextFieldResID, sizeof( unsigned short));

		mEncodings[i].mLanguageGroup = EncodingName;
		mEncodings[i].mCSID = CSID;
		mEncodings[i].mPropFont = PropFont;
		mEncodings[i].mFixedFont = FixedFont;
		mEncodings[i].mPropFontSize = PropFontSize;
		mEncodings[i].mFixedFontSize = FixedFontSize;
		SetEncodingWithPref(mEncodings[i]);
	}
}

void
CAppearanceFontsMediator::LoadEncodings()
{
	Handle hndl;
	::UseResFile(LMGetCurApRefNum());
	hndl = ::Get1Resource(FENC_RESTYPE,FNEC_RESID);
	if (hndl)
	{
		if (!*hndl)
		{
			::LoadResource(hndl);
		}
		if (*hndl)
		{
			::DetachResource(hndl);
			ThrowIfResError_();
			ReadEncodings(hndl);
			DisposeHandle(hndl);
		}
	}
}


void
CAppearanceFontsMediator::PopulateEncodingsMenus(PaneIDT menuID)
{
	if (!mEncodings)
	{
		LoadEncodings();
	}
	LGAPopup *theMenu = (LGAPopup *)FindPaneByID(menuID);
	XP_ASSERT(theMenu);
	for (int i = 0; i < mEncodingsCount; ++i)
	{
		MenuHandle	menuH = theMenu->GetMacMenuH();
		AppendMenu(menuH, "\pabc");
		SetMenuItemText(menuH, i + 1, mEncodings[i].mLanguageGroup);
	}
	theMenu->SetMaxValue(mEncodingsCount);
	theMenu->SetValue(1);
}

void
CAppearanceFontsMediator::SetPrefWithEncoding(const Encoding& enc)
{
	char *propFontTemplate = "intl.font%hu.prop_font";
	char *fixedFontTemplate = "intl.font%hu.fixed_font";
	char *propFontSizeTemplate = "intl.font%hu.prop_size";
	char *fixedFontSizeTemplate = "intl.font%hu.fixed_size";
	char prefBuffer[255];

	if (!enc.mPropFontLocked)
	{
		sprintf(prefBuffer, propFontTemplate, enc.mCSID);
		PREF_SetCharPref(prefBuffer, (char *)enc.mPropFont);
	}

	if (!enc.mFixedFontLocked)
	{
		sprintf(prefBuffer, fixedFontTemplate, enc.mCSID);
		PREF_SetCharPref(prefBuffer, (char *)enc.mFixedFont);
	}

	if (!enc.mPropFontSizeLocked)
	{
		sprintf(prefBuffer, propFontSizeTemplate, enc.mCSID);
		PREF_SetIntPref(prefBuffer, enc.mPropFontSize);
	}

	if (!enc.mFixedFontSizeLocked)
	{
		sprintf(prefBuffer, fixedFontSizeTemplate, enc.mCSID);
		PREF_SetIntPref(prefBuffer, enc.mFixedFontSize);
	}
}

void
CAppearanceFontsMediator::WriteEncodingPrefs()
{
	for(int i = 0; i < mEncodingsCount; i++)
	{
		SetPrefWithEncoding(mEncodings[i]);
	}
}


Int16
CAppearanceFontsMediator::GetFontSize(LGAPopup* whichPopup)
{
	Str255		sizeString;
	Int32		fontSize = 12;
	MenuHandle	sizeMenu;
	short		menuSize;
	short		inMenuItem;
	
	sizeMenu = whichPopup->GetMacMenuH();
	menuSize = CountMItems(sizeMenu);
	inMenuItem = whichPopup->GetValue();
	
	GetMenuItemText(sizeMenu, inMenuItem, sizeString);
	
	myStringToNum(sizeString, &fontSize);
	return fontSize;
}

void
CAppearanceFontsMediator::FontMenuChanged(PaneIDT changedMenuID)
{
	CSizePopup	*sizePopup =
			(CSizePopup *)FindPaneByID(ePropFontMenu == changedMenuID ?
											ePropSizeMenu :
											eFixedSizeMenu);
	XP_ASSERT(sizePopup);
	LGAPopup	*fontPopup = (LGAPopup *)FindPaneByID(changedMenuID);
	XP_ASSERT(fontPopup);
	sizePopup->MarkRealFontSizes(fontPopup);
	UpdateEncoding(changedMenuID);
}

void
CAppearanceFontsMediator::SizeMenuChanged(PaneIDT changedMenuID)
{
	UpdateEncoding(changedMenuID);
}

void
CAppearanceFontsMediator::ListenToMessage(MessageT inMessage, void *ioParam)
{
	switch (inMessage)
	{
		case eCharSetMenu:
			UpdateMenus();
			break;
		case msg_ChangeFontSize:
			break;
		case ePropFontMenu:
		case eFixedFontMenu:
			FontMenuChanged(inMessage);
			break;
		case ePropSizeMenu:
		case eFixedSizeMenu:
			SizeMenuChanged(inMessage);
			break;
		default:
			CPrefsMediator::ListenToMessage(inMessage, ioParam);
			break;
	}
}

void
CAppearanceFontsMediator::LoadPrefs()
{
	PopulateEncodingsMenus(eCharSetMenu);
	UpdateMenus();								
}

void
CAppearanceFontsMediator::WritePrefs()
{
	WriteEncodingPrefs();
}

enum
{
	eForegroundColorButton = 12201,
	eBackgroundColorButton,
	eUnvisitedColorButton,
	eVisitedColorButton,
	eUnderlineLinksBox,
	eUseDefaultButton,
	eOverrideDocColors
};

CAppearanceColorsMediator::CAppearanceColorsMediator(LStream*)
:	CPrefsMediator(class_ID)
{
}

void
CAppearanceColorsMediator::ListenToMessage(MessageT inMessage, void *ioParam)
{
	switch (inMessage)
	{
		case eUseDefaultButton:
			UseDefaults();
			break;
		default:
			CPrefsMediator::ListenToMessage(inMessage, ioParam);
			break;
	}
}

void
CAppearanceColorsMediator::UseDefaults()
{
	ReadDefaultPref(eForegroundColorButton);
	ReadDefaultPref(eUnvisitedColorButton);
	ReadDefaultPref(eVisitedColorButton);
	ReadDefaultPref(eBackgroundColorButton);
}

enum
{
	eBlankPageRButton = 12301,
	eHomePageRButton,
	eLastPageRButton,
	eHomePageTextEditBox,
	eHomePageChooseButton,
	eExpireAfterTextEditBox,
	eExpireNowButton,
	eUseCurrentButton
};


CBrowserMainMediator::CBrowserMainMediator(LStream*)
:	CPrefsMediator(class_ID)
,	mHomePageURLLocked(false)
,	mCurrentURL(nil)
{
}

void
CBrowserMainMediator::ListenToMessage(MessageT inMessage, void *ioParam)
{
	switch (inMessage)
	{
//		case msg_ControlClicked:
//			break;
		case eHomePageChooseButton:
			SetEditFieldWithLocalFileURL(eHomePageTextEditBox, mHomePageURLLocked);
			// do whatever it takes to find a home page file
			break;
		case eExpireNowButton:
			// do whatever it takes to expire links
			ExpireNow();
			break;
		case eUseCurrentButton:
			// do whatever it takes to find the current page
			if (mCurrentURL && *mCurrentURL)
			{
				LStr255	pURL(mCurrentURL);
				LEditField	*theField =
					(LEditField *)FindPaneByID(eHomePageTextEditBox);
				XP_ASSERT(theField);
				theField->SetDescriptor(pURL);
				theField->SelectAll();
			}
			break;
		default:
			CPrefsMediator::ListenToMessage(inMessage, ioParam);
			break;
	}
}

void
CBrowserMainMediator::ExpireNow()
{
	StPrepareForDialog	prepare;
	short	response = ::CautionAlert(eExpireNowButton, NULL);
	if (1 == response)
	{
		GH_ClearGlobalHistory();
		XP_RefreshAnchors();
	}
}

Boolean
CBrowserMainMediator::ExpireDaysValidationFunc(CValidEditField *daysTilExpire)
{
	Boolean		result;
	result = ConstrainEditField(daysTilExpire, EXPIRE_MIN, EXPIRE_MAX);
	if (!result)
	{
		StPrepareForDialog	prepare;
		::StopAlert(1064, NULL);
	}
	return result;
}

void
CBrowserMainMediator::LoadMainPane()
{
	CPrefsMediator::LoadMainPane();
	SetValidationFunction(eExpireAfterTextEditBox, ExpireDaysValidationFunc);
}

void
CBrowserMainMediator::UpdateFromIC()
{
	SetEditFieldsWithICPref(kICWWWHomePage, eHomePageTextEditBox);
	URLChoosingButtons(!UseIC() && !mHomePageURLLocked);
}

void
CBrowserMainMediator::URLChoosingButtons(Boolean enable)
{
	LButton	*currentURLButton = (LButton *)FindPaneByID(eUseCurrentButton);
	XP_ASSERT(currentURLButton);
	LButton	*chooseURLButton = (LButton *)FindPaneByID(eHomePageChooseButton);
	XP_ASSERT(chooseURLButton);
	if (enable)
	{
		currentURLButton->Enable();
		chooseURLButton->Enable();
	}
	else
	{
		currentURLButton->Disable();
		chooseURLButton->Disable();
	}
}

//-----------------------------------
void CBrowserMainMediator::LoadPrefs()
//-----------------------------------
{
	mHomePageURLLocked = PaneHasLockedPref(eHomePageTextEditBox);
	if (!mHomePageURLLocked)	// If locked we don't need the current url.
	{
		URLChoosingButtons(true);
		CWindowMediator	*mediator = CWindowMediator::GetWindowMediator();
		CBrowserWindow	*browserWindow =
			(CBrowserWindow	*)mediator->FetchTopWindow(WindowType_Browser);
		if (browserWindow)
		{
			CNSContext	*context = browserWindow->GetWindowContext();
			mCurrentURL = XP_STRDUP(context->GetCurrentURL());
		}
	}
	else
	{
		URLChoosingButtons(false);
	}
	LButton	*currentURLButton = (LButton *)FindPaneByID(eUseCurrentButton);
	XP_ASSERT(currentURLButton);
	if (mHomePageURLLocked || (!mCurrentURL) || (!(*mCurrentURL)))
		currentURLButton->Disable();
	else
		currentURLButton->Enable();
}

//-----------------------------------
void CBrowserMainMediator::WritePrefs()
//-----------------------------------
{
}

enum
{
	eBuiltInLanguageStringResID = 5028,
	eBuiltInLanguageCodeResID = 5029,
	eLanguageList = 12401,
	eAddLanguageButton,
	eDeleteLanguageButton,
	eAddLanguageList,
	eAddLanguageOtherEditField,
	eAddLanguageDialogID = 12004
};



CBrowserLanguagesMediator::CBrowserLanguagesMediator(LStream*)
:	CPrefsMediator(class_ID)
,	mLanguagesLocked(false)
,	mBuiltInCount(0)
,	mLanguageStringArray(nil)
,	mLangaugeCodeArray(nil)
{
}

CBrowserLanguagesMediator::~CBrowserLanguagesMediator()
{
	int i;
	if (mLanguageStringArray)
	{
		for (i = 0; i < mBuiltInCount; ++i)
		{
			if (mLanguageStringArray[i])
			{
				XP_FREE(mLanguageStringArray[i]);
			}
		}
		XP_FREE(mLanguageStringArray);
	}
	if (mLangaugeCodeArray)
	{
		for (i = 0; i < mBuiltInCount; ++i)
		{
			if (mLangaugeCodeArray[i])
			{
				XP_FREE(mLangaugeCodeArray[i]);
			}
		}
		XP_FREE(mLangaugeCodeArray);
	}
}

char *
CBrowserLanguagesMediator::GetLanguageDisplayString(	const char *languageCode)
{
	char	*result;
	int		i;
	for (i = 0; i < mBuiltInCount; ++i)
	{
		if (!strcmp(languageCode, mLangaugeCodeArray[i]))
		{
			const	char *	const	formatString = "%s [%s]";
			int	newStringLength =	strlen(mLangaugeCodeArray[i]) +
									strlen(mLanguageStringArray[i]) +
									strlen(formatString) -
									4;	// the two "%s"s in the format string
			result = (char *)XP_ALLOC(newStringLength + 1);
			sprintf(result, formatString, mLanguageStringArray[i], mLangaugeCodeArray[i]);
			break;
		}
	}
	if (i == mBuiltInCount)	// then we didn't find a match
	{
		result = (char *)XP_ALLOC(strlen(languageCode) + 1);
		strcpy(result, languageCode);
	}
	return result;
}

void
CBrowserLanguagesMediator::SetLanguageListWithPref(	const char *prefName,
														PaneIDT	languageListID,
														Boolean	&locked)
{
	locked = PREF_PrefIsLocked(prefName);
	CDragOrderTextList	*languageList =
				(CDragOrderTextList *)FindPaneByID(languageListID);
	XP_ASSERT(languageList);

	char	*theString;
	int		prefResult = PREF_CopyCharPref(prefName, &theString);
	if (prefResult == PREF_NOERROR && theString != nil)
	{
		TableIndexT	rowNumber = 0;
		char	*languageCode;
		char	*strtokFirstParam = theString;
		#pragma warn_possunwant off
		while (languageCode = strtok(strtokFirstParam, ", "))
		#pragma warn_possunwant reset
		{
			char	*display = GetLanguageDisplayString(languageCode);
			Str255	pDisplay;
			strtokFirstParam = nil;
			BlockMoveData(display, &pDisplay[1], pDisplay[0] = strlen(display));
			languageList->InsertRows(1, rowNumber++, pDisplay, sizeof(Str255), false);
			XP_FREE(display);
		}
		XP_FREE(theString);
		languageList->Refresh();
	}
	if (locked)
	{
		LButton	*button =
				(LButton *)FindPaneByID(eAddLanguageButton);
		XP_ASSERT(button);
		button->Disable();
		button = (LButton *)FindPaneByID(eDeleteLanguageButton);
		XP_ASSERT(button);
		button->Disable();
		languageList->LockOrder();
	}
}

char *
CBrowserLanguagesMediator::AppendLanguageCode(	const char *originalString,
													const char *stringToAdd)
{
	int		originalLength = originalString ? strlen(originalString) : 0;
	int		lengthToAdd;
	char	*occur = strchr(stringToAdd, '[');
	char	*occurRight;
	if (occur)
	{
		++occur;
		occurRight = strchr(occur, ']');
		if (occurRight)
		{
			lengthToAdd = occurRight - occur;
		}
	}
	else
	{
		lengthToAdd = strlen(stringToAdd);
	}
	char	*result = nil;
	if (originalLength || lengthToAdd)
	{
		const	char *	const junctionString = ", ";
		result = (char *)XP_ALLOC(	originalLength +
									lengthToAdd +
									strlen(junctionString) + 1);
		result[0] = '\0';
		if (originalLength)
		{
			strcpy(result, originalString);
			strcat(result, junctionString);
		}
		if (lengthToAdd)
		{
			const char	*source = occur ? occur : stringToAdd;
			strncat(result, source, lengthToAdd);
		}
	}
	return result;
}

void CBrowserLanguagesMediator::SetPrefWithLanguageList(const char *prefName,
														PaneIDT	/*languageListID*/,
														Boolean	locked)
{
	if (!locked)
	{
		char	*prefString = nil;
		CDragOrderTextList *languageList =
				(CDragOrderTextList *)FindPaneByID(eLanguageList);
		XP_ASSERT(languageList);
		TableIndexT	rows, cols;
		languageList->GetTableSize(rows, cols);

		STableCell	iCell(1, 1);
		for (int i = 0; i < rows; ++i)
		{
			Str255	languageStr;
			Uint32	dataSize = sizeof(Str255);
			languageList->GetCellData(iCell, languageStr, dataSize);
			++iCell.row;
			languageStr[languageStr[0] + 1] = '\0';	// &languageStr[1] is now a C string
			char	*temp = prefString;
			prefString = AppendLanguageCode(prefString, (char *)&languageStr[1]);
			if (temp)
			{
				XP_FREE(temp);
			}
		}

		// only set if different than current
		char	terminator = '\0';
		char	*currentValue;

		int		prefResult = PREF_CopyCharPref(prefName, &currentValue);
		if (prefResult != PREF_NOERROR)
		{
			currentValue = &terminator;
		}
		if (prefResult != PREF_NOERROR || strcmp(currentValue, prefString))
		{
			PREF_SetCharPref(prefName, prefString);
			XP_FREE(prefString);
		}
		if (currentValue && (currentValue != &terminator))
		{
			XP_FREE(currentValue);
		}
	}
}

void
CBrowserLanguagesMediator::UpdateButtons(LTextColumn *languageList)
{
	if (!mLanguagesLocked)
	{
		if (!languageList)
		{
			languageList = (CDragOrderTextList *)FindPaneByID(eLanguageList);
			XP_ASSERT(languageList);
		}
		STableCell		currentCell;
		currentCell = languageList->GetFirstSelectedCell();
		LButton	*button =
				(LButton *)FindPaneByID(eDeleteLanguageButton);
		XP_ASSERT(button);
		if (currentCell.row)
		{
			button->Enable();
		}
		else
		{
			button->Disable();
		}
	}
}

void
CBrowserLanguagesMediator::FillAddList(LTextColumn *list)
{
	int i;
	for (i = 0; i < mBuiltInCount; ++i)
	{
		char	*displayString;
		const	char *	const	formatString = "%s [%s]";
		int	newStringLength =	strlen(mLangaugeCodeArray[i]) +
								strlen(mLanguageStringArray[i]) +
								strlen(formatString) -
								4;	// the two "%s"s in the format string
		displayString = (char *)XP_ALLOC(newStringLength + 1);
		sprintf(displayString, formatString, mLanguageStringArray[i], mLangaugeCodeArray[i]);
		Str255		pDisplayString;
		BlockMoveData(displayString, &pDisplayString[1], newStringLength);
		XP_FREE(displayString);
		pDisplayString[0] = newStringLength;
		list->InsertRows(1, i, &pDisplayString, sizeof(pDisplayString), false);
	}
}


Boolean
CBrowserLanguagesMediator::GetNewLanguage(char *&newLanguage)
{
	Boolean	result;

	StDialogHandler	handler(eAddLanguageDialogID, sWindow->GetSuperCommander());
	LWindow			*dialog = handler.GetDialog();

	mAddLanguageList =
		(LTextColumn *)dialog->FindPaneByID(eAddLanguageList);
	XP_ASSERT(mAddLanguageList);
	CEditFieldControl	*theField =
		(CEditFieldControl *)dialog->FindPaneByID(eAddLanguageOtherEditField);
	XP_ASSERT(theField);

	mOtherTextEmpty = true;

	mAddLanguageList->AddListener(&handler);
	theField->AddListener(this);

	FillAddList(mAddLanguageList);
	MessageT message = msg_Nothing;
	do
	{
		message = handler.DoDialog();
		if ('2sel' == message)
		{
			theField->SetDescriptor("\p");
			mOtherTextEmpty = true;
			message = msg_OK;
		}
		if (msg_Nothing != message &&
			msg_OK != message  &&
			msg_Cancel != message)
		{
			message = msg_Nothing;
		}
	} while (msg_Nothing == message);

	if (msg_OK == message)
	{
		Str255	newLanguageName;
		theField->GetDescriptor(newLanguageName);
		int		newLanguageNameSize = newLanguageName[0];
		if (newLanguageNameSize)
		{
			newLanguage = (char *)XP_ALLOC(newLanguageNameSize + 1);
			BlockMoveData(&newLanguageName[1], newLanguage, newLanguageNameSize);
			newLanguage[newLanguageNameSize] = '\0';
			result = true;
		}
		else
		{
			STableCell	selectedCell = mAddLanguageList->GetFirstSelectedCell();
			Str255		selectedLanguage;
			if (selectedCell.row)
			{
				long	dataSize = sizeof(selectedLanguage);
				mAddLanguageList->GetCellData(selectedCell, selectedLanguage, dataSize);
				newLanguage = (char *)XP_ALLOC(selectedLanguage[0] + 1);
				BlockMoveData(&selectedLanguage[1], newLanguage, selectedLanguage[0]);
				newLanguage[selectedLanguage[0]] = '\0';
				result = true;
			}
			else
			{
				result = false;
			}
		}
	}
	else
	{
		result = false;
	}
	return result;
}

Boolean
CBrowserLanguagesMediator::GetNewUniqueLanguage(char *&newLanguage)
{
	Boolean	result = GetNewLanguage(newLanguage);
	if (result)
	{
		char	*tempString = newLanguage;
		newLanguage = GetLanguageDisplayString(tempString);
		XP_FREE(tempString);

		CDragOrderTextList *languageList =
				(CDragOrderTextList *)FindPaneByID(eLanguageList);
		XP_ASSERT(languageList);
		TableIndexT	rows, cols;
		languageList->GetTableSize(rows, cols);

		STableCell	iCell(1, 1);
		for (int i = 0; i < rows; ++i)
		{
			Str255	languageStr;
			Uint32	dataSize = sizeof(Str255);
			languageList->GetCellData(iCell, languageStr, dataSize);
			++iCell.row;
			LStr255	pNewLangage(newLanguage);
			if (!pNewLangage.CompareTo(languageStr))	// we already have this lanuage
			{
				result = false;
				XP_FREE(newLanguage);
				break;
			}
		}
	}
	return result;
}


void
CBrowserLanguagesMediator::ListenToMessage(MessageT inMessage, void *ioParam)
{
	switch (inMessage)
	{
		case CEditFieldControl::msg_ChangedText:
			Str255	languageStr;
			((CEditFieldControl *)ioParam)->GetDescriptor(languageStr);
			if ((mOtherTextEmpty && languageStr[0]) ||	// The value of mOtherTextEmpty
				(!mOtherTextEmpty && !languageStr[0]))	// needs to change.
			{
//				STableCell	selected = mAddLanguageList->GetFirstSelectedCell();
//				if (selected.row)
//				{
					if (mOtherTextEmpty)
					{
						mOtherTextEmpty = false;
						mAddLanguageList->Deactivate();
					}
					else
					{
						mOtherTextEmpty = true;
						mAddLanguageList->Activate();
					}
//				}
			}
			break;
		case eSelectionChanged:
			UpdateButtons((CDragOrderTextList *)ioParam);
			break;
		case eAddLanguageButton:
			XP_ASSERT(!mLanguagesLocked);
			char	*newLanguage;
			if (GetNewUniqueLanguage(newLanguage))
			{
				CDragOrderTextList *languageList =
						(CDragOrderTextList *)FindPaneByID(eLanguageList);
				XP_ASSERT(languageList);
				STableCell		currentCell;
				currentCell = languageList->GetFirstSelectedCell();
				TableIndexT	insertAfter = currentCell.row;
				if (!insertAfter)
				{
					// add it at the end
					TableIndexT	cols;
					languageList->GetTableSize(insertAfter, cols);
				}
				LStr255	pNewLanguage(newLanguage);
				languageList->InsertRows(	1,
											insertAfter,
											pNewLanguage,
											sizeof(Str255),
											true);
				if (newLanguage)
				{
					XP_FREE(newLanguage);
				}
				UpdateButtons(languageList);
			}
			break;
		case eDeleteLanguageButton:
			XP_ASSERT(!mLanguagesLocked);
			CDragOrderTextList *languageList =
					(CDragOrderTextList *)FindPaneByID(eLanguageList);
			XP_ASSERT(languageList);
			STableCell		currentCell;
			currentCell = languageList->GetFirstSelectedCell();
			languageList->RemoveRows(1, currentCell.row, true);
			UpdateButtons(languageList);
			break;
		default:
			CPrefsMediator::ListenToMessage(inMessage, ioParam);
			break;
	}
}


void
CBrowserLanguagesMediator::LoadBuiltInArrays()
{
	// I guess we can tell how many strings are in a string list from the first
	// two bytes of the resource. Is this documented?
	short	**countHandle = (short **)GetResource('STR#', eBuiltInLanguageStringResID);
	XP_ASSERT(countHandle);
	mBuiltInCount = **countHandle;
	mLanguageStringArray = (char **)XP_ALLOC(mBuiltInCount * sizeof(char *));
	mLangaugeCodeArray = (char **)XP_ALLOC(mBuiltInCount * sizeof(char *));
	int i;
	for (i = 0; i < mBuiltInCount; ++i)
	{
		Str255	builtInString; Str255	builtInCode;

		::GetIndString(builtInString, eBuiltInLanguageStringResID, i + 1);
		::GetIndString(builtInCode, eBuiltInLanguageCodeResID, i + 1);
		mLanguageStringArray[i] = (char *)XP_ALLOC(builtInString[0] + 1);
		mLangaugeCodeArray[i] = (char *)XP_ALLOC(builtInCode[0] + 1);
		::BlockMoveData(&builtInString[1], mLanguageStringArray[i], (Size)builtInString[0]);
		::BlockMoveData(&builtInCode[1], mLangaugeCodeArray[i], (Size)builtInCode[0]);
		mLanguageStringArray[i][builtInString[0]] = '\0';
		mLangaugeCodeArray[i][builtInCode[0]] = '\0';
	}
}

//-----------------------------------
void CBrowserLanguagesMediator::LoadMainPane()
//-----------------------------------
{
	CPrefsMediator::LoadMainPane();
	CDragOrderTextList *languageList =
			(CDragOrderTextList *)FindPaneByID(eLanguageList);
	XP_ASSERT(languageList);
	languageList->AddListener(this);
	UpdateButtons(languageList);
}

//-----------------------------------
void CBrowserLanguagesMediator::LoadPrefs()
//-----------------------------------
{
	LoadBuiltInArrays();
	SetLanguageListWithPref("intl.accept_languages",
							eLanguageList,
							mLanguagesLocked);
}

//-----------------------------------
void CBrowserLanguagesMediator::WritePrefs()
//-----------------------------------
{
	SetPrefWithLanguageList("intl.accept_languages",
							eLanguageList,
							mLanguagesLocked);
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

CBrowserApplicationsMediator::CBrowserApplicationsMediator(LStream*)
:	CPrefsMediator(class_ID)
,	mModified(false)
,	mMIMETable(nil)
{
}

void
CBrowserApplicationsMediator::EditMimeEntry()
{
//	StBlockingDialogHandler	theHandler(eEditTypeDialogResID, this);
	StDialogHandler	theHandler(eEditTypeDialogResID, sWindow->GetSuperCommander());
	CEditMIMEWindow* theDialog = (CEditMIMEWindow*)theHandler.GetDialog();
	XP_ASSERT(theDialog);
	UReanimator::LinkListenerToControls(theDialog, theDialog, eEditTypeDialogResID);
	theDialog->SetMimeTable(mMIMETable);
	
	// Get the info that the dialog should display
	CMIMEListPane::PrefCellInfo cellInfo;
	TableCellT	cellToEdit;
	mMIMETable->GetSelectedCell(cellToEdit);
	mMIMETable->GetCellData(cellToEdit, &cellInfo);
	
	// Make a copy of the info to edit
	CMIMEListPane::PrefCellInfo newInfo;
	newInfo.fMapper = new CMimeMapper(*cellInfo.fMapper); 
	newInfo.fIconInfo = cellInfo.fIconInfo;
	theDialog->SetCellInfo(newInfo);

	// Let the user have at it...
	theDialog->Show();
	MessageT theMessage = msg_Nothing;	
	while ((theMessage != msg_Cancel) && (theMessage != msg_OK))
	{
		theMessage = theHandler.DoDialog();
		
		if (theMessage == msg_OK &&
			cellInfo.fMapper->GetMimeName() != newInfo.fMapper->GetMimeName() &&
			mMIMETable->MimeTypeExists(newInfo.fMapper))
		{
			ErrorManager::PlainAlert(mPREFS_DUPLICATE_MIME);
			theMessage = msg_Nothing;
		}
		// The CEditMIMEWindow will handle all other messages
	}
	
	// Process the munged data as appropriate
	if (theMessage == msg_OK && theDialog->Modified())
	{
		delete cellInfo.fMapper;							// Delete the old mapper
		newInfo = theDialog->GetCellInfo();					// Get the edited info
		mMIMETable->SetCellData(cellToEdit, &newInfo);		// Write the edited info into the table
		mModified = TRUE;									// Remember that something changed
		mMIMETable->Refresh();								// Let table sync to the new data
	}
	else
	{
		delete newInfo.fMapper;								// Delete the changed but discarded mapper
		newInfo.fMapper = NULL;
	}	
}

void
CBrowserApplicationsMediator::NewMimeEntry()
{
//	StBlockingDialogHandler	theHandler(eEditTypeDialogResID, this);
	StDialogHandler	theHandler(eEditTypeDialogResID, sWindow->GetSuperCommander());
	CEditMIMEWindow* theDialog = (CEditMIMEWindow *)theHandler.GetDialog();
	XP_ASSERT(theDialog);
	UReanimator::LinkListenerToControls(theDialog, theDialog, eEditTypeDialogResID);
	theDialog->SetMimeTable(mMIMETable);
	
	// Create a new default mapper and put it in the table
	CStr255 emptyType = "";
	CMimeMapper* newMapper = CPrefs::CreateDefaultUnknownMapper(emptyType, FALSE);
	ThrowIfNil_(newMapper);
	ThrowIfNil_(mMIMETable);
	CMIMEListPane::PrefCellInfo cellInfo;
	mMIMETable->InsertRows(1, 0, &cellInfo);
	mMIMETable->BindCellToApplication(1, newMapper);

	TableCellT newCell;
	newCell.row = newCell.col = 1;
	mMIMETable->GetCellData(newCell, &cellInfo);
	theDialog->SetCellInfo(cellInfo);
	
	// Let the user have at it...
	theDialog->Show();
	MessageT theMessage = msg_Nothing;	
	while ((theMessage != msg_Cancel) && (theMessage != msg_OK))
	{
		theMessage = theHandler.DoDialog();
		
		if (theMessage == msg_OK && mMIMETable->MimeTypeExists(cellInfo.fMapper))
		{
			ErrorManager::PlainAlert(mPREFS_DUPLICATE_MIME);
			theMessage = msg_Nothing;
		}
		// The CEditMimeWindow will handle all other messages
	}
	
	// Process the munged data as appropriate
	CMIMEListPane::PrefCellInfo newInfo = theDialog->GetCellInfo();
	if (theMessage == msg_OK && theDialog->Modified())
	{
		mModified = TRUE;								// Remember that something changed
		mMIMETable->SetCellData(newCell, &newInfo);		// Write the edited info into the table
		mMIMETable->SelectCell(newCell);				// Select the new cell
		mMIMETable->ScrollImageTo(0, 0, TRUE);
		mMIMETable->Refresh();							// Let table sync to the new data
	}
	else
	{
		delete newInfo.fMapper;							// Delete the discarded mapper
		mMIMETable->RemoveRows(1, newCell.row);			// Remove the row added to the table
	}	
}

void
CBrowserApplicationsMediator::DeleteMimeEntry()
{
	if (ErrorManager::PlainConfirm((const char*) GetCString(DELETE_MIMETYPE)))
	{
		XP_ASSERT(mMIMETable);
		TableCellT cellToDelete;
		mMIMETable->GetSelectedCell(cellToDelete);
		PrefCellInfo cellInfo;
		mMIMETable->GetCellData(cellToDelete, &cellInfo);
		
		// Instead of freeing the item, add it to a list of deleted mime types
		// and at commit time, delete the items from xp pref storage.
		mDeletedTypes.LArray::InsertItemsAt(1, LArray::index_Last, &cellInfo.fMapper);
		
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

void
CBrowserApplicationsMediator::ListenToMessage(MessageT inMessage, void *ioParam)
{
	switch (inMessage)
	{
		case eHelperDeleteButton:
			DeleteMimeEntry();
			break;
		case eHelperNewButton:
			NewMimeEntry();
			break;
		case eHelperEditButton:
		case CMIMEListPane::msg_DoubleClick:
			EditMimeEntry();
			break;
		case CMIMEListPane::msg_SelectionChanged:
			LButton	*deleteButton = (LButton *)FindPaneByID(eHelperDeleteButton);
			XP_ASSERT(deleteButton);
			LButton	*editButton = (LButton *)FindPaneByID(eHelperEditButton);
			XP_ASSERT(editButton);

			XP_ASSERT(ioParam);
			CMIMEListPane	*mimeTable = (CMIMEListPane *)ioParam;
			TableCellT	cell;
			mimeTable->GetSelectedCell(cell);
			Boolean inactive;
			if (cell.row > 0)
			{
				PrefCellInfo cellInfo;
				mimeTable->GetCellData(cell, &cellInfo);

				inactive = CMimeMapper::NetscapeCanHandle(cellInfo.fMapper->GetMimeName());
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
				deleteButton->Enable();
			break;
		default:
			CPrefsMediator::ListenToMessage(inMessage, ioParam);
			break;
	}
}

void
CBrowserApplicationsMediator::LoadMainPane()
{
	CPrefsMediator::LoadMainPane();
	mMIMETable = (CMIMEListPane *)FindPaneByID(eHelperTable);
	XP_ASSERT(mMIMETable);
	mMIMETable->AddListener(this);
	TableCellT	currentCell= {1, 1};		// This has to be done after
	mMIMETable->SelectCell(currentCell);	// mMIMETable->AddListener(); to set the buttons.
}

void
CBrowserApplicationsMediator::WritePrefs()
{
	if (mModified)
	{
		CPrefs::sMimeTypes.DeleteAll(FALSE);
		XP_ASSERT(mMIMETable);
		TableIndexT rows, cols;
		mMIMETable->GetTableSize(rows, cols);
		for (int i = 1; i <= rows; i++)
		{
			CMIMEListPane::PrefCellInfo cellInfo;
			mMIMETable->GetCellInfo(cellInfo, i);
			
			cellInfo.fMapper->WriteMimePrefs();
			CMimeMapper* mapper = new CMimeMapper(*cellInfo.fMapper);
			CPrefs::sMimeTypes.InsertItemsAt(1, LArray::index_Last, &mapper);
		}
		for (Int32 i = 1; i <= mDeletedTypes.GetCount(); i++)
		{
			CMimeMapper* mapper;
			mDeletedTypes.FetchItemAt(i, &mapper);
			PREF_DeleteBranch(mapper->GetBasePref());
		}
	}
}


enum
{
	eAuthorNameEditField = 13201,
	eAutoSaveCheckBox,
	eAutoSaveIntervalEditField,
	eNewDocTemplateURLEditField,
	eRestoreDefaultButton,
	eChooseLocalFileButton,
	eUseHTMLSourceEditorBox,
	eHTMLSourceEditorFilePicker,
	eUseImageEditorBox,
	eImageEditorFilePicker
};

#ifdef EDITOR

CEditorMainMediator::CEditorMainMediator(LStream*)
:	CPrefsMediator(class_ID)
,	mNewDocURLLocked(false)
{
}

void
CEditorMainMediator::ListenToMessage(MessageT inMessage, void *ioParam)
{
	switch (inMessage)
	{
		case eUseHTMLSourceEditorBox:
			CFilePicker *fPicker =
					(CFilePicker *)FindPaneByID(eHTMLSourceEditorFilePicker);
			XP_ASSERT(fPicker);
			if (!fPicker->WasSet() && *(Int32 *)ioParam)
			{	// If the user has clicked the checkbox and the file picker is not set
				// then we may want to trigger the file picker
				if (mNeedsPrefs)
				{
					// If mNeedsPrefs, then we are setting up the pane. If the picker
					// is not set (can happen if the app file was physically deleted),
					// then we need to unset the "use" check box.
					LGACheckbox *checkbox =
							(LGACheckbox *)FindPaneByID(inMessage);
					XP_ASSERT(checkbox);
					checkbox->SetValue(false);
				}
				else
				{
					fPicker->ListenToMessage(msg_Browse, nil);
					if (!fPicker->WasSet())
					{	// If the file picker is still unset, that means that the user
						// cancelled the file browse so we don't want the checkbox set.
						LGACheckbox *checkbox =
								(LGACheckbox *)FindPaneByID(inMessage);
						XP_ASSERT(checkbox);
						checkbox->SetValue(false);
					}
				}
			}
			break;
		case eUseImageEditorBox:
			fPicker = (CFilePicker *)FindPaneByID(eImageEditorFilePicker);
			XP_ASSERT(fPicker);
			if (!fPicker->WasSet() && *(Int32 *)ioParam)
			{	// If the user has clicked the checkbox and the file picker is not set
				// then we may want to trigger the file picker
				if (mNeedsPrefs)
				{
					// If mNeedsPrefs, then we are setting up the pane. If the picker
					// is not set (can happen if the app file was physically deleted),
					// then we need to unset the "use" check box.
					LGACheckbox *checkbox =
							(LGACheckbox *)FindPaneByID(inMessage);
					XP_ASSERT(checkbox);
					checkbox->SetValue(false);
				}
				else
				{
					fPicker->ListenToMessage(msg_Browse, nil);
					if (!fPicker->WasSet())
					{	// If the file picker is still unset, that means that the user
						// cancelled the file browse so we don't want the checkbox set.
						LGACheckbox *checkbox =
								(LGACheckbox *)FindPaneByID(inMessage);
						XP_ASSERT(checkbox);
						checkbox->SetValue(false);
					}
				}
			}
			break;
		case msg_FolderChanged:
			PaneIDT	checkBoxID;
			switch (((CFilePicker *)ioParam)->GetPaneID())
			{
				case eHTMLSourceEditorFilePicker:
					checkBoxID = eUseHTMLSourceEditorBox;
					break;
				case eImageEditorFilePicker:
					checkBoxID = eUseImageEditorBox;
					break;
			}
			LGACheckbox *checkbox = (LGACheckbox *)FindPaneByID(checkBoxID);
			XP_ASSERT(checkbox);
			checkbox->SetValue(true);
			break;
		case eChooseLocalFileButton:
			SetEditFieldWithLocalFileURL(	eNewDocTemplateURLEditField,
											mNewDocURLLocked);
			break;
		case eRestoreDefaultButton:
			RestoreDefaultURL();
			break;
		default:
			CPrefsMediator::ListenToMessage(inMessage, ioParam);
			break;
	}
}

void
CEditorMainMediator::RestoreDefaultURL()
{
	ReadDefaultPref(eNewDocTemplateURLEditField);
}

Boolean
CEditorMainMediator::SaveIntervalValidationFunc(CValidEditField *saveInterval)
{
	// If the checkbox isn't set then this value is really
	// ignored, so I will only put up the alert if the checkbox
	// is set, but I will force a valid value in any case.

	Boolean		result = true;

	// force valid value
	if (1 > saveInterval->GetValue())
	{
		int32	newInterval = 10;
		PREF_GetDefaultIntPref("editor.auto_save_delay", &newInterval);
		saveInterval->SetValue(newInterval);
		saveInterval->SelectAll();
		result = false;
	}
	if (!result)	// if the value is within the range, who cares
	{
		// Check for the check box...
		// We are assuming that the checkbox is a sub of the field's superview.
		LView	*superView = saveInterval->GetSuperView();
		XP_ASSERT(superView);
		LGACheckbox	*checkbox =
				(LGACheckbox *)superView->FindPaneByID(eAutoSaveCheckBox);
		XP_ASSERT(checkbox);
		if (checkbox->GetValue())
		{
			StPrepareForDialog	prepare;
			::StopAlert(1067, NULL);
		}
		else
		{
			result = true;	// go ahead and let them switch (after correcting the value)
							// if the checkbox isn't set
		}
	}
	return result;
}

//-----------------------------------
void CEditorMainMediator::LoadMainPane()
//-----------------------------------
{
	CPrefsMediator::LoadMainPane();
	SetValidationFunction(eAutoSaveIntervalEditField, SaveIntervalValidationFunc);
}

#endif // EDITOR

enum
{
	eMaintainLinksBox = 13401,
	eKeepImagesBox,
	ePublishLocationField,
	eBrowseLocationField
};

enum
{
	eOnlineRButton = 13501,
	eOfflineRButton,
	eAskMeRButton
};

enum
{
	eImagesBox = 13901,
	eJaveBox,
	eJavaScriptBox,
	eStyleSheetBox,
	eAutoInstallBox,
	eFTPPasswordBox,
	eCookieAlwaysRButton,
	eCookieOriginatingServerRButton,
	eCookieNeverRButton,
	eCookieWarningBox
};

enum
{
	eDiskCacheEditField = 14101,
	eClearDiskCacheButton,
	eOncePerSessionRButton,
	eEveryUseRButton,
	eNeverRButton,
	eCacheLocationFilePicker
};

CAdvancedCacheMediator::CAdvancedCacheMediator(LStream*)
:	CPrefsMediator(class_ID)
{
}

void CAdvancedCacheMediator::ListenToMessage(MessageT inMessage, void *ioParam)
{
	switch (inMessage)
	{
		case eClearDiskCacheButton:
			ClearDiskCacheNow();
			break;
		default:
			CPrefsMediator::ListenToMessage(inMessage, ioParam);
			break;
	}
}

void CAdvancedCacheMediator::ClearDiskCacheNow()
{
	int32 originalDiskCacheSize;	// in Kbytes
	PREF_GetIntPref("browser.cache.disk_cache_size", &originalDiskCacheSize);
	if (originalDiskCacheSize)	// if the current cache size is zero then do nothing
	{
		UDesktop::Deactivate();
		short	response = ::CautionAlert(eClearDiskCacheButton, NULL);	
		UDesktop::Activate();
		if (1 == response)
		{
			TrySetCursor(watchCursor);
			
			// This is the approved way to clear the cache
	        PREF_SetIntPref("browser.cache.disk_cache_size", 0);
	        PREF_SetIntPref("browser.cache.disk_cache_size", originalDiskCacheSize);
	        
			::SetCursor( &UQDGlobals::GetQDGlobals()->arrow );
		}
	}
}

enum
{
	eManualProxyDialogResID = 12006,
	eDirectConnectRButton = 14201,
	eManualRButton,
	eViewProxyConfigButton,
	eAutoProxyConfigRButton,
	eConfigURLTextBox,
	eReloadButton,
	eFTPEditField,
	eFTPPortEditField,
	eGopherEditField,
	eGopherPortEditField,
	eHTTPEditField,
	eHTTPPortEditField,
	eSSLEditField,
	eSSLPortEditField,
	eWAISEditField,
	eWAISPortEditField,
	eNoProxyEditField,
	eSOCKSEditField,
	eSOCKSPortEditField
};

CAdvancedProxiesMediator::Protocol::Protocol():
mProtocolServer(nil),
mServerLocked(false),
mProtocolPort(0),
mPortLocked(false)
{
}

CAdvancedProxiesMediator::Protocol::Protocol(const Protocol& original):
mProtocolServer(nil),
mServerLocked(original.mServerLocked),
mProtocolPort(original.mProtocolPort),
mPortLocked(original.mPortLocked)
{
	if (original.mProtocolServer)
	{
		mProtocolServer = XP_STRDUP(original.mProtocolServer);
	}
	// else set to nil above
}

CAdvancedProxiesMediator::Protocol::~Protocol()
{
	if (mProtocolServer)
	{
		XP_FREE(mProtocolServer);
	}
}

void
CAdvancedProxiesMediator::Protocol::LoadFromPref(const char *serverPrefName,
													const char *portPrefName)
{
	mServerLocked	= PREF_PrefIsLocked(serverPrefName);
	mPortLocked		= PREF_PrefIsLocked(portPrefName);
	PREF_CopyCharPref(serverPrefName, &mProtocolServer);
	PREF_GetIntPref(portPrefName, &mProtocolPort);
}

void
CAdvancedProxiesMediator::Protocol::WriteToPref(	const char *serverPrefName,
													const char *portPrefName)
{
	if (!mServerLocked)
	{
		char	*currentStringValue = nil;
		PREF_CopyCharPref(serverPrefName, &currentStringValue);
		if (!currentStringValue || strcmp(currentStringValue, mProtocolServer))
		{
			PREF_SetCharPref(serverPrefName, mProtocolServer);
		}
		if (currentStringValue)
		{
			XP_FREE(currentStringValue);
		}
	}
	if (!mPortLocked)
	{
		int32	currentIntValue;
		currentIntValue = mProtocolPort + 1;	// anything != mCheckInterval
		PREF_GetIntPref(portPrefName, &currentIntValue);
		if (currentIntValue != mProtocolPort)
		{
			PREF_SetIntPref(portPrefName, mProtocolPort);
		}
	}
}

void
CAdvancedProxiesMediator::Protocol::PreEdit(	LView *dialog,
												ResIDT serverEditID,
												ResIDT portEditID)
{
	LEditField		*server =
						(LEditField *)dialog->FindPaneByID(serverEditID);
	XP_ASSERT(server);
	if (mProtocolServer)
	{
		LStr255	protocolServerPStr(mProtocolServer);
		server->SetDescriptor(protocolServerPStr);
		server->SelectAll();
	}
	if (mServerLocked)
	{
		server->Disable();
	}
	LEditField *port =
					(LEditField *)dialog->FindPaneByID(portEditID);
	XP_ASSERT(port);
	port->SetValue(mProtocolPort);
	port->SelectAll();
	if (mPortLocked)
	{
		port->Disable();
	}
}

void
CAdvancedProxiesMediator::Protocol::PostEdit(LView *dialog,
												ResIDT serverEditID,
												ResIDT portEditID)
{
	LEditField		*server =
						(LEditField *)dialog->FindPaneByID(serverEditID);
	XP_ASSERT(server);
	Str255	fieldValue;
	if (!mServerLocked)
	{
		server->GetDescriptor(fieldValue);
		int		stringLength = fieldValue[0];
		char	*serverString = (char *)XP_ALLOC(stringLength + 1);
		if (serverString)
		{
			strncpy(serverString, (char *)&fieldValue[1], stringLength + 1);
			serverString[stringLength] = '\0';
			if (mProtocolServer)
			{
				XP_FREE(mProtocolServer);
			}
			mProtocolServer = serverString;
		}
	}
	LEditField *port =
					(LEditField *)dialog->FindPaneByID(portEditID);
	XP_ASSERT(port);
	if (!mPortLocked)
	{
		mProtocolPort = port->GetValue();
	}
}



CAdvancedProxiesMediator::ManualProxy::ManualProxy():
mNoProxyList(nil),
mNoProxyListLocked(false)
{
}

CAdvancedProxiesMediator::ManualProxy::ManualProxy(const ManualProxy& original):
mFTP(original.mFTP),
mGopher(original.mGopher),
mHTTP(original.mHTTP),
mSSL(original.mSSL),
mWAIS(original.mWAIS),
mNoProxyList(nil),
mNoProxyListLocked(original.mNoProxyListLocked),
mSOCKS(original.mSOCKS)
{
	if (original.mNoProxyList)
	{
		mNoProxyList = XP_STRDUP(original.mNoProxyList);
	}
	// else set to nil above
}

CAdvancedProxiesMediator::ManualProxy::~ManualProxy()
{
	XP_FREEIF(mNoProxyList);
}

void
CAdvancedProxiesMediator::ManualProxy::LoadPrefs()
{
	mFTP.LoadFromPref("network.proxy.ftp", "network.proxy.ftp_port");
	mGopher.LoadFromPref("network.proxy.gopher", "network.proxy.gopher_port");
	mHTTP.LoadFromPref("network.proxy.http", "network.proxy.http_port");
	mWAIS.LoadFromPref("network.proxy.wais", "network.proxy.wais_port");
	mSSL.LoadFromPref("network.proxy.ssl", "network.proxy.ssl_port");

	mNoProxyListLocked		= PREF_PrefIsLocked("network.proxy.no_proxies_on");
	PREF_CopyCharPref("network.proxy.no_proxies_on", &mNoProxyList);

	mSOCKS.LoadFromPref("network.hosts.socks_server", "network.hosts.socks_serverport");
}

void
CAdvancedProxiesMediator::ManualProxy::WritePrefs()
{
	mFTP.WriteToPref("network.proxy.ftp", "network.proxy.ftp_port");
	mGopher.WriteToPref("network.proxy.gopher", "network.proxy.gopher_port");
	mHTTP.WriteToPref("network.proxy.http", "network.proxy.http_port");
	mWAIS.WriteToPref("network.proxy.wais", "network.proxy.wais_port");
	mSSL.WriteToPref("network.proxy.ssl", "network.proxy.ssl_port");

	char	*currentStringValue = nil;
	if (!mNoProxyListLocked)
	{
		PREF_CopyCharPref("network.proxy.no_proxies_on", &currentStringValue);
		if (!currentStringValue || strcmp(currentStringValue, mNoProxyList))
		{
			PREF_SetCharPref("network.proxy.no_proxies_on", mNoProxyList);
		}
		if (currentStringValue)
		{
			XP_FREE(currentStringValue);
		}
	}
	mSOCKS.WriteToPref("network.hosts.socks_server", "network.hosts.socks_serverport");
}


Boolean
CAdvancedProxiesMediator::ManualProxy::EditPrefs()
{
	Boolean	result = false;

	StDialogHandler	handler(eManualProxyDialogResID, nil);
	LWindow			*dialog = handler.GetDialog();

	mFTP.PreEdit(dialog, eFTPEditField, eFTPPortEditField);
	mGopher.PreEdit(dialog, eGopherEditField, eGopherPortEditField);
	mHTTP.PreEdit(dialog, eHTTPEditField, eHTTPPortEditField);
	mSSL.PreEdit(dialog, eSSLEditField, eSSLPortEditField);
	mWAIS.PreEdit(dialog, eWAISEditField, eWAISPortEditField);

	LEditField		*noProxy =
						(LEditField *)dialog->FindPaneByID(eNoProxyEditField);
	XP_ASSERT(noProxy);
	if (mNoProxyList)
	{
		LStr255	noProxyPStr(mNoProxyList);
		noProxy->SetDescriptor(noProxyPStr);
		noProxy->SelectAll();
	}
	if (mNoProxyListLocked)
	{
		noProxy->Disable();
	}

	mSOCKS.PreEdit(dialog, eSOCKSEditField, eSOCKSPortEditField);

	// Run the dialog
	MessageT message = msg_Nothing;
	do
	{
		message = handler.DoDialog();
		if (msg_Nothing != message && msg_OK != message  && msg_Cancel != message)
		{
			message = msg_Nothing;
		}
	} while (msg_Nothing == message);

	if (msg_OK == message)
	{
		result = true;
		// read all the controls back to the struct

		mFTP.PostEdit(dialog, eFTPEditField, eFTPPortEditField);
		mGopher.PostEdit(dialog, eGopherEditField, eGopherPortEditField);
		mHTTP.PostEdit(dialog, eHTTPEditField, eHTTPPortEditField);
		mSSL.PostEdit(dialog, eSSLEditField, eSSLPortEditField);
		mWAIS.PostEdit(dialog, eWAISEditField, eWAISPortEditField);

		Str255	fieldValue;
		if (!mNoProxyListLocked)
		{
			noProxy->GetDescriptor(fieldValue);
			int		stringLength = fieldValue[0];
			char	*noProxyString = (char *)XP_ALLOC(stringLength + 1);
			if (noProxyString)
			{
				strncpy(noProxyString, (char *)&fieldValue[1], stringLength + 1);
				noProxyString[stringLength] = '\0';
				if (mNoProxyList)
				{
					XP_FREE(mNoProxyList);
				}
				mNoProxyList = noProxyString;
			}
		}
		mSOCKS.PostEdit(dialog, eSOCKSEditField, eSOCKSPortEditField);
	}
	else
	{
		result = false;
	}
	return result;
}


CAdvancedProxiesMediator::CAdvancedProxiesMediator(LStream*)
:	CPrefsMediator(class_ID)
,	mManualProxy(nil)
{
}

void
CAdvancedProxiesMediator::ListenToMessage(MessageT inMessage, void *ioParam)
{
	switch (inMessage)
	{
		case msg_ControlClicked:
			break;
		case eViewProxyConfigButton:
			LGARadioButton	*theButton = (LGARadioButton *)FindPaneByID(eManualRButton);
			XP_ASSERT(theButton);
			if (!theButton->GetValue())
			{
				theButton->SetValue(true);
			}
			ManualProxy	*tempManualProxy;
			if (!mManualProxy)
			{
				tempManualProxy = new ManualProxy;
				tempManualProxy->LoadPrefs();
			}
			else
			{
				tempManualProxy = new ManualProxy(*mManualProxy);
			}
			if (tempManualProxy->EditPrefs())
			{
				if (mManualProxy)
				{
					delete mManualProxy;
				}
				mManualProxy = tempManualProxy;
			}
			else
			{
				delete tempManualProxy;
			}
			break;
		case eReloadButton:
			LEditField	*theField = (LEditField *)FindPaneByID(eConfigURLTextBox);
			XP_ASSERT(theField);
			Str255	fieldValue;
			theField->GetDescriptor(fieldValue);
			int		stringLength = fieldValue[0];
			char	*prefString = (char *)XP_ALLOC(stringLength + 1);
			XP_ASSERT(prefString);
			strncpy(prefString, (char *)&fieldValue[1], stringLength);
			prefString[stringLength] = '\0';
			NET_SetProxyServer(PROXY_AUTOCONF_URL, prefString);
			NET_ReloadProxyConfig(nil);	// The context is unused in the function.
			XP_FREE(prefString);
			break;
		default:
			CPrefsMediator::ListenToMessage(inMessage, ioParam);
			break;
	}
}

//-----------------------------------
void CAdvancedProxiesMediator::WritePrefs()
//-----------------------------------
{
	if (mManualProxy)
		mManualProxy->WritePrefs();
}

#ifdef MOZ_MAIL_NEWS
enum
{
	eMessageSizeLimitBox = 13801,
	eMessageSizeLimitEditField,
	eKeepByDaysRButton,
	eKeepDaysEditField,
	eKeepAllRButton,
	eKeepByCountRButton,
	eKeepCountEditField,
	eKeepOnlyUnreadBox,
	eCompactingBox,
	eCompactingEditField
};

CAdvancedDiskSpaceMediator::CAdvancedDiskSpaceMediator(LStream*)
:	CPrefsMediator(class_ID)
{
}

void
CAdvancedDiskSpaceMediator::ListenToMessage(MessageT inMessage, void *ioParam)
{
	switch (inMessage)
	{
//		case eNewsMessagesMoreButton:
//			break;
		default:
			CPrefsMediator::ListenToMessage(inMessage, ioParam);
			break;
	}
}

// static
Boolean
CAdvancedDiskSpaceMediator::DiskSpaceValidationFunc(CValidEditField *editField)
{
	Boolean okeyDokey = true;

	// force valid value
	if (1 > editField->GetValue())
	{
		const char *prefName = nil;
		PaneIDT	controllingPaneID;
		switch (editField->GetPaneID())
		{
			case eMessageSizeLimitEditField:
				controllingPaneID = eMessageSizeLimitBox;
				prefName = "mail.max_size";
				break;
			case eKeepDaysEditField:
				controllingPaneID = eKeepByDaysRButton;
				prefName = "news.keep.days";
				break;
			case eKeepCountEditField:
				controllingPaneID = eKeepByCountRButton;
				prefName = "news.keep.count";
				break;
			case eCompactingEditField:
				controllingPaneID = eCompactingBox;
				prefName = "mail.purge_threshhold";
				break;
			default:
				break;	// XP_ASSERT()?
		}
		LControl* controllingPane = nil;
		if (controllingPaneID)
			controllingPane =
			(LControl *)((editField->GetSuperView())->FindPaneByID(controllingPaneID));
		XP_ASSERT(controllingPane);
		int32 newValue;
		PREF_GetDefaultIntPref(prefName, &newValue);
		editField->SetValue(newValue);
		editField->SelectAll();
		okeyDokey = (controllingPane->GetValue() == 0);
	}
	if (!okeyDokey)
	{											// If the value is within the range, or
		StPrepareForDialog prepare;				// the control is not on, who cares?
		::StopAlert(1067, NULL);
	}
	return okeyDokey;
}

//-----------------------------------
void CAdvancedDiskSpaceMediator::LoadMainPane()
//-----------------------------------
{
	CPrefsMediator::LoadMainPane();
	SetValidationFunction(eMessageSizeLimitEditField, DiskSpaceValidationFunc);
	SetValidationFunction(eKeepDaysEditField, DiskSpaceValidationFunc);
	SetValidationFunction(eKeepCountEditField, DiskSpaceValidationFunc);
	SetValidationFunction(eCompactingEditField, DiskSpaceValidationFunc);
}

#endif // MOZ_MAIL_NEWS

//-----------------------------------
void UAssortedPrefMediators::RegisterViewClasses()
//-----------------------------------
{
	RegisterClass_(CEditFieldControl);

	RegisterClass_( CEditMIMEWindow);
	RegisterClass_( CMIMEListPane);
	RegisterClass_(CColorButton);
	RegisterClass_( CFilePicker);

	RegisterClass_(COtherSizeDialog);
	RegisterClass_(CSizePopup);
	
	RegisterClass_( CGAEditBroadcaster);
	RegisterClass_(CValidEditField);
	RegisterClass_( LCicnButton);
//	RegisterClass_( 'sbox', (ClassCreatorFunc)OneClickLListBox::CreateOneClickLListBox );
	RegisterClass_(OneRowLListBox);	// added by ftang
	UPrefControls::RegisterPrefControlViews();
} // CPrefsDialog::RegisterViewClasses
