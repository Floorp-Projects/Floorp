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

#include <TextServices.h>
#include "CHTMLView.h"

// dangling prototype
Boolean GetCaretPosition(MWContext *context, LO_Element * element, int32 caretPos, 
									int32* caretX, int32* caretYLow, int32* caretYHigh );


class LGAPopup;
class CPatternButtonPopup;
class CColorPopup;
class CComposeSession;
class CFontMenuPopup;

class HTMLInlineTSMProxy;
class HoldUpdatesProxy;

class CEditView: public CHTMLView
{
#if !defined(__MWERKS__) || (__MWERKS__ >= 0x2000)
	typedef CHTMLView inherited;
#endif

public:
	enum { pane_ID = 'html', class_ID = 'Edtp', dashMark = '-' };
	enum {
			STRPOUND_EDITOR_MENUS			= 5101,
			EDITOR_MENU_SHOW_PARA_SYMBOLS		= 1,
			EDITOR_MENU_HIDE_PARA_SYMBOLS		= 2,
			EDITOR_MENU_UNDO					= 3,
			EDITOR_MENU_REDO					= 4,
			EDITOR_MENU_SHOW_COMP_TOOLBAR		= 5,
			EDITOR_MENU_HIDE_COMP_TOOLBAR		= 6,
			EDITOR_MENU_SHOW_FORMAT_TOOLBAR		= 7,
			EDITOR_MENU_HIDE_FORMAT_TOOLBAR		= 8,
			EDITOR_MENU_SHOW_TABLE_BORDERS		= 9,
			EDITOR_MENU_HIDE_TABLE_BORDERS		= 10,
			EDITOR_MENU_CHARACTER_ATTRIBS		= 11,
			EDITOR_MENU_IMAGE_ATTRIBUTES		= 12,
			EDITOR_MENU_LINK_ATTRIBUTES			= 13,
			EDITOR_MENU_LINE_ATTRIBUTES			= 14,
			EDITOR_MENU_TABLE_ATTRIBUTES		= 15,
			EDITOR_MENU_UNKNOWN_ATTRIBUTES		= 16,
			EDITOR_MENU_TARGET_ATTRIBUTES		= 17
	};
	enum {
			eMouseHysteresis = 6,
			eSelectionBorder = 3
	};
	
	// ¥¥ Constructors
					CEditView(LStream * inStream);
					~CEditView();
	virtual void	FinishCreateSelf(void);

	// ¥¥ Command handling
	virtual	Boolean ObeyCommand( CommandT inCommand, void *ioParam );
	virtual void	ListenToMessage( MessageT inMessage, void* ioParam );
	virtual void  	FindCommandStatus( CommandT inCommand,
							Boolean& outEnabled, Boolean& outUsesMark, 
							Char16& outMark,Str255 outName );
			Boolean	FindCommandStatusForContextMenu( CommandT inCommand,
							Boolean &outEnabled, Boolean &outUsesMark,
							Char16 &outMark, Str255 outName );
	
			Boolean IsPastable(Char16 theChar);
			int 	FindQueuedKeys( char *keys_in_q );
	virtual Boolean HandleKeyPress( const EventRecord& inKeyEvent );
	
	virtual void 	AdaptToSuperFrameSize( Int32 inSurrWidthDelta, 
											Int32 inSurrHeightDelta, Boolean inRefresh );
								
	virtual void	BeTarget();
	virtual void	DontBeTarget();

			void	HandleCut();
			void	HandleCopy();
			void	HandlePaste();
	
	TSMDocumentID	midocID;
	
	virtual void	CreateFindWindow();
	virtual void	SetContext( CBrowserContext* inNewContext );
	virtual URL_Struct *GetURLForPrinting( Boolean& outSuppressURLCaption, MWContext *printingContext );

	void			TakeOffDuty();
	virtual void	PutOnDuty(LCommander*);
	
	Bool 			PtInSelectedRegion(SPoint32 cpPoint );
	virtual void	DrawSelf( void );
	virtual void 	ActivateSelf();
	virtual void 	DeactivateSelf();
	
	Bool			SaveDocument();
	Bool			SaveDocumentAs();
	Bool			VerifySaveUpToDate();
	

	// ¥¥ cursor calls and caret functionality
	virtual void	AdjustCursorSelf( Point inPortPt, const EventRecord& inMacEvent );

	virtual void	SpendTime( const EventRecord& inMacEvent );
	void	 		DrawCaret( Boolean doErase );
	void	 		EraseCaret();
	void			HideCaret( Boolean mhide ) { EraseCaret(); mHideCaret = mhide; }
	void			PlaceCaret(int32 caretX, int32 caretYLow, int32 caretYHigh);
	void			RemoveCaret();
	void 			DisplayGenericCaret( MWContext *context, LO_Element * pLoAny, 
										ED_CaretObjectPosition caretPos );
	
	// ¥¥ Drag and Drop 
	virtual Boolean	ItemIsAcceptable (DragReference	dragRef, ItemReference itemRef);
	virtual void	ReceiveDragItem( DragReference inDragRef, DragAttributes inDragAttr, 
									ItemReference inItemRef, Rect& inItemBounds );
	virtual void	DoDragSendData( FlavorType inFlavor, ItemReference inItemRef,
									DragReference inDragRef );
	
	virtual void	ClickSelf (const SMouseDownEvent& where );
	virtual	Boolean	ClickTrackSelection( const SMouseDownEvent&	inMouseDown, 
										CHTMLClickRecord& inClickRecord );
	
	virtual Boolean	SetDefaultCSID( Int16 inPreferredCSID );
	CBrowserContext	*GetNSContext() { return mContext; };

	void			SetHoldUpdates(HoldUpdatesProxy* inHoldUpdates) {mHoldUpdates = inHoldUpdates;};

	Boolean			mEditorDoneLoading;
	Boolean			IsDoneLoading() const { return mEditorDoneLoading; }

	// ¥¥ FE_* calls
	void			DocumentChanged( int32 iStartY, int32 iHeight );
	void			GetDocAndWindowPosition( SPoint32 &frameLocation, SPoint32 &imageLocation, SDimension16 &frameSize );
	
	// ¥¥ formatting query calls
	void			UseCharFormattingCache() { CanUseCharFormatting(); mUseCharFormattingCache = true; }
	void			DontUseCharFormattingCache() { mUseCharFormattingCache = false; }

	class StUseCharFormattingCache
	{
		public:
			StUseCharFormattingCache( CEditView& p ) : view(p) { view.UseCharFormattingCache(); }
			~StUseCharFormattingCache() { view.DontUseCharFormattingCache(); }
		
		private:
			CEditView &view;	
	};

	LGAPopup		* mParagraphToolbarPopup;
	LGAPopup		* mSizeToolbarPopup;
	CFontMenuPopup	* mFontToolbarPopup;
	CPatternButtonPopup	* mAlignToolbarPopup;
	CColorPopup		* mColorPopup;

protected:
	virtual Boolean		IsGrowCachingEnabled() const { return !mEditorDoneLoading; }

	virtual void 		LayoutNewDocument( URL_Struct *inURL, Int32 *inWidth,
										Int32 *inHeight, Int32 *inMarginWidth, Int32 *inMarginHeight );

	// ¥¥ FE_* calls
	virtual void 		SetDocPosition( int inLocation, Int32 inX, Int32 inY,
									Boolean inScrollEvenIfVisible = false);
	virtual void 		DisplayLineFeed( int inLocation, LO_LinefeedStruct *inLinefeedStruct, XP_Bool inNeedBG );
	virtual void 		DisplayHR( int inLocation, LO_HorizRuleStruct *inRuleStruct );
	virtual void 		DisplaySubtext( int inLocation, LO_TextStruct *inText, 
										Int32 inStartPos, Int32 inEndPos, XP_Bool inNeedBG );
	virtual void 		EraseBackground( int inLocation, Int32 inX, Int32 inY,
										Uint32 inWidth, Uint32 inHeight, LO_Color *inColor );
	virtual void		GetDefaultBackgroundColor(LO_Color* outColor) const;
	virtual void 		DisplayTable( int inLocation, LO_TableStruct *inTableStruct );
	virtual void		DisplayCell( int inLocation, LO_CellStruct *inCellStruct );
	virtual	void 		InvalidateEntireTableOrCell( LO_Element* inElement );
	virtual	void 		DisplayAddRowOrColBorder( XP_Rect* inRect, XP_Bool inDoErase );
	virtual void		UpdateEnableStates();
	virtual void		DisplayFeedback( int inLocation, LO_Element *inElement );
	virtual void		DisplaySelectionFeedback( uint16 ele_attrmask, const Rect &inRect );
	
	
	virtual void		InsideDropArea( DragReference inDragRef );
	virtual void 		EnterDropArea( DragReference inDragRef, Boolean inDragHasLeftSender );
	DragReference 		mDragRef;

	enum {
		ED_SELECTION_BORDER = 3
		};
	
    enum { MAX_Q_SIZE = 12 };		// Used for checking out key strokes waiting in FindQueuedKeys

	void 			InsertDefaultLine();
	void			DoReload( void );
	void			ToFromList( intn listType, ED_ListType elementType );
	void			NoteEditorRepagination( void );
	Boolean			CanUseCharFormatting();

	Boolean			IsMouseInSelection( SPoint32 pt, CL_Layer *curLayer, Rect& selectRect );
	Boolean			mDoContinueSelection;

	HTMLInlineTSMProxy*		mProxy;
	HoldUpdatesProxy* 		mHoldUpdates;

	// more caret blinking and related stuff
	Boolean			mCaretDrawn, mCaretActive;
	Boolean			mHideCaret;
	Boolean			mDisplayParagraphMarks;

	Point			mOldPoint;	// Last place cursor was adjusted. No initializing
	long			mOldLastElementOver;	// id of the last element the cursor was over

	unsigned long	mLastBlink;
	int32			mCaretX;
	int32			mCaretYLow;
	int32			mCaretYHigh;
	
	Boolean			mUseCharFormattingCache;
	Boolean			mIsCharFormatting;
	
	// these are only to be used during drag of data in composer
	char			*mDragData;	// warning this really isn't a "char" but void* data!!!
	int32			mDragDataLength;
};	// class CEditView


//======================================
class CMailEditView : public CEditView
//======================================
{
public:
	enum { pane_ID = 'html', class_ID = 'MEdp' };
	
	// ¥¥ Constructors

					CMailEditView(LStream * inStream);
	virtual void	InstallBackgroundColor();
	virtual void	GetDefaultBackgroundColor(LO_Color* outColor) const;
	virtual void	InitMailCompose();
			void	SetComposeSession( CComposeSession *c ) { mComposeSession = c; };
			void	SetInitialText( const char *textp );
			void    InsertMessageCompositionText( const char* text,
					 XP_Bool leaveCursorBeginning, XP_Bool isHTML );
			void	DisplayDefaultTextBody();

private:
	Int32			mStartQuoteOffset;
	Int32			mEndQuoteOffset;
	Boolean			mHasAutoQuoted;
	Boolean			mHasInsertSignature;
	Boolean			mCursorSet;
	char			*mInitialText; // Draft text
	CComposeSession *mComposeSession;
}; // class CMailEditView

