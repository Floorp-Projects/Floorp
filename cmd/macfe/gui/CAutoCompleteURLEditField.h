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

// CAutoCompleteURLEditField.h

// Subclass of CURLEditField that calls urlMatch function to do
// URL string matching

#pragma once

#include "CURLEditField.h"

#include <Types.h>	// for UInt32 typedef
#include <LStream.h>

class CAutoCompleteURLEditField : public CURLEditField
{
private:

	typedef CURLEditField	Inherited;
	
public:
	enum {	class_ID = 'AcUF' };
	
	const static UInt32 matchDelay;	// ticks before we start URL matching
	
	CAutoCompleteURLEditField(LStream* inStream);

	virtual Boolean		HandleKeyPress(const EventRecord& inKeyEvent);

	virtual	void		SpendTime(const EventRecord	&inMacEvent);

protected:
	UInt32	lastKeyPressTicks;	// contains tick count of last key press
};