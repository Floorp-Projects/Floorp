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

#ifndef CHECKEDUNION_H
#define CHECKEDUNION_H

//
// Use the CheckedUnion2 macro to create a checked union of two structures T1 and T2
// with the following features:
//
// 1.  In DEBUG versions the union remembers which of the structures in it, if any,
//     is currently valid.
// 2.  The structures T1 and T2 can have constructors.
// 3.  Methods are provided to explicitly construct a T1 or T2 inside the union.
//
//
// To use a CheckedUnion2, do the following:
//
// Declare the structures T1 and T2 and the checked union as follows:
//
//   struct Foo { ... };
//   struct Bar { ... };
//   CheckedUnion2(Foo, Bar) cu;
//
// To construct a Foo inside the checked union, call the following, replacing the ...
// with arguments to Foo's constructor.  Constructing a Bar is analogous.
//
//   new(cu.initFoo()) Foo(...);
//
// To get a reference to the Foo inside the checked union, call the following.  In DEBUG
// versions it will raise an assert if the checked union does not currently contain a Foo.
// Getting a reference to a Bar is analogous.
//
//   cu.getFoo()
//
// To erase the union's knowledge of the structure it contains, call the following.  This
// is a no-op in non-DEBUG versions.
//
//   cu.clear();
//
// Caution:  T1 and T2 should not have destructors; the checked union does not call them.
//           T1 and T2 should not have elements that require more than pointer alignment.
//

#define UncheckedUnion2(T1, T2)																		\
union UncheckedUnion_##T1##_##T2																	\
{																									\
  private:																							\
	void *aligner;			/* Unused field to ensure that the union is properly aligned */			\
	char f##T1[sizeof(T1)];	/* Fields of T1 */														\
	char f##T2[sizeof(T2)];	/* Fields of T2 */														\
																									\
  public:																							\
	void *init##T1() {return f##T1;}																\
	void *init##T2() {return f##T2;}																\
	void clear() {}																					\
																									\
	const T1 &get##T1() const {return *static_cast<const T1 *>(static_cast<const void *>(f##T1));}	\
	T1 &get##T1() {return *static_cast<T1 *>(static_cast<void *>(f##T1));}							\
	const T2 &get##T2() const {return *static_cast<const T2 *>(static_cast<const void *>(f##T2));}	\
	T2 &get##T2() {return *static_cast<T2 *>(static_cast<void *>(f##T2));}							\
}

#define CheckedUnion2DEBUG(T1, T2)																	\
class CheckedUnion_##T1##_##T2																		\
{																									\
	T1 *p##T1;	/* Pointer to T1 (to simplify looking at T1 in a debugger) or nil if none */		\
	T2 *p##T2;	/* Pointer to T2 (to simplify looking at T2 in a debugger) or nil if none */		\
	UncheckedUnion2(T1, T2) u;																		\
																									\
  public:																							\
	CheckedUnion_##T1##_##T2() {p##T1 = 0; p##T2 = 0;}												\
	void *init##T1() {p##T1 = &u.get##T1(); p##T2 = 0; return u.init##T1();}						\
	void *init##T2() {p##T1 = 0; p##T2 = &u.get##T2(); return u.init##T2();}						\
	void clear() {p##T1 = 0; p##T2 = 0;}															\
																									\
	const T1 &get##T1() const {assert(p##T1); return u.get##T1();}									\
	T1 &get##T1() {assert(p##T1); return u.get##T1();}												\
	const T2 &get##T2() const {assert(p##T2); return u.get##T2();}									\
	T2 &get##T2() {assert(p##T2); return u.get##T2();}												\
}

#ifdef DEBUG
 #define CheckedUnion2(T1, T2) CheckedUnion2DEBUG(T1, T2)
#else
 #define CheckedUnion2(T1, T2) UncheckedUnion2(T1, T2)
#endif


//
// CheckedUnion3 is analogous to CheckedUnion2, but with three alternatives.
//

#define UncheckedUnion3(T1, T2, T3)																	\
union UncheckedUnion_##T1##_##T2##_##T3																\
{																									\
  private:																							\
	void *aligner;			/* Unused field to ensure that the union is properly aligned */			\
	char f##T1[sizeof(T1)];	/* Fields of T1 */														\
	char f##T2[sizeof(T2)];	/* Fields of T2 */														\
	char f##T3[sizeof(T3)];	/* Fields of T3 */														\
																									\
  public:																							\
	void *init##T1() {return f##T1;}																\
	void *init##T2() {return f##T2;}																\
	void *init##T3() {return f##T3;}																\
	void clear() {}																					\
																									\
	const T1 &get##T1() const {return *static_cast<const T1 *>(static_cast<const void *>(f##T1));}	\
	T1 &get##T1() {return *static_cast<T1 *>(static_cast<void *>(f##T1));}							\
	const T2 &get##T2() const {return *static_cast<const T2 *>(static_cast<const void *>(f##T2));}	\
	T2 &get##T2() {return *static_cast<T2 *>(static_cast<void *>(f##T2));}							\
	const T3 &get##T3() const {return *static_cast<const T3 *>(static_cast<const void *>(f##T3));}	\
	T3 &get##T3() {return *static_cast<T3 *>(static_cast<void *>(f##T3));}							\
}

#define CheckedUnion3DEBUG(T1, T2, T3)																\
class CheckedUnion_##T1##_##T2##_##T3																\
{																									\
	T1 *p##T1;	/* Pointer to T1 (to simplify looking at T1 in a debugger) or nil if none */		\
	T2 *p##T2;	/* Pointer to T2 (to simplify looking at T2 in a debugger) or nil if none */		\
	T3 *p##T3;	/* Pointer to T3 (to simplify looking at T3 in a debugger) or nil if none */		\
	UncheckedUnion3(T1, T2, T3) u;																	\
																									\
  public:																							\
	CheckedUnion_##T1##_##T2##_##T3() {p##T1 = 0; p##T2 = 0; p##T3 = 0;}							\
	void *init##T1() {p##T1 = &u.get##T1(); p##T2 = 0; p##T3 = 0; return u.init##T1();}				\
	void *init##T2() {p##T1 = 0; p##T2 = &u.get##T2(); p##T3 = 0; return u.init##T2();}				\
	void *init##T3() {p##T1 = 0; p##T2 = 0; p##T3 = &u.get##T3(); return u.init##T3();}				\
	void clear() {p##T1 = 0; p##T2 = 0; p##T3 = 0;}													\
																									\
	const T1 &get##T1() const {assert(p##T1); return u.get##T1();}									\
	T1 &get##T1() {assert(p##T1); return u.get##T1();}												\
	const T2 &get##T2() const {assert(p##T2); return u.get##T2();}									\
	T2 &get##T2() {assert(p##T2); return u.get##T2();}												\
	const T3 &get##T3() const {assert(p##T3); return u.get##T3();}									\
	T3 &get##T3() {assert(p##T3); return u.get##T3();}												\
}

#ifdef DEBUG
 #define CheckedUnion3(T1, T2, T3) CheckedUnion3DEBUG(T1, T2, T3)
#else
 #define CheckedUnion3(T1, T2, T3) UncheckedUnion3(T1, T2, T3)
#endif


//
// CheckedUnion4 is analogous to CheckedUnion2, but with four alternatives.
//

#define UncheckedUnion4(T1, T2, T3, T4)																\
union UncheckedUnion_##T1##_##T2##_##T3##_##T4														\
{																									\
  private:																							\
	void *aligner;			/* Unused field to ensure that the union is properly aligned */			\
	char f##T1[sizeof(T1)];	/* Fields of T1 */														\
	char f##T2[sizeof(T2)];	/* Fields of T2 */														\
	char f##T3[sizeof(T3)];	/* Fields of T3 */														\
	char f##T4[sizeof(T4)];	/* Fields of T4 */														\
																									\
  public:																							\
	void *init##T1() {return f##T1;}																\
	void *init##T2() {return f##T2;}																\
	void *init##T3() {return f##T3;}																\
	void *init##T4() {return f##T4;}																\
	void clear() {}																					\
																									\
	const T1 &get##T1() const {return *static_cast<const T1 *>(static_cast<const void *>(f##T1));}	\
	T1 &get##T1() {return *static_cast<T1 *>(static_cast<void *>(f##T1));}							\
	const T2 &get##T2() const {return *static_cast<const T2 *>(static_cast<const void *>(f##T2));}	\
	T2 &get##T2() {return *static_cast<T2 *>(static_cast<void *>(f##T2));}							\
	const T3 &get##T3() const {return *static_cast<const T3 *>(static_cast<const void *>(f##T3));}	\
	T3 &get##T3() {return *static_cast<T3 *>(static_cast<void *>(f##T3));}							\
	const T4 &get##T4() const {return *static_cast<const T4 *>(static_cast<const void *>(f##T4));}	\
	T4 &get##T4() {return *static_cast<T4 *>(static_cast<void *>(f##T4));}							\
}

#define CheckedUnion4DEBUG(T1, T2, T3, T4)															\
class CheckedUnion_##T1##_##T2##_##T3##_##T4														\
{																									\
	T1 *p##T1;	/* Pointer to T1 (to simplify looking at T1 in a debugger) or nil if none */		\
	T2 *p##T2;	/* Pointer to T2 (to simplify looking at T2 in a debugger) or nil if none */		\
	T3 *p##T3;	/* Pointer to T3 (to simplify looking at T3 in a debugger) or nil if none */		\
	T4 *p##T4;	/* Pointer to T4 (to simplify looking at T4 in a debugger) or nil if none */		\
	UncheckedUnion4(T1, T2, T3, T4) u;																\
																									\
  public:																							\
	CheckedUnion_##T1##_##T2##_##T3##_##T4() {p##T1 = 0; p##T2 = 0; p##T3 = 0; p##T4 = 0;}			\
	void *init##T1() {p##T1 = &u.get##T1(); p##T2 = 0; p##T3 = 0; p##T4 = 0; return u.init##T1();}	\
	void *init##T2() {p##T1 = 0; p##T2 = &u.get##T2(); p##T3 = 0; p##T4 = 0; return u.init##T2();}	\
	void *init##T3() {p##T1 = 0; p##T2 = 0; p##T3 = &u.get##T3(); p##T4 = 0; return u.init##T3();}	\
	void *init##T4() {p##T1 = 0; p##T2 = 0; p##T3 = 0; p##T4 = &u.get##T4(); return u.init##T4();}	\
	void clear() {p##T1 = 0; p##T2 = 0; p##T3 = 0; p##T4 = 0;}										\
																									\
	const T1 &get##T1() const {assert(p##T1); return u.get##T1();}									\
	T1 &get##T1() {assert(p##T1); return u.get##T1();}												\
	const T2 &get##T2() const {assert(p##T2); return u.get##T2();}									\
	T2 &get##T2() {assert(p##T2); return u.get##T2();}												\
	const T3 &get##T3() const {assert(p##T3); return u.get##T3();}									\
	T3 &get##T3() {assert(p##T3); return u.get##T3();}												\
	const T4 &get##T4() const {assert(p##T4); return u.get##T4();}									\
	T4 &get##T4() {assert(p##T4); return u.get##T4();}												\
}

#ifdef DEBUG
 #define CheckedUnion4(T1, T2, T3, T4) CheckedUnion4DEBUG(T1, T2, T3, T4)
#else
 #define CheckedUnion4(T1, T2, T3, T4) UncheckedUnion4(T1, T2, T3, T4)
#endif

#endif
