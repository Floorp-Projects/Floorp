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

#include "prefwutil.h"

#include "resgui.h"
#include "uprefd.h"
#include "macutil.h"
#include "ufilemgr.h"
#include "uerrmgr.h"
#include "xp_mem.h"
#include "macgui.h"
#include "COffscreenCaption.h"

Boolean	CFilePicker::sResult = FALSE;
Boolean	CFilePicker::sUseDefault = FALSE;		// Use default directory
CStr255	CFilePicker::sPrevName;

CFilePicker::CFilePicker( LStream* inStream ): LView( inStream )
{
	// WARNING: if you add any streamable data here (because you're making a new custom
	// Cppb in constructor) you will clobber CPrefFilePicker, which inherits from
	// this class.  So beware!
	fCurrentValue.vRefNum = 0;
	fCurrentValue.parID = 0;
	fCurrentValue.name[ 0 ] = 0;
	fSet = FALSE;
	fBrowseButton = NULL;
	fPathName = NULL;
	fPickTypes = Applications;
}

void CFilePicker::FinishCreateSelf()
{
	ThrowIfNil_(fBrowseButton	= dynamic_cast<LControl*>(FindPaneByID(kBrowseButton)));
	ThrowIfNil_(fPathName		= dynamic_cast<LCaption*>(FindPaneByID(kPathNameCaption)));

	fBrowseButton->SetValueMessage(msg_Browse);
	fBrowseButton->AddListener(this);
}

void CFilePicker::SetFSSpec( const FSSpec& fileSpec, Boolean touchSetFlag )
{
	if (touchSetFlag)
		fSet = TRUE;
	fCurrentValue = fileSpec;
	
	SetCaptionForPath( fPathName, fileSpec );
	BroadcastMessage( msg_FolderChanged, this );
}

void CFilePicker::ListenToMessage( MessageT inMessage, void* /*ioParam*/ )
{
	StandardFileReply		reply;
	FSSpec					currentValue;

	currentValue = GetFSSpec();
	reply.sfFile = currentValue;
	
	if ( inMessage == msg_Browse )
	{
		switch ( fPickTypes )
		{
			case MailFiles:
				reply.sfFile = CPrefs::GetFilePrototype( CPrefs::MailFolder );
			case AnyFile:
			case Folders:
			case TextFiles:
			case ImageFiles:
			case Applications:
			{
				// ¥ pose the dialog and get the users new choice
				if ( CFilePicker::DoCustomGetFile( reply, fPickTypes, TRUE ) )
					SetFSSpec( reply.sfFile );
			}
			break;
		}
	}
}

void CFilePicker::SetCaptionForPath( LCaption* captionToSet, const FSSpec& folderSpec )
{
	CStr255	pathName = FSSpecToPathName( folderSpec );
	SetCaptionDescriptor( captionToSet, pathName, smTruncMiddle );	
}

CStr255 CFilePicker::FSSpecToPathName( const FSSpec& spec )
{
	char* pPathName = CFileMgr::PathNameFromFSSpec( spec, true );
	if ( pPathName == NULL && fPickTypes == Applications )
	{
		CStr255 badName(spec.name);
		ErrorManager::PlainAlert
			( (char *)GetCString(BAD_APP_LOCATION_RESID), badName,
				(char *)GetCString(REBUILD_DESKTOP_RESID), NULL );
		return badName;
	}
	CStr255 cPathName(pPathName);
	XP_FREE( pPathName );
	return cPathName;
}

void CFilePicker::SetButtonTitle( Handle buttonHdl, CStr255& name, const Rect& buttonRect )
{	
	short			result;	//
	short			width;	//

	sPrevName = name;
	
	CStr255 finalString(::GetCString(SELECT_RESID));

	width = ( buttonRect.right - buttonRect.left )
		- ( StringWidth( finalString ) + 2 * CharWidth( 'Ê' ) );

	result = ::TruncString( width, name, smTruncMiddle );
	
	finalString += name;
	SetControlTitle( (ControlHandle)(buttonHdl), finalString );
	ValidRect( &buttonRect );
}

#define kIsFolderFlag				0x10	// flag indicating the file is a directory.
#define kInvisibleFlag				0x4000	// flag indicating that the file is invisible.

#define kGetDirButton				10
#define kDefaultButton				12

PROCEDURE( CFilePicker::OnlyFoldersFileFilter, uppFileFilterYDProcInfo )
pascal Boolean CFilePicker::OnlyFoldersFileFilter( CInfoPBPtr pBlock, void* /*data*/ )
{	
	Boolean		isFolder;
	Boolean		isInvisible;
	Boolean		dontShow;
	
	isFolder = ((pBlock->hFileInfo).ioFlAttrib) & kIsFolderFlag;
	isInvisible = ((pBlock->dirInfo).ioDrUsrWds.frFlags) & kInvisibleFlag;
	dontShow = !isFolder || isInvisible;

	return dontShow;
}

PROCEDURE( CFilePicker::IsMailFileFilter, uppFileFilterYDProcInfo )
pascal Boolean CFilePicker::IsMailFileFilter( CInfoPBPtr pBlock, void* /*data*/ )
{
	Boolean		isEudora;
	Boolean		isMine;
	Boolean		dontShow;
	
	isMine = ( pBlock->hFileInfo).ioFlFndrInfo.fdCreator == emSignature;
	isEudora = ( pBlock->hFileInfo).ioFlFndrInfo.fdCreator == 'CSOm';
	dontShow = !isMine && !isEudora;
	
	return dontShow;
}

PROCEDURE( CFilePicker::SetCurrDirHook, uppDlgHookYDProcInfo )
pascal short CFilePicker::SetCurrDirHook( short item, DialogPtr /*dialog*/, void* /*data*/ )
{
	if ( item == sfHookFirstCall )
		return sfHookChangeSelection;
	return item;
}

PROCEDURE( CFilePicker::DirectoryHook, uppDlgHookYDProcInfo )
pascal short CFilePicker::DirectoryHook( short item, DialogPtr dialog, void* data )
{
	short 				type;				//	menu item selected
	Handle				handle;				//	needed for GetDialogItem
	Rect				rect;				//	needed for GetDialogItem

	CStr255				tempName = sPrevName;
	CStr255 			name;

	CInfoPBRec			pb;
	StandardFileReply*	sfrPtr;
	OSErr				err;
	short				returnVal;

	// ¥ default, except in special cases below
	returnVal = item;
	
	// ¥ this function is only for main dialog box
	if ( GetWRefCon((WindowPtr)dialog ) != sfMainDialogRefCon )
		return returnVal;					
	
	DirInfo* dipb = (DirInfo*)&pb;
	// ¥ get sfrPtr from 3rd parameter to hook function
	sfrPtr = (StandardFileReply*)((PickClosure*)data)->reply;

	GetDialogItem( dialog, kGetDirButton, &type, &handle, &rect );

	if ( item == sfHookFirstCall )
	{
		// ¥ determine current folder name and set title of Select button
		dipb->ioCompletion = NULL;
		dipb->ioNamePtr = (StringPtr)&tempName;
		dipb->ioVRefNum = sfrPtr->sfFile.vRefNum;
		dipb->ioDrDirID = sfrPtr->sfFile.parID;
		dipb->ioFDirIndex = - 1;
		err = PBGetCatInfoSync(&pb);
		name = tempName;
		CFilePicker::SetButtonTitle( handle, name, rect );

		return sfHookChangeSelection;		
	}
	else
	{
		// ¥ track name of folder that can be selected
		// (also allow selection of folder aliases)
		if ( ( sfrPtr->sfIsFolder) || ( sfrPtr->sfIsVolume) ||
			 ((sfrPtr->sfFlags & kIsAlias) && (sfrPtr->sfType == kContainerFolderAliasType)) )
			name = sfrPtr->sfFile.name;
		else
		{
			dipb->ioCompletion = NULL;
			dipb->ioNamePtr = (StringPtr)&tempName;
			dipb->ioVRefNum = sfrPtr->sfFile.vRefNum;
			dipb->ioFDirIndex = -1;
			dipb->ioDrDirID = sfrPtr->sfFile.parID;
			err = PBGetCatInfoSync(&pb);
			name = tempName;
		}
		
		// ¥ change directory name in button title as needed
		if ( name != sPrevName )
			CFilePicker::SetButtonTitle( handle, name, rect );

		switch ( item )
		{
			// ¥ force return by faking a cancel
			case kGetDirButton:	
				sResult = TRUE;			
				returnVal = sfItemCancelButton;
			break;
			
			// ¥ use default directory
			case kDefaultButton:
				sUseDefault = TRUE;
				returnVal = sfItemCancelButton;
			break;									
			
			// ¥ flag no directory was selected
			case sfItemCancelButton:
				sResult = FALSE;
			break;	
			
			default:
				break;
		}
	}
	return returnVal;
}

Boolean CFilePicker::DoCustomGetFile( StandardFileReply& reply, 
	CFilePicker::PickEnum pickTypes, Boolean isInited )
{
	Point				loc = { -1, -1 };
	OSErr				gesErr;
	long				gesResponse;
	FileFilterYDUPP		filter = NULL;
	DlgHookYDUPP		dialogHook;
	short				dlogID = 0;
	OSType				types[ 4 ];
	OSType*				typeP;
	short				typeCount = -1;
	Boolean*			resultP = &reply.sfGood;
	Boolean				wantPreview = FALSE;
	PickClosure			closure;
	
	reply.sfIsVolume = 0;
	reply.sfIsFolder = 0;
	reply.sfFlags = 0;
	reply.sfScript = smSystemScript;
	reply.sfGood = FALSE;
	closure.reply = &reply;
	closure.inited = isInited;
	typeP = types;
	
	if ( isInited )
		dialogHook = &PROCPTR( SetCurrDirHook );
	else
		dialogHook = NULL;
		
	// ¥Êinitialize name of previous selection
	sPrevName = reply.sfFile.name;

	StPrepareForDialog preparer;	
	gesErr = Gestalt( gestaltStandardFileAttr, &gesResponse );
	if ( gesResponse & ( 1L << gestaltStandardFile58 ) )
	{
		switch ( pickTypes )
		{
			case Folders:
			{
				filter = &PROCPTR( OnlyFoldersFileFilter );	
				dlogID = DLOG_GETFOLDER;
				dialogHook = &PROCPTR( DirectoryHook );
				resultP = &sResult;
				typeP = NULL;
			}
			break;
			
			case AnyFile:
			{
				typeP = NULL;
			}
			break;
			
			case MailFiles:
			{
				filter = &PROCPTR( IsMailFileFilter );
				types[ 0 ] = 'TEXT';
				typeCount = 1;
			}
			break;
			
			case TextFiles:
			{
				types[ 0 ] = 'TEXT';
				typeCount = 1;
			}
			break;
			
			case ImageFiles:
			{
				types[ 0 ] = 'GIFf';
				types[ 1 ] = 'TEXT';
				types[ 2 ] = 'JPEG';
				typeCount = 3;
				wantPreview = TRUE;
			}
			break;
			
			case Applications:
			{
				types[0] = 'APPL';
				types[1] = 'adrp';	// Application aliases
				typeCount = 2;
			}
			break;	
		}
	}
	
	if ( wantPreview && UEnvironment::HasGestaltAttribute( gestaltQuickTime, 0xFFFF ) )
		::CustomGetFilePreview( filter, typeCount, typeP, &reply, dlogID, loc, dialogHook,
			NULL, NULL, NULL, &closure );
	else
		::CustomGetFile( filter, typeCount, typeP, &reply, dlogID, loc, dialogHook,
			NULL, NULL, NULL, &closure );

	// follow any folder aliases that may be returned
	if (*resultP && pickTypes == Folders) {
		Boolean b1, b2;
		ThrowIfOSErr_( ResolveAliasFile(&reply.sfFile, true, &b1, &b2) );
	}
	
	return *resultP;
}

Boolean CFilePicker::DoCustomPutFile( StandardFileReply& reply,
	const CStr255& prompt, Boolean inited )
{
	Point			loc = { -1, -1 };
	CStr255			filename = CStr255::sEmptyString;
	DlgHookYDUPP	dialogHook = NULL;
	
	if ( inited  )
	{
		filename = reply.sfFile.name;
		dialogHook = &PROCPTR( SetCurrDirHook );
	}
	
	CustomPutFile( (ConstStr255Param)prompt,
		(ConstStr255Param)filename,
		&reply,
		0,
		loc,
		dialogHook,
		NULL,
		NULL,
		NULL,
		NULL );
		
	return reply.sfGood;
}	

// ===========================================================================
//	COtherSizeDialog
// ===========================================================================

const ResIDT	edit_SizeField = 		1504;

COtherSizeDialog::COtherSizeDialog( LStream* inStream ): LDialogBox( inStream )
{
}

void COtherSizeDialog::FinishCreateSelf()
{
	LDialogBox::FinishCreateSelf();
	
	mSizeField = (LEditField*)FindPaneByID( edit_SizeField );

	//	Set the edit field to go on duty whenever
	//	this dialog box goes on duty.

	fRef = NULL;
	SetLatentSub( mSizeField );
}

void COtherSizeDialog::SetValue( Int32 inFontSize )
{
	if ( inFontSize > 128 )
		inFontSize = 128;
		
	mSizeField->SetValue( inFontSize );
	mSizeField->SelectAll();
}

Int32 COtherSizeDialog::GetValue() const
{
	Int32		size;
	
	size = mSizeField->GetValue();
	if ( size > 128 )
		size = 128;
		
	return size;
}

void COtherSizeDialog::SetReference( LControl* which )
{
	fRef = which;
}

// ---------------------------------------------------------------------------
//		¥ ListenToMessage
// ---------------------------------------------------------------------------
//	This member function illustrates an alternate method for this dialog box
//	to communicate with its invoker when the user clicks the defualt (OK) button.
//	Usually, when the user clicks the default button, LDialogBox::ListenToMessage()
//	calls the supercommander's ObeyCommand() function passing it the value message
//	of the default button and a pointer to the dialog box object in the ioParam
//	parameter. This method also passes the value message of the default button,
//	but rather than sending a pointer to the dialog in ioParam, it sends the
//	new point size.
//
//	The advantage of using the alternate method is that the client that invokes
//	the dialog doesn't have to call any of the dialog class's member functions
//	to get its value and is not responsible for closing the dialog box. The
//	disadvantage is that you have to pass all the information back through
//	ObeyCommand()'s single ioParam parameter. 

void COtherSizeDialog::ListenToMessage(MessageT inMessage, void *ioParam)
{
	Int32	newFontSize;

	switch ( inMessage )
	{
		case msg_ChangeFontSize:
			//	This is the command number associated with this dialog's
			//	OK button. It's also used as a command number to send
			//	to the dialog's supercommander which is presumably
			//	the commander that created and invoked the dialog.

			newFontSize = mSizeField->GetValue();

			//	Note that we use the second parameter of ObeyCommand() to
			//	specify the new font size. When the menu mechanism invokes
			//	ObeyCommand(), the second parameter is always nil.
			
			BroadcastMessage( msg_ChangeFontSize, this );
			
			//	Since we want the OK button to close the dialog box as
			//	well as change the font size, we change the message to close
			//	and let the flow fall through to the default case.

			inMessage = cmd_Close;
			
			//	FALLTHROUGH INTENTIONAL
			//	to get the base class to close the dialog box

		default:
			LDialogBox::ListenToMessage(inMessage, ioParam);
			break;
	}
}


LArrowGroup::LArrowGroup( LStream* inStream ): LView( inStream )
{
	fSize = NULL;
	fArrows = NULL;
	fValue = 0L;
	fMinValue = 0L;
	fMaxValue = 99999999L;
	fStringID = MEGA_RESID;
	this->BuildControls();
}

void LArrowGroup::SetStringID(ResIDT stringID)
{
	fStringID = stringID;
}

void LArrowGroup::ListenToMessage( MessageT message, void* ioParam )
{
	if ( message == msg_ArrowsHit )
	{
		Int16	whichHalf = *(Int16*)ioParam;
		
		if ( whichHalf == mTopHalf )
			SetValue( fValue + 1 );
		else
			SetValue( fValue - 1 );
	}
}

void LArrowGroup::SetValue( Int32 value )
{
	if ( value < fMinValue )
		value = fMinValue;
	else if ( value > fMaxValue )
		value = fMaxValue;
		
	CStr255	string;
	NumToString( value, string );
	string += CStr255(*GetString(fStringID));
	fSize->SetDescriptor( string );
	fValue = value;
}

void LArrowGroup::SetMaxValue( Int32 newMax )
{
	if ( fValue > newMax )
		SetValue( newMax );
	fMaxValue = newMax;
}

void LArrowGroup::SetMinValue( Int32 newMin )
{
	if ( fValue < newMin )
		SetValue( newMin );
	fMinValue = newMin;
}


void LArrowGroup::BuildControls()
{
	SPaneInfo		paneInfo;
	
	paneInfo.paneID = 'Actl';
	paneInfo.width = 15;
	paneInfo.height = 25;
	paneInfo.visible = TRUE;
	paneInfo.enabled = TRUE;
	paneInfo.left = mFrameSize.width - 16;
	paneInfo.top = 0;
	paneInfo.userCon = 0;
	paneInfo.superView = this;

	fArrows = new LArrowControl( paneInfo, msg_ArrowsHit );
	ThrowIfNil_(fArrows);
	fArrows->AddListener( this );
	
	paneInfo.paneID = 'capt';
	paneInfo.width = mFrameSize.width - 16;
	paneInfo.height = 12;
	paneInfo.left = 0;
	paneInfo.top = 5;
	
	fSize = new COffscreenCaption(paneInfo, "\p", 10000);
}

CColorButton::CColorButton( LStream *inStream ): LButton( inStream )
{
	fInside = FALSE;
	mNormalID = 0;
	mPushedID = 1;
}

void CColorButton::DrawGraphic( ResIDT inGraphicID )
{
	Rect	frame;

	CalcLocalFrameRect( frame );
	UGraphics::SetFore( CPrefs::Black );
	if ( IsEnabled() )
	{
		if ( inGraphicID == mPushedID )
		{
			::PenSize( 2, 2 );
			::PenPat( &qd.black );
			::FrameRect( &frame );
		}
		else
		{
			::PenSize( 1, 1 );
			::PenPat( &qd.black );
			::FrameRect( &frame );
			UGraphics::SetFore( CPrefs::White );
			::InsetRect( &frame, 1, 1 );
			::FrameRect( &frame );
			UGraphics::SetFore( CPrefs::Black );
		}
	}
	else
	{
		::PenSize( 1, 1 );
		::PenPat( &qd.gray );
		::FrameRect( &frame );
		::PenPat( &qd.black );

	}
		
	RGBForeColor( &fColor );

	if ( inGraphicID == mPushedID )
		::InsetRect( &frame, 3, 3 );
	else
		::InsetRect( &frame, 2, 2 );
		
	::FillRect( &frame, &( UQDGlobals::GetQDGlobals()->black ) );
	UGraphics::SetFore( CPrefs::Black );
}

void CColorButton::HotSpotResult( short inHotSpot )
{
	Point where;
	where.h = where.v = 0;
	RGBColor outColor;

	if ( ::GetColor(where, (unsigned char *)*GetString(PICK_COLOR_RESID),
														&fColor,&outColor) )
	{
		if ( !UGraphics::EqualColor(fColor, outColor) )
		{
			fColor = outColor;
			BroadcastValueMessage();
		}
	}

	// ¥ turn off the black border
	HotSpotAction( inHotSpot, FALSE, TRUE );

}

FileIconsLister::FileIconsLister (CGAPopupMenu * target)
:	StdPopup (target)
{
	fIcons = nil;
}

FileIconsLister::~FileIconsLister()
{	// Debugging purposes only
}

void
FileIconsLister::SetIconList (CApplicationIconInfo * icons)
{
	fIcons = icons;
}

short
FileIconsLister::GetCount()
{
	if (fIcons == NULL)
		return 0;
	else
		return fIcons->GetFileTypeArraySize();
}

CStr255	
FileIconsLister::GetText (short item)
{
	CFileType * fileType;
	fIcons->fDocumentIcons->FetchItemAt (item, &fileType);
	return fileType->fIconSig;
}

// ¥¥ Constructors/destructors

CMimeTable::CMimeTable(LStream *inStream) : LTable(inStream)
{
	fContainer = NULL;
	fNetscapeIcon = NULL;
	fPluginIcon = NULL;
}

void CMimeTable::FinishCreateSelf()
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
	CApplicationIconInfo* netscapeInfo = fApplList.GetAppInfo(emSignature);
	ThrowIfNil_(netscapeInfo);
	ThrowIfNil_(netscapeInfo->fDocumentIcons);
	fNetscapeIcon = netscapeInfo->fApplicationIcon;
	
	// Cache a handle to the standard plug-in icon
	LArrayIterator iterator(*(netscapeInfo->fDocumentIcons));
	CFileType* fileInfo;
	while (iterator.Next(&fileInfo))
	{
		if (fileInfo->fIconSig == emPluginFile)	
		{
			fPluginIcon = fileInfo->fIcon;
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

CApplicationIconInfo* CMimeTable::GetAppInfo(CMimeMapper* mapper)
{
	return fApplList.GetAppInfo(mapper->GetAppSig(), mapper);
}


// Sets cell data to values of the mapper
void CMimeTable::BindCellToApplication(TableIndexT row, CMimeMapper* mapper)
{
	CApplicationIconInfo* appInfo = GetAppInfo(mapper);
	
	// Now figure out the extensions from the netlib
	PrefCellInfo cellInfo(mapper, appInfo);
	TableCellT cell;
	cell.row = row;
	cell.col = 1;
	SetCellData(cell, &cellInfo);
}


void CMimeTable::DrawSelf()
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


void CMimeTable::HiliteCell(const TableCellT &inCell)
{
	StColorState saveTheColor;
	RGBColor white = { 0xFFFF, 0xFFFF, 0xFFFF};
	RGBBackColor(&white);
	
	LTable::HiliteCell(inCell);
}


void CMimeTable::UnhiliteCell(const TableCellT &inCell)
{
	StColorState saveTheColor;
	RGBColor white = { 0xFFFF, 0xFFFF, 0xFFFF};
	RGBBackColor(&white);
	
	LTable::UnhiliteCell(inCell);
}



#define offsetTable			7
#define offsetTextTop		15
#define offsetMimeType		(offsetTable + 0)
#define offsetIcon			(offsetTable + 166)
#define offsetHandler		(offsetIcon + 24)
#define widthMimeType		(offsetIcon - offsetMimeType - 5)

void CMimeTable::DrawCell(const TableCellT	&inCell)
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
				iconHandle = fPluginIcon;
			else if (loadAction == CMimeMapper::Internal)
				iconHandle = fNetscapeIcon;
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
void CMimeTable::GetCellInfo(PrefCellInfo& cellInfo, int row)
{
	TableCellT	cell;
	cell.row = row;
	cell.col = 1;
	GetCellData(cell, &cellInfo);
}

void CMimeTable::FreeMappers()
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


//
// Scroll the view as little as possible to move the specified cell
// entirely within the frame.  Currently we only handle scrolling
// up or down a single line (when the selection moves because of
// and arrow key press).
//
void CMimeTable::ScrollCellIntoFrame(const TableCellT& inCell)
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

Boolean	CMimeTable::HandleKeyPress(const EventRecord &inKeyEvent)
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
				SelectCell(currentCell);
			handled = true;
			break;

		case char_DownArrow:
			currentCell.row++;
			if (currentCell.row <= tableRows)
				SelectCell(currentCell);
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



/************************************************************************************
 Preferences MimeMapping window
 ************************************************************************************/
// utility callback function for getting icons
pascal Handle MyIconGetter( ResType theType, Ptr yourDataPtr );

// function that creates an item cache that we want
OSErr CreateCache(Handle & cache, short resID);

static IconGetterUPP MyIconGetterUPP = NULL;

pascal Handle MyIconGetter( ResType theType, Ptr yourDataPtr )
{
	Handle res, resCopy;
	
	resCopy = res = ::Get1Resource( theType, (short)yourDataPtr );
	
	::HandToHand( &resCopy );
	::ReleaseResource( res );
	return resCopy;
}


OSErr CreateCache( Handle& cache, short resID )
{
	if ( MyIconGetterUPP == NULL )
		MyIconGetterUPP = NewIconGetterProc( MyIconGetter );

	OSErr err = MakeIconCache( &cache, MyIconGetterUPP, (Ptr)resID );

	if ( err )
		return err;

	Rect r;
	r.top = r.left = 0;
	r.bottom = r.right = 16;

	err = LoadIconCache( &r, atHorizontalCenter, ttNone, cache );
	return err;
}


Handle CFileType::sDefaultDocIcon = NULL;

CFileType::CFileType( OSType iconSig )
{
	InitializeDefaults();
	fIconSig = iconSig;
	fIcon = sDefaultDocIcon;
}

CFileType::~CFileType()
{
	if ( fIcon && ( fIcon != sDefaultDocIcon ) )
		::DisposeIconSuite( fIcon, TRUE );
}

// Initializes default values
void CFileType::InitializeDefaults()
{
	if ( sDefaultDocIcon == NULL )
	{
		StUseResFile resFile(kSystemResFile);
		::CreateCache( sDefaultDocIcon, genericDocumentIconResource );
	}
	ThrowIfNil_( sDefaultDocIcon );
}

// Clear the default values
void CFileType::ClearDefaults()
{
	if ( sDefaultDocIcon )
		::DisposeIconSuite( sDefaultDocIcon, TRUE );
	sDefaultDocIcon = NULL;
}

// ¥¥ globals
Handle	CApplicationIconInfo::sDefaultAppIcon = NULL;
LArray *	CApplicationIconInfo::sDocumentIcons = NULL;
Boolean	CApplicationIconInfo::sHandlesAE = TRUE;

// Call me when application has not been found. Assigns default values
CApplicationIconInfo::CApplicationIconInfo( OSType appSig )
{
	InitializeDefaults();
	
	fAppSig = appSig;
	fApplicationIcon = sDefaultAppIcon;
	fDocumentIcons = sDocumentIcons;
	fHandlesAE = sHandlesAE;
	fApplicationFound = FALSE;
}

// Call me when app was found
CApplicationIconInfo::CApplicationIconInfo(
	OSType		appSig,
	Handle		appIcon, 
	LArray*		documentIcons,
	Boolean		handlesAE)
{
	InitializeDefaults();
	
	fAppSig = appSig;
	fApplicationIcon = appIcon;
	fDocumentIcons = documentIcons;
	fHandlesAE = handlesAE;
	fApplicationFound = TRUE;
}

CApplicationIconInfo::~CApplicationIconInfo()
{
	if ( fDocumentIcons == sDocumentIcons )
		return;	// Do not delete our defaults
	for ( Int32 i = 1; i <= fDocumentIcons->GetCount(); i++ )
	{
		CFileType* f;
		fDocumentIcons->FetchItemAt( i, &f );
		delete f;
	}

	delete fDocumentIcons;

	if ( fApplicationIcon != sDefaultAppIcon )
		::DisposeIconSuite( fApplicationIcon, TRUE );
}

// ¥¥ access

// Gets file type by the index
CFileType* CApplicationIconInfo::GetFileType( int index )
{
	CFileType* f;
	if (index > fDocumentIcons->GetCount() )
#ifdef DEBUG
 		XP_ABORT();
#else
 		return NULL; 
#endif
	fDocumentIcons->FetchItemAt( index, &f );
	return f;
}	

// Gets number of file types
int CApplicationIconInfo::GetFileTypeArraySize()
{
	return fDocumentIcons->GetCount();
}

// ¥¥ misc

void CApplicationIconInfo::InitializeDefaults()
{
	if (sDocumentIcons != NULL)
		return;

	// Create a default document icon list, size 1, generic icon of type 'TEXT'
	sDocumentIcons = new LArray;
	ThrowIfNil_( sDocumentIcons );
	
	CFileType* newType = new CFileType( 'TEXT' );
	ThrowIfNil_( newType );

	sDocumentIcons->InsertItemsAt( 1, LArray::index_Last, &newType );
	
	// Get the default application icon
	StUseResFile resFile(kSystemResFile);
	::CreateCache( sDefaultAppIcon, genericApplicationIconResource );
	ThrowIfNil_( sDefaultAppIcon );
}

void CApplicationIconInfo::ClearDefaults()
{
	if ( sDocumentIcons == NULL )
		return;	// Nothing allocated, nothing deleted
	for ( Int32 i = 1; i <= sDocumentIcons->GetCount(); i++ )
	{
		CFileType* f;
		sDocumentIcons->FetchItemAt( i, &f );
		delete f;
	}

	delete sDocumentIcons;
	sDocumentIcons = NULL;

	if ( sDefaultAppIcon )
		DisposeIconSuite( sDefaultAppIcon, TRUE );
	sDefaultAppIcon = NULL;
	
	CFileType::ClearDefaults();
}



CApplicationList::CApplicationList() : LArray()
{
	CApplicationIconInfo::InitializeDefaults();
}

CApplicationList::~CApplicationList()
{
	for (Int32 i = 1; i <= mItemCount; i++)
	{
		CApplicationIconInfo * f;
		FetchItemAt(i, &f);
		delete f;
	}
	CApplicationIconInfo::ClearDefaults();
}


CApplicationIconInfo* CApplicationList::GetAppInfo(OSType appSig, CMimeMapper* mapper)
{
	// Try to find the application that matches the sig:
	for ( Int32 i = 1; i <= mItemCount; i++ )
	{
		CApplicationIconInfo* f;
		FetchItemAt( i, &f );
		if ( f->fAppSig == appSig)
			return f;
	}
	
	// Could not find an application, must create a new entry
	return CreateNewEntry(appSig, mapper);
}


// Creates application icon info for an app with a given signature
// If app is not found, defaults are created
CApplicationIconInfo* CApplicationList::CreateNewEntry(OSType appSig, CMimeMapper* mapper)
{
	FSSpec						appSpec;
	CApplicationIconInfo*		appInfo = NULL;
	
	OSErr err = CFileMgr::FindApplication(appSig, appSpec );
	
	if ( err != noErr )
	{
		// Application not found, create defaults
		appInfo = new CApplicationIconInfo(appSig);
	}
	else
	{
		if (mapper)
			mapper->SetAppName( appSpec.name );
		appInfo = AppInfoFromFileSpec(appSig, appSpec);	
	}

	InsertItemsAt(1, LArray::index_Last, &appInfo);
	return appInfo;
}

// Horrendous bundle parsing routine.
// Its parses the bundle, gets all the document icons, and an application icon
// Failure can occur in many places. Unhandled failures result in a creation of
// the 'generic' applicatiohe resource
CApplicationIconInfo* CApplicationList::AppInfoFromFileSpec( OSType appSig, FSSpec appSpec )
{
	CApplicationIconInfo*	newInfo = NULL;
	short					refNum;
	Handle					bundle = NULL;
	OSErr					err;
	Handle					appIcon = NULL;
	Boolean					handlesAE = TRUE;
	
	LArray*					documentIcons = NULL;
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
		documentIcons = new LArray();
		ThrowIfNil_( documentIcons );

		SetResLoad( FALSE );
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
		::BlockMoveData( *bundle, &appSig, 4 );

		GetResourcePointers( bundle, iconOffset, frefOffset, numOfIcons, numOfFrefs );

		// We have the offsets for FREF and ICN# resources
		// Not every FREF has a matching icon, so we read in all 
		// the FREFs and try to find the matching icon for each
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
				
			for ( int j = 0; j <= numOfIcons; j++ )
			{
				BNDLIds iconBndl = iconOffset[j];
				if ( iconBndl.localID == frefBNDL.localID )
				{
					err = ::CreateCache( iconFamily, iconBndl.resID );
					if (err)
						iconFamily = NULL;
					break;
				}
			}
			
			if ( iconType == 'APPL' )
			{
				// Special handling of the application icon
				if ( iconFamily == NULL )
					appIcon = CApplicationIconInfo::sDefaultAppIcon;
				else
					appIcon = iconFamily;
			}
			else
			{							// Document icons
				CFileType* newType = NULL;
				if ( iconFamily == NULL )
					newType = new CFileType( iconType );
				else
					newType = new CFileType( iconType, iconFamily );

				documentIcons->InsertItemsAt( 1, LArray::index_Last, &newType );
			}
		}	// Done parsing all the file types
		
		HUnlock( bundle );
		HPurge( bundle );
		ReleaseResource( bundle );
		// Figure out if the application handles AppleEvents
		Handle sizeRes = ::Get1Resource( 'SIZE', -1 );
		if ( sizeRes == NULL )
			handlesAE = FALSE;
		else
		{
			char aeChar;
			aeChar = (*sizeRes)[1];
			handlesAE = aeChar & 2;
		}
		
		// Error handling: no file types
		if ( documentIcons->GetCount() == 0 )	// No icons were read, add 1 dummy one
		{
			CFileType* newType = new CFileType( 'TEXT' );
			documentIcons->InsertItemsAt( 1, LArray::index_Last, &newType );
		}
		// Error handling: no application icon
		if ( appIcon == NULL )
			appIcon = CApplicationIconInfo::sDefaultAppIcon;
		newInfo = new CApplicationIconInfo( appSig, appIcon, documentIcons, handlesAE );
	}
	Catch_(inErr)
	{
		newInfo = new CApplicationIconInfo( appSig );
	}
	EndCatch_
	
	CloseResFile( refNum );
	err = ResError();
	ThrowIfNil_( newInfo );
	return newInfo;
}

void CApplicationList::GetResourcePointers(
	Handle bundle,
	BNDLIds* &iconOffset,
	BNDLIds* &frefOffset,
	short& numOfIcons,
	short& numOfFrefs)
{
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
}

PrefCellInfo::PrefCellInfo(CMimeMapper * mapper, CApplicationIconInfo * iconInfo)
{
	fMapper = mapper; 
	fIconInfo = iconInfo;
}

PrefCellInfo::PrefCellInfo()
{
	fMapper = NULL;
	fIconInfo = NULL;
}


Boolean
LFocusEditField::HandleKeyPress(
	const EventRecord	&inKeyEvent)
{
	Boolean		keyHandled = true;
	LControl	*keyButton = nil;
	
	switch (inKeyEvent.message & charCodeMask) {
	
		case char_Enter:
		case char_Return:
			BroadcastMessage(mReturnMessage, this);
			return true;
		default:
			return LEditField::HandleKeyPress(inKeyEvent);
	}
}

LFocusEditField::LFocusEditField(
	const LFocusEditField	&inOriginal)
		: LEditField(inOriginal),
		  LBroadcaster(inOriginal)
{
	mFocusBox = nil;
	if (inOriginal.mFocusBox != nil) {
		mFocusBox = new LFocusBox(*(inOriginal.mFocusBox));
	}
}

LFocusEditField::LFocusEditField(
	LStream	*inStream)
		: LEditField(inStream)
{
	mFocusBox = new LFocusBox;
	mFocusBox->Hide();
	mFocusBox->AttachPane(this);
}
LFocusEditField::~LFocusEditField()
{
	
}
void
LFocusEditField::BeTarget()
{
	if(mFocusBox != nil) {
		mFocusBox->Show();
	}
	LEditField::BeTarget();
}
void
LFocusEditField::DontBeTarget()
{
	if(mFocusBox != nil) {
		mFocusBox->Hide();
	}
	LEditField::DontBeTarget();
}
LFocusBox*
LFocusEditField::GetFocusBox()
{
	return mFocusBox;
}

/********************************************************************

OneClickLListBox implementation.

Derived from LListBox. Overrides ClickSelf so that it sends a 
message when it has only been clicked once, not twice.

********************************************************************/
OneClickLListBox::OneClickLListBox( LStream* inStream ): LListBox( inStream )
{
	mSingleClickMessage = msg_Nothing;
}


void	OneClickLListBox::ClickSelf(const SMouseDownEvent &inMouseDown)
{
	if(SwitchTarget(this))
	{
		FocusDraw();
		if (::LClick(inMouseDown.whereLocal, inMouseDown.macEvent.modifiers, mMacListH)) 
		{
			BroadcastMessage(mDoubleClickMessage, this);
		} else {
			BroadcastMessage(mSingleClickMessage, this);
		}
	}
}

Boolean		OneRowLListBox::HandleKeyPress(const EventRecord &inKeyEvent)
{
	if(OneClickLListBox::HandleKeyPress(inKeyEvent))
	{
		Char16	theKey = inKeyEvent.message & charCodeMask;
		// based on LListBox::HandleKeyPress--only broadcast when our class handled it
		// (window might close--deleting us--before we broadcast)
		if ( UKeyFilters::IsNavigationKey(theKey) ||
			( UKeyFilters::IsPrintingChar(theKey) && ! (inKeyEvent.modifiers & cmdKey) ) )
		{
			BroadcastMessage(mSingleClickMessage, this);
		}
		return true;
	}
	else
		return false;
}


int16		OneRowLListBox::GetRows() 
{ 
	return ((**mMacListH).dataBounds.bottom); 
}

OneRowLListBox::OneRowLListBox( LStream* inStream ): OneClickLListBox( inStream )
{
	FocusDraw();
	if((*mMacListH)->dataBounds.right == 0)
		::LAddColumn(1 , 0, mMacListH);
	(*mMacListH)->selFlags |= lOnlyOne;
}

void 	OneRowLListBox::AddRow(int32 rowNum, char* data, int16 datalen)
{
	if(SwitchTarget(this))
	{
		FocusDraw();
		Cell theCell;
		theCell.h = 0;
		rowNum = ::LAddRow(1 , rowNum, mMacListH);
		theCell.v = rowNum;
		::LSetCell(data,datalen ,theCell, mMacListH);
	}
}
void 	OneRowLListBox::GetCell(int32 rowNum, char* data, int16* datalen)
{
	FocusDraw();
	Cell theCell;
	theCell.h = 0;
	theCell.v = rowNum;
	::LGetCell(data,datalen ,theCell, mMacListH);
}
void 	OneRowLListBox::SetCell(int32 rowNum, char* data, int16 datalen)
{
	if(SwitchTarget(this))
	{
		FocusDraw();
		Cell theCell;
		theCell.h = 0;
		theCell.v = rowNum;
		::LSetCell(data,datalen ,theCell, mMacListH);
	}
}

void 	OneRowLListBox::RemoveRow(int32 rowNum)
{
	FocusDraw();
	::LDelRow(1 , rowNum, mMacListH);
}
