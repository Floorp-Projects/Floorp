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

// STL Headers
#include <vector.h>

// RDF Headers
#include "rdf.h"
#include "htrdf.h"


#pragma mark -- class CHyperTreeFlexTable --


class CHyperTreeFlexTable :
	public CStandardFlexTable, public CDynamicTooltipMixin
{
public:
		enum {
			class_ID = 'htFT',
			nameColumn = 'NCnC'
		};
		
	CHyperTreeFlexTable(LStream *inStream);
	~CHyperTreeFlexTable();

	virtual void 	ChangeSort ( const LTableHeader::SortChange* inSortChange ) ;

	virtual void	OpenView(HT_View inHTView);
	virtual void	ExpandNode(HT_Resource inHTNode);
	virtual void	CollapseNode(HT_Resource inHTNode);
	virtual void	SyncSelectionWithHT ( ) ;

	HT_View			GetHTView()	const { return mHTView; }
	
	bool			IsViewSameAsBeforeDrag() const { return mViewBeforeDrag == mHTView; }

protected:

	
		// CStandardFlexTable Overrides
	virtual void OpenRow ( TableIndexT inRow ) ;
	virtual Boolean	CellInitiatesDrag(const STableCell&) const;
	virtual Boolean CellSelects ( const STableCell& inCell ) const;
	virtual Boolean RowCanAcceptDrop ( DragReference inDragRef, TableIndexT inDropRow ) ;
	virtual Boolean RowCanAcceptDropBetweenAbove( DragReference inDragRef, TableIndexT inDropRow ) ;
	virtual FlavorType FindAcceptableFlavor ( DragReference inDragRef, ItemReference inItemRef ) ;
	virtual Boolean NodeCanAcceptDrop ( DragReference inDragRef, HT_Resource inTargetNode ) ;
	virtual void HiliteDropRow ( TableIndexT inRow, Boolean inDrawBarAbove ) ;
	virtual Boolean	RowIsContainer ( const TableIndexT & /* inRow */ ) const ;
	virtual void DrawCellContents( const STableCell &inCell, const Rect &inLocalRect);
	virtual ResIDT GetIconID(TableIndexT inRow) const;		
	virtual UInt16 GetNestedLevel(TableIndexT inRow) const;
	virtual void SetCellExpansion( const STableCell& inCell, Boolean inExpand);
	virtual Boolean	CellHasDropFlag(const STableCell& inCell, Boolean& outIsExpanded) const;
		
		// Stuff related to hiliting
	virtual TableIndexT	GetHiliteColumn() const { return 1; } ;
	virtual Boolean GetHiliteTextRect ( TableIndexT inRow, Rect& outRect) const ;
	virtual void GetMainRowText( TableIndexT inRow, char* outText, UInt16 inMaxBufferLength) const ;
	virtual void DoHiliteRgn ( RgnHandle inHiliteRgn ) const;
	virtual void DoHiliteRect ( const Rect & inHiliteRect ) const;
	virtual void HiliteSelection( Boolean inActively, Boolean inHilite) ;
	
	virtual void SetUpTableHelpers() ;	
	
	virtual Uint32 FindTitleColumnID ( ) const ;

		// Handle drag and drop
	virtual void DragSelection(const STableCell& inCell, const SMouseDownEvent &inMouseDown);
	virtual void DoDragSendData ( FlavorType inFlavor, ItemReference inItemRef, DragReference inDragRef) ;
	virtual void ReceiveDragItem ( DragReference inDragRef, DragAttributes inDragAttrs,
										ItemReference inItemRef, Rect &inItemBounds ) ;
	virtual Boolean ItemIsAcceptable ( DragReference inDragRef, ItemReference inItemRef ) ;
	virtual void HandleDropOfHTResource ( HT_Resource node ) ;
	virtual void HandleDropOfPageProxy ( vector<char> & inURLAndTitle ) ;
	virtual void HandleDropOfLocalFile ( const char* inFileURL, const char* fileName ) ;
	virtual void DeleteSelectionByDragToTrash ( LArray & inItems ) ;

		// for dynamic tooltip tracking and mouse cursor tracking
	virtual void MouseWithin ( Point inPortPt, const EventRecord& ) ;	
	virtual void MouseLeave ( ) ;
	virtual void AdjustCursorSelf ( Point /*inPoint*/, const EventRecord& inEvent ) ;
	virtual void FindTooltipForMouseLocation ( const EventRecord& inMacEvent,
													StringPtr outTip ) ;

		// for inline editing
	virtual void InlineEditorDone ( ) ;
	virtual bool CanDoInlineEditing ( ) ;

		// command stuff
	virtual void	DeleteSelection ( );
	virtual void	FindCommandStatus ( CommandT inCommand, Boolean &outEnabled,
										Boolean &outUsesMark, Char16 &outMark, Str255 outName) ;

//-----------------------------------
// Data
//-----------------------------------
	HT_View					mHTView;
	HT_Notification			mHTNotificationData;
	DragSendDataUPP			mSendDataUPP;
	HT_View					mViewBeforeDrag;
	HT_Resource				mDropNode;			// the HT node that corresponds to mDropRow

	STableCell				mTooltipCell;		// tracks where mouse is for tooltips	

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