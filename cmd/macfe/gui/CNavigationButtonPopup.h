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

// ===========================================================================
//	CNavigationButtonPopup.h
//
// ===========================================================================

#ifndef CNavigationButtonPopup_H
#define CNavigationButtonPopup_H
#pragma once

// Includes

#include "CToolbarBevelButton.h"
#include "shist.h"

// Forward declarations

class CBrowserContext;

// Class declaration

class CNavigationButtonPopup : public CToolbarBevelButton
{
public:
	enum { class_ID = 'TbNv' };

	typedef CToolbarBevelButton super;
							
							CNavigationButtonPopup(LStream* inStream);
	virtual	 				~CNavigationButtonPopup();
	
protected:
	virtual void			ClickSelf ( const SMouseDownEvent & inEvent );

	virtual void			AdjustMenuContents();
	
	virtual void			InsertHistoryItemIntoMenu(
												Int32				inHistoryItemIndex,
												Int16				inAfterItem);

	virtual Boolean			HandleNewValue(Int32 inNewValue);

	Boolean					AssertPreconditions();

	CBrowserContext*		mBrowserContext;
	History*				mHistory;
	History_entry*			mCurrentEntry;
	Int32					mCurrentEntryIndex;
	Int32					mNumItemsInHistory;
};


#endif
