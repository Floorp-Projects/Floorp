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
// Interface for the table that is the main component of the personal toolbar.
//

#pragma once

#include <vector>

#include <LSmallIconTable.h>
#include "ntypes.h"
#include "CDynamicTooltips.h"
#include "CURLDragHelper.h"
#include "CRDFNotificationHandler.h"


//
// CUserButtonInfo
//
class CUserButtonInfo 
{
public:

	CUserButtonInfo( const string & pName, const string & pURL, Uint32 nBitmapID, Uint32 nBitmapIndex, 
						bool bIsFolder, HT_Resource inRes );
	CUserButtonInfo ( ) ;
	~CUserButtonInfo();

	const string & GetName(void) const 				{ return mName; }
	const string & GetURL(void) const 				{ return mURL; }
	HT_Resource GetHTResource() const				{ return mResource; }
	
	bool IsResourceID(void) const 					{ return mIsResourceID; }
	Uint32 GetBitmapID(void) const 					{ return mBitmapID; }
	Uint32 GetBitmapIndex(void) const 				{ return mBitmapIndex; }
	bool IsFolder(void) const 						{ return mIsFolder;}

private:
	string mName;
	string mURL;
	bool	mIsResourceID;
	Uint32	mBitmapID;
	Uint32	mBitmapIndex;
	string	mBitmapFile;
	bool	mIsFolder;
	HT_Resource mResource;

}; // CUserButtonInfo


class CPersonalToolbarTable : public LSmallIconTable, public LDragAndDrop,
								public CDynamicTooltipMixin, public CHTAwareURLDragMixin,
								public CRDFNotificationHandler
{
	public:
	
		enum { class_ID = 'PerT', kTextTraitsID = 130 };
		enum { kBOOKMARK_ICON = 15313, kFOLDER_ICON = -3999 };
		enum { kMinPersonalToolbarChars = 15, kMaxPersonalToolbarChars = 30 };
		
		CPersonalToolbarTable ( LStream* inStream ) ;
		virtual ~CPersonalToolbarTable ( ) ;

			// calculate tooltip to display url text for the item mouse is over
		virtual void FindTooltipForMouseLocation ( const EventRecord& inMacEvent,
													StringPtr outTip );

	protected:
	
		class SomethingBadInHTException { } ;

		typedef vector<CUserButtonInfo>		ButtonList;
		typedef ButtonList::iterator		ButtonListIterator;
		typedef ButtonList::const_iterator	ButtonListConstIterator;
		
		virtual void FillInToolbar ( ) ;
		void InitializeButtonInfo ( ) ;
		void ToolbarChanged ( ) ;

			//--- utilities
		Uint32 GetMaxToolbarButtonChars() const { return mMaxToolbarButtonChars; }
		Uint32 GetMinToolbarButtonChars() const { return mMinToolbarButtonChars; }
		void SetMaxToolbarButtonChars(Uint32 inNewMax) { mMaxToolbarButtonChars = inNewMax; }
		void SetMinToolbarButtonChars(Uint32 inNewMin) { mMinToolbarButtonChars = inNewMin; }
		HT_View GetHTView ( ) const { return mToolbarView; }
		const CUserButtonInfo & GetInfoForPPColumn ( const TableIndexT & inCol ) const;

		void AddButton ( HT_Resource inBookmark, Uint32 inIndex) ;
		void AddButton ( const string & inURL, const string & inTitle, Uint32 inIndex ) ;
		void RemoveButton ( Uint32 inIndex );

		void HandleNotification( HT_Notification notifyStruct, HT_Resource node, HT_Event event);

			// for handling mouse tracking
		virtual void MouseLeave ( ) ;
		virtual void MouseWithin ( Point inPortPt, const EventRecord& ) ;
		virtual void Click ( SMouseDownEvent &inMouseDown ) ;

		virtual void ClickCell(const STableCell	&inCell, const SMouseDownEvent &inMouseDown);
		virtual Boolean ClickSelect( const STableCell &inCell, const SMouseDownEvent &inMouseDown);
		virtual void FinishCreateSelf ( ) ;
		virtual void DrawCell ( const STableCell &inCell, const Rect &inLocalRect ) ;
		virtual void RedrawCellWithHilite ( const STableCell inCell, bool inHiliteOn ) ;
		virtual void ResizeFrameBy ( Int16 inWidth, Int16 inHeight, Boolean inRefresh );

			// override to do nothing
		virtual void HiliteSelection ( Boolean /*inActively*/, Boolean /*inHilite*/) { } ;
		
			// drag and drop overrides for drop feedback, etc
		void HiliteDropArea ( DragReference inDragRef );
		Boolean ItemIsAcceptable ( DragReference inDragRef, ItemReference inItemRef ) ;
		void InsideDropArea ( DragReference inDragRef ) ;
		void DrawDividingLine ( TableIndexT inCol ) ;
		void LeaveDropArea( DragReference inDragRef ) ;
		virtual void HandleDropOfPageProxy ( const char* inURL, const char* inTitle ) ;
		virtual void HandleDropOfLocalFile ( const char* inFileURL, const char* fileName,
												const HFSFlavor & /*inFileData*/ ) ;
		virtual void HandleDropOfText ( const char* inTextData ) ;
		virtual void HandleDropOfHTResource ( HT_Resource node ) ;

		virtual void ComputeItemDropAreas ( const Rect & inLocalCellRect, Rect & oLeftSide, 
												Rect & oRightSide ) ;
		virtual void ComputeFolderDropAreas ( const Rect & inLocalCellRect, Rect & oLeftSide, 
												Rect & oRightSide ) ;
		virtual Rect ComputeTextRect ( const SIconTableRec & inText, const Rect & inLocalRect ) ;
		virtual void RedrawCellWithTextClipping ( const STableCell & inCell ) ;

			// send data when a drop occurs
		void DoDragSendData( FlavorType inFlavor, ItemReference inItemRef, DragReference inDragRef) ;
		void ReceiveDragItem ( DragReference inDragRef, DragAttributes inDragAttrs,
								ItemReference inItemRef, Rect &inItemBounds ) ;
	
			// these are valid only during a D&D and are used for hiliting and determining
			// the drop location
		STableCell	mDraggedCell;
		TableIndexT mDropCol;			// position where a drop will go
		bool		mDropOn;			// are they on a folder?
		Rect		mTextHiliteRect;	// cached rect drawn behind selected folder title
		TableIndexT mHiliteCol;			// which column is mouse hovering over?

		HT_View		mToolbarView;
		HT_Resource	mToolbarRoot;

		ButtonList*	mButtonList;		// list of buttons pulled from HT
		bool		mIsInitialized;		// is this class ready for prime time?
		
		static DragSendDataUPP sSendDataUPP;

		static Uint32 mMaxToolbarButtonChars;
		static Uint32 mMinToolbarButtonChars;
		static const char* kMaxButtonCharsPref;
		static const char* kMinButtonCharsPref;

}; // CPersonalToolbarTable
