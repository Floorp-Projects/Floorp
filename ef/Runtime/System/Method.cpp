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
#include "Method.h"


// ----------------------------------------------------------------------------
// Signature

//
// Return the number of Java environment slots that this signature's arguments
// would take.  All types count as one slot except for long and double, which
// take two slots each.
//
uint Signature::nArgumentSlots() const
{
	assert(nArguments == 0 || argumentTypes);
	uint nSlots = 0;
	const Type **a = argumentTypes;
	for (uint i = nArguments; i; i--)
		nSlots += nTypeKindSlots((*a++)->typeKind);
	return nSlots;
}

