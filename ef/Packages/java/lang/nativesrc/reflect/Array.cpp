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
#include "java_lang_reflect_Array.h"
#include <stdio.h>
#include "SysCallsRuntime.h"
#include "ErrorHandling.h"

static inline JavaArray &toArray(Java_java_lang_Object *obj)
{
  if (!obj)
    sysThrowNullPointerException();

  if (obj->type->typeKind != tkArray)
    sysThrowNamedException("java/lang/IllegalArgumentException");

  return *(JavaArray *) obj;
}

static inline void checkIndex(JavaArray &array, Int32 index)
{
  if (index < 0 || index >= (Int32) array.length)
    sysThrowArrayIndexOutOfBoundsException();
}


extern "C" {


/*
 * Class : java/lang/reflect/Array
 * Method : get
 * Signature : (Ljava/lang/Object;I)Ljava/lang/Object;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Object *)
Netscape_Java_java_lang_reflect_Array_get(Java_java_lang_Object *obj, int32 index)
{
  JavaArray &array = toArray(obj);
  
  checkIndex(array, index);
  
  try {
    return (Java_java_lang_Object *) array.get(index);
  } catch (RuntimeError) {
    sysThrowNamedException("java/lang/IllegalArgumentException");
  }
}


/*
 * Class : java/lang/reflect/Array
 * Method : getBoolean
 * Signature : (Ljava/lang/Object;I)Z
 */
NS_EXPORT NS_NATIVECALL(uint32 /* bool */)
Netscape_Java_java_lang_reflect_Array_getBoolean(Java_java_lang_Object *obj, 
						 int32 index)
{
  JavaArray &array = toArray(obj);

  checkIndex(array, index);

  try {
    return array.getBoolean(index);
  } catch (RuntimeError) {
    sysThrowNamedException("java/lang/IllegalArgumentException");
  }
}


/*
 * Class : java/lang/reflect/Array
 * Method : getByte
 * Signature : (Ljava/lang/Object;I)B
 */
NS_EXPORT NS_NATIVECALL(uint32 /* byte */)
Netscape_Java_java_lang_reflect_Array_getByte(Java_java_lang_Object *obj, 
					      int32 index)
{
  JavaArray &array = toArray(obj);

  checkIndex(array, index);

  try {
    return array.getByte(index);
  } catch (RuntimeError) {
    sysThrowNamedException("java/lang/IllegalArgumentException");
  }
}


/*
 * Class : java/lang/reflect/Array
 * Method : getChar
 * Signature : (Ljava/lang/Object;I)C
 */
NS_EXPORT NS_NATIVECALL(int32 /* char */)
Netscape_Java_java_lang_reflect_Array_getChar(Java_java_lang_Object *obj, 
					      int32 index)
{
  JavaArray &array = toArray(obj);

  checkIndex(array, index);

  try {
    return array.getChar(index);
  } catch (RuntimeError) {
    sysThrowNamedException("java/lang/IllegalArgumentException");
  }
}


/*
 * Class : java/lang/reflect/Array
 * Method : getDouble
 * Signature : (Ljava/lang/Object;I)D
 */
NS_EXPORT NS_NATIVECALL(Flt64)
Netscape_Java_java_lang_reflect_Array_getDouble(Java_java_lang_Object *obj, 
						int32 index)
{
  JavaArray &array = toArray(obj);

  checkIndex(array, index);

  try {
    return array.getDouble(index);
  } catch (RuntimeError) {
    sysThrowNamedException("java/lang/IllegalArgumentException");
  }
}


/*
 * Class : java/lang/reflect/Array
 * Method : getFloat
 * Signature : (Ljava/lang/Object;I)F
 */
NS_EXPORT NS_NATIVECALL(Flt32)
Netscape_Java_java_lang_reflect_Array_getFloat(Java_java_lang_Object *obj, 
					       int32 index)
{
  JavaArray &array = toArray(obj);

  checkIndex(array, index);

  try {
    return array.getFloat(index);
  } catch (RuntimeError) {
    sysThrowNamedException("java/lang/IllegalArgumentException");
  }
}


/*
 * Class : java/lang/reflect/Array
 * Method : getInt
 * Signature : (Ljava/lang/Object;I)I
 */
NS_EXPORT NS_NATIVECALL(int32)
Netscape_Java_java_lang_reflect_Array_getInt(Java_java_lang_Object *obj, 
					     int32 index)
{
  JavaArray &array = toArray(obj);

  checkIndex(array, index);

  try {
    return array.getInt(index);
  } catch (RuntimeError) {
    sysThrowNamedException("java/lang/IllegalArgumentException");
  }
}


/*
 * Class : java/lang/reflect/Array
 * Method : getLength
 * Signature : (Ljava/lang/Object;)I
 */
NS_EXPORT NS_NATIVECALL(int32)
Netscape_Java_java_lang_reflect_Array_getLength(Java_java_lang_Object *obj)
{
  JavaArray &array = toArray(obj);

  return array.length;
}


/*
 * Class : java/lang/reflect/Array
 * Method : getLong
 * Signature : (Ljava/lang/Object;I)J
 */
NS_EXPORT NS_NATIVECALL(int64)
Netscape_Java_java_lang_reflect_Array_getLong(Java_java_lang_Object *obj, int32 index)
{
  JavaArray &array = toArray(obj);

  checkIndex(array, index);

  try {
    return array.getLong(index);
  } catch (RuntimeError) {
    sysThrowNamedException("java/lang/IllegalArgumentException");
  }
}


/*
 * Class : java/lang/reflect/Array
 * Method : getShort
 * Signature : (Ljava/lang/Object;I)S
 */
NS_EXPORT NS_NATIVECALL(int32 /* short */)
Netscape_Java_java_lang_reflect_Array_getShort(Java_java_lang_Object *obj, int32 index)
{
  JavaArray &array = toArray(obj);

  checkIndex(array, index);

  try {
    return array.getShort(index);
  } catch (RuntimeError) {
    sysThrowNamedException("java/lang/IllegalArgumentException");
  }
}


/*
 * Class : java/lang/reflect/Array
 * Method : multiNewArray
 * Signature : (Ljava/lang/Class;[I)Ljava/lang/Object;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Object *)
Netscape_Java_java_lang_reflect_Array_multiNewArray(Java_java_lang_Class *inClass, 
						    ArrayOf_int32 *dimensions)
{
    if (!inClass || !dimensions)
	sysThrowNullPointerException();

    if (dimensions->length < 0)
	sysThrowNamedException("java/lang/NegativeArraySizeException");
    else if (dimensions->length >= 255 || dimensions->length == 0)
	sysThrowNamedException("java/lang/IllegalArgumentException");
	
    /* We special-case 2D and 3D arrays since we have sysCalls defined for them */
    Int32 numDimensions = dimensions->length;

    switch (numDimensions) {
    case 2:
	return (Java_java_lang_Object *) sysNew2DArray(&((Type *) inClass)->getArrayType().getArrayType(),
						       dimensions->elements[0],
						       dimensions->elements[1]);
    
    case 3:
	return (Java_java_lang_Object *) sysNew3DArray(&((Type *) inClass)->getArrayType().getArrayType().getArrayType(),
						       dimensions->elements[0],
						       dimensions->elements[1],
						       dimensions->elements[2]);
    default:
	return (Java_java_lang_Object *) sysNewNDArray((const Type *) inClass, dimensions);
    }
    
}


/*
 * Class : java/lang/reflect/Array
 * Method : newArray
 * Signature : (Ljava/lang/Class;I)Ljava/lang/Object;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Object *)
Netscape_Java_java_lang_reflect_Array_newArray(Java_java_lang_Class *inClass,
					       int32 size)
{
    if (!inClass)
	sysThrowNullPointerException();

    if (size < 0)
	sysThrowNamedException("java/lang/NegativeArraySizeException");

    const Type *typeOfArray = (const Type *) inClass;

    if (typeOfArray->isPrimitive()) {
	JavaArray *primitiveArray = (JavaArray *) sysNewPrimitiveArray(typeOfArray->typeKind, 
								       size);
	
	return (Java_java_lang_Object *) primitiveArray;
    } else {
	JavaArray *objectArray = (JavaArray *) sysNewObjectArray(typeOfArray, size);
	
	return (Java_java_lang_Object *) objectArray;
    }
}


/*
 * Class : java/lang/reflect/Array
 * Method : set
 * Signature : (Ljava/lang/Object;ILjava/lang/Object;)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_reflect_Array_set(Java_java_lang_Object *obj, int32 index, 
					  Java_java_lang_Object *value)
{
  JavaArray &array = toArray(obj);

  checkIndex(array, index);

  try {
    array.set(index, value);
  } catch (RuntimeError) {
    sysThrowNamedException("java/lang/IllegalArgumentException");
  }
}


/*
 * Class : java/lang/reflect/Array
 * Method : setBoolean
 * Signature : (Ljava/lang/Object;IZ)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_reflect_Array_setBoolean(Java_java_lang_Object *obj, 
						 int32 index, 
						 uint32 /* bool */ value)
{
  JavaArray &array = toArray(obj);

  checkIndex(array, index);

  try {
    array.setBoolean(index, (Int8) value);
  } catch (RuntimeError) {
    sysThrowNamedException("java/lang/IllegalArgumentException");
  }
}


/*
 * Class : java/lang/reflect/Array
 * Method : setByte
 * Signature : (Ljava/lang/Object;IB)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_reflect_Array_setByte(Java_java_lang_Object *obj, 
					      int32 index, 
					      uint32 /* byte */ value)
{
  JavaArray &array = toArray(obj);

  checkIndex(array, index);

  try {
    array.setByte(index, (Int8) value);
  } catch (RuntimeError) {
    sysThrowNamedException("java/lang/IllegalArgumentException");
  }
}


/*
 * Class : java/lang/reflect/Array
 * Method : setChar
 * Signature : (Ljava/lang/Object;IC)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_reflect_Array_setChar(Java_java_lang_Object *obj, 
					      int32 index, 
					      int32 /* char */ value)
{
  JavaArray &array = toArray(obj);

  checkIndex(array, index);

  try {
    array.setChar(index, (Int16) value);
  } catch (RuntimeError) {
    sysThrowNamedException("java/lang/IllegalArgumentException");
  }
}


/*
 * Class : java/lang/reflect/Array
 * Method : setDouble
 * Signature : (Ljava/lang/Object;ID)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_reflect_Array_setDouble(Java_java_lang_Object *obj, 
						int32 index, 
						Flt64 value)
{
  JavaArray &array = toArray(obj);

  checkIndex(array, index);

  try {
    array.setDouble(index, value);
  } catch (RuntimeError) {
    sysThrowNamedException("java/lang/IllegalArgumentException");
  }
}


/*
 * Class : java/lang/reflect/Array
 * Method : setFloat
 * Signature : (Ljava/lang/Object;IF)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_reflect_Array_setFloat(Java_java_lang_Object *obj, 
					       int32 index, Flt32 value)
{
  JavaArray &array = toArray(obj);

  checkIndex(array, index);

  try {
    array.setFloat(index, value);
  } catch (RuntimeError) {
    sysThrowNamedException("java/lang/IllegalArgumentException");
  }
}


/*
 * Class : java/lang/reflect/Array
 * Method : setInt
 * Signature : (Ljava/lang/Object;II)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_reflect_Array_setInt(Java_java_lang_Object *obj, 
					     int32 index, 
					     int32 value)
{
  JavaArray &array = toArray(obj);

  checkIndex(array, index);

  try {
    array.setInt(index, value);
  } catch (RuntimeError) {
    sysThrowNamedException("java/lang/IllegalArgumentException");
  }
}


/*
 * Class : java/lang/reflect/Array
 * Method : setLong
 * Signature : (Ljava/lang/Object;IJ)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_reflect_Array_setLong(Java_java_lang_Object *obj, 
					      int32 index, 
					      int64 value)
{
  JavaArray &array = toArray(obj);

  checkIndex(array, index);

  try {
    array.setLong(index, value);
  } catch (RuntimeError) {
    sysThrowNamedException("java/lang/IllegalArgumentException");
  }
}


/*
 * Class : java/lang/reflect/Array
 * Method : setShort
 * Signature : (Ljava/lang/Object;IS)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_reflect_Array_setShort(Java_java_lang_Object *obj, int32 index, 
					       int32 /* short */value)
{
  JavaArray &array = toArray(obj);

  checkIndex(array, index);

  try {
    array.setShort(index, (Int16) value);
  } catch (RuntimeError) {
    sysThrowNamedException("java/lang/IllegalArgumentException");
  }
}

}



