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
#ifndef ADDRESS_H
#define ADDRESS_H

#include "JavaObject.h"
#include "LogModule.h"

union Value;

//
// If we do batch compilation, we may not be able to emit code that references raw physical
// addresses because they may move around by the time the generated code is loaded.
// Thus, in a batch compiler we represent immediate pointers by Address structures instead
// of ptr constants.  The addr type is an abstraction for either an Address (if batch-
// compiling) or a ptr (if JIT-compiling).
//
#if BATCH_COMPILATION
//
// The value of an Address is reloc(base)+offset.  It is not legal to construct an Address
// whose offset is outside the base object's bounds.  Two Addresses are comparable for inequality
// only if their base is the same.
// 
class Address
{
	obj base;									// Base address that will be relocated by loader
	Int32 offset;								// Offset from relocated base

  public:
	obj getBase() const {return base;}
	Int32 getOffset() const {return offset;}

	void setBase(obj b) {base = b; offset = 0;}
	void setBase(obj b, Int32 o) {base = b; offset = o;}

	bool operator!() const {return base == 0 && offset == 0;}
	bool operator==(const Address &a) const {return base == a.base && offset == a.offset;}
	bool operator!=(const Address &a) const {return base != a.base || offset != a.offset;}
	bool operator<(const Address &a) const {assert(base == a.base); return offset < a.offset;}
	bool operator<=(const Address &a) const {assert(base == a.base); return offset <= a.offset;}
	bool operator>(const Address &a) const {assert(base == a.base); return offset > a.offset;}
	bool operator>=(const Address &a) const {assert(base == a.base); return offset >= a.offset;}

	void operator+=(Int32 d) {offset += d;}
	void operator-=(Int32 d) {offset += d;}	// BUG

	Address operator+(Int32 d) const {Address a; a.setBase(base, offset + d); return a;}
	Address operator-(Int32 d) const {Address a; a.setBase(base, offset - d); return a;}
	Int32 operator-(const Address &a) const {assert(base == a.base); return offset - a.offset;}
};

typedef Address addr;

// Private structure for nullAddr.
struct ConstructedAddress: Address
{
	ConstructedAddress(obj b) {setBase(b);}
	ConstructedAddress(obj b, Int32 o) {setBase(b, o);}
};
extern const ConstructedAddress nullAddr;

#else

typedef ptr addr;
#define nullAddr ((ptr)0)

#endif

#ifdef DEBUG_LOG
int print(LogModuleObject &f, addr a);
#endif
inline addr staticAddress(void *s);
inline addr functionAddress(void (*f)());
inline addr objectAddress(cobj o);
inline obj addressObject(addr a);
inline void* addressFunction(addr a);
bool read(TypeKind tk, addr a, Value &v);
void write(TypeKind tk, addr a, const Value &v);
inline Type &addressAsType(addr a);
Type *objectAddressType(addr a);
addr lookupInterface(Type &type, Uint32 interfaceIndex);


// --- INLINES ----------------------------------------------------------------

//
// Return the addr, possibly with relocation information, of the given static variable.
//
inline addr staticAddress(void *s)
{
  #if BATCH_COMPILATION
	Address a;
	a.setBase(reinterpret_cast<obj>(s));
	return a;
  #else
	return reinterpret_cast<addr>(s);
  #endif
}


//
// Return the addr, possibly with relocation information, of the given function.
//
inline addr functionAddress(void (*f)())
{
  #if BATCH_COMPILATION
	Address a;
	a.setBase((obj)f);
	return a;
  #else
	return (addr)f;
  #endif
}


//
// Return the addr, possibly with relocation information, of the given object.
//
inline addr objectAddress(cobj o)
{
  #if BATCH_COMPILATION
	Address a;
	a.setBase(const_cast<obj>(o));
	return a;
  #else
	return reinterpret_cast<addr>(const_cast<obj>(o));
  #endif
}


//
// Return the object at the given addr (which must have been obtained by objectAddress).
//
inline obj addressObject(addr a)
{
  #if BATCH_COMPILATION
	assert(a.getOffset() == 0);
	return a.getBase();
  #else
	return reinterpret_cast<obj>(a);
  #endif
}


//
// Return the function at the given addr (which must have been obtained by objectAddress).
//
inline void* addressFunction(addr a)
{
  #if BATCH_COMPILATION
	assert(a.getOffset() == 0);
	return (void*) a.getBase();
  #else
	return reinterpret_cast<void*>(a);
  #endif
}


//
// Assert that this addr is really a Type and return that Type.
//
inline Type &addressAsType(addr a)
{
	JavaObject *o = addressObject(a);
	assert(o);
	return asType(*o);
}

#endif
