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


/*
	Features (not including the look/feel/behavior from inherited classes):
	
	(1)	LMenu based. This allows flexible use of the class for both
		resource-based menus and menus built dynamically. Additionally,
		the benefits of attaching commands to menu items (using Mcmd and/or
		dynamically) is gained.
	(2) The "current item" can either be marked or not. If marking is specified,
		a mark character can be specified as well.
	(3) Popup or Popdown behavior.
	(4) A delay before popup value is supported. This gives rise to the "quick click"
		feature described below.
	(5) Quick clicks are supported (ala Greg's Browser and others). This allows a
		value or command to be specified such that if the button is released before
		the popup delay value, either the value will change to the specified value
		or the specified command will be sent. Note that if the quick click is
		command-based, the command does not have to coincide with a menu command
		for a menu item (it can be any arbitrary command). Quick click commands are
		supported even when there is no menu (as long as the quick click command
		is an enabled command).
	(6) Use of factory methods for menu management allows subclasses to add menu
		caching, etc.
	(7) Use of template methods (or hooks) for handling "quick clicks" and value
		changes allow subclasses to further customize the behavior. For example,
		a subclass may implement a "smart" button which will perform an action
		directly (instead of relying on broadcaster/listener delegation) by overriding
		HandleNewValue. For example, CGuideMenuPopup (which is a subclass of
		CPatternButtonPopup) can be inserted into a PPob window resource and
		It Will Just Workª without additional code to link broadcasters to listeners
		and additional ListenToMessage cases sprinkled about.
	(8) Fully configurable through Constructor PPobs and accessor methods.	
		
		
		
	CAVEAT:	Although we do correctly save and restore the system font, if Mercutio
			MDEF is being used in the app, then it must be used in all menus which
			change the system font in order to not mess up the system font. It is
			probably a wise idea to use Mercutio MDEF in *all* menus throughout the
			app to be safe.
*/		

#include "CPatternButtonPopup.h"
		
#include <UMemoryMgr.h>		
		
#include "StSetBroadcasting.h"
#include "CDrawingState.h"
#include "CApplicationEventAttachment.h"
#include "CTargetedUpdateMenuRegistry.h"

// ---------------------------------------------------------------------------
//		¥ CPatternButtonPopup
// ---------------------------------------------------------------------------
// Stream-based ctor

CPatternButtonPopup::CPatternButtonPopup(
	LStream* inStream)

	:	mMenu(nil),
	
		mPopupMenuID(0),
		mPopupTextTraitsID(0),
		mInitialCurrentItem(0),
		mTicksBeforeDisplayingPopup(9),
		mQuickClickValueOrCommand(0),
		mQuickClickIsCommandBased(false),
		mResourceBasedMenu(false),
		mPopdownBehavior(false),
		mMarkCurrentItem(false),
		mMarkCharacter(0),
		mDetachResource(false),
		
		mOwnsMenu(false),
		mPopUpMenuSelectWasCalled(true),	// Start out true to ensure proper behavior
											// even if super::HandlePopupMenuSelect()
											// is called instead of our own
		
		super(inStream)
{
	// Read in the data from the stream
		
	ThrowIfNil_(inStream);
		
	LStream& theStream = *inStream;
	
	theStream >> mPopupMenuID;
	theStream >> mPopupTextTraitsID;
	theStream >> mInitialCurrentItem;	
	theStream >> mTicksBeforeDisplayingPopup;
	theStream >> mQuickClickValueOrCommand;
	theStream >> mQuickClickIsCommandBased;
	theStream >> mResourceBasedMenu;
	theStream >> mPopdownBehavior;
	theStream >> mMarkCurrentItem;
	theStream >> mMarkCharacter;
	theStream >> mDetachResource;
	
	// Ignore the ticks before displaying popup value specified in the resource.
	// That was a mistake.
	
	mTicksBeforeDisplayingPopup = GetDblTime();
}

// ---------------------------------------------------------------------------
//		¥ ~CPatternButtonPopup
// ---------------------------------------------------------------------------
// dtor

CPatternButtonPopup::~CPatternButtonPopup()
{
	CPatternButtonPopup::EliminatePreviousMenu();
}
						
#pragma mark -

// ---------------------------------------------------------------------------
//		¥ FinishCreateSelf
// ---------------------------------------------------------------------------

void
CPatternButtonPopup::FinishCreateSelf()
{
	super::FinishCreateSelf();
	
	MakeNewMenu();
}

						
#pragma mark -
						
// ---------------------------------------------------------------------------
//		¥ GetMenu
// ---------------------------------------------------------------------------
//	The menu factory methods (GetMenu, OwnsMenu, SetMenu, AdoptMenu, 
//	MakeNewMenu, and EliminatePreviousMenu) *must* be overriden as a group.
//	The main reason for overriding this group of methods would be to
//	do some form of menu caching.

LMenu*
CPatternButtonPopup::GetMenu() const
{
	return mMenu;
	
}	
	
// ---------------------------------------------------------------------------
//		¥ OwnsMenu
// ---------------------------------------------------------------------------
//	The menu factory methods (GetMenu, OwnsMenu, SetMenu, AdoptMenu, 
//	MakeNewMenu, and EliminatePreviousMenu) *must* be overriden as a group.
//	The main reason for overriding this group of methods would be to
//	do some form of menu caching.
	
Boolean
CPatternButtonPopup::OwnsMenu() const
{
	return mOwnsMenu;
	
}	
					
// ---------------------------------------------------------------------------
//		¥ SetMenu
// ---------------------------------------------------------------------------
//	The menu factory methods (GetMenu, OwnsMenu, SetMenu, AdoptMenu, 
//	MakeNewMenu, and EliminatePreviousMenu) *must* be overriden as a group.
//	The main reason for overriding this group of methods would be to
//	do some form of menu caching.

void
CPatternButtonPopup::SetMenu(LMenu* inMenu)
{
	EliminatePreviousMenu();
	
	mMenu		= inMenu;
	mOwnsMenu	= false;
}

// ---------------------------------------------------------------------------
//		¥ AdoptMenu
// ---------------------------------------------------------------------------
//	The menu factory methods (GetMenu, OwnsMenu, SetMenu, AdoptMenu, 
//	MakeNewMenu, and EliminatePreviousMenu) *must* be overriden as a group.
//	The main reason for overriding this group of methods would be to
//	do some form of menu caching.

void
CPatternButtonPopup::AdoptMenu(
	LMenu* inMenuToAdopt)
{
	EliminatePreviousMenu();
	
	mMenu		= inMenuToAdopt;
	mOwnsMenu	= true;
}

// ---------------------------------------------------------------------------
//		¥ MakeNewMenu
// ---------------------------------------------------------------------------
//	The menu factory methods (GetMenu, OwnsMenu, SetMenu, AdoptMenu, 
//	MakeNewMenu, and EliminatePreviousMenu) *must* be overriden as a group.
//	The main reason for overriding this group of methods would be to
//	do some form of menu caching.

void
CPatternButtonPopup::MakeNewMenu()
{
	// Create popup menu if appropriate
	
	if (mResourceBasedMenu)
	{
		if (mPopupMenuID)
		{
			// Resource-based menu (via ::GetMenu)
			
			AdoptMenu(new LMenu(mPopupMenuID));
			mValue = 0;
			SetPopupMinMaxValues();
			
			// Detach the resource as specified
			
			if (mDetachResource)
			{
				::DetachResource((Handle) GetMenu()->GetMacMenuH());
			}
			
			// For resource-based menus, set the initial current item
			
			StSetBroadcasting setBroadcasting(this, false);
			
			try
			{
				SetValue(mInitialCurrentItem);
			}
			catch (const CValueRangeException&) { }
			catch (const CAttemptToSetDisabledValueException&) { }
		}
	}
	else
	{
		if (mPopupMenuID)
		{
			// Dynamic menu (via ::NewMenu)
			
			AdoptMenu(new LMenu(mPopupMenuID, "\p"));
			mValue = 0;
			SetPopupMinMaxValues();
		}
	}
}						

// ---------------------------------------------------------------------------
//		¥ EliminatePreviousMenu
// ---------------------------------------------------------------------------
//	The menu factory methods (GetMenu, OwnsMenu, SetMenu, AdoptMenu, 
//	MakeNewMenu, and EliminatePreviousMenu) *must* be overriden as a group.
//	The main reason for overriding this group of methods would be to
//	do some form of menu caching.

void
CPatternButtonPopup::EliminatePreviousMenu()
{
	if (mMenu)
	{
		if (mOwnsMenu)
		{
			delete mMenu;
		}
		
		mMenu = nil;
	}
}						

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ TrackHotSpot
// ---------------------------------------------------------------------------

Boolean
CPatternButtonPopup::TrackHotSpot(
	Int16		inHotSpot,
	Point		inPoint,
	Int16		inModifiers)
{
	Boolean pointInHotSpot;
										// The value we set mPopUpMenuSelectWasCalled
										// is not important. We just want to restore
										// the value when this function returns.
	StValueChanger<Boolean> theValueChanger(mPopUpMenuSelectWasCalled, true);
	
										// If no menu, then allow normal tracking and
										// perform a quick click if button is released
										// in the hot spot.
	if (!GetMenu() || !GetMenu()->GetMacMenuH())
	{
		pointInHotSpot = super::TrackHotSpot(inHotSpot, inPoint, inModifiers);

		if (pointInHotSpot)
		{
			if (mQuickClickIsCommandBased)
			{
				HandleQuickClick();
			}
		}
		
		return pointInHotSpot;
	}
	
	Point	currPt		= inPoint;
	UInt32	endTicks	= sWhenLastMouseDown + mTicksBeforeDisplayingPopup;
	
										// For the initial mouse down, the
										// mouse is currently inside the HotSpot
										// when it was previously outside
	Boolean		currInside = true;
	Boolean		prevInside = false;
	HotSpotAction(inHotSpot, currInside, prevInside);
	
	// If the controlKey is down then we bypass quick click tracking in favor of
	// the popup menu. Additionally, we don't even bother tracking unless
	// mQuickClickValueOrCommand is non-zero

	if (!CApplicationEventAttachment::CurrentEventHasModifiers(controlKey) && mQuickClickValueOrCommand)
	{		
										// Track the mouse while it is down
		Point	currPt = inPoint;
		while (::StillDown() && ::TickCount() < endTicks)
		{
			::GetMouse(&currPt);		// Must keep track if mouse moves from
			prevInside = currInside;	// In-to-Out or Out-To-In
			currInside = PointInHotSpot(currPt, inHotSpot);
			
			if (!currInside)
			{
										// Reset endTicks if mouse moves outside										
				endTicks = ::TickCount() + mTicksBeforeDisplayingPopup;
			}
			else
			{
										// Quick Click tracking
				if (!::StillDown())
				{
										// Set button in "up" state
					HotSpotAction(inHotSpot, false, true);
					
										// Check if MouseUp occurred in HotSpot
					::GetMouse(&currPt);
					pointInHotSpot = PointInHotSpot(currPt, inHotSpot);

					if (pointInHotSpot)
					{
						HandleQuickClick();
					}
					
					return pointInHotSpot;
				}
			}
			
			HotSpotAction(inHotSpot, currInside, prevInside);
		}
	}

	if (!::StillDown())
	{
							// Set button in "up" state
		HotSpotAction(inHotSpot, false, true);
		
							// Check if MouseUp occurred in HotSpot
		::GetMouse(&currPt);
		pointInHotSpot = PointInHotSpot(currPt, inHotSpot);

		if (pointInHotSpot)
		{
			HandleQuickClick();
		}
		
		return pointInHotSpot;
	}
	
										// We tracked inside the button longer than
										// mTicksBeforeDisplayingPopup and the mouse is
										// still down, so we now skip normal tracking
										// to display the popup menu
	Int16 menuID	= 0;
	Int16 menuItem	= GetValue();
	Point popLocation;
	
	GetPopupMenuPosition(popLocation);
	
										// Handle user interaction with the menu
	HandlePopupMenuSelect(popLocation, mPopdownBehavior ? 1 : menuItem, menuID, menuItem);

	if (!mPopUpMenuSelectWasCalled && mQuickClickValueOrCommand)
	{
										// If the popup menu was not displayed,
										// then we will check once more if MouseUp
										// occurred in HotSpot so we can do
										// the quick click
		::GetMouse(&currPt);
		pointInHotSpot = PointInHotSpot(currPt, inHotSpot);

		if (pointInHotSpot)
		{
			HandleQuickClick();
		}
	}
	
	Rect	localFrame;
	Point	mousePt;
	
	FocusDraw();		
	CalcLocalFrameRect(localFrame);
	::GetMouse(&mousePt);
	
	mMouseInFrame = ::PtInRect(mousePt, &localFrame);

										// Set the new item, if specified. NOTE: We set the
										// value here so that this control acts the same as
										// LStdControl and LGAPopup
	if (menuItem > 0)
	{
		try
		{
			SetValue(menuItem);
		}
		catch (const CValueRangeException&) { }
		catch (const CAttemptToSetDisabledValueException&) { }
	}
	else
	{
										// Set button in "up" state
		HotSpotAction(inHotSpot, false, true);
	}
	
	return (menuItem > 0);
}

// ---------------------------------------------------------------------------
//		¥ HandlePopupMenuSelect
// ---------------------------------------------------------------------------
//	Handle user interaction with the popup menu

void
CPatternButtonPopup::HandlePopupMenuSelect(
	Point		inPopupLoc,
	Int16		inCurrentItem,
	Int16&		outMenuID,
	Int16&		outMenuItem)
{		
	ThrowIfNil_(GetMenu());
	ThrowIfNil_(GetMenu()->GetMacMenuH());

	mPopUpMenuSelectWasCalled = false;
	
	// Handle the actual insertion into the hierarchical menubar.
	//
	// From Apple Sample Code (PopupMenuWithCurFont.c):
	//		"Believe it or not, it's important to insert
	//		 the menu before diddling the font characteristics."
	//
	// Note that this is also what LGAPopup.cp does. Unfortunately,
	// neither this nor the StMercutioMDEFTextState seems to help
	// with the font metrics problems we see with Mercurtio MDEF.
		
	::InsertMenu(GetMenu()->GetMacMenuH(), hierMenu);

	Int16 saveFont = ::LMGetSysFontFam();
	Int16 saveSize = ::LMGetSysFontSize();
	
	StMercutioMDEFTextState theMercutioMDEFTextState;
	
	try
	{
		// Reconfigure the system font so that the menu will be drawn in our desired 
		// font and size.
		
		if (mPopupTextTraitsID)
		{				
			FocusDraw();
			
			TextTraitsH traitsH = UTextTraits::LoadTextTraits(mPopupTextTraitsID);
			
			if (traitsH)
			{
				// Bug #64133 kellys
				// If setting to application font, get the application font for current script
				if((**traitsH).fontNumber == 1)
					::LMSetSysFontFam ( ::GetScriptVariable(::FontToScript(1), smScriptAppFond) );
				else
					::LMSetSysFontFam ( (**traitsH).fontNumber );
				::LMSetSysFontSize((**traitsH).size);
				::LMSetLastSPExtra(-1L);
			}
		}
		
		// Adjust the contents of the menu
		
		AdjustMenuContents();
		
		// Setup the currently selected menu item
		
		SetupCurrentMenuItem(GetMenu()->GetMacMenuH(), GetValue());

		// Call PopupMenuSelect and wait for it to return

		Int32 result = 0;
		
		if (::StillDown())
		{
			mPopUpMenuSelectWasCalled = true;
			
			result = ::PopUpMenuSelect(
											GetMenu()->GetMacMenuH(),
											inPopupLoc.v,
											inPopupLoc.h,
											inCurrentItem);
		}											
		
		// Extract the values from the returned result these are then passed 
		// back out to the caller
		
		outMenuID	= HiWord(result);
		outMenuItem	= LoWord(result);
	}
	catch (...)
	{
		// Ignore errors
	}

	// Restore the system font
	
	::LMSetSysFontFam(saveFont);
	::LMSetSysFontSize(saveSize);
	::LMSetLastSPExtra(-1L);

	// Finally get the menu removed
	
	::DeleteMenu(mPopupMenuID);
}

// ---------------------------------------------------------------------------
//		¥ HotSpotResult
// ---------------------------------------------------------------------------
//	Note that actual hot spot result processing occurs in TrackHotSpot. This
//	is because the information necessary to perform the correct action is
//	only available while tracking.

void
CPatternButtonPopup::HotSpotResult(Int16 inHotSpot)
{
	if (!IsBehaviourToggle())
	{
		// Make sure that non-toggle buttons return to the "up" state
		
		HotSpotAction(inHotSpot, false, true);
	}
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ AdjustMenuContents
// ---------------------------------------------------------------------------
//	Do stuff to the menu (add or remove menu items). The default implementation
//	is intentionally a noop. Override to add interesting behavior.

void
CPatternButtonPopup::AdjustMenuContents()
{
}

// ---------------------------------------------------------------------------
//		¥ SetupCurrentMenuItem
// ---------------------------------------------------------------------------
//	Mark new item and unmark old item

void
CPatternButtonPopup::SetupCurrentMenuItem(MenuHandle inMenuH, Int16 inCurrentItem)
{
	ThrowIfNil_(inMenuH);
		
	if (mMarkCurrentItem)
	{
		if (GetValue() != inCurrentItem)
		{
			Int16 oldItem = GetValue();
			
			if (oldItem > 0)
			{
				::SetItemMark(inMenuH, oldItem, noMark);
			}
		}
		
		if (mMarkCharacter && (inCurrentItem > 0))
		{
			::SetItemMark(inMenuH, inCurrentItem, mMarkCharacter);
		}
		else 
		{
			::SetItemMark(inMenuH, inCurrentItem, noMark);
		}
	}
}

// ---------------------------------------------------------------------------
//		¥ GetPopupMenuPosition
// ---------------------------------------------------------------------------
//	Get the position for displaying the popup menu in global coordinates

void
CPatternButtonPopup::GetPopupMenuPosition(Point &outPopupLoc)
{
	Rect popupRect;

	CalcLocalFrameRect(popupRect);
	
	if (mPopdownBehavior)
	{
		outPopupLoc.v = popupRect.bottom + 1;
		outPopupLoc.h = popupRect.left   + 2;
	}
	else
	{
		outPopupLoc.v = popupRect.top;
		outPopupLoc.h = popupRect.left;
	}
		
	LocalToPortPoint(outPopupLoc);
	PortToGlobalPoint(outPopupLoc);
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ SetValue
// ---------------------------------------------------------------------------
//	This is overridden to adjust the mark on the previous/new menu items and
//	to ensure that the value is always broadcast (even if it hasn't really
//	changed) and to call a "hook" for derived classes to perform some action
//	on value changes.
//
//	Note that this implementation should be considered "final" (i.e. no further
//	overrides allowed) in order to preserve the semantics of this class.
//
//	Exceptions thrown: CValueRangeException, CAttemptToSetDisabledValueException

void
CPatternButtonPopup::SetValue(Int32 inValue)
{
	ThrowIfNil_(GetMenu());
	ThrowIfNil_(GetMenu()->GetMacMenuH());
	
	if (inValue < 0 || inValue > ::CountMItems(GetMenu()->GetMacMenuH()))
	{
		throw CValueRangeException();
	}
	
	if (!GetMenu()->ItemIsEnabled(inValue))
	{
		throw CAttemptToSetDisabledValueException();
	}
		
		// If mMarkCurrentItem is false, it is assumed that we want to
		// be able to select the same command several times in a row.
		// By hacking mValue to 0, we make sure that the menu command
		// will always be broadcast.
		
	if (!mMarkCurrentItem)
	{
		mValue = 0;
	}

	if (mValue != inValue)
	{
		// Setup the menu item
		
		SetupCurrentMenuItem(GetMenu()->GetMacMenuH(), inValue);

		// Set the new value. If the HandleNewValue hook indicates that
		// the value was handled, then we make sure that the call to
		// the base class SetValue does not broadcast the new value.
			
		if (HandleNewValue(inValue))
		{
			StSetBroadcasting setBroadcasting(this, false);

			super::SetValue(inValue);
		}
		else
		{
			super::SetValue(inValue);
		}
	}
}



// ---------------------------------------------------------------------------
//		¥ SetPopupMinMaxValues
// ---------------------------------------------------------------------------
//	Setup the minimum and maximum values for the control based on the
//	current menu state.
//
//	NOTE: It is important for clients to call this method after inserting/removing
//	menu items dynamically.

void
CPatternButtonPopup::SetPopupMinMaxValues()
{
	if (GetMenu() && GetMenu()->GetMacMenuH())
	{
		mMinValue = 0;
		mMaxValue = ::CountMItems(GetMenu()->GetMacMenuH());
	}
	else
	{
		mMaxValue = mMinValue = 0;
	}
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ OKToSendCommand
// ---------------------------------------------------------------------------

Boolean
CPatternButtonPopup::OKToSendCommand(
	Int32 inCommand)
{
	LCommander* theTarget		= LCommander::GetTarget();
	Boolean 	enabled			= false;
	Boolean		usesMark		= false;
	Str255		outName;
	Char16		outMark;
			
	if (inCommand && theTarget)
	{
		theTarget->ProcessCommandStatus(inCommand, enabled, usesMark, outMark, outName);
	}

	return enabled;
}

// ---------------------------------------------------------------------------
//		¥ BroadcastValueMessage
// ---------------------------------------------------------------------------
//	Broadcast the menu command if there is one, otherwise broadcast the value

void CPatternButtonPopup::BroadcastValueMessage()
{
	ThrowIfNil_(GetMenu());
	ThrowIfNil_(GetMenu()->GetMacMenuH());
	
	if (mValueMessage != msg_Nothing)
	{
		CommandT theCommand	= GetMenu()->CommandFromIndex(GetValue());
		
		if (theCommand != cmd_Nothing)
		{
			if (OKToSendCommand(theCommand))
			{
				BroadcastMessage(theCommand, nil);
			}
		}
		else
		{
			Int32 value = mValue;
			
			BroadcastMessage(mValueMessage, &value);
		}
	}
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ HandleQuickClick
// ---------------------------------------------------------------------------
//	Hook for handling quick clicks.

void
CPatternButtonPopup::HandleQuickClick()
{
	if (mQuickClickValueOrCommand)
	{
		if (mQuickClickIsCommandBased)
		{
			if (OKToSendCommand(mQuickClickValueOrCommand))
			{
				BroadcastMessage(mQuickClickValueOrCommand, nil);
			}
		}
		else
		{
			try
			{
				SetValue(mQuickClickValueOrCommand);
			}
			catch (const CValueRangeException&) { }
			catch (const CAttemptToSetDisabledValueException&) { }
		}
	}
}

// ---------------------------------------------------------------------------
//		¥ HandleNewValue
// ---------------------------------------------------------------------------
//	Hook for handling value changes. Called by SetValue.
//
//	Some CPatternButtonPopups may override this to perform an action rather
//	than using broadcaster/listener. Returns true if HandleNewValue handled the new
//	value (this will cause the new value to not be broadcast).
// 
//	Note that the setting of the new value is done by CPatternButtonPopup::SetValue.
//  Therefore, GetValue() will still return the old value here, so the old value is
//	still available in this method.

Boolean
CPatternButtonPopup::HandleNewValue(Int32 inNewValue)
{
#pragma unused(inNewValue)

	return false;
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ HandleEnablingPolicy
// ---------------------------------------------------------------------------
//	This is our required implementation of the MPaneEnablerPolicy interface.
//
//	The enabling policy is as follows:
//
//	- If all of the menu items associated with the menu items are disabled
//	  then disable the button. Note that a menu item without a command
//	  is automatically enabled and a menu item with a command is enabled
//	  only if the command associated with it is enabled.
//	- If there is (no menu or no menu items in the menu) and
//	  mQuickClickIsCommandBased is true and it is *not OK* to send the
//	  mQuickClickValueOrCommand command then disable the button.
//	- Otherwise enable the buton.

void
CPatternButtonPopup::HandleEnablingPolicy()
{
	LCommander* theTarget		= LCommander::GetTarget();
	MenuHandle	macMenuH		= nil;
	MessageT	buttonCommand	= GetValueMessage();
	Boolean 	enabled			= false;
	Boolean		usesMark		= false;
	Str255		outName;
	Char16		outMark;
	
	if (!CTargetedUpdateMenuRegistry::UseRegistryToUpdateMenus() ||
			CTargetedUpdateMenuRegistry::CommandInRegistry(buttonCommand))
	{
		if (!IsActive() || !IsVisible())
			return;
			
		if (!theTarget)
			return;
		
		if (GetMenu() && GetMenu()->GetMacMenuH() && ::CountMItems(GetMenu()->GetMacMenuH()))
		{
			macMenuH = GetMenu()->GetMacMenuH();
		}
		
		if (macMenuH)
		{
			// We have a menu
			
			CommandT	theCommand;
			Boolean		allDisabled	= true;
			Int16		menuItem	= 0;
			
			while (GetMenu()->FindNextCommand(menuItem, theCommand))
			{
				if (theCommand)
				{
					outName[0] = 0;
		
					// Call ProcessCommandStatus() to determine whether to enable the menu items.
					// If they are all disabled then disable the button itself.

					theTarget->ProcessCommandStatus(theCommand, enabled, usesMark, outMark, outName);

					if (outName[0] != 0)
					{
						::SetMenuItemText(macMenuH, menuItem, outName);
					}

					if (enabled)
					{
						allDisabled = false;
						::EnableItem(macMenuH, menuItem);
					}
					else
					{
						::DisableItem(macMenuH, menuItem);
					}

					if (usesMark)
					{
						if ( outMark && mMarkCharacter )
							outMark = mMarkCharacter;
						::SetItemMark(macMenuH, menuItem, outMark);
					}
				}
				else
				{
					allDisabled = false;
					::EnableItem(macMenuH, menuItem);
				}
			}

			if (menuItem == 0)
			{				
					// The menu does not have any commands, so we check the command attached to
					// the button itself.
				
				theTarget->ProcessCommandStatus(buttonCommand, enabled, usesMark, outMark, outName);
				allDisabled = (!enabled);
			}

			if (allDisabled)
			{
				Disable();
			}
			else
			{
				Enable();
			}
		}
		else
		{
			// We don't have a menu
			
			if (mQuickClickIsCommandBased && OKToSendCommand(mQuickClickValueOrCommand))
			{
				Enable();			
			}
			else
			{
				Disable();
			}
		}
	}
}
