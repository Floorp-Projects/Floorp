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
// File: CShelfMixin.h
//
// Interface for the CShelf class which you can add to your window class to provide
// a wrapper around the basic functionality for manipulating a collapsable shelf in the
// browser window.
//

#pragma once

#include <string>

class LDividedView;


class CShelf
{
	public:
	
		enum { kClosed = false, kOpen = true } ;
		
		CShelf ( LDividedView* inDivView, const char* inPrefString ) ;
		virtual ~CShelf ( ) ;
		
		virtual void ToggleShelf ( bool inUpdatePref = true ) ;
		virtual void SetShelfState ( bool inNewState, bool inUpdatePref = true );
		virtual bool IsShelfOpen ( ) ;
		
		// in case this class doesn't provide enough of functionality...
		LDividedView* GetShelf() const { return mShelf; } ;
		
	protected:
	
		LDividedView* mShelf;
		string mPrefString;
		
}; // CShelfMixin
