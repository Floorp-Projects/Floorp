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
//	CSwatchBrokerView.h
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#pragma once

#include <LView.h>
#include <LBroadcaster.h>
#include <LListener.h>



// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

const MessageT	msg_BrokeredViewAboutToChangeSize	= 'bvas';	// CBrokeredView*
const MessageT	msg_BrokeredViewChangeSize 			= 'bvcs';	// CBrokeredView*
const MessageT	msg_BrokeredViewAboutToMove			= 'bvam';	// CBrokeredView*
const MessageT	msg_BrokeredViewMoved				= 'bvmv';	// CBrokeredView*



class CBrokeredView :
				public LView,
				public LBroadcaster
{
	public:
		enum { class_ID = 'BkVw' };
							   CBrokeredView(LStream* inStream);
		void					GetPendingFrameSize(SDimension16& outSize) const;
		void					GetPendingFrameLocation(SPoint32& outLocation) const;
		
		virtual void			ResizeFrameBy(
									Int16 				inWidthDelta,
									Int16 				inHeightDelta,
									Boolean 			inRefresh);
	
		virtual void			MoveBy(
									Int32				inHorizDelta,
									Int32 				inVertDelta,
									Boolean 			inRefresh);
	protected:

		SDimension16			mPendingSize;
		SPoint32				mPendingLocation;
};




class CSwatchBrokerView :
				public LView,
				public LListener
{
	public:
		enum { class_ID = 'SwBv' };

								CSwatchBrokerView(LStream* inStream);
		
		virtual	void			AdaptToSuperFrameSize(
									Int32				inSurrWidthDelta,
									Int32				inSurrHeightDelta,
									Boolean				inRefresh);

		virtual	void			ListenToMessage(
									MessageT			inMessage,
									void*				ioParam);

	protected:

		virtual void			FinishCreateSelf(void);
		
		virtual	void			AdaptToBrokeredViewResize(
									CBrokeredView*		inView,
									const SDimension16	inNewSize,
									Boolean				inRefresh);
		
		virtual	void			AdaptToBrokeredViewMove(
									CBrokeredView*		inView,
									const SPoint32&		inNewLocation,
									Boolean				inRefresh);
	
		PaneIDT					mDynamicPaneID;	
		CBrokeredView*			mDynamicPane;
		
		PaneIDT					mBalancePaneID;
		LPane*					mBalancePane;
};




