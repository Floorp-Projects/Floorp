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

#ifndef CPatternButtonPopup_H
#define CPatternButtonPopup_H
#pragma once

// Includes

#include "CPatternButton.h"

#include "MPaneEnablerPolicy.h"

// Forward declarations

class LMenu;

// Class declaration

class CPatternButtonPopup : public CPatternButton
{
				  
public:
	enum { class_ID = 'PbPu' };
						
	typedef CPatternButton super;
							
	class CValueRangeException { };
	class CAttemptToSetDisabledValueException { };
								
						// ¥ Construction/destruction
						
						CPatternButtonPopup(LStream* inStream);
	virtual 			~CPatternButtonPopup();
						
						// ¥ Menu factory methods
						
	virtual LMenu*		GetMenu() const;
	virtual void		SetMenu(LMenu* inMenu);
	virtual void		AdoptMenu(LMenu* inMenuToAdopt);
	virtual Boolean		OwnsMenu() const;
	
						// ¥ Value mutators
						
	virtual	void		SetValue(Int32 inValue);
	virtual	void		SetPopupMinMaxValues();
	
						// ¥ Property accessors
						
	UInt32				GetTicksBeforeDisplayingPopup() const;
	void				SetTicksBeforeDisplayingPopup(UInt32 inTicksBeforeDisplayingPopup);
	
	Int32				GetQuickClickValueOrCommand() const;
	void				SetQuickClickValueOrCommand(Int32 inQuickClickValueOrCommand);

	Boolean				QuickClickIsCommandBased() const;
	void				SetQuickClickIsCommandBased(Boolean inQuickClickIsCommandBase);
	
	Boolean				MenuIsResourceBased() const;
	void				SetMenuIsResourceBased(Boolean inResourceBasedMenu);
	
	Boolean				MarkCurrentItem() const;
	void				SetMarkCurrentItem(Boolean inMarkCurrentItem);
	
	Uint8				GetMarkCharacter() const;
	void				SetMarkCharacter(Uint8 inMarkCharacter);
	
	Boolean				PopdownBehavior() const;
	void				SetPopdownBehavior(Boolean inPopdownBehavior);
	
protected:
	virtual void		FinishCreateSelf();
	
						// ¥ Menu factory methods
	
	virtual void		MakeNewMenu();
	virtual void		EliminatePreviousMenu();

						// ¥ Tracking

	virtual	Boolean		TrackHotSpot(
												Int16		inHotSpot,
												Point		inPoint,
												Int16		inModifiers);
	virtual	void		HandlePopupMenuSelect(
												Point		inPopupLoc,
												Int16		inCurrentItem,
												Int16&		outMenuID,
												Int16&		outMenuItem);
	virtual void		HotSpotResult(Int16 inHotSpot);

						// ¥ Miscellaneous

	virtual Boolean		OKToSendCommand(Int32 inCommand);
	virtual	void		BroadcastValueMessage();
	virtual void		AdjustMenuContents();
	virtual	void		SetupCurrentMenuItem(
												MenuHandle	inMenuH,
												Int16		inCurrentItem);
	virtual	void		GetPopupMenuPosition(Point& outPopupLoc);

						// ¥ "Template" methods (or hooks)
						
	virtual void		HandleQuickClick();
	virtual Boolean		HandleNewValue(Int32 inNewValue);
	
	virtual void		HandleEnablingPolicy();

private:
	LMenu*				mMenu;

	ResIDT				mPopupMenuID;
	ResIDT				mPopupTextTraitsID;
	Int32				mInitialCurrentItem;
	Uint32				mTicksBeforeDisplayingPopup;
	Int32				mQuickClickValueOrCommand;
	Boolean				mQuickClickIsCommandBased;
	Boolean				mResourceBasedMenu;
	Boolean				mPopdownBehavior;
	Boolean				mMarkCurrentItem;		
	Uint8				mMarkCharacter;
	Boolean				mDetachResource;

	Boolean				mOwnsMenu;
	Boolean				mPopUpMenuSelectWasCalled;
};

// Inline methods

// ---------------------------------------------------------------------------
//		¥ GetTicksBeforeDisplayingPopup
// ---------------------------------------------------------------------------
						
inline UInt32
CPatternButtonPopup::GetTicksBeforeDisplayingPopup() const
{
	return mTicksBeforeDisplayingPopup;							
}

// ---------------------------------------------------------------------------
//		¥ SetTicksBeforeDisplayingPopup
// ---------------------------------------------------------------------------
	
inline void
CPatternButtonPopup::SetTicksBeforeDisplayingPopup(UInt32 inTicksBeforeDisplayingPopup)
{
	mTicksBeforeDisplayingPopup = inTicksBeforeDisplayingPopup;
}

// ---------------------------------------------------------------------------
//		¥ GetQuickClickValueOrCommand
// ---------------------------------------------------------------------------

inline Int32
CPatternButtonPopup::GetQuickClickValueOrCommand() const
{
	return mQuickClickValueOrCommand;							
}

// ---------------------------------------------------------------------------
//		¥ SetQuickClickValueOrCommand
// ---------------------------------------------------------------------------
	
inline void
CPatternButtonPopup::SetQuickClickValueOrCommand(Int32 inQuickClickValueOrCommand)
{
	mQuickClickValueOrCommand = inQuickClickValueOrCommand;
}

// ---------------------------------------------------------------------------
//		¥ QuickClickIsCommandBased
// ---------------------------------------------------------------------------
	
inline Boolean
CPatternButtonPopup::QuickClickIsCommandBased() const
{
	return mQuickClickIsCommandBased;
}

// ---------------------------------------------------------------------------
//		¥ SetQuickClickIsCommandBased
// ---------------------------------------------------------------------------
	
inline void
CPatternButtonPopup::SetQuickClickIsCommandBased(Boolean inQuickClickIsCommandBase)
{
	mQuickClickIsCommandBased = inQuickClickIsCommandBase;
}

// ---------------------------------------------------------------------------
//		¥ MenuIsResourceBased
// ---------------------------------------------------------------------------

inline Boolean
CPatternButtonPopup::MenuIsResourceBased() const
{
	return mResourceBasedMenu;
}

// ---------------------------------------------------------------------------
//		¥ SetMenuIsResourceBased
// ---------------------------------------------------------------------------
	
inline void
CPatternButtonPopup::SetMenuIsResourceBased(Boolean inResourceBasedMenu)
{
	mResourceBasedMenu = inResourceBasedMenu;
}

// ---------------------------------------------------------------------------
//		¥ MarkCurrentItem
// ---------------------------------------------------------------------------
	
inline Boolean
CPatternButtonPopup::MarkCurrentItem() const
{
	return mMarkCurrentItem;
}

// ---------------------------------------------------------------------------
//		¥ SetMarkCurrentItem
// ---------------------------------------------------------------------------
	
inline void
CPatternButtonPopup::SetMarkCurrentItem(Boolean inMarkCurrentItem)
{
	mMarkCurrentItem = inMarkCurrentItem;
}

// ---------------------------------------------------------------------------
//		¥ PopdownBehavior
// ---------------------------------------------------------------------------

inline Boolean
CPatternButtonPopup::PopdownBehavior() const
{
	return mPopdownBehavior;
}

// ---------------------------------------------------------------------------
//		¥ SetPopdownBehavior
// ---------------------------------------------------------------------------
	
inline void
CPatternButtonPopup::SetPopdownBehavior(
	Boolean inPopdownBehavior)
{
	mPopdownBehavior = inPopdownBehavior;
}

// ---------------------------------------------------------------------------
//		¥ GetMarkCharacter
// ---------------------------------------------------------------------------
	
inline Uint8
CPatternButtonPopup::GetMarkCharacter() const
{
	return mMarkCharacter;
}

// ---------------------------------------------------------------------------
//		¥ SetMarkCharacter
// ---------------------------------------------------------------------------

inline void
CPatternButtonPopup::SetMarkCharacter(Uint8 inMarkCharacter)
{
	mMarkCharacter = inMarkCharacter;
}

#endif
