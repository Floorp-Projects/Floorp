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
