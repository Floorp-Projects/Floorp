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

#include "CTabSwitcher.h"
#include "ntypes.h"	// MWContext
#include "lo_ele.h"		// LO_Color
#include "intl_csi.h"	// INTL_GetCSIWinCSID

#ifdef COOL_IMAGE_RADIO_BUTTONS
# include "CBevelButton.h"
#endif

class OneRowLListBox;
class CColorButton;
class CTabControl;
class CLargeEditField;
class LGAEditField;
class LGAPopup;


class CChameleonView: public LView
{
public:
	enum				{ class_ID = 'cviw' };
						CChameleonView(LStream * inStream) : LView(inStream) {};
	virtual void		SetColor(RGBColor textColor);
	virtual void		DrawSelf();	

protected:

	RGBColor			fTextColor;
	
};


class CChameleonCaption: public LCaption
{
public:
	enum				{class_ID = 'ccpt' };
						CChameleonCaption(LStream * inStream) : LCaption(inStream) {};
	virtual void		SetColor(RGBColor textColor, RGBColor backColor);
	
protected:

	RGBColor			fTextColor;
	RGBColor			fBackColor;
	virtual void		DrawSelf();	
};

	
// This class simply creates a dialog and extracts the context from the SuperCommand so
// that we can set the values of the controls in the dialog based on the context which created it.

class CEditDialog: public LDialogBox
{
public:
						CEditDialog( LStream* inStream ): LDialogBox( inStream ), mUndoInited(false) { pExtra = NULL; }
						~CEditDialog() { XP_FREEIF(pExtra); }
	static Boolean		Start(ResIDT inWindowID, MWContext * context = NULL, short initTabValue = 0, Boolean insert = FALSE);

	Boolean				AllowSubRemoval( LCommander *inSub );
	void				FindCommandStatus( CommandT inCommand, Boolean &outEnabled, 
									Boolean&, Char16&, Str255);
	virtual void		ListenToMessage( MessageT inMessage, void* ioParam );

	virtual void		InitializeDialogControls() = NULL;
	
	void				SetContext(MWContext*	context) {fContext = context;}				
	MWContext*			GetContext() { return fContext;}				

	void				SetInitTabValue(short initValue) {fInitTabValue = initValue;}
	short				GetInitTabValue() { return fInitTabValue;}
					
	void				SetInWindowID(ResIDT inWindowID) {fInWindowID = inWindowID;}
	ResIDT				GetInWindowID() { return fInWindowID;}
					
	void				SetInsertFlag(Boolean insert) {fInsert = insert;}
	Boolean				GetInsertFlag() { return fInsert;}

	int16				GetWinCSID() { return INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(fContext)); }

	static void			ChooseImageFile(CLargeEditField* editField);

protected:

	virtual Boolean		CommitChanges(Boolean allPanes) = NULL;
	virtual void		Help() = NULL;

	MWContext*			fContext;
			short		fInitTabValue;
			Boolean		fInsert;
			ResIDT		fInWindowID;

	char*				pExtra;
	Boolean				mUndoInited;
};


class CEditTabSwitcher: public CTabSwitcher
{
public:
	enum				{ class_ID = 'EtSw' };
	
						CEditTabSwitcher(LStream* inStream);
	virtual				~CEditTabSwitcher();
						
	virtual	void		DoPostLoad(LView* inLoadedPage, Boolean inFromCache);
	void 				SetData(MWContext* context, Boolean insert);
	void				Help();

protected:	
	MWContext*			fContext;
	Boolean				fInsert;
	char*				fLinkName;			// need to share between link and image pages
	
};


class CTableInsertDialog: public CEditDialog
{
public:
	enum				{ class_ID = 'ETBT' };
	
						CTableInsertDialog( LStream* inStream );
	virtual				~CTableInsertDialog();
						
	virtual Boolean		CommitChanges(Boolean allPanes);
	void				AdjustEnable();
	virtual void		FinishCreateSelf();
	virtual void		InitializeDialogControls();
	virtual void		ListenToMessage( MessageT inMessage, void* ioParam );

protected:	
	virtual void		Help();
	
	LGAEditField*		fNumRowsEditText;
	LGAEditField*		fNumColsEditText;
	
	LControl*			fBorderCheckBox;
	LGAEditField*		fBorderWidthEditText;
	LGAEditField*		fCellSpacingEditText;
	LGAEditField*		fCellPaddingEditText;
	
	LControl*			fCustomWidth;
	LGAEditField*		fWidthEditText;
	LControl*			fWidthPopup;
	
	LControl*			fCustomHeight;
	LGAEditField*		fHeightEditText;
	LControl*			fHeightPopup;
	
	LControl*			fCustomColor;
	CColorButton*		fColorCustomColor;

	LControl*			fCaptionAboveBelow;
	
	LGAPopup*			mTableAlignment;
	
	LControl*			mFastLayout;
	LControl*			mUseImage;
	CLargeEditField*	mImageFileName;
	LControl*			mLeaveImage;
};


class CFormatMsgColorAndImageDlog: public CEditDialog
{
public:
	enum				{ class_ID = 'Ec+i' };
	
						CFormatMsgColorAndImageDlog( LStream* inStream ) : CEditDialog( inStream ) {;}
	virtual				~CFormatMsgColorAndImageDlog() {;}
						
	virtual Boolean		CommitChanges(Boolean allPanes);
	virtual void		InitializeDialogControls();

protected:	
	virtual void		Help();
};


class CTarget: public CEditDialog
{
public:
	enum				{ class_ID = 'ETRG' };
	
						CTarget( LStream* inStream );
	virtual				~CTarget();
						
	void				CleanUpTargetString(char *target);
	Boolean				AlreadyExistsInDocument(char *anchor);
	virtual Boolean		CommitChanges(Boolean allPanes);
	virtual void		InitializeDialogControls();

protected:	
	virtual void		Help() {;} // Sorry, no help.
	
	char*				fOriginalTarget;
	CLargeEditField*	fTargetName;
};

class CLineProp: public CEditDialog
{
public:
	enum				{ class_ID = 'EDL0' };
	
						CLineProp( LStream* inStream );
	virtual				~CLineProp( );
						
	virtual Boolean		CommitChanges(Boolean allPanes);
	virtual void		FinishCreateSelf();
	virtual void		InitializeDialogControls();
//	virtual void		ListenToMessage( MessageT inMessage, void* ioParam );

protected:	
	virtual void		Help();

	LControl*			fLeftAlign;
	LControl*			fCenterAlign;
	LControl*			fRightAlign;
	
	LGAEditField*		fHeightEditText;
	LGAEditField*		fWidthEditText;
	LControl*			fPixelPercent;
	LControl*			fShading;
};


class CUnknownTag: public CEditDialog
{
public:
	enum				{ class_ID = 'EDUT' };
	
						CUnknownTag( LStream* inStream );
	virtual				~CUnknownTag();
						
	virtual Boolean		CommitChanges(Boolean allPanes);
	virtual void		InitializeDialogControls();
	virtual void		FinishCreateSelf();
	virtual void		ListenToMessage( MessageT inMessage, void* ioParam );

protected:	
	virtual void		Help();
	
	CLargeEditField*	fTargetName;
};


class MultipleSelectionSingleColumn: public LListBox
{
public:
	enum				{ class_ID = 'MSSC' };
	
						MultipleSelectionSingleColumn( LStream* inStream );
						
	virtual int16		NumItems();		
	virtual void		DeselectAll();		
	virtual void		SelectAll();		
	virtual void		AddItem( char* data, Boolean isSelected );
	virtual StringPtr	GetItem(Str255 outDescriptor, int32 rowNum) const;	// rowNum is zero based
	virtual void		RemoveAllItems();
	virtual Boolean		IsSelected(int32 rowNum);	// rowNum is zero based

};


class CPublishHistory
{
public:
	// Do we have any history at all?
	static Boolean		IsTherePublishHistory();
	
	// Get a particular entry
	static char*		GetPublishHistoryCharPtr(short whichone);
	 
	// Set a particular entry
	static void			SetPublishHistoryCharPtr(char* entry, short whichone);
	
	// Put an entry at the top of the list (and remove any duplicate)
	static void			AddPublishHistoryEntry(char *entry);
};

class CPublish: public CEditDialog
{
public:
	enum				{ class_ID = 'EPLS' };
	
						CPublish( LStream* inStream );
	virtual				~CPublish();
						
	virtual Boolean		CommitChanges(Boolean allPanes);
	virtual void		FinishCreateSelf();
	virtual void		InitializeDialogControls();
	virtual void		ListenToMessage( MessageT inMessage, void* ioParam );

protected:	
	virtual void		Help();
	char *				DocName();
	
	LCaption*			fLocalLocation;
	
	LControl*			fImageFiles;
	LControl*			fFolderFiles;
	LControl*			fDefaultLocation;
	
	MultipleSelectionSingleColumn*		fFileList;
	
	LGAEditField*		fPublishLocation;
	LGAEditField*		fUserID;
	LGAEditField*		fPassword;

	LControl*			fSavePassword;
	LGAPopup*			mHistoryList;
};


// This is a dialog box which contains a Tab control.
// This code was written using Cmd-C & Cmd-V from the CPrefWindow class.
// We don't need everything in CPrefWindow though, and I'm too lazy to make
// a nice base class for both at the moment.

class CTabbedDialog : public CEditDialog
{
public:
	enum 				{class_ID = 'EDTB'};
	
						CTabbedDialog( LStream* inStream );
	virtual				~CTabbedDialog();
	
	static void			RegisterViewTypes();
	void				FinishCreateSelf();
	virtual void		InitializeDialogControls();
	
	virtual void		SavePlace( LStream* ) { }
	virtual void		RestorePlace( LStream* ) { }

//	virtual void		ListenToMessage( MessageT inMessage, void* ioParam );
	
protected:		   
	virtual void		Help();
	virtual Boolean		CommitChanges(Boolean allPanes);
		   
	CTabControl*		mTabControl;
	CEditTabSwitcher*	mTabSwitcher;

};


/*****************************************************************************
 * class CEditorPrefContain. (used to be CPrefContain)
 *	Container for a related group of controls (1 pane of preference window)
 * 		and know how to:
 * - get proper variables from data, and assign the values to controls
 * - get values from controls back into data.
 *****************************************************************************/
class CEditorPrefContain : public LView, public LListener, public LTabGroup
{
public:
						CEditorPrefContain( LStream* inStream ) : LView( inStream ) {};
	virtual				~CEditorPrefContain() { };

	// ¥ link to little controls, and reset their values
	virtual void		FinishCreateSelf() { LView::FinishCreateSelf(); UReanimator::LinkListenerToControls(this, this, GetPaneID()); ControlsFromPref();}

	// ¥Êlistens to 'default' message
	void				ListenToMessage( MessageT, void* ) {};
								
	// ¥ initialize from preferences
	virtual void		ControlsFromPref() = 0;
	virtual void		PrefsFromControls() = 0;

	virtual void		DrawSelf();
};


class CEditContain: public CEditorPrefContain, public LBroadcaster
{
public:
						CEditContain(LStream* inStream): CEditorPrefContain( inStream ){ pExtra = NULL; }
						~CEditContain(){ XP_FREEIF(pExtra); }
	
	void				SetContext(MWContext*	context) {fContext = context;}				
	MWContext*			GetContext() { return fContext;}				

	void				SetInsertFlag(Boolean insert) {fInsert = insert;}
	Boolean				GetInsertFlag() { return fInsert;}
	
	void				SetLinkToLinkName(char** LinkNameLink) {fLinkName = LinkNameLink;}
	void				SetExtraHTMLString(char *s) { pExtra = s; }; 
	virtual void		Help() = NULL;
	
	int16				GetWinCSID() { return INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(fContext)); }

	virtual Boolean		AllFieldsOK() = NULL;

protected:
	MWContext*			fContext;
		   Boolean		fInsert;
		   char**		fLinkName;
		   char*		pExtra;
};


class CEDCharacterContain: public CEditContain
{
public:	
	enum 				{class_ID = '1edl'};
	
						CEDCharacterContain( LStream* inStream ) : CEditContain( inStream ){};
	
	virtual void		FinishCreateSelf();
	
	virtual void		PrefsFromControls();
	virtual void		ControlsFromPref();

	virtual void		ListenToMessage( MessageT inMessage, void* ioParam );
	virtual void		Help();
	
	virtual Boolean		AllFieldsOK() { return TRUE;}
	
protected:
	Boolean				fColorChanged;
	Boolean				fSizeChanged;
	
	LControl*			fTextSizePopup;
	LControl*			mFontMenu;
	Boolean				mFontChanged;
	
	LControl*			fColorDefaultRadio;
	LControl*			fColorCustomRadio;
	CColorButton*		fColorCustomColor;
	
	LControl*			fTextBoldCheck;
	LControl*			fTextItalicCheck;
	LControl*			fTextSuperscriptCheck;
	LControl*			fTextSubscriptCheck;
	LControl*			fTextNoBreaksCheck;
	LControl*			fTextUnderlineCheck;
	LControl*			fTextStrikethroughCheck;
	LControl*			fTextBlinkingCheck;

	LControl*			fClearTextStylesButton;
	LControl*			fClearAllStylesButton;
};


class CEDParagraphContain: public CEditContain
{
public:	
	enum 				{class_ID = '2edl'};
	
						CEDParagraphContain( LStream* inStream ) : CEditContain( inStream ){};
	
	virtual void		FinishCreateSelf();

	virtual void		PrefsFromControls();
	virtual void		ControlsFromPref();

	virtual void		ListenToMessage( MessageT inMessage, void* ioParam );
	
	virtual void		Help();
	
	virtual Boolean		AllFieldsOK();

protected:
	void				AdjustPopupsVisibility();
	
	LControl*			fParagraphStylePopup;
	LControl*			fContainerStylePopup;
	
	LControl*			fListStylePopup;
	LControl*			fNumberPopup;
	LControl*			fBulletPopup;
	LControl*			fStartNumberCaption;
	LGAEditField*		fStartNumberEditText;

	LControl*			fLeftAlignRadio;
	LControl*			fCenterAlignRadio;
	LControl*			fRightAlignRadio;

};


#ifdef COOL_IMAGE_RADIO_BUTTONS
class CImageAlignButton: public CBevelButton
{
public:
					enum { class_ID = 'BvRB' };
					
					CImageAlignButton( LStream* inStream ) : CBevelButton( inStream ){};
	virtual void		SetValue(
								Int32				inValue);

private:
		virtual void		HotSpotAction(
									Int16			inHotSpot,
									Boolean 		inCurrInside,
									Boolean			inPrevInside);
									
		virtual void		HotSpotResult(Int16 inHotSpot);
};
#endif


class CEDLinkContain: public CEditContain
{
public:	
	enum 				{class_ID = '3edl'};
	
						CEDLinkContain( LStream* inStream ) : CEditContain( inStream ){};
	virtual				~CEDLinkContain();
						
	virtual void		FinishCreateSelf();

	virtual void		PrefsFromControls();
	virtual void		ControlsFromPref();
	virtual void		Show();
	virtual void		Hide();

	virtual void		ListenToMessage( MessageT inMessage, void* ioParam );
	virtual void		Help();
	
	virtual Boolean		AllFieldsOK() { return TRUE;}

protected:
	void				SelectedFileUpdate();
	void				CurrentFileTargs();
	
	CLargeEditField*	fLinkedTextEdit;

	LControl*			fChooseFileLinkButton;
	LControl*			fRemoveLinkButton;
	CLargeEditField*	fLinkPageTextEdit;

	LControl*			fCurrentDocumentRadio;
	LControl*			fSelectedFileRadio;
	OneRowLListBox*		fTargetList;
	char*				fTargs;
};


class CEDImageContain: public CEditContain
{
public:	
	enum 				{class_ID = '4edl'};
	
						CEDImageContain( LStream* inStream );
	virtual				~CEDImageContain();
	
	virtual void		FinishCreateSelf();

	virtual void		PrefsFromControls();
	virtual void		ControlsFromPref();
	EDT_ImageData *		ImageDataFromControls();
	virtual void		Show();
	virtual void		Hide();

	virtual void		ListenToMessage( MessageT inMessage, void* ioParam );
	virtual void		Help();
	virtual Boolean		AllFieldsOK();
	
protected:
	void				AdjustEnable();

	char *				fSrcStr;
	char *				fLowSrcStr;
	
	CLargeEditField*	fImageFileName;	// was CEditBroadcaster
	CLargeEditField*	fImageAltFileName;
	CLargeEditField*	fImageAltTextEdit;
	
	LGAEditField*		fHeightTextEdit;
	LGAEditField*		fWidthTextEdit;
	LControl*			fImageLockedCheckBox;
	
	int32  				fOriginalWidth;        /* Width and Height we got on initial loading */
    int32  				fOriginalHeight;
    
	LGAEditField*		fLeftRightBorderTextEdit;
	LGAEditField*		fTopBottomBorderTextEdit;
	LGAEditField*		fSolidBorderTextEdit;

	LControl*			fCopyImageCheck;
	LControl*			fBackgroundImageCheck;
	LControl*			fRemoveImageMapButton;
	LControl*			fEditImageButton;

    Boolean				fLooseImageMap;
    Boolean				mBorderUnspecified;
	
	LControl*			mImageAlignmentPopup;
};


class CEDDocPropGeneralContain: public CEditContain
{
public:	
	enum 				{class_ID = '5edl'};
	
						CEDDocPropGeneralContain( LStream* inStream ) : CEditContain( inStream ){};
	
	virtual void		FinishCreateSelf();
	
	virtual void		PrefsFromControls();
	virtual void		ControlsFromPref();
	void				AddMeta(char *Name, CLargeEditField* value);

//	virtual void		ListenToMessage( MessageT inMessage, void* ioParam );
	virtual void		Help();
	virtual Boolean		AllFieldsOK() { return TRUE;}
	
protected:
	CLargeEditField*	fLocation;
	CLargeEditField*	fTitle;
	CLargeEditField*	fAuthor;
	CLargeEditField*	fDescription;
	CLargeEditField*	fKeywords;
	CLargeEditField*	fClassification;
};


// This should be moved to XP code in the future

typedef struct _EDT_ColorSchemeData {
    char *     pSchemeName;
    LO_Color   ColorText;
    LO_Color   ColorLink;
    LO_Color   ColorActiveLink;
    LO_Color   ColorFollowedLink;
    LO_Color   ColorBackground;
    char *     pBackgroundImage;
} EDT_ColorSchemeData;


class AppearanceContain: public CEditContain
{
						AppearanceContain( LStream* inStream ) : CEditContain( inStream ){};
	virtual void		FinishCreateSelf();
	virtual void		ListenToMessage( MessageT inMessage, void* ioParam );
	void 				ChooseImageFile();

protected:
	void 				UpdateTheWholeDamnDialogBox();
	
	LControl*			fCustomColor;
	LControl*			fBrowserColor;
	
	LControl*			fColorScheme;
	
	CChameleonView*		fExampleView;
	CColorButton*		fNormalText;
	CColorButton*		fLinkedText;
	CColorButton*		fActiveLinkedText;
	CColorButton*		fFollowedLinkedText;
	
	CChameleonCaption*	fExampleNormalText;
	CChameleonCaption*	fExampleLinkedTex;
	CChameleonCaption*	fExampleActiveLinkedText;
	CChameleonCaption*	fExampleFollowedLinkedText;
	
	CColorButton*		fSolidColor;
	LControl*			fImageFile;
	CLargeEditField*	fImageFileName;

	XP_List*			fSchemeData;
};


class CEDDocPropAppearanceContain: public AppearanceContain
{
public:	
	enum 				{class_ID = '6edl'};
	
						CEDDocPropAppearanceContain( LStream* inStream ) : AppearanceContain( inStream ){};
	virtual				~CEDDocPropAppearanceContain();
	
	
	virtual void		PrefsFromControls();
	virtual void		ControlsFromPref();

	virtual Boolean		AllFieldsOK() { return TRUE;}
	virtual void		Help();
};


class CEDDocAppearanceNoTab: public CEDDocPropAppearanceContain
{
public:	
	enum 				{class_ID = '6edL'};
	
						CEDDocAppearanceNoTab( LStream* inStream ) : CEDDocPropAppearanceContain( inStream ){};
	virtual				~CEDDocAppearanceNoTab() {;}
	
	virtual void		DrawSelf();
};


class CEDDocPropAdvancedContain: public CEditContain
{
public:	
	enum 				{class_ID = '7edl'};
	
						CEDDocPropAdvancedContain( LStream* inStream ) : CEditContain( inStream ){};
	virtual				~CEDDocPropAdvancedContain();
	
	virtual void		FinishCreateSelf();
	void				PutStringsInBuffer();
	Boolean				BufferUnique();
	virtual void		PrefsFromControls();
	virtual void		ControlsFromPref();

	virtual void		ListenToMessage( MessageT inMessage, void* ioParam );
	virtual void		Help();
	virtual Boolean		AllFieldsOK() { return TRUE;}
	
protected:
	int16				fbufferlen;
	char*				fbuffer;

	OneRowLListBox*		fSystemVariables;
	OneRowLListBox*		fUserVariables;
		
	CLargeEditField*	fName;
	CLargeEditField*	fValue;
};

class CEDTableContain: public CEditContain
{
public:	
	enum 				{class_ID = '8edl'};
	
						CEDTableContain( LStream* inStream ) : CEditContain( inStream ) { pExtra = NULL; }
						~CEDTableContain() { XP_FREEIF(pExtra); }
	
	virtual void		FinishCreateSelf();
	virtual void		Help();
	void				AdjustEnable();
	
	virtual void		PrefsFromControls();
	virtual void		ControlsFromPref();

	virtual void		ListenToMessage( MessageT inMessage, void* ioParam );
	virtual Boolean		AllFieldsOK();
	
protected:
	LControl*			fBorderCheckBox;
	LGAEditField*		fBorderWidthEditText;
	LGAEditField*		fCellSpacingEditText;
	LGAEditField*		fCellPaddingEditText;
	
	LControl*			fCustomWidth;
	LGAEditField*		fWidthEditText;
	LControl*			fWidthPopup;
	
	LControl*			fCustomHeight;
	LGAEditField*		fHeightEditText;
	LControl*			fHeightPopup;
	
	LControl*			fCustomColor;
	CColorButton*		fColorCustomColor;

	LControl*			fIncludeCaption;
	LControl*			fCaptionAboveBelow;
	
	LGAPopup*			mTableAlignment;

	LControl*			mFastLayout;
	LControl*			mUseImage;
	CLargeEditField*	mImageFileName;
	LControl*			mLeaveImage;
	
	char*				pExtra;
};


class CEDTableCellContain: public CEditContain
{
public:	
	enum 				{class_ID = 'aedl'};
	
						CEDTableCellContain( LStream* inStream ) : CEditContain( inStream ) { pExtra = NULL; }
						~CEDTableCellContain() { XP_FREEIF(pExtra); }
	
	virtual void		FinishCreateSelf();
	virtual void		Help();
	void				AdjustEnable();
	
	virtual void		PrefsFromControls();
	virtual void		ControlsFromPref();

	virtual void		ListenToMessage( MessageT inMessage, void* ioParam );
	virtual Boolean		AllFieldsOK();
	
protected:
	LGAEditField*		fRowSpanEditText;
	LGAEditField*		fColSpanEditText;

	LGAPopup*			fHorizontalAlignment;
	LGAPopup*			fVerticalAlignment;

	LControl*			fHeaderStyle;
	LControl*			fWrapText;

	LControl*			fCustomWidth;
	LGAEditField*		fWidthEditText;
	LControl*			fWidthPopup;
	
	LControl*			fCustomHeight;
	LGAEditField*		fHeightEditText;
	LControl*			fHeightPopup;
	
	LControl*			fCustomColor;
	CColorButton*		fColorCustomColor;
	
	LControl*			mNextButton;
	LControl*			mPreviousButton;

	LControl*			mUseImage;
	CLargeEditField*	mImageFileName;
	LControl*			mLeaveImage;

	char*				pExtra;
};

