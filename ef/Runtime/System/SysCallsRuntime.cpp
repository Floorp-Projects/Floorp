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
#include "SysCalls.h"
#include "JavaObject.h"
#include <string.h>
#include <stdlib.h>
#include "Monitor.h"
#include "Exceptions.h"

#include "ErrorHandling.h"
#include "JavaVM.h"

extern "C" {

//
// Create a new object of the indicated size
//
SYSCALL_FUNC(void*) sysNew(const Class &inClass)
{
  return (&const_cast<Class *>(&inClass)->newInstance());
}

//
// If test is true then throw a NegativeArraySizeException
//
inline void throwNegativeArraySizeExceptionIf(bool test)
{
	if (test) {
		JavaObject& exception = const_cast<Class *>(&Standard::get(cNegativeArraySizeException))->newInstance();
		sysThrow(exception);
	}
}

SYSCALL_FUNC(void *) sysNewPrimitiveArray(TypeKind tk, Int32 length)
{
    throwNegativeArraySizeExceptionIf(length < 0);

    void *mem = calloc(arrayEltsOffset(tk) + getTypeKindSize(tk)*length, 1);
    return (new (mem) JavaArray(Array::obtain(tk), length));
}

//
// Create a new array of Java ints
//
SYSCALL_FUNC(void*) sysNewIntArray(Int32 length)
{
  return sysNewPrimitiveArray(tkInt, length);
}

//
// Throw an Exception
//
#if !defined(_WIN32) && !defined(LINUX)

SYSCALL_FUNC(void) sysThrow(const JavaObject& /* inObject */)
{
  trespass("\n *** Throw is unimplemented on this platform ***\n");
}
 
#endif

//
// Create a new array of Java chars
//
SYSCALL_FUNC(void *) sysNewCharArray(Int32 length)
{
  return sysNewPrimitiveArray(tkChar, length);
}

//
// Create a new array of Java bytes
//
SYSCALL_FUNC(void *) sysNewByteArray(Int32 length)
{
  return sysNewPrimitiveArray(tkByte, length);
}

SYSCALL_FUNC(void) sysCheckArrayStore(const JavaArray *arr, const JavaObject *obj)
{  
  /* Legal to assign a null reference to an array */
  if (!obj) 
	  return;

  /* Make sure that the type of obj is the same as that of the component
   * type of the array arr
   */
  const Type *componentType = &(static_cast<const Array *>(&arr->getType()))->componentType;
  const Type *objType = &obj->getType();
  bool ret;

  if (isPrimitiveKind(componentType->typeKind))
    ret = (componentType == objType);
  else 
	ret = implements(*objType, *componentType);

  if (!ret)
	sysThrowNamedException("java/lang/ArrayStoreException");
}

SYSCALL_FUNC(void) sysMonitorEnter(JavaObject *obj)
{
	if (obj == NULL)                                                        
		sysThrowNamedException("java/lang/NullPointerException");       
	Monitor::lock(*obj);
}

SYSCALL_FUNC(void) sysMonitorExit(JavaObject * obj)
{
	if (obj == NULL)                                                        
		sysThrowNamedException("java/lang/NullPointerException");       
	Monitor::unlock(*obj);
}

//
// Create a new array of Java booleans
//
SYSCALL_FUNC(void*) sysNewBooleanArray(Int32 length)
{
  return sysNewPrimitiveArray(tkBoolean, length);
}

//
// Create a new array of Java shorts
//
SYSCALL_FUNC(void*) sysNewShortArray(Int32 length)
{
  return sysNewPrimitiveArray(tkShort, length);
}

//
// Create a new array of Java longs
//
SYSCALL_FUNC(void*) sysNewLongArray(Int32 length)
{
  return sysNewPrimitiveArray(tkLong, length);
}

SYSCALL_FUNC(void *) sysNewFloatArray(Int32 length)
{
  return sysNewPrimitiveArray(tkFloat, length);
}

SYSCALL_FUNC(void *) sysNewDoubleArray(Int32 length)
{
  return sysNewPrimitiveArray(tkDouble, length);
}

//
// Create a new array of Java objects
//
SYSCALL_FUNC(void *) sysNewObjectArray(const Type *type, Int32 length)
{
	throwNegativeArraySizeExceptionIf(length < 0);
	const Array &typeOfArray = type->getArrayType();
	void *mem = calloc(arrayObjectEltsOffset + getTypeKindSize(tkObject)*length, 1);
	return (new (mem) JavaArray(typeOfArray, length));
}

SYSCALL_FUNC(void *) sysNew2DArray(const Type *type, Int32 length1, Int32 length2)
{
	throwNegativeArraySizeExceptionIf(length1 < 0 || length2 < 0);

	const Array &arrayType = asArray(*type);
    const Array &componentType = asArray(arrayType.componentType);
    const Type &elementType = componentType.componentType;

	JavaArray* newArray = (JavaArray *) sysNewObjectArray(&componentType, length1);

	JavaArray** elements = (JavaArray **) ((char *) newArray+arrayObjectEltsOffset);
	for (Int32 i = 0; i < length1; i++)
		elements[i] = (JavaArray *) sysNewObjectArray(&elementType, length2);

	return (void*) newArray;
}


SYSCALL_FUNC(void*) sysNew3DArray(const Type *type, Int32 length1, Int32 length2, Int32 length3)
{
	throwNegativeArraySizeExceptionIf(length1 < 0 || length2 < 0 || length3 < 0);

	const Array &arrayType = asArray(*type);
	const Array &componentType = asArray(arrayType.componentType);
	const Array &componentComponentType = asArray(componentType.componentType);
    const Type &elementType = componentComponentType.componentType;

	JavaArray* newArray = (JavaArray *) sysNewObjectArray(&componentType, length1);

	JavaArray** elements = (JavaArray **) (((char *) newArray)+arrayObjectEltsOffset);
	for (Int32 i = 0; i < length1; i++) {
		elements[i] = (JavaArray *) sysNewObjectArray(&componentComponentType, length2);
		
		JavaArray** elementElements = (JavaArray **) ((char *) (elements[i])+arrayObjectEltsOffset);
		for (Int32 j = 0; j < length2; j++)
			elementElements[j] = (JavaArray *) sysNewObjectArray(&elementType, length3);
	}

	return (void*) newArray;
}

SYSCALL_FUNC(void*) sysNewNDArray(const Type *type, JavaArray* lengthsArray)
{
	struct StackEntry {
		JavaArray*	array;
		Int32		index;
	};

	Int32* lengths = (Int32*) ((char *) lengthsArray + arrayObjectEltsOffset);
	Int32 nDimensions = lengthsArray->length;

	// FIX: replace this by mem from gc.
	StackEntry* stack = (StackEntry*) calloc(sizeof(StackEntry),nDimensions);
	StackEntry* stackPtr = stack;

	for (Uint8 i = 0; i < nDimensions; i++)
		throwNegativeArraySizeExceptionIf(lengths[i] < 0);

	JavaArray* newArray = (JavaArray *) sysNewObjectArray(&asArray(*type).componentType, lengths[0]);

	// start the stack.
	stackPtr->array = newArray;
	stackPtr->index = 0;

	while (stackPtr >= stack) {
		// get the current entry.
		StackEntry& entry = *stackPtr;

		// depth of this entry.
		Uint8 depth = stackPtr - stack;
		JavaArray* array = entry.array;
		Int32 index = entry.index++;

		if (index < lengths[depth]) {
			// create element array[index].
			JavaArray** elements = (JavaArray**) ((char *) array+arrayObjectEltsOffset);
			elements[index] = (JavaArray *) sysNewObjectArray(&asArray(array->getType()).componentType, lengths[depth + 1]);

			if (depth < (nDimensions - 2)) {
				// push the new element and start over.
				++stackPtr;
				stackPtr->index = 0;
				stackPtr->array = elements[index];
			}
		} else {
			// we are done with this array.
			--stackPtr;
		}
	}
	return (void *) newArray;
}

/* Throw an object of the type specified by fully qualified exception name */
NS_EXTERN SYSCALL_FUNC(void) sysThrowNamedException(const char *exceptionName)
{
	const Class *clazz = static_cast<const Class *>(VM::getCentral().addClass(exceptionName).getThisClass());

	/* Ensure that named class is a sub-class of java/lang/Exception */
#ifdef DEBUG
	const Class *excClazz = static_cast<const Class *>(VM::getCentral().addClass("java/lang/Exception").getThisClass());

	assert(excClazz->isAssignableFrom(*clazz));
#endif

	sysThrow(const_cast<Class *>(clazz)->newInstance());
}


/* Use the following from jitted code directly only if
 * you're on a platform that does *not* need stubs
 * to call native code
 */
#if !(defined _WIN32) && !(defined LINUX)
SYSCALL_FUNC(void) sysThrowNullPointerException()
{
  sysThrow(const_cast<Class *>(&Standard::get(cNullPointerException))->newInstance());
}

SYSCALL_FUNC(void) sysThrowArrayIndexOutOfBoundsException()
{
  sysThrow(const_cast<Class *>(&Standard::get(cArrayIndexOutOfBoundsException))->newInstance());
}

SYSCALL_FUNC(void) sysThrowClassCastException()
{
  sysThrow(const_cast<Class *>(&Standard::get(cClassCastException))->newInstance());
}

static SYSCALL_FUNC(void) sysThrowIllegalMonitorStateException()
{
  sysThrow(const_cast<Class *>(&Standard::get(cIllegalMonitorStateException))->newInstance());
}

#endif
}
