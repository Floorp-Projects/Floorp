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

#include "ClassWorld.h"
#include <stdlib.h>
#if 0
#include "ClassCentral.h"
#endif

// Size of each TypeKind
const int8 typeKindSizes[nTypeKinds] =
{
    0,	// tkVoid
	1,	// tkBoolean
	1,	// tkUByte
	1,	// tkByte
	2,	// tkChar
	2,	// tkShort
	4,	// tkInt
	8,	// tkLong
	4,	// tkFloat
	8,	// tkDouble
	4,	// tkObject
	4,	// tkSpecial
	4,	// tkArray
	4	// tkInterface
};

// lg2 of the size of each TypeKind
const int8 lgTypeKindSizes[nTypeKinds] =
{
	-1,	// tkVoid
	0,	// tkBoolean
	0,	// tkUByte
	0,	// tkByte
	1,	// tkChar
	1,	// tkShort
	2,	// tkInt
	3,	// tkLong
	2,	// tkFloat
	3,	// tkDouble
	2,	// tkObject
	2,	// tkSpecial
	2,	// tkArray
	2	// tkInterface
};

// Number of environment slots taken by each TypeKind
const int8 typeKindNSlots[nTypeKinds] =
{
	0,	// tkVoid
	1,	// tkBoolean
	1,	// tkUByte
	1,	// tkByte
	1,	// tkChar
	1,	// tkShort
	1,	// tkInt
	2,	// tkLong
	1,	// tkFloat
	2,	// tkDouble
	1,	// tkObject
	1,	// tkSpecial
	1,	// tkArray
	1	// tkInterface
};


// ----------------------------------------------------------------------------


// Distinguished classes
const Class *cObject;
const Class *cThrowable;
const Class *cException;
const Class *cRuntimeException;
const Class *cArithmeticException;
const Class *cArrayStoreException;
const Class *cClassCastException;
const Class *cArrayIndexOutOfBoundsException;
const Class *cNegativeArraySizeException;
const Class *cNullPointerException;

// Internal classes
const Class *cPrimitiveType;
const Class *cPrimitiveArray;
const Class *cObjectArray;
const Class *cClass;
const Class *cInterface;

// Distinguished interfaces
const Interface *iCloneable;

// Distinguished objects
const JavaObject *oArithmeticException;
const JavaObject *oArrayStoreException;
const JavaObject *oClassCastException;
const JavaObject *oArrayIndexOutOfBoundsException;
const JavaObject *oNegativeArraySizeException;
const JavaObject *oNullPointerException;


//
// Initialize the distinguished classes and objects.
// This routine must be called before any of these are referenced.
// Soon to be obsolete.
//
void initDistinguishedObjects()
{
    cPrimitiveType = &Class::make(pkgInternal, "PrimitiveType", 0, 0, 0, 0, 0, 0, 0, true, sizeof(PrimitiveType), 0);
	cPrimitiveArray = &Class::make(pkgInternal, "PrimitiveArray", 0, 0, 0, 0, 0, 0, 0, true, sizeof(Array), 0);
	cObjectArray = &Class::make(pkgInternal, "ObjectArray", 0, 0, 0, 0, 0, 0, 0, true, sizeof(Array), 0);
	cClass = &Class::make(pkgInternal, "Class", 0, 0, 0, 0, 0, 0, 0, true, sizeof(Class), 0);
	cInterface = &Class::make(pkgInternal, "Interface", 0, 0, 0, 0, 0, 0, 0, true, sizeof(Interface), 0);

	PrimitiveType::staticInit();
	Array::staticInit();

    cObject = &Class::make(pkgJavaLang, "Object", 0, 0, 0, 0, 0, 0, 0, false, sizeof(JavaObject), 0);
    cThrowable = &Class::make(pkgJavaLang, "Throwable", cObject, 0, 0, 0, 0, 0, 0, false, sizeof(JavaObject), 0);
	cException = &Class::make(pkgJavaLang, "Exception", cThrowable, 0, 0, 0, 0, 0, 0, false, sizeof(JavaObject), 0);
	cRuntimeException = &Class::make(pkgJavaLang, "RuntimeException", cException, 0, 0, 0, 0, 0, 0, false, sizeof(JavaObject), 0);
	cArithmeticException = &Class::make(pkgJavaLang, "ArithmeticException", cRuntimeException, 0, 0, 0, 0, 0, 0, false, sizeof(JavaObject), 0);
	cArrayStoreException = &Class::make(pkgJavaLang, "ArrayStoreException", cRuntimeException, 0, 0, 0, 0, 0, 0, false, sizeof(JavaObject), 0);
	cClassCastException = &Class::make(pkgJavaLang, "ClassCastException", cRuntimeException, 0, 0, 0, 0, 0, 0, false, sizeof(JavaObject), 0);
	cIndexOutOfBoundsException = &Class::make(pkgJavaLang, "IndexOutOfBoundsException", cRuntimeException, 0, 0, 0, 0, 0, 0, false, sizeof(JavaObject), 0);
	cArrayIndexOutOfBoundsException = &Class::make(pkgJavaLang, "ArrayIndexOutOfBoundsException", cIndexOutOfBoundsException, 0, 0, 0, 0, 0, 0, false, sizeof(JavaObject), 0);
	cNegativeArraySizeException = &Class::make(pkgJavaLang, "NegativeArraySizeException", cRuntimeException, 0, 0, 0, 0, 0, 0, false, sizeof(JavaObject), 0);
	cNullPointerException = &Class::make(pkgJavaLang, "NullPointerException", cRuntimeException, 0, 0, 0, 0, 0, 0, false, sizeof(JavaObject), 0);

	iCloneable = new Interface(pkgJavaLang, "Cloneable", 0, 0, 0, 0, 0, 0);

	oArithmeticException = new JavaObject(*cArithmeticException);
	oArrayStoreException = new JavaObject(*cArrayStoreException);
	oClassCastException = new JavaObject(*cClassCastException);
	oArrayIndexOutOfBoundsException = new JavaObject(*cArrayIndexOutOfBoundsException);
	oNegativeArraySizeException = new JavaObject(*cNegativeArraySizeException);
	oNullPointerException = new JavaObject(*cNullPointerException);
}

#if 0
inline const Class *toClass(ClassCentral &central, const char *className)
{
  return static_cast<Class *>(central.addClass(className).getThisClass());
}

inline const Interface *toInterface(ClassCentral &central, 
									const char *className)
{
  return static_cast<Interface *>(central.addClass(className).getThisClass());
}

//
// Initialize the distinguished classes and objects.
// This routine must be called before any of these are referenced.
//
void initDistinguishedObjects(ClassCentral &c)
{
    cPrimitiveType = &Class::make(pkgInternal, "PrimitiveType", 0, 0, 0, 0, 0, 0, 0, true, sizeof(PrimitiveType), 0);
	cPrimitiveArray = &Class::make(pkgInternal, "PrimitiveArray", 0, 0, 0, 0, 0, 0, 0, true, sizeof(Array), 0);
	cObjectArray = &Class::make(pkgInternal, "ObjectArray", 0, 0, 0, 0, 0, 0, 0, true, sizeof(Array), 0);
	cClass = &Class::make(pkgInternal, "Class", 0, 0, 0, 0, 0, 0, 0, true, sizeof(Class), 0);
	cInterface = &Class::make(pkgInternal, "Interface", 0, 0, 0, 0, 0, 0, 0, true, sizeof(Interface), 0);

	PrimitiveType::staticInit();
	Array::staticInit();

	cObject = toClass(c, "java/lang/Object");
    cThrowable = toClass(c, "java/lang/Throwable");
	cException = toClass(c, "java/lang/Exception");
	cRuntimeException = toClass(c, "java/lang/RuntimeException");
	cArithmeticException = toClass(c, "java/lang/ArithmeticException");
	cArrayStoreException = toClass(c, "java/lang/ArrayStoreException");
	cClassCastException = toClass(c, "java/lang/ClassCastException");
	cIndexOutOfBoundsException = toClass(c, "java/lang/IndexOutOfBoundsException");
	cArrayIndexOutOfBoundsException = toClass(c, "java/lang/ArrayIndexOutOfBoundsException");
	cNegativeArraySizeException = toClass(c, "java/lang/NegativeArraySizeException");
	cNullPointerException = toClass(c, "java/lang/NullPointerException");

	iCloneable = toInterface(c, "java/lang/Cloneable");

	oArithmeticException = new JavaObject(*cArithmeticException);
	oArrayStoreException = new JavaObject(*cArrayStoreException);
	oClassCastException = new JavaObject(*cClassCastException);
	oArrayIndexOutOfBoundsException = new JavaObject(*cIndexOutOfBoundsException);
	oNegativeArraySizeException = new JavaObject(*cNegativeArraySizeException);
	oNullPointerException = new JavaObject(*cNullPointerException);
}
#endif

// ----------------------------------------------------------------------------
// Type

//
// Return true if sub is a subtype of super.  In other words, return true if
// an instance or variable of type sub can be assigned to a variable of type super.
//
bool implements(const Type &sub, const Type &super)
{
	// Every type is a subtype of itself.
	if (&super == &sub)
		return true;

	TypeKind superKind = super.typeKind;
	TypeKind subKind = sub.typeKind;

	switch (subKind) {
		case tkObject:
			switch (superKind) {
				// A class's supertypes can only be its superclasses or superinterfaces --
				// never primitive objects, arrays, or special objects.
				case tkObject:
					return static_cast<const Class *>(&sub)->implements(*static_cast<const Class *>(&super));
				case tkInterface:
					return static_cast<const Class *>(&sub)->implements(*static_cast<const Interface *>(&super));
				default:;
			}
			break;

		case tkArray:
			switch (superKind) {
				// An array's supertypes can only be:
				//   the class Object,
				//   the interface Cloneable, or
				//   arrays whose element type is a supertype of this array's element type.
				case tkObject:
					return &super == cObject;
				case tkArray:
					return implements(static_cast<const Array *>(&sub)->elementType,
									  static_cast<const Array *>(&super)->elementType);
				case tkInterface:
					return &super == iCloneable;
				default:;
			}
			break;

		case tkInterface:
			switch (superKind) {
				// An interface's supertypes can only be:
				//   the class Object, or
				//   the interface's superinterfaces.
				case tkObject:
					return &super == cObject;
				case tkInterface:
					return static_cast<const Interface *>(&sub)->implements(*static_cast<const Interface *>(&super));
				default:;
			}
			break;

		default:;
	}

	// There are no other nontrivial subtyping relationships.
	return false;
}


// ----------------------------------------------------------------------------
// PrimitiveType

const PrimitiveType *PrimitiveType::primitiveTypes[nPrimitiveTypeKinds];

//
// Initialize the table used by the obtain method.
// cPrimitiveType must have already been initialized.
//
void PrimitiveType::staticInit()
{
	assert(cPrimitiveType);
	for (TypeKind tk = tkVoid; tk < nPrimitiveTypeKinds; tk = (TypeKind)(tk+1))
		primitiveTypes[tk] = new PrimitiveType(*cPrimitiveType, tk);
}


// ----------------------------------------------------------------------------
// Array

const Array *Array::primitiveArrays[nPrimitiveTypeKinds];


//
// Initialize a type for arrays.
// Do not call this constructor directly; use obtain or Type::getArrayType instead.
// 
Array::Array(const Type &elementType):
	Type(isPrimitiveKind(elementType.typeKind) ? *cPrimitiveArray : *cObjectArray, tkArray, elementType.final, 0),
	elementType(elementType)
{
	assert(elementType.typeKind != tkVoid);
}


//
// Initialize the table used by the obtain method.
// PrimitiveType::obtain must have already been initialized.
//
void Array::staticInit()
{
	primitiveArrays[tkVoid] = 0;	// Can't have an array of type Void.
	for (TypeKind tk = tkBoolean; tk < nPrimitiveTypeKinds; tk = (TypeKind)(tk+1))
		primitiveArrays[tk] = &PrimitiveType::obtain(tk).getArrayType();
}


// ----------------------------------------------------------------------------
// ClassOrInterface

#ifdef DEBUG
//
// Print a reference to this class or interface for debugging purposes.
// Return the number of characters printed.
//
int ClassOrInterface::printRef(FILE *f) const
{
	int o = package.printRef(f);
	return o + fprintf(f, ".%s", name);
}
#endif



// ----------------------------------------------------------------------------
// Class


//
// Initialize the fields of a Class object.  This constructor may only be called
// from within Class::make because the Class object must be specially allocated to
// allow room for its vtable.
//
inline Class::Class(Package &package, const char *name, const Class *_parent, 
					uint32 nInterfaces,
					Interface **interfaces, 
					uint32 nFields, 
					Field **fields,
					uint32 nMethods,
					Method **methods,
					bool final, uint32 instanceSize, uint32 nVTableSlots):
	ClassOrInterface(*cClass, tkObject, final, nVTableSlots, package, name, 
					 nInterfaces, interfaces,
					 nFields, fields,
					 nMethods, methods),
	instanceSize(instanceSize)
{
  parent = const_cast<Class *>(_parent);
}


//
// Initialize a new Class object that describes essential information such as layout
// and inheritance relationships of objects of a single Java class.
//
// package is the package that defined this class.
// name is the unqualified name of this class.
// parent is the parent class or nil if this is the Class for the Object class.
// interfaces is an array of nInterfaces elements that lists all interfaces from which
//   this class derives except that the interfaces array does not include interfaces
//   from which the parent class derives.
// extraSize is the number of additional bytes taken by fields of this class and should
//   be a multiple of 4.
// nExtraVTableSlots is the number of additional virtual methods that this class declares;
//   it does not include the one vtable slot used for class membership testing.
//
Class &Class::make(Package &package, const char *name, const Class *parent, uint32 nInterfaces, Interface **interfaces,
				   uint32 nFields, Field **fields,
				   uint32 nMethods, Method **methods,
				   bool final, uint32 extraSize, uint32 nExtraVTableSlots)
{
	uint32 nVTableSlots = nExtraVTableSlots + 1;
	if (parent) {
		assert(!parent->final);
		nVTableSlots += parent->nVTableSlots;
		extraSize += parent->instanceSize;
	}
	void *mem = malloc(sizeof(Class) + nVTableSlots*sizeof(VTableEntry));
	return *new(mem) Class(package, name, parent, nInterfaces, interfaces, 
						   nFields, fields, nMethods, methods,
						   final, extraSize, nVTableSlots);
}


//
// Return true if this class implements class c.
//
bool Class::implements(const Class &c) const
{
	const Class *a = this;
	do {
		if (a == &c)
			return true;
		a = a->parent;
	} while (a);
	return false;
}


//
// Return true if this class implements interface i.
//
bool Class::implements(const Interface &i) const
{
	const Class *a = this;
	do {
		uint32 n = a->nInterfaces;
		Interface *const*intfs = a->interfaces;
		while (n--) {
			if (*intfs == &i)
				return true;
			intfs++;
		}
		a = a->parent;
	} while (a);
	return false;
}


// ----------------------------------------------------------------------------
// Interface

//
// Return true if this interface implements interface i.
//
bool Interface::implements(const Interface &i) const
{
	uint32 n = nInterfaces;
	Interface *const*intfs = interfaces;
	while (n--) {
		if (*intfs == &i)
			return true;
		intfs++;
	}
	return false;
}

