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


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDragBarContainer.h
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#pragma once

#include "CSwatchBrokerView.h"
#include <LDragAndDrop.h>
#include <LListener.h>
#include <LArray.h>

class CDragBarDockControl;
class CDragBar;

class CDragBarContainer :
			public CBrokeredView,
			public LDropArea,
			public LListener
{
	public:

		enum { class_ID = 'DbCt' };

								CDragBarContainer(LStream* inStream);
		virtual					~CDragBarContainer();
		
		virtual Boolean			PointInDropArea(Point inGlobalPt);
	
		virtual void			ListenToMessage(
									MessageT			inMessage,
									void*				ioParam);

		virtual void			SavePlace(LStream *outPlace);
		virtual void			RestorePlace(LStream *inPlace);

		// from JavaScript one can hide/show drag bars independently of docking
		virtual void			HideBar(CDragBar* inBar, Boolean inRefresh = false);
		virtual void			ShowBar(CDragBar* inBar, Boolean inRefresh = false);
	protected:

		virtual	void			NoteCollapseBar(CDragBar* inBar);
		virtual	void			NoteExpandBar(CDragBar* inBar);
	
		virtual	void			RepositionBars(void);

		virtual	void			SwapBars(
									CDragBar* 			inSouceBar, 
									CDragBar*			inDestBar,
									Boolean				inRefresh = false);
									
		virtual	void			AdjustContainer(void);
		virtual	void			AdjustDock(void);

		virtual	void			FinishCreateSelf(void);

			// DROP AREA BEHAVIOUR

		virtual Boolean			ItemIsAcceptable(
									DragReference		inDragRef,
									ItemReference		inItemRef);

		virtual void			EnterDropArea(
									DragReference		inDragRef,
									Boolean				inDragHasLeftSender);
									
		virtual void			LeaveDropArea(
									DragReference		inDragRef);
									
		virtual void			InsideDropArea(
									DragReference		inDragRef);
									
		virtual void			ReceiveDragItem(
									DragReference		inDragRef,
									DragAttributes		inDragAttrs,
									ItemReference		inItemRef,
									Rect				&inItemBounds);


		ResIDT					mBarListResID;
		LArray					mBars;		
		CDragBarDockControl*	mDock;
};


