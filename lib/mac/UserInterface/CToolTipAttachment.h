/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#pragma once

#include <LPane.h>
#include "CMouseDispatcher.h"
#include "PascalString.h"


class CToolTipPane;

const CommandT msg_HideTooltip = 1000;		// cmd sent to hide the tooltip


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CToolTipAttachment
//	
//	This attachment can be attached to any pane from within Constructor.  Tool
//	Tips require a CMouseDispatcher object be present in order to get time.
//
//	This attachment reads in a resource ID of a PPob which contains the
//	definition of a CToolTipPane (or subclass thereof).
//
//	You should not need to subclass this class unless you want to modify the
//	behaviour of the mouse tracking mechanism.  If you want to change the
//	contents or appearance of the tips, you should subclass the CToolTipPane
//	class (see notes below).
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class CToolTipAttachment : public CMouseTrackAttachment
{
	public:
		enum { class_ID = 'TTaT' };
		
		static void			Enable(Boolean inEnableTips);

							CToolTipAttachment(LStream* inStream);	
							CToolTipAttachment(UInt32 inDelayTicks, ResIDT inPaneResID);	
		virtual				~CToolTipAttachment();

		virtual	void		NoteTipDied(CToolTipPane* inTip);
		
	protected:
	
		virtual void		ExecuteSelf(
									MessageT			inMessage,
									void*				ioParam);
	
		virtual void		MouseEnter(
									Point				inPortPt,
									const EventRecord&	inMacEvent);

		virtual void		MouseWithin(
									Point				inPortPt,
									const EventRecord&	inMacEvent);

		virtual void		MouseLeave(void);

		virtual	void		ShowToolTip(const EventRecord&	inMacEvent);
		virtual	void		HideToolTip(void);
		
		Boolean				IsDelayElapsed(Uint32 inTicks) const;
		Boolean				IsToolTipActive(void) const;
		Boolean				IsTipCancelingEvent(const EventRecord& inMacEvent) const;
		void				ResetTriggerInterval(Uint32 inWithTicks);

		virtual void		CalcTipText(
									LWindow*				inOwningWindow,
									const EventRecord&		inMacEvent,
									StringPtr				outTipText);

		ResIDT				mTipPaneResID;		
		Uint32				mEnterTicks;
		Uint32				mDelayTicks;

		static CToolTipPane* sActiveTip;
		static Boolean 		sTipsEnabled;
};

inline Boolean CToolTipAttachment::IsDelayElapsed(Uint32 inTicks) const
	{	return ((inTicks - mEnterTicks) > mDelayTicks);		}
inline Boolean CToolTipAttachment::IsToolTipActive(void) const
	{	return (sActiveTip != NULL);						}
inline void CToolTipAttachment::ResetTriggerInterval(Uint32 inWithTicks)
	{	mEnterTicks = inWithTicks;							}
inline void	CToolTipAttachment::Enable(Boolean inEnableTips)
	{ 	sTipsEnabled = inEnableTips; 						}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CToolTipPane
//
//	This is your basic little tip blurb.  If you want to change the appearance
//	or content calculation of a Tool Tip, subclass this class and not the
//	controlling attachment.
//
//	Instances of this class are reanimated when the controlling attachment
//	needs to display one.
//
//	NOTE: Unless you want to see this pane get resized and positioned in its
//	owning window, you better make sure that the visible flag in the resource
//	definition is off.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class CToolTipPane : public LPane
{
	public:
		enum { class_ID = 'TTpn' };
		
							CToolTipPane(LStream* inStream);
		virtual				~CToolTipPane();
	
		virtual void		SetParent(CToolTipAttachment*	inParent);
							
		virtual void		SetOwningPane(LPane*			inOwningPane);
							
		virtual void		CalcFrameWithRespectTo(
									LWindow*				inOwningWindow,
									const EventRecord&		inMacEvent,
									Rect& 					outTipFrame);
	
		virtual	void 		CalcTipText(
									LWindow*				inOwningWindow,
									const EventRecord&		inMacEvent,
									StringPtr				outTipText);

		virtual void		SetDescriptor(ConstStringPtr	inDescriptor);
		
		virtual Boolean		WantsToCancel(Point				inMouseLocal);

	protected:

		virtual	void		ForceInPortFrame(
									LWindow*				inOwningWindow,
									Rect&					ioTipFrame);

		virtual	void		DrawSelf(void);
	
		ResIDT				mTipTraitsID;
		CStr255				mTip;		// I hate that this needs to be a CStr255
		CToolTipAttachment*	mParent;
		LPane*				mOwningPane;
};


class CSharedToolTipAttachment : public CToolTipAttachment
{
	public:
		enum { class_ID = 'STat' };

							CSharedToolTipAttachment(LStream* inStream);
	protected:
		virtual void		CalcTipText(
									LWindow*				inOwningWindow,
									const EventRecord&		inMacEvent,
									StringPtr				outTipText);

		ResIDT				mStringListID;
		UInt16				mStringIndex;
};
