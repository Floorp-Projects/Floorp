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
#include "java_lang_reflect_Constructor.h"
#include "java_lang_Class.h"

#include "JavaVM.h"
#include "FieldOrMethod.h"
#include "SysCallsRuntime.h"
#include "StackWalker.h"

#include <stdio.h>

// Make sure that the class that called this method can access this field.
// Throw an IllegalAccessException if it can't.
static inline void checkCallingClass(const Method &method)
{
    Frame thisFrame;
    Method &callerMethod = thisFrame.getCallingJavaMethod();
    
    if (callerMethod.getDeclaringClass() != method.getDeclaringClass()) {
	if ((method.getModifiers() & CR_METHOD_PRIVATE)) 
	    sysThrowNamedException("java/lang/IllegalAccessException");
	else if ((method.getModifiers() & CR_METHOD_PROTECTED)) {
	    if (!method.getDeclaringClass()->isAssignableFrom(*(const Type *) callerMethod.getDeclaringClass()))
	        sysThrowNamedException("java/lang/IllegalAccessException");
	}
    }
}

static void throwConstructorException(RuntimeError err)
{
    // FIXME Need to check for illegalAccess, which we currently do not enforce
    // Also need to throw an InvocationTargetException
    switch (err.cause) {
    case RuntimeError::nullPointer:
	sysThrowNullPointerException();
	break;

    case RuntimeError::illegalArgument:
	sysThrowNamedException("java/lang/IllegalArgumentException");
	break;

    case RuntimeError::notInstantiable:
	sysThrowNamedException("java/lang/InstantiationException");
	break;

    default:
	trespass("Unknown Runtime Error in throwConstructorException()");
    }
}

extern "C" {

/* XXX Routines returning strings will have to be redone once the VM supports 
 * unicode strings directly
 */

static inline Constructor &toConstructor(Java_java_lang_reflect_Constructor *inConstr)
{
  if (!inConstr) 
    sysThrowNullPointerException();

  return *(Constructor *) inConstr;
}

/*
 * Class : java/lang/reflect/Constructor
 * Method : equals
 * Signature : (Ljava/lang/Object;)Z
 */
NS_EXPORT NS_NATIVECALL(uint32 /* bool */)
Netscape_Java_java_lang_reflect_Constructor_equals(Java_java_lang_reflect_Constructor *inConstr, 
						   Java_java_lang_Object *inObj)
{
  Constructor &constructor = toConstructor(inConstr);
  JavaObject *obj = (JavaObject *) inObj;

  return obj && constructor.equals(*obj);  
}


/*
 * Class : java/lang/reflect/Constructor
 * Method : getDeclaringClass
 * Signature : ()Ljava/lang/Class;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Class *)
Netscape_Java_java_lang_reflect_Constructor_getDeclaringClass(Java_java_lang_reflect_Constructor *inConstr)
{
  Constructor &constructor = toConstructor(inConstr);

  return ((Java_java_lang_Class *) constructor.getDeclaringClass());
}


/*
 * Class : java/lang/reflect/Constructor
 * Method : getExceptionTypes
 * Signature : ()[Ljava/lang/Class;
 */
NS_EXPORT NS_NATIVECALL(ArrayOf_Java_java_lang_Class *)
Netscape_Java_java_lang_reflect_Constructor_getExceptionTypes(Java_java_lang_reflect_Constructor *)
{
  printf("Netscape_Java_java_lang_reflect_Constructor_getExceptionTypes() not implemented\n");
  return 0;
}


/*
 * Class : java/lang/reflect/Constructor
 * Method : getModifiers
 * Signature : ()I
 */
NS_EXPORT NS_NATIVECALL(int32)
Netscape_Java_java_lang_reflect_Constructor_getModifiers(Java_java_lang_reflect_Constructor *inConstr)
{
  Constructor &constructor = toConstructor(inConstr);

  return constructor.getModifiers();
}


/*
 * Class : java/lang/reflect/Constructor
 * Method : getParameterTypes
 * Signature : ()[Ljava/lang/Class;
 */
NS_EXPORT NS_NATIVECALL(ArrayOf_Java_java_lang_Class *)
Netscape_Java_java_lang_reflect_Constructor_getParameterTypes(Java_java_lang_reflect_Constructor *inConstr)
{
  Constructor &constructor = toConstructor(inConstr);
  
  const Type **params;
  Int32 numParams;
  
  numParams = constructor.getParameterTypes(params);

  void *arr = sysNewObjectArray(&VM::getStandardClass(cClass), numParams-1);

  ArrayOf_Java_java_lang_Class *clazzArray = (ArrayOf_Java_java_lang_Class *) arr;

  // The first parameter is always ourselves
  for (Int32 i = 1; i < numParams; i++)
    clazzArray->elements[i-1] = (Java_java_lang_Class *)params[i];

  return clazzArray;
}


/*
 * Class : java/lang/reflect/Constructor
 * Method : newInstance
 * Signature : ([Ljava/lang/Object;)Ljava/lang/Object;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Object *)
Netscape_Java_java_lang_reflect_Constructor_newInstance(Java_java_lang_reflect_Constructor *inConstr, 
							ArrayOf_Java_java_lang_Object *inParams)
{
  Constructor &constructor = toConstructor(inConstr);
  
  checkCallingClass(constructor);

  JavaObject **params = (JavaObject **) inParams->elements;
  Int32 numParams = (inParams) ? inParams->length : 0;

  try {
      return (Java_java_lang_Object *) &constructor.newInstance(params, numParams);
  } catch (RuntimeError err) {
      throwConstructorException(err);
  } 
}


/*
 * Class : java/lang/reflect/Constructor
 * Method : toString
 * Signature : ()Ljava/lang/String;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_String *)
Netscape_Java_java_lang_reflect_Constructor_toString(Java_java_lang_reflect_Constructor *inConstr)
{
  Constructor &constructor = toConstructor(inConstr);

  const char *str = constructor.toString();
  
  return ((Java_java_lang_String *) &VM::intern(str));
}

}





