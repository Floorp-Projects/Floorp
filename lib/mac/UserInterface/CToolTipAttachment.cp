/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef PowerPlant_PCH
#include PowerPlant_PCH
#endif

#include "CToolTipAttachment.h"
#include "UGraphicGizmos.h"

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CToolTipAttachment::sTipsEnabled = true;
CToolTipPane* CToolTipAttachment::sActiveTip = NULL;

// This is a special value that we use to indicate that the mouse has
// gone down inside a pane with a CToolTipAttachment.  The reason we need
// this is because we need to reset the trigger interval when the mouse
// goes up.  Since mouse up events don't trigger an attachment execution,
// we look for this value during the next pass through the mouse track
// dispatch and reset the interval there if necessary.

const UInt32 ToolTipsTicks_Indefinite = 0xFFFFFFFF;
const UInt32 ToolTipsTicks_AutoRemoveAfter = 3600;

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	CToolTipAttachment
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CToolTipAttachment::CToolTipAttachment(LStream* inStream)
	:	CMouseTrackAttachment(inStream)
{
	*inStream >> mDelayTicks;
	*inStream >> mTipPaneResID;
}

CToolTipAttachment::CToolTipAttachment(UInt32 inDelayTicks, ResIDT inPaneResID)
:	mDelayTicks(inDelayTicks)
,	mTipPaneResID(inPaneResID)
{
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	~CToolTipAttachment
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CToolTipAttachment::~CToolTipAttachment()
{
	HideToolTip();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	NoteTipDied
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CToolTipAttachment::NoteTipDied(CToolTipPane* inTip)
{
	Assert_(sActiveTip != NULL);
	Assert_(sActiveTip == inTip);
	
	sActiveTip = NULL;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	ExecuteSelf
//
//	We add the tracking of events that should hide the current tool tip (if
//	any) to the base CMouseTrackerAttachment execution.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CToolTipAttachment::ExecuteSelf(
	MessageT			inMessage,
	void*				ioParam)
{
	if ((inMessage == msg_KeyPress) && IsToolTipActive())
		{
		// We hide the tip, but updating is deferred in an update
		// event.
		HideToolTip();
		}
	else if (inMessage == msg_Click)
		{
		// Hide the tip, and in this case we want to immediately update
		// the space occupied by the removed tip.  We do this because we
		// know that the tip is essentially guaranteed to be in close
		// proximity to the cursor.
		
		if (IsToolTipActive())
			{
			HideToolTip();
			mOwningPane->UpdatePort();
			}
		
		// Here we note that the mouse has gone down.  We reset the interval
		// on the next pass through MouseWithin().
		ResetTriggerInterval(ToolTipsTicks_Indefinite);
		}
	else if ( inMessage == msg_HideTooltip )
		HideToolTip();
	else
	{
		if (sActiveTip && inMessage == msg_MouseWithin)
			((SMouseTrackParms*)ioParam)->paneOfAttachment = sActiveTip;
		CMouseTrackAttachment::ExecuteSelf(inMessage, ioParam);
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	MouseEnter
//
//	All we need to do here is note the time that the mouse entered the pane.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CToolTipAttachment::MouseEnter(
	Point				/* inPortPt */,
	const EventRecord&	/* inMacEvent */)
{
	if (!EnsureOwningPane())
		return;

	ResetTriggerInterval(::LMGetTicks());
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	MouseWithin
//	
//	The mouse is still in the currently tracked pane.  Here we determine
//	whether a tip needs to be hidden or shown.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CToolTipAttachment::MouseWithin(
	Point				inPortPt,
	const EventRecord&	inMacEvent)
{
	if (!EnsureOwningPane())
		return;

	if (!sTipsEnabled)
		return;

	// First we check the case where we just got time after a mouse track
	// in the attached pane.  If so, simply reset the interval.  Otherwise
	// if the user tracks for longer than the trigger interval the tip will
	// display immediately after the mouse is released, which is not
	// correct behaviour.
	if (mEnterTicks == ToolTipsTicks_Indefinite)
		ResetTriggerInterval(::LMGetTicks());
		
	// If the tip is active and the corresponding event is one that should
	// cancel the tip, kill it.
	else if (IsToolTipActive())
	{
		if (IsTipCancelingEvent(inMacEvent)
		|| ::LMGetTicks() - mEnterTicks > ToolTipsTicks_AutoRemoveAfter)
			HideToolTip();
		else if (sActiveTip->WantsToCancel(inPortPt))
		{
			HideToolTip();
			ShowToolTip(inMacEvent); // in the new location
		}
	}
		
	// If the tip is not active and the trigger interval has elapsed
	// then we need to show the new tip.
	else if (!IsToolTipActive() && IsDelayElapsed(::LMGetTicks()))
		ShowToolTip(inMacEvent); // pass the event, some panes need the point!
}							

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	MouseLeave
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CToolTipAttachment::MouseLeave()
{
	if (!EnsureOwningPane())
		return;

	HideToolTip();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	IsTipCancelingEvent
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CToolTipAttachment::IsTipCancelingEvent(const EventRecord& inMacEvent) const
{
	Boolean bShouldCancel = (inMacEvent.what != nullEvent &&
							 inMacEvent.what != updateEvt &&
							(inMacEvent.what != osEvt
								|| (inMacEvent.message & osEvtMessageMask)
								!= (mouseMovedMessage << 24)));
	return bShouldCancel;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	CalcTipText
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CToolTipAttachment::CalcTipText(
	LWindow*				inOwningWindow,
	const EventRecord&		inMacEvent,
	StringPtr				outTipText)
{
	sActiveTip->CalcTipText(inOwningWindow, inMacEvent, outTipText);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	ShowToolTip
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CToolTipAttachment::ShowToolTip(const EventRecord&	inMacEvent)
{
	try
		{
		GrafPtr theMacPort = mOwningPane->GetMacPort();
		ThrowIfNULL_(theMacPort);
		
		LWindow* theWindow = LWindow::FetchWindowObject(theMacPort);
		ThrowIfNULL_(theWindow);

		Assert_(sActiveTip == NULL);
		// This is a bit of a skanky cast.  The alternative was to call ReadObjects() and
		// explicitly cast the void* (on which RTTI will not help you. This seemed to be
		// the lesser of the two evils.				
		sActiveTip = dynamic_cast<CToolTipPane*>(UReanimator::CreateView(mTipPaneResID, theWindow, theWindow));
		ThrowIfNULL_(sActiveTip);
		sActiveTip->SetParent(this);
		sActiveTip->SetOwningPane(mOwningPane);
		
		// Calculate the tip text first because the size and positioning rely on it.
		// The practice of having the tip pane calculate the text and then have that
		// same text set as its descriptor may seem weird.  It's purpose is to
		// allow CToolTipPane subclasses to override the text calc method without
		// overriding this class in order to alter the way it gets set.
		//
		// If no text is returned, abort instead of showing an empty tooltip
		Str255 theTipText;
		this->CalcTipText(theWindow, inMacEvent, theTipText);
		Rect theTipPortFrame;
		if ( theTipText[0] )
			{
			sActiveTip->SetDescriptor(theTipText);

			theWindow->FocusDraw();
			sActiveTip->CalcFrameWithRespectTo(theWindow, inMacEvent, theTipPortFrame);
			// Returning a zero-width rectangle is another way that the tip pane can
			// say "don't show me".
			if (RectWidth(theTipPortFrame) == 0)
				theTipText[0] = 0;
			}
		if ( theTipText[0] )
			{
			sActiveTip->ResizeFrameTo(RectWidth(theTipPortFrame), RectHeight(theTipPortFrame), false);		
			sActiveTip->PlaceInSuperFrameAt(theTipPortFrame.left, theTipPortFrame.top, false);
			sActiveTip->Show();
			}
		else
			{
			delete sActiveTip;
			sActiveTip = NULL;
			}
		}
	catch(...)	
		{
		delete sActiveTip;
		sActiveTip = NULL;
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	HideToolTip
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CToolTipAttachment::HideToolTip()
{
	// calc the tip's port rect 
	if (IsToolTipActive())
		{
		Rect thePortFrame;
		sActiveTip->CalcPortFrameRect(thePortFrame);
		sActiveTip->InvalPortRect(&thePortFrame);
	
		// hide the tip
		delete sActiveTip;
		sActiveTip = NULL;
		}
	
	ResetTriggerInterval(::LMGetTicks());
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	CToolTipPane
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CToolTipPane::CToolTipPane(LStream* inStream)
	:	LPane(inStream)
	,	mOwningPane(nil)
	,	mParent(nil)
{
	*inStream >> mTipTraitsID;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	~CToolTipPane
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CToolTipPane::~CToolTipPane()
{
	if (mParent != NULL)
		mParent->NoteTipDied(this);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	SetParent
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CToolTipPane::SetParent(CToolTipAttachment* inParent)
{
	mParent = inParent;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	SetOwningPane
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CToolTipPane::SetOwningPane(LPane* inOwningPane)
{
	mOwningPane = inOwningPane;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	CalcFrameWithRespectTo
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CToolTipPane::CalcFrameWithRespectTo(
	LWindow*				inOwningWindow,
	const EventRecord&		/* inMacEvent */,
	Rect& 					outPortFrame)
{
	StTextState theTextSaver;
	UTextTraits::SetPortTextTraits(mTipTraitsID);
	
	FontInfo theFontInfo;
	::GetFontInfo(&theFontInfo);
	Int16 theTextHeight	= theFontInfo.ascent + theFontInfo.descent + theFontInfo.leading + (2 * 2);
	Int16 theTextWidth = ::StringWidth(mTip) + (2 * ::CharWidth(char_Space));
	
	inOwningWindow->FocusDraw();	

	Rect theOwningPortFrame;
	mOwningPane->CalcPortFrameRect(theOwningPortFrame);
	
	outPortFrame.left = ((theOwningPortFrame.left + theOwningPortFrame.right) >> 1) - (theTextWidth >> 1);
	outPortFrame.top = theOwningPortFrame.bottom + 3;
	outPortFrame.right = outPortFrame.left + theTextWidth; 
	outPortFrame.bottom = outPortFrame.top + theTextHeight;

	ForceInPortFrame(inOwningWindow, outPortFrame);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	CalcTipText
//
//	The default implementation of the tool tip is to return the descriptor of
//	the pane that owns it.  If you need a different behaviour, sublclass the
//	CToolTipPane (not the attachment), and override this method.  See notes
//	in the header file.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CToolTipPane::CalcTipText(
	LWindow*	/* inOwningWindow */,
	const EventRecord&	/* inMacEvent */,
	StringPtr	outTipText)
{
	mOwningPane->GetDescriptor(outTipText);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	ForceInPortFrame
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CToolTipPane::ForceInPortFrame(
	LWindow*				inOwningWindow,
	Rect&					ioTipFrame)
{
	// Now make sure the tip is fits inside the owning window	
	Rect theWindowPortFrame;
	inOwningWindow->CalcPortFrameRect(theWindowPortFrame);
	
	Int16 theHOffset = 0, theVOffset = 0;
	
	if (ioTipFrame.right > theWindowPortFrame.right)
		theHOffset = theWindowPortFrame.right - ioTipFrame.right;		// bump it to the left
	else if (ioTipFrame.left < theWindowPortFrame.left)
		theHOffset = theWindowPortFrame.left - ioTipFrame.left;		// bump it to the right
		
	if (ioTipFrame.bottom > theWindowPortFrame.bottom)
	{
		Point mouseLocal;
		GetMouse(&mouseLocal);
		theVOffset = mouseLocal.v - 1 - ioTipFrame.bottom;	// bump it up
//	WAS:	theVOffset = theWindowPortFrame.bottom - ioTipFrame.bottom;	// bump it up
//			The original version left the mouse within the tooltip frame.  This
//			led to the tooltip being deleted every null event, because CMouseDispatcher
//			would calculate that the pane that the mouse was in was the tooltip pane
//			itself.  Solution: move the pane up above the mouse position.
	}
	else if (ioTipFrame.top < theWindowPortFrame.top)
		theVOffset = theWindowPortFrame.top - ioTipFrame.top;			// bump it down

	if ((theHOffset != 0) || (theVOffset != 0))
		::OffsetRect(&ioTipFrame, theHOffset, theVOffset);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	SetDescriptor
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CToolTipPane::SetDescriptor(ConstStringPtr inDescriptor)
{
	mTip = inDescriptor;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	DrawSelf
//	
//	This is the default drawing of the tip pane.  To change tip appearance
//	override this method.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CToolTipPane::DrawSelf()
{
	StColorPenState thePenSaver;
	thePenSaver.Normalize();
	
	Rect theFrame;
	CalcLocalFrameRect(theFrame);
	
	// ¥ Setup a device loop so that we can handle drawing at the correct bit depth
	StDeviceLoop	theLoop ( theFrame );
	Int16			depth;
	while ( theLoop.NextDepth ( depth )) 
	{
		if ( depth < 4 )		// ¥ BLACK & WHITE
		{
			::EraseRect(&theFrame);
			::FrameRect(&theFrame);
		}
		else
		{
			// We want the light tinge of the owning window.  It makes a good fill for
			// the tip and is likely to look really nice as it accents the window's
			// structure appearance.
			RGBColor theLightTinge, theDarkTinge;
			UGraphicGizmos::CalcWindowTingeColors(GetMacPort(), theLightTinge, theDarkTinge);
			::RGBForeColor(&theLightTinge);
			::PaintRect(&theFrame);
		}
	}
	thePenSaver.Normalize();
	UTextTraits::SetPortTextTraits(mTipTraitsID);
	UGraphicGizmos::CenterStringInRect(mTip, theFrame);
}
	
Boolean CToolTipPane::WantsToCancel(Point inMouseLocal)
{
	return false;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	CSharedToolTipAttachment
//
//	Allows many buttons to share one tooltip pane.  A ToolTipPane resource is 54
//	bytes, and the only part that is unique is usually the string index/id pair.
//	Using this saves making a new pain [sic] for every new button.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CSharedToolTipAttachment::CSharedToolTipAttachment(LStream* inStream)
	:	CToolTipAttachment(inStream)
{
	*inStream >> mStringListID;
	*inStream >> mStringIndex;
}

void CSharedToolTipAttachment::CalcTipText(
	LWindow*	/* inOwningWindow */,
	const EventRecord&	/* inMacEvent */,
	StringPtr	outTipText)
{
	::GetIndString(outTipText, mStringListID, mStringIndex);
}

