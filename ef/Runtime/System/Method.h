/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
