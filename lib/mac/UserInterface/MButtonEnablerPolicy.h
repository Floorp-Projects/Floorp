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
//	CButtonEnabler.
//	
//	For an object to be enabled by a CButtonEnabler attachment, it should
//	mix-in and implement this interface. Note that a default policy is
//	implemented by CButtonEnabler. This is intended for LControl-derived
//	objects which cannot be changed to mix-in MButtonEnablerPolicy and for
//	which the default policy in CButtonEnabler already Does The Right Thingª.
// ===========================================================================

#ifndef MButtonEnablerPolicy_H
#define MButtonEnablerPolicy_H
#pragma once

// Class declaration

class MButtonEnablerPolicy
{
private:
	
	friend class CButtonEnabler;
	
	virtual void		HandleButtonEnabling() = 0;
};

#endif
