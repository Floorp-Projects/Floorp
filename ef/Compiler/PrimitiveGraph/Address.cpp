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
#include "Value.h"

#if BATCH_COMPILATION
const ConstructedAddress nullAddr(0);
#endif


#ifdef DEBUG_LOG
//
// Print this addr for debugging purposes.
// Return the number of characters printed.
//
int print(LogModuleObject &f, addr a)
{
	if (!a)
		return UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("null"));
	else
	  #if BATCH_COMPILATION
		return UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("0x%.8X+%d", (size_t)a.getBase(), a.getOffset()));
	  #else
		return UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("0x%.8X", (size_t)a));
	  #endif
}
#endif


#if BATCH_COMPILATION
 inline void *addrToPointer(addr a) {return (char *)a.getBase() + a.getOffset();}
#else
 inline void *addrToPointer(addr a) {return a;}
#endif

//
// Read a field of the given TypeKind at a and store it in v.
// Return true if the value was read successfully or false if it cannot
// be read at compile time.
//
// The current implementation never returns false, but it could be modified
// to do so if needed to support late binding in the batch compiler.
//
bool read(TypeKind tk, addr a, Value &v)
{
	const void *p = addrToPointer(a);
	assert(p);
	switch (tk) {
		case tkVoid:
			break;
		case tkBoolean:
		case tkUByte:
			v.i = *static_cast<const Uint8 *>(p);
			break;
		case tkByte:
			v.i = *static_cast<const Int8 *>(p);
			break;
		case tkChar:
			v.i = *static_cast<const Uint16 *>(p);
			break;
		case tkShort:
			v.i = *static_cast<const Int16 *>(p);
			break;
		case tkInt:
			v.i = *static_cast<const Int32 *>(p);
			break;
		case tkLong:
			v.l = *static_cast<const Int64 *>(p);
			break;
		case tkFloat:
			v.f = *static_cast<const Flt32 *>(p);
			break;
		case tkDouble:
			v.d = *static_cast<const Flt64 *>(p);
			break;
		case tkObject:
		case tkSpecial:
		case tkArray:
		case tkInterface:
			v.a = objectAddress(*static_cast<const obj *>(p));
			break;
	}
	return true;
}


//
// Write v into a field of the given TypeKind at a.
//
void write(TypeKind tk, addr a, const Value &v)
{
	void *p = addrToPointer(a);
	assert(p);
	switch (tk) {
		case tkVoid:
			break;
		case tkBoolean:
			assert((v.i & -2) == 0);
		case tkUByte:
		case tkByte:
			*static_cast<Uint8 *>(p) = (Uint8)v.i;
			break;
		case tkChar:
		case tkShort:
			*static_cast<Uint16 *>(p) = (Uint16)v.i;
			break;
		case tkInt:
			*static_cast<Int32 *>(p) = v.i;
			break;
		case tkLong:
			*static_cast<Int64 *>(p) = v.l;
			break;
		case tkFloat:
			*static_cast<Flt32 *>(p) = v.f;
			break;
		case tkDouble:
			*static_cast<Flt64 *>(p) = v.d;
			break;
		case tkObject:
		case tkSpecial:
		case tkArray:
		case tkInterface:
			*static_cast<void **>(p) = addrToPointer(v.a);
			break;
	}
}

//
// If known, return the type of the object at the given address.  If not known,
// return nil.
//
Type *objectAddressType(addr a)
{
	if (!a)
		return 0;

	Value type;
	assert(objectTypeOffset == 0);
	if (!read(tkObject, a, type))
		return 0;
	return &addressAsType(type.a);
}

