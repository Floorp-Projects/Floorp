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
//	This is a mix-in class which handles the enabling policy for
//	CPaneEnabler.
//	
//	For an object to be enabled by a CPaneEnabler attachment, it should
//	mix-in and implement this interface. Note that a default policy is
//	implemented by CPaneEnabler. This is intended for LControl and LPane 
//	derived classes which cannot be changed to mix-in MPaneEnablerPolicy and for
//	which the default policy in CPaneEnabler already Does The Right Thingª.
// ===========================================================================

#ifndef MPaneEnablerPolicy_H
#define MPaneEnablerPolicy_H
#pragma once

// Class declaration

class MPaneEnablerPolicy
{
private:
	
	friend class CPaneEnabler;
	
	virtual void		HandleEnablingPolicy() = 0;
};

#endif
