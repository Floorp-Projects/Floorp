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

// prefwutils.h
// Various utilities used by preference window
// They are apart from Prefw, so that our file size is manageable

#pragma once

#include "MoreMixedMode.h"

#include <LTable.h>
#include <LGAEditField.h>

class CValidEditField;
class LArrowControl;
class CApplicationIconInfo;
class CPrefHelpersContain;
class CMimeMapper;
class CStr255;

/********************************************************************************
 * Classes
 ********************************************************************************/

//======================================
class CFilePicker
//======================================
	:	public LView
	,	public LListener
	,	public LBroadcaster
{
public:
	enum				{ class_ID = 'fpck' }; // illegal, needs one UC char.- jrm
	
	enum				PickEnum { Folders = 0, Applications, TextFiles,
							ImageFiles, MailFiles, AnyFile };

						CFilePicker( LStream* inStream );

		
	virtual void		ListenToMessage( MessageT inMessage, void* ioParam );

	void				SetFSSpec( const FSSpec& fileSpec, Boolean touchSetFlag = true );
	const FSSpec&		GetFSSpec() const { return fCurrentValue; }

	void				SetPickType( CFilePicker::PickEnum pickTypes ) { fPickTypes = pickTypes; }
	void				SetCaptionForPath( LCaption* captionToSet, const FSSpec& folderSpec );
	CStr255				FSSpecToPathName( const FSSpec& spec );
	
	Boolean				WasSet() const { return fSet; }

	
	static Boolean		DoCustomGetFile( StandardFileReply& spec,
							CFilePicker::PickEnum fileType,
							Boolean inited );
	static Boolean		DoCustomPutFile( StandardFileReply& spec,
							const CStr255& prompt,
							Boolean inited );
protected:
	struct PickClosure
	{
		StandardFileReply*		reply;
		Boolean					inited;
	};

	enum EPaneIDs {
		kPathNameCaption	= 1,
		kBrowseButton		= 2
	};

	virtual void		FinishCreateSelf();
		
	static pascal short		SetCurrDirHook( short item, DialogPtr dialog, void* data );
	PROCDECL( static, SetCurrDirHook )
	static pascal short		DirectoryHook( short item, DialogPtr dialog, void* data );
	PROCDECL( static, DirectoryHook )

	static pascal Boolean	OnlyFoldersFileFilter( CInfoPBPtr pBlock, void* data );
	static pascal Boolean	IsMailFileFilter( CInfoPBPtr pBlock, void* data );
	PROCDECL( static, OnlyFoldersFileFilter )
	PROCDECL( static, IsMailFileFilter )
	
	static void			SetButtonTitle( Handle buttonHdl, CStr255& name, const Rect& buttonRect );
	
	static CStr255		sPrevName;
	static Boolean		sResult;
	static Boolean		sUseDefault;
	
	FSSpec				fCurrentValue;
	LControl*			fBrowseButton;
	LCaption*			fPathName;
	Boolean				fSet;
	PickEnum			fPickTypes;
}; // class CFilePicker

//	COtherSizeDialog.cp	<- double-click + Command-D to see class implementation
//
//	This is a PowerPlant dialog box to handle the OtherÉ command in the Size
//	menu. 

class LEditField;

class COtherSizeDialog: public LDialogBox, public LBroadcaster
{
public:
	enum { class_ID = 'OFnt' };
						COtherSizeDialog( LStream* inStream );
	

	virtual	void		SetValue( Int32 inFontSize );
	virtual Int32		GetValue() const;

	void				SetReference( LControl* which );

	virtual void		ListenToMessage( MessageT inMessage, void* ioParam );

	LControl*			fRef;
protected:
	virtual void		FinishCreateSelf();
	LEditField*			mSizeField;	
};

class LArrowGroup: public LView, public LListener
{
public:
						LArrowGroup( LStream* inStream );

	void				ListenToMessage( MessageT message, void* ioParam );	

	void				SetValue( Int32 value );
	Int32				GetValue() const { return fValue; }
	
	void				SetMaxValue( Int32 value );
	void				SetMinValue( Int32 value );
	void				SetStringID(ResIDT stringID);
protected:
	void				BuildControls();
	
	Int32				fValue;
	Int32				fMinValue;
	Int32				fMaxValue;
	ResIDT				fStringID;
	LCaption*			fSize;
	LArrowControl*		fArrows;
};

/*****************************************************************************
 * class CColorButton
 * Just a button that pops up a color wheel when pressed
 *****************************************************************************/
	
class CColorButton: public LButton
{
public:
	enum { class_ID = 'pcol' };
	// ¥¥ constructors
						CColorButton( LStream* inStream );

	// ¥¥ colors
	void				SetColor( const RGBColor& color ) { fColor = color; }
	RGBColor			GetColor() { return fColor; }

	// ¥¥ control overrides
	virtual void		HotSpotResult( short inHotSpot );
	virtual void		DrawGraphic( ResIDT inGraphicID );
protected:
	RGBColor			fColor;
	Boolean				fInside;
};

//-----------------------------------------------------------------------------
#include "PopupBox.h"
class FileIconsLister: public StdPopup {
public:
					FileIconsLister (CGAPopupMenu * target);
	virtual			~FileIconsLister();
	CStr255			GetText (short item);
	void			SetIconList(CApplicationIconInfo *);
	short			GetCount();
private:
	CApplicationIconInfo *	fIcons;
};

/*****************************************************************************
 * class PrefCellInfo
 * All the information needed to draw a cell. This is what the table stores
 *****************************************************************************/
class PrefCellInfo
{
public:
					PrefCellInfo();
					PrefCellInfo(CMimeMapper* mapper, CApplicationIconInfo* iconInfo); 

	CMimeMapper*	fMapper;			// The mapper from the preference MIME list
	CApplicationIconInfo* fIconInfo;	// Information about icon to draw
};

//-----------------------------------------------------------------------------------
// CApplicationList
// Application list is a list that contains information about 
//-----------------------------------------------------------------------------------
struct BNDLIds
{	// Utility structure for bundle parsing
	Int16 localID;
	Int16 resID;
};

class CApplicationList : public LArray
{
public:
	// ¥¥ constructors
								CApplicationList();
	virtual						~CApplicationList();

	// ¥¥ access
	// Gets information specified by the mapper
	CApplicationIconInfo*		GetAppInfo(OSType appSig, CMimeMapper* mapper = NULL);

private:
	// Creates application icon info for an app with a given signature
	CApplicationIconInfo*		CreateNewEntry(OSType appSig, CMimeMapper* mapper = NULL);
	// Creates application icon info for an app with given specs
	CApplicationIconInfo*		AppInfoFromFileSpec(OSType appSig, FSSpec appSpec);
	void						GetResourcePointers(Handle bundle,
									BNDLIds* &iconOffset, BNDLIds * &frefOffset,
									short& numOfIcons, short & numOfFrefs);
};


/*****************************************************************************
 * class CMimeTable
 * A container view that contains all the CMimeInfo views. Here we are 
 * faking a list view. This view expands so that it contains all of its 
 * subviews.
 *****************************************************************************/

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

class CMimeTable : public LTable, public LCommander
{
public:
	// ¥¥ Constructors/destructors/access

						CMimeTable(LStream *inStream);
	void				FinishCreateSelf();
	void 				BindCellToApplication(TableIndexT row, CMimeMapper * mapper);
	CApplicationIconInfo* GetAppInfo(CMimeMapper* mapper);
	
	// ¥¥ access
	void				SetContainer( CPrefHelpersContain* container) { fContainer = container; }
	void				GetCellInfo(PrefCellInfo& cellInfo, int row);
	void				FreeMappers();

	// ¥¥ Cell selection
	virtual void		DrawCell( const TableCellT& inCell );

	// Drawing
	virtual void		DrawSelf();
	virtual void		HiliteCell(const TableCellT &inCell);
	virtual void		UnhiliteCell(const TableCellT &inCell);
			void 		ScrollCellIntoFrame(const TableCellT& inCell);

	// Events
	virtual Boolean		HandleKeyPress(const EventRecord &inKeyEvent);

protected:
	CApplicationList			fApplList;		// List of application and their icons
	CPrefHelpersContain*		fContainer;		// Containing view
	Handle						fNetscapeIcon;	// Icon for Netscape
	Handle						fPluginIcon;	// Icon for plug-ins
};

//-----------------------------------------------------------------------------------
// CFileType holds information about a single file type
//-----------------------------------------------------------------------------------
class CFileType
{
public:
								CFileType( OSType iconSig );
								CFileType( OSType iconSig, Handle icon ) {fIcon = icon; fIconSig = iconSig;}
								~CFileType();

	static void					ClearDefaults();		// Does class globals memory cleanup
	static void					InitializeDefaults();	// Initializes default values

	static Handle				sDefaultDocIcon;
	
	Handle						fIcon;		// Really an icon suite
	OSType						fIconSig;
};

//-----------------------------------------------------------------------------------
// CApplicationIconInfo
// holds all icon information about an application
//-----------------------------------------------------------------------------------
class CApplicationIconInfo
{
public:
	// ¥¥Êconstructors/destructors
								// Call me when application has not been found
								CApplicationIconInfo( OSType appSig );
								// Call me when app was found
								CApplicationIconInfo( OSType appSig, Handle appIcon,
									LArray* documentIcons, Boolean handlesAE );

								~CApplicationIconInfo();
	// ¥¥ access
	CFileType*					GetFileType( int i );	// Gets file type by the index
	int							GetFileTypeArraySize();	// Gets number of file types
	// ¥¥ misc
	static void					InitializeDefaults();	// Initializes default values
	static void					ClearDefaults();		// Does class globals memory cleanup
	
	static	Handle				sDefaultAppIcon;	// Defaults, in case that application is not found
	Handle						fApplicationIcon;	// Handle of application icons (iconSuite)
	LArray*						fDocumentIcons;		// List of CFileType objects
	Boolean						fHandlesAE;			// Does it handle apple events
	OSType						fAppSig;			// Signature of the application
	Boolean						fApplicationFound;	// Was application found on my disk?

private:
	static	LArray*				sDocumentIcons;		// ditto
	static	Boolean				sHandlesAE;			// ditto

};

/*****************************************************************************
 * Class LFocusEditField
 * ----------------------
 * Just like an LListBox, except that it will send messages on
 * a single click. Used in the Document Encoding Dialog Box.
 *****************************************************************************/

class LFocusEditField : public LEditField , public LBroadcaster{
public:
	enum { class_ID = 'Fedi' };

	LFocusEditField(
				const LFocusEditField	&inOriginal);
	LFocusEditField(
				LStream				*inStream);
	virtual	~LFocusEditField();
	LFocusBox*		GetFocusBox();

	Int16				GetReturnMessage() { return mReturnMessage; }
	virtual void		SetReturnMessage(Int16 inMessage) 
											{ mReturnMessage = inMessage;}
	virtual Boolean		HandleKeyPress(	const EventRecord	&inKeyEvent);

	private:
	Int16				mReturnMessage;

protected:
	LFocusBox		*mFocusBox;

	virtual void		BeTarget();
	virtual void		DontBeTarget();

};

/*****************************************************************************
 * Class OneClickLListBox
 * ----------------------
 * Just like an LListBox, except that it will send messages on
 * a single click. Used in the Document Encoding Dialog Box.
 *****************************************************************************/
 
 class OneClickLListBox : public LListBox
{

	public:
						OneClickLListBox(LStream * inStream);
	
	Int16				GetSingleClickMessage() { return mSingleClickMessage; }
	virtual void		SetSingleClickMessage(Int16 inMessage) 
											{ mSingleClickMessage = inMessage;}
	virtual void		ClickSelf(const SMouseDownEvent &inMouseDown);

	protected:
	Int16				mSingleClickMessage;
	
};

/*****************************************************************************
 * Class OneRowLListBox
 *****************************************************************************/
class OneRowLListBox : public OneClickLListBox
{

	public:
	enum { class_ID = 'ocLB' };
						OneRowLListBox(LStream * inStream);
	virtual Boolean		HandleKeyPress(const EventRecord &inKeyEvent);
	
	virtual Int16		GetRows();		
	virtual void		AddRow(Int32 rowNum, char* data, Int16 datalen);
	virtual void		RemoveRow(Int32 rowNum);
	virtual void		GetCell(Int32 rowNum, char* data, Int16* datalen);
	virtual void		SetCell(Int32 rowNum, char* data, Int16 datalen);
};
