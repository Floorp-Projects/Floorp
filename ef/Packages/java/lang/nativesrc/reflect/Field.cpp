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
#include <stdio.h>
#include "java_lang_reflect_Field.h"
#include "java_lang_Class.h"

#include "JavaVM.h"
#include "SysCallsRuntime.h"
#include "StackWalker.h"

static inline void throwFieldException(RuntimeError err)
{
  // FIXME Need to check for illegalAccess, which we currently do not enforce
  if (err.cause == RuntimeError::nullPointer)
    sysThrowNullPointerException();
  else if (err.cause == RuntimeError::illegalArgument)
    sysThrowNamedException("java/lang/IllegalArgumentException");
  else
    trespass("Unknown Runtime Error in throwFieldException()");
}

extern "C" {

/* XXX Routines returning strings will have to be redone once the VM supports 
 * unicode strings directly
 */

static inline Field &toField(Java_java_lang_reflect_Field *inField)
{
  if (!inField)
    sysThrowNullPointerException();

  return *(Field *) inField;
}


/*
 * Class : java/lang/reflect/Field
 * Method : equals
 * Signature : (Ljava/lang/Object;)Z
 */
NS_EXPORT NS_NATIVECALL(uint32 /* bool */)
Netscape_Java_java_lang_reflect_Field_equals(Java_java_lang_reflect_Field *inField, 
					     Java_java_lang_Object *inObj)
{
  Field &field = toField(inField);

  JavaObject *obj = (JavaObject *) inObj;

  return obj && field.equals(*obj);
}

// Make sure that the class that called this method can access this field.
// Throw an IllegalAccessException if it can't.
static inline void checkCallingClass(const Field &field)
{
    Frame thisFrame;
    Method &callerMethod = thisFrame.getCallingJavaMethod();
    
    if (callerMethod.getDeclaringClass() != field.getDeclaringClass()) {
	if ((field.getModifiers() & CR_FIELD_PRIVATE)) 
	    sysThrowNamedException("java/lang/IllegalAccessException");
	else if ((field.getModifiers() & CR_FIELD_PROTECTED)) {
	    if (!field.getDeclaringClass()->isAssignableFrom(*(const Type *) callerMethod.getDeclaringClass()))
	        sysThrowNamedException("java/lang/IllegalAccessException");
	}
    }
}


#if 0
	/* Warning this code assumes that the caller was a Java Method */\
	thisFrame.moveToPrevFrame();		/* system guard frame */\
	thisFrame.moveToPrevFrame();		/* java frame */\
	Method *callerMethod = thisFrame.getMethod();\
	if (callerMethod->getDeclaringClass() != field.getDeclaringClass()) {\
	    if ((field.getModifiers() & CR_FIELD_PRIVATE)) \
               sysThrowNamedException("java/lang/IllegalAccessException");\
	    else if ((field.getModifiers() & CR_FIELD_PROTECTED)) {\
             if (!field.getDeclaringClass()->isAssignableFrom(*(const Type *) callerMethod->getDeclaringClass()))\
	        sysThrowNamedException("java/lang/IllegalAccessException");\
	    }\
	}
#endif

/*
 * Class : java/lang/reflect/Field
 * Method : get
 * Signature : (Ljava/lang/Object;)Ljava/lang/Object;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Object *)
Netscape_Java_java_lang_reflect_Field_get(Java_java_lang_reflect_Field *inField, 
					  Java_java_lang_Object *obj)
{
  Field &field = toField(inField);

  checkCallingClass(field);

  try {
    return (Java_java_lang_Object *)&field.get((JavaObject *) obj);
  } catch (RuntimeError err) {
    throwFieldException(err);
  }
  
}


/*
 * Class : java/lang/reflect/Field
 * Method : getBoolean
 * Signature : (Ljava/lang/Object;)Z
 */
NS_EXPORT NS_NATIVECALL(uint32 /* bool */)
Netscape_Java_java_lang_reflect_Field_getBoolean(Java_java_lang_reflect_Field *inField,
						 Java_java_lang_Object *obj)
{
  Field &field = toField(inField);

  checkCallingClass(field);

  try {
    return field.getBoolean((JavaObject *) obj);
  } catch (RuntimeError err) {
    throwFieldException(err);
  }

}


/*
 * Class : java/lang/reflect/Field
 * Method : getByte
 * Signature : (Ljava/lang/Object;)B
 */
NS_EXPORT NS_NATIVECALL(uint32 /* byte */)
Netscape_Java_java_lang_reflect_Field_getByte(Java_java_lang_reflect_Field *inField, 
					      Java_java_lang_Object *obj)
{
  Field &field = toField(inField);

  checkCallingClass(field);

  try {
    return field.getByte((JavaObject *) obj);
  } catch (RuntimeError err) {
    throwFieldException(err);
  }
}



/*
 * Class : java/lang/reflect/Field
 * Method : getChar
 * Signature : (Ljava/lang/Object;)C
 */
NS_EXPORT NS_NATIVECALL(int32 /* char */)
Netscape_Java_java_lang_reflect_Field_getChar(Java_java_lang_reflect_Field *inField, 
					      Java_java_lang_Object *obj)
{
    Field &field = toField(inField);

    checkCallingClass(field);    

    try {
      return field.getChar((JavaObject *) obj);
    } catch (RuntimeError err) {
      throwFieldException(err);
    }
}


/*
 * Class : java/lang/reflect/Field
 * Method : getDouble
 * Signature : (Ljava/lang/Object;)D
 */
NS_EXPORT NS_NATIVECALL(Flt64)
    Netscape_Java_java_lang_reflect_Field_getDouble(Java_java_lang_reflect_Field *inField, 
						    Java_java_lang_Object *obj)
{
    Field &field = toField(inField);

    checkCallingClass(field);

    try {
      return field.getDouble((JavaObject *) obj);
    } catch (RuntimeError err) {
      throwFieldException(err);
    }
}


/*
 * Class : java/lang/reflect/Field
 * Method : getFloat
 * Signature : (Ljava/lang/Object;)F
 */
NS_EXPORT NS_NATIVECALL(Flt32)
Netscape_Java_java_lang_reflect_Field_getFloat(Java_java_lang_reflect_Field *inField, 
					       Java_java_lang_Object *obj)
{
    Field &field = toField(inField);

    checkCallingClass(field);    

    try {
      return field.getFloat((JavaObject *) obj);
    } catch (RuntimeError err) {
      throwFieldException(err);
    }
}


/*
 * Class : java/lang/reflect/Field
 * Method : getInt
 * Signature : (Ljava/lang/Object;)I
 */
NS_EXPORT NS_NATIVECALL(int32)
Netscape_Java_java_lang_reflect_Field_getInt(Java_java_lang_reflect_Field *inField, 
					     Java_java_lang_Object *obj)
{
    Field &field = toField(inField);

    checkCallingClass(field);

    try {
      return field.getInt((JavaObject *) obj);
    } catch (RuntimeError err) {
      throwFieldException(err);
    }
}


/*
 * Class : java/lang/reflect/Field
 * Method : getLong
 * Signature : (Ljava/lang/Object;)J
 */
NS_EXPORT NS_NATIVECALL(int64)
Netscape_Java_java_lang_reflect_Field_getLong(Java_java_lang_reflect_Field *inField, 
					      Java_java_lang_Object *obj)
{
    Field &field = toField(inField);

    checkCallingClass(field);

    try {
      return field.getLong((JavaObject *) obj);
    } catch (RuntimeError err) {
      throwFieldException(err);
    }
}


/*
 * Class : java/lang/reflect/Field
 * Method : getShort
 * Signature : (Ljava/lang/Object;)S
 */
NS_EXPORT NS_NATIVECALL(int32 /* short */)
Netscape_Java_java_lang_reflect_Field_getShort(Java_java_lang_reflect_Field *inField, 
					       Java_java_lang_Object *obj)
{
    Field &field = toField(inField);

    checkCallingClass(field);    

    try {
      return field.getShort((JavaObject *) obj);
    } catch (RuntimeError err) {
      throwFieldException(err);
    }
}

/*
 * Class : java/lang/reflect/Field
 * Method : getDeclaringClass
 * Signature : ()Ljava/lang/Class;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Class *)
Netscape_Java_java_lang_reflect_Field_getDeclaringClass(Java_java_lang_reflect_Field *inField)
{
  Field &field = toField(inField);

  return ((Java_java_lang_Class *) field.getDeclaringClass());
}


/*
 * Class : java/lang/reflect/Field
 * Method : getModifiers
 * Signature : ()I
 */
NS_EXPORT NS_NATIVECALL(int32)
Netscape_Java_java_lang_reflect_Field_getModifiers(Java_java_lang_reflect_Field *inField)
{
  Field &field = toField(inField);

  return field.getModifiers();
}


/*
 * Class : java/lang/reflect/Field
 * Method : getName
 * Signature : ()Ljava/lang/String;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_String *)
Netscape_Java_java_lang_reflect_Field_getName(Java_java_lang_reflect_Field *inField)
{
  Field &field = toField(inField);

  const char *str = field.getName();

  return ((Java_java_lang_String *) &VM::intern(str));
}

       
/*
 * Class : java/lang/reflect/Field
 * Method : set
 * Signature : (Ljava/lang/Object;Ljava/lang/Object;)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_reflect_Field_set(Java_java_lang_reflect_Field *inField, 
					  Java_java_lang_Object *obj, 
					  Java_java_lang_Object *value)
{
  Field &field = toField(inField);

  checkCallingClass(field);

  if (value == 0)
    sysThrowNullPointerException();

  try {
    field.set((JavaObject *) obj, *(JavaObject *) value);
  } catch (RuntimeError err) {
    throwFieldException(err);
  }
}

/*
 * Class : java/lang/reflect/Field
 * Method : getType
 * Signature : ()Ljava/lang/Class;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Class *)
Netscape_Java_java_lang_reflect_Field_getType(Java_java_lang_reflect_Field *inField)
{
  Field &field = toField(inField);

  return (Java_java_lang_Class *) &field.getType();
}

/*
 * Class : java/lang/reflect/Field
 * Method : setBoolean
 * Signature : (Ljava/lang/Object;Z)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_reflect_Field_setBoolean(Java_java_lang_reflect_Field *inField,
						 Java_java_lang_Object *obj, 
						 uint32 /* bool */value)
{
  Field &field = toField(inField);

  checkCallingClass(field);

  try {
    field.setBoolean((JavaObject *) obj, (Int8) value);
  } catch (RuntimeError err) {
    throwFieldException(err);
  }
}


/*
 * Class : java/lang/reflect/Field
 * Method : setByte
 * Signature : (Ljava/lang/Object;B)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_reflect_Field_setByte(Java_java_lang_reflect_Field *inField,
					      Java_java_lang_Object *obj, 
					      uint32 /* byte */value)
{
    Field &field = toField(inField);

    checkCallingClass(field);    

    try {
      field.setByte((JavaObject *) obj, (Uint8) value);
    } catch (RuntimeError err) {
      throwFieldException(err);
    }
}


/*
 * Class : java/lang/reflect/Field
 * Method : setChar
 * Signature : (Ljava/lang/Object;C)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_reflect_Field_setChar(Java_java_lang_reflect_Field *inField, 
					      Java_java_lang_Object *obj, 
					      int32 /* char */value)
{
    Field &field = toField(inField);

    checkCallingClass(field);    

    try {
      field.setChar((JavaObject *) obj, (Int16) value);
    } catch (RuntimeError err) {
      throwFieldException(err);
    }
}


/*
 * Class : java/lang/reflect/Field
 * Method : setDouble
 * Signature : (Ljava/lang/Object;D)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_reflect_Field_setDouble(Java_java_lang_reflect_Field *inField,
						Java_java_lang_Object *obj, 
						Flt64 value)
{
    Field &field = toField(inField);
    
    checkCallingClass(field);

    try {
      field.setDouble((JavaObject *) obj, value);
    } catch (RuntimeError err) {
      throwFieldException(err);
    }
}


/*
 * Class : java/lang/reflect/Field
 * Method : setFloat
 * Signature : (Ljava/lang/Object;F)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_reflect_Field_setFloat(Java_java_lang_reflect_Field *inField, 
					       Java_java_lang_Object *obj, 
					       Flt32 value)
{
    Field &field = toField(inField);

    checkCallingClass(field);    

    try {
      field.setFloat((JavaObject *) obj, value);   
    } catch (RuntimeError err) {
      throwFieldException(err);
    }
}


/*
 * Class : java/lang/reflect/Field
 * Method : setInt
 * Signature : (Ljava/lang/Object;I)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_reflect_Field_setInt(Java_java_lang_reflect_Field *inField, 
					     Java_java_lang_Object *obj, 
					     int32 value)
{
    Field &field = toField(inField);

    checkCallingClass(field);    
 
    try {
      field.setInt((JavaObject *) obj, (Int32) value);  
    } catch (RuntimeError err) {
      throwFieldException(err);
    }
}


/*
 * Class : java/lang/reflect/Field
 * Method : setLong
 * Signature : (Ljava/lang/Object;J)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_reflect_Field_setLong(Java_java_lang_reflect_Field *inField, 
					      Java_java_lang_Object *obj, 
					      int64 value)
{
    Field &field = toField(inField);

    checkCallingClass(field);    

    try {
      field.setLong((JavaObject *) obj, value);  
    } catch (RuntimeError err) {
      throwFieldException(err);
    }
}


/*
 * Class : java/lang/reflect/Field
 * Method : setShort
 * Signature : (Ljava/lang/Object;S)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_reflect_Field_setShort(Java_java_lang_reflect_Field *inField, 
					       Java_java_lang_Object *obj, 
					       int32 /* short */value)
{
    Field &field = toField(inField);

    checkCallingClass(field);

    try {
      field.setShort((JavaObject *) obj, (Int16) value); 
    } catch (RuntimeError err) {
      throwFieldException(err);
    }
}

/*
 * Class : java/lang/reflect/Field
 * Method : toString
 * Signature : ()Ljava/lang/String;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_String *)
Netscape_Java_java_lang_reflect_Field_toString(Java_java_lang_reflect_Field *inField)
{
  Field &field = toField(inField);
  
  const char *str = field.toString();

  return ((Java_java_lang_String *) &VM::intern(str));
}

}

