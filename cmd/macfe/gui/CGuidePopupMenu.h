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
//	CGuidePopupMenu.h
// ===========================================================================

#ifndef CGuidePopupMenu_H
#define CGuidePopupMenu_H
#pragma once

// Includes

#include "CPatternButtonPopup.h"

#include <vector.h>
#include <string>

#include "CAutoPtr.h"

// Class declaration

class CGuidePopupMenu : public CPatternButtonPopup
{
public:
	enum { class_ID = 'PbGp' };
						
	typedef CPatternButtonPopup super;
							
							CGuidePopupMenu(LStream* inStream);
			 				~CGuidePopupMenu();
						
	short					GetItem(const string& inURL) const;
	string					GetURL(short item) const;
						
	LMenu*					GetMenu() const;
	void					SetMenu(LMenu* inMenu);
	void					AdoptMenu(LMenu* inMenuToAdopt);
	Boolean					OwnsMenu() const;
	
protected:
	void					MakeNewMenu();
	void					EliminatePreviousMenu();
	void					SetupMenu();

	void					FinishCreateSelf();

	Boolean					HandleNewValue(Int32 inNewValue);
	
	static CAutoPtr<LMenu>	sMenu;
	static Boolean			sOwnsMenu;
	static Boolean			sMenuIsSetup;			
	static vector<string>	sURLs;	
};


#endif
