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

//
// Mike Pinkerton, Netscape Communications
//
// Contains:
//
// ¥ÊCHyperTreeFlexTable
// 		A subclass of CStandardFlexTable to handle working with XP RDF Hyper Trees
//
// ¥ CHyperTreeSelector
//		Modifies the selection model used by PP to also consult HT to see if a row is
//		selected
//

#pragma once

#include "CStandardFlexTable.h"
#include "LTableRowSelector.h"
#include "CDynamicTooltips.h"
#include "CURLDragHelper.h"
#include "CImageIconMixin.h"

// STL Headers
#include <vector.h>

#pragma mark -- class CHyperTreeFlexTable --


class CHyperTreeFlexTable :
	public CStandardFlexTable, public CHTAwareURLDragMixin, public CTiledImageMixin
{
public:
	enum {
		class_ID = 'htFT',
		nameColumn = 'NCnC'
	};
	friend class CTreeIcon;
		
	CHyperTreeFlexTable(LStream *inStream);
	~CHyperTreeFlexTable();

	virtual void 	ChangeSort ( const LTableHeader::SortChange* inSortChange ) ;

	virtual void	OpenView(HT_View inHTView);
	virtual void	ExpandNode(HT_Resource inHTNode);
	virtual void	CollapseNode(HT_Resource inHTNode);
	virtual void	SyncSelectionWithHT ( ) ;

	HT_View			GetHTView()	const { return mHTView; }
	
	bool			IsViewSameAsBeforeDrag() const { return mViewBeforeDrag == mHTView; }

		// get/set the frame to which urls are dispatched. It's ok not to set
		// this as the default will be the top-most HTML view.
	virtual void		SetTargetFrame ( const char* inFrame ) ;
	virtual const char*	GetTargetFrame ( ) const ;

	virtual void	RedrawRow ( HT_Resource ) const ;
	
	virtual void	SetupColumns ( ) ;
		
protected:
		
		// Background image tiling stuff
	virtual void DrawStandby ( const Point & inTopLeft, IconTransformType inTransform ) const;
	virtual void DrawSelf ( ) ;
	virtual void ImageIsReady ( ) ;
	virtual void EraseTableBackground ( ) const;
	
		// CStandardFlexTable Overrides
	virtual void OpenRow ( TableIndexT inRow ) ;
	virtual Boolean	CellInitiatesDrag(const STableCell&) const;
	virtual Boolean CellSelects ( const STableCell& inCell ) const;
	virtual Boolean RowCanAcceptDrop ( DragReference inDragRef, TableIndexT inDropRow ) ;
	virtual Boolean RowCanAcceptDropBetweenAbove( DragReference inDragRef, TableIndexT inDropRow ) ;
	virtual void HiliteDropRow ( TableIndexT inRow, Boolean inDrawBarAbove ) ;
	virtual Boolean	RowIsContainer ( const TableIndexT & inRow ) const ;
	virtual Boolean	RowIsContainer ( const TableIndexT & inRow, Boolean* outIsExpanded ) const ;
	virtual void DrawCellContents( const STableCell &inCell, const Rect &inLocalRect);
	virtual void EraseCellBackground( const STableCell& inCell, const Rect& inLocalRect);
	virtual ResIDT GetIconID(TableIndexT inRow) const;		
	virtual void DrawIconsSelf( const STableCell& inCell, IconTransformType inTransformType,
										const Rect& inIconRect) const ;
	virtual UInt16 GetNestedLevel(TableIndexT inRow) const;
	virtual void SetCellExpansion( const STableCell& inCell, Boolean inExpand);
	virtual Boolean	CellHasDropFlag(const STableCell& inCell, Boolean& outIsExpanded) const;
	virtual Boolean TableSupportsNaturalOrderSort ( ) const ;
#ifdef USE_SELECTION_PROP
	virtual Boolean CellWantsClick( const STableCell & /*inCell*/ ) const ;
	virtual void ClickCell ( const STableCell & inCell, const SMouseDownEvent & inMouse ) ;		
#endif

		// Stuff related to hiliting
	virtual TableIndexT	GetHiliteColumn() const { return FindTitleColumnID(); } ;
	virtual Boolean GetHiliteTextRect ( TableIndexT inRow, Boolean inOkIfRowHidden, Rect& outRect) const ;
	virtual void GetMainRowText( TableIndexT inRow, char* outText, UInt16 inMaxBufferLength) const ;
	virtual void DoHiliteRgn ( RgnHandle inHiliteRgn ) const;
	virtual void DoHiliteRect ( const Rect & inHiliteRect ) const;
	virtual void HiliteSelection( Boolean inActively, Boolean inHilite) ;
	
	virtual void SetUpTableHelpers() ;	
	
	virtual Uint32 FindTitleColumnID ( ) const ;

		// Handle drag and drop
	virtual OSErr DragSelection(const STableCell& inCell, const SMouseDownEvent &inMouseDown);
	virtual void DoDragSendData ( FlavorType inFlavor, ItemReference inItemRef, DragReference inDragRef) ;
	virtual Boolean ItemIsAcceptable ( DragReference inDragRef, ItemReference inItemRef ) ;
	virtual void HandleDropOfHTResource ( HT_Resource node ) ;
	virtual void HandleDropOfPageProxy ( const char* inURL, const char* inTitle ) ;
	virtual void HandleDropOfLocalFile ( const char* inFileURL, const char* fileName,
											const HFSFlavor & /*inFileData*/ ) ;
	virtual void HandleDropOfText ( const char* inTextData ) ;
	virtual void DeleteSelectionByDragToTrash ( LArray & inItems ) ;
	virtual void ReceiveDragItem ( DragReference inDragRef, DragAttributes /*inDragAttrs*/,
											ItemReference inItemRef, Rect & /*inItemBounds*/ ) ;

		// for dynamic tooltip tracking and mouse cursor tracking
	virtual void AdjustCursorSelf ( Point /*inPoint*/, const EventRecord& inEvent ) ;
	virtual void CalcToolTipText( const STableCell& inCell,
									StringPtr outText,
									TextDrawingStuff& outStuff,
									Boolean& outTruncationOnly);	

		// Tree behavior properties
	virtual Boolean CanDoInlineEditing ( ) const;
	virtual Boolean TableDesiresSelectionTracking( ) const;

		// for inline editing
	virtual void InlineEditorDone ( ) ;

		// command stuff
	virtual void	DeleteSelection ( const EventRecord& inEvent );
	virtual void	FindCommandStatus ( CommandT inCommand, Boolean &outEnabled,
										Boolean &outUsesMark, Char16 &outMark, Str255 outName) ;
	virtual void 		SetEditParam(int w, int h, char* str, SPoint32& ImagePoint);

	HT_Resource		TopNode ( ) const { return HT_TopNode(GetHTView()); } 
	
//-----------------------------------
// Data
//-----------------------------------
	HT_View					mHTView;
	HT_Notification			mHTNotificationData;
	DragSendDataUPP			mSendDataUPP;
	HT_View					mViewBeforeDrag;
	HT_Resource				mDropNode;			// the HT node that corresponds to mDropRow

	STableCell				mTooltipCell;		// tracks where mouse is for tooltips

	bool					mHasBackgroundImage;	// is there a background image to be drawn?

	string					mTargetFrame;		// which frame are urls dispatched to?

}; // class CHyperTreeFlexTable


#pragma mark -- class CHyperTreeSelector --


//
// class CHyperTreeSelector
//
// A replacement for the standard selector class used by LTableView to sync the 
// selection with the backend HT_API 
//

class CHyperTreeSelector : public LTableRowSelector
{
public:

	CHyperTreeSelector ( LTableView *inTableView, Boolean inAllowMultiple=true )
		: LTableRowSelector ( inTableView, inAllowMultiple ), mTreeView(NULL) { } ;

		// overridden to ask HT if row is selected
	virtual Boolean CellIsSelected ( const STableCell &inCell ) const;

		// keep us in sync with HT when a new view is opened
	virtual void SyncSelectorWithHT ( ) ;
	
	void TreeView ( HT_View inNewView ) { mTreeView = inNewView; } ;

protected:

	virtual void DoSelect (	const TableIndexT inRow, Boolean inSelect, Boolean inHilite,
								Boolean	inNotify ) ;

	HT_View mTreeView;

}; // class CHyperTreeSelector


#pragma mark -- class CPopdownFlexTable --


//
// class CPopdownFlexTable
//
// The flex table used by the popdown view. It needs to broadcast some messages to the
// coordinator (such as when to close up) and do funny things to handle closing when
// items are opened.
//

class CPopdownFlexTable : public CHyperTreeFlexTable
{
public:	
	enum { class_ID = 'pdFT' };
	enum { msg_ClosePopdownTree = 'clos' };
		
	CPopdownFlexTable(LStream *inStream);
	~CPopdownFlexTable();

	static HT_Pane HTPaneOriginatingDragAndDrop ( ) { return sPaneOriginatingDragAndDrop; }
	
private:

	void	OpenSelection();
	void	OpenRow ( TableIndexT inRow ) ;
	OSErr	DragSelection(const STableCell& inCell, const SMouseDownEvent &inMouseDown);

	// this is a holder for the pane that originates the drag and drop so that when
	// a drop is done on a different popdown tree, the HT_Resources still have a valid pane.
	// This will be deleted once the d&d finishes.
	static HT_Pane sPaneOriginatingDragAndDrop;
	
}; // class CPopdownFlexTable


#pragma mark -- class CTreeIcon --


//
// class CTreeIcon
//
// A very simple class that knows how to draw gifs as icons.
//

class CTreeIcon : public CImageIconMixin
{
public:
	CTreeIcon ( const string & inURL, const CHyperTreeFlexTable* inTree, HT_Resource inNode ) ;
	~CTreeIcon ( ) ;
	
private:
	void ImageIsReady ( ) ;
	void DrawStandby ( const Point & inTopLeft, IconTransformType inTransform ) const;
	
	const CHyperTreeFlexTable*	mTree;
	HT_Resource					mNode;
	
}; // class CTreeIcon