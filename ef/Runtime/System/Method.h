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
#ifndef METHOD_H
#define METHOD_H

#include "JavaObject.h"

// Utility class for passing method signatures around.
// This struct is not related to the heavyweight Method class.
struct Signature
{
	uint nArguments;							// Number of method arguments (including "this" if present)
	const Type **argumentTypes;					// Array of nArgument Types of arguments
	const Type *resultType;						// Type of result; the Void primitive type if no result
	bool isStatic;								// True if method has no "this" argument

	uint getNResults() const {return resultType->typeKind != tkVoid;}
	uint nArgumentSlots() const;
	uint nResultSlots() const;
};


// --- INLINES ----------------------------------------------------------------


//
// Return the number of Java environment slots that this signature's result
// would take.  Void takes no slots, long and double take two slots, and all
// other types take one slot.
//
inline uint Signature::nResultSlots() const
{
	assert(resultType);
	return nTypeKindSlots(resultType->typeKind);
}

#endif
