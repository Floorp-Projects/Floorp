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
#include "java_lang_reflect_Method.h"
#include "java_lang_Class.h"

#include "JavaVM.h"
#include "SysCallsRuntime.h"
#include "StackWalker.h"

static inline void throwMethodException(RuntimeError err)
{
  // FIXME Need to check for illegalAccess, which we currently do not enforce
  // Also need to throw an InvocationTargetException
  if (err.cause == RuntimeError::nullPointer)
    sysThrowNullPointerException();
  else if (err.cause == RuntimeError::illegalArgument)
    sysThrowNamedException("java/lang/IllegalArgumentException");
  else
    trespass("Unknown Runtime Error in throwMethodException()");

}

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

extern "C" {

/* XXX Routines returning strings will have to be redone once the VM supports 
 * unicode strings directly
 */

static inline Method &toMethod(Java_java_lang_reflect_Method *inMethod)
{
  if (!inMethod)
    sysThrowNullPointerException();

  return *(Method *) inMethod;
}



/*
 * Class : java/lang/reflect/Method
 * Method : equals
 * Signature : (Ljava/lang/Object;)Z
 */
NS_EXPORT NS_NATIVECALL(uint32 /* bool */)
Netscape_Java_java_lang_reflect_Method_equals(Java_java_lang_reflect_Method *inMethod, 
					      Java_java_lang_Object *inObj)
{
  Method &method = toMethod(inMethod);
  JavaObject *obj = (JavaObject *) inObj;

  return obj && method.equals(*obj);  
}  


/*
 * Class : java/lang/reflect/Method
 * Method : getDeclaringClass
 * Signature : ()Ljava/lang/Class;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Class *)
Netscape_Java_java_lang_reflect_Method_getDeclaringClass(Java_java_lang_reflect_Method *inMethod)
{
  Method &method = toMethod(inMethod);
  
  return ((Java_java_lang_Class *) method.getDeclaringClass());
}


/*
 * Class : java/lang/reflect/Method
 * Method : getExceptionTypes
 * Signature : ()[Ljava/lang/Class;
 */
NS_EXPORT NS_NATIVECALL(ArrayOf_Java_java_lang_Class *)
Netscape_Java_java_lang_reflect_Method_getExceptionTypes(Java_java_lang_reflect_Method *)
{
  printf("Netscape_Java_java_lang_reflect_Method_getExceptionTypes() not implemented\n");
  return 0;
}


/*
 * Class : java/lang/reflect/Method
 * Method : getModifiers
 * Signature : ()I
 */
NS_EXPORT NS_NATIVECALL(int32)
Netscape_Java_java_lang_reflect_Method_getModifiers(Java_java_lang_reflect_Method *inMethod)
{
  Method &method = toMethod(inMethod);

  return method.getModifiers();
}



/*
 * Class : java/lang/reflect/Method
 * Method : getName
 * Signature : ()Ljava/lang/String;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_String *)
Netscape_Java_java_lang_reflect_Method_getName(Java_java_lang_reflect_Method *inMethod)
{
  Method &method = toMethod(inMethod);

  const char *name = method.getName();

  return ((Java_java_lang_String *) &VM::intern(name));
}


/*
 * Class : java/lang/reflect/Method
 * Method : getParameterTypes
 * Signature : ()[Ljava/lang/Class;
 */
NS_EXPORT NS_NATIVECALL(ArrayOf_Java_java_lang_Class *)
Netscape_Java_java_lang_reflect_Method_getParameterTypes(Java_java_lang_reflect_Method *inMethod)
{
  Method &method = toMethod(inMethod);
  const Type **params;
  Int32 numParams;

  numParams = method.getParameterTypes(params);

  void *arr = sysNewObjectArray(&VM::getStandardClass(cClass), numParams);

  ArrayOf_Java_java_lang_Class *clazzArray = (ArrayOf_Java_java_lang_Class *) arr;
  for (Int32 i = 0; i < numParams; i++)
    clazzArray->elements[i] = (Java_java_lang_Class *) params[i];

  return clazzArray;
}



/*
 * Class : java/lang/reflect/Method
 * Method : getReturnType
 * Signature : ()Ljava/lang/Class;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Class *)
Netscape_Java_java_lang_reflect_Method_getReturnType(Java_java_lang_reflect_Method *inMethod)
{
  Method &method = toMethod(inMethod);
  
  return ((Java_java_lang_Class *) &method.getReturnType());
}


/*
 * Class : java/lang/reflect/Method
 * Method : invoke
 * Signature : (Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Object *)
Netscape_Java_java_lang_reflect_Method_invoke(Java_java_lang_reflect_Method *inMethod,
					      Java_java_lang_Object *inObj, 
					      ArrayOf_Java_java_lang_Object *inParams)
{
  Method &method = toMethod(inMethod);

  checkCallingClass(method);

  if (!(method.getModifiers() & CR_METHOD_STATIC) && !inObj)
    sysThrowNullPointerException();

  JavaObject *obj = (JavaObject *) inObj;
  JavaObject **params;
  Int32 numParams;

  if (inParams) {
    params = (JavaObject **) inParams->elements;
    numParams = inParams->length;
  } else {
    JavaObject *dummy[1];
    
    // It is permissible for params to be non-null only if 
    // the method takes no arguments
    params = (JavaObject **) dummy;
    numParams = 0;
  }
  
  try {
    return ((Java_java_lang_Object *) method.invoke(obj, params, numParams));
  } catch (RuntimeError err) {
    throwMethodException(err);
  }
}


/*
 * Class : java/lang/reflect/Method
 * Method : toString
 * Signature : ()Ljava/lang/String;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_String *)
Netscape_Java_java_lang_reflect_Method_toString(Java_java_lang_reflect_Method *inMethod)
{
  Method &method = toMethod(inMethod);

  const char *str = method.toString();

  return ((Java_java_lang_String *) &VM::intern(str));
}



}



