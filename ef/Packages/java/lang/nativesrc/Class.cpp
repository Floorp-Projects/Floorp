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
#include "java_lang_Class.h"
#include "java_lang_reflect_Field.h"
#include "java_lang_reflect_Method.h"
#include "java_lang_reflect_Constructor.h"

#include "JavaVM.h"
#include "JavaString.h"

#include "prprf.h"
#include "SysCallsRuntime.h"
#include "StackWalker.h"

enum FieldOrMethodCategory {
  fieldOrMethodCategoryPublic=0,
  fieldOrMethodCategoryDeclared=1
};

static inline void throwClassVerifyException(VerifyError err) 
{
    // FIXME Need to be more specific in the exceptions we throw here
    switch (err.cause) {
    case VerifyError::noClassDefFound:
    case VerifyError::badClassFormat:
    default:
	sysThrowNamedException("java/lang/ClassNotFoundException");
	break;

    case VerifyError::illegalAccess:
	sysThrowNamedException("java/lang/IllegalAccessException");
	break;       
    }
    
}

static inline void throwClassRuntimeException(RuntimeError err) 
{
  // FIXME Need to be more specific in the exceptions we throw here
  switch (err.cause) {
  case RuntimeError::illegalAccess:
    sysThrowNamedException("java/lang/IllegalAccessException");
    break;
    
  case RuntimeError::nullPointer:
    sysThrowNullPointerException();
    break;

  default:
    sysThrowNamedException("java/lang/IllegalArgumentException");
    break;
  }
}

extern "C" {

/* Some interesting things to remember:
 * (1) What is passed to all native methods of java/lang/Class is an instance
 * of the VM class Class.
 * (2) All native routines here do not check for security restrictions; it is
 * assumed that security is dealt with at a higher level.
 */

static inline Type &toType(Java_java_lang_Class &inClass)
{
    Type &type = *(Type *) (&inClass);
    return type;
}

static inline JavaObject &toObject(Java_java_lang_Object &inObject)
{
    return *(Class *)(&inObject); 
}

/*
 * Class : java/lang/Class
 * Method : forName
 * Signature : (Ljava/lang/String;)Ljava/lang/Class;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Class *)
Netscape_Java_java_lang_Class_forName(Java_java_lang_String *nameString)
{
    if (!nameString) 
	sysThrowNamedException("java/lang/ClassNotFoundException");

    ClassCentral &central = VM::getCentral();
    char *name = ((JavaString *) nameString)->convertUtf();
    
    // Replace dots with slashes to obtain the fully qualified name of the class.
    // central.addClass() expects a fully qualified class name.
    for (char *p = name; *p; p++)
      if (*p == '.') *p = '/';

	Class *clz;
    try {
      clz = static_cast<Class *>(central.addClass(name).getThisClass());
    } catch(VerifyError err) {
	throwClassVerifyException(err);
    } catch(RuntimeError err) {
	throwClassRuntimeException(err);
    }

    /* Free the memory we just allocated */
    JavaString::freeUtf(name);

    return (Java_java_lang_Class *) clz;
}


/*
 * Class : java/lang/Class
 * Method : forName0
 * Signature : (Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Class *)
Netscape_Java_java_lang_Class_forName0(Java_java_lang_String *nameString,
                                       uint32 /* bool */,
                                       Java_java_lang_ClassLoader *)
{
    // IMPLEMENT
    return Netscape_Java_java_lang_Class_forName(nameString);
}



/*
 * Class : java/lang/Class
 * Method : newInstance0
 * Signature : ()Ljava/lang/Object;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Object *)
Netscape_Java_java_lang_Class_newInstance0(Java_java_lang_Class *inClass)
{
    Type &type = toType(*inClass);
    
    // We can't instantiate primitive types, arrays or interfaces
    if (type.typeKind != tkObject)
	sysThrowNamedException("java/lang/InstantiationException"); 
    
    const Class &clazz = asClass(type);
    
    // Check to see if the caller of the class has package access to this class
    Frame thisFrame;
    
    // Skip over our caller's frame, that of java.lang.Class.newInstance()
    Method &callerMethod = thisFrame.getCallingJavaMethod(2);
    ClassOrInterface *callingClass; 
    callingClass = callerMethod.getDeclaringClass();
    
    bool hasPackageAccess = false;
    if (callingClass->package.name == clazz.package.name)
	hasPackageAccess = true;
    
    // Check argument type. Make sure that the class that called this
    // method is a class that can access the class clazz.
    if (!(clazz.getModifiers() & CR_ACC_PUBLIC) && !hasPackageAccess)
	sysThrowNamedException("java/lang/IllegalAccessException");
    
    // Make sure that the class is instantiable. The class is
    // instantiable if it is not abstract, and has at least one constructor
    // that the calling class can access.
    if ((clazz.getModifiers() & CR_ACC_ABSTRACT) || clazz.isInterface())
      sysThrowNamedException("java/lang/InstantiationException");
    
    Constructor *constructor = NULL;
    const Constructor **declaredConstructors;
    Int32 nDeclaredConstructors;
    
    // Check that there is at least one constructor that the calling class can call
    nDeclaredConstructors = 
        const_cast<Class *>(&clazz)->getDeclaredConstructors(declaredConstructors);
    
        /* Now go through each of these declared constructors and see if we have
        * package access to any of them
        * FIXME This is horrendous, too much work. Think about not doing this much
        * work everytime
    */
    for (int i = 0; i < nDeclaredConstructors; i++) {
        constructor = const_cast<Constructor*>(declaredConstructors[i]);
        
        // Find a constructor that takes no arguments other than 'this'
        if (constructor->getSignature().nArguments > 1)
            continue;
        
        int32 modifiers = constructor->getModifiers();
        if (modifiers & CR_METHOD_PUBLIC)
            break;
        
        // Can't invoke private constructors
        if (modifiers & CR_METHOD_PRIVATE)
            continue;
        
        // See if we can invoke a constructor with package scope
        if (hasPackageAccess)
            break;
    }
    
    if (i == nDeclaredConstructors)
        sysThrowNamedException("java/lang/IllegalAccessException");

    try {
	return (Java_java_lang_Object *)&constructor->newInstance(NULL, 0);
    } catch (RuntimeError err) {
	throwClassRuntimeException(err);
    }
}


/*
 * Class : java/lang/Class
 * Method : isInstance
 * Signature : (Ljava/lang/Object;)Z
 */
NS_EXPORT NS_NATIVECALL(uint32 /* bool */)
Netscape_Java_java_lang_Class_isInstance(Java_java_lang_Class *inClass, 
					 Java_java_lang_Object *inObj)
{
    Type &type = toType(*inClass);
    if (type.isPrimitive())
        return (Uint32)false;

    if (!inObj)
        return false;

    const Class &clazz = asClass(type);
    return (Uint32) clazz.isInstance(toObject(*inObj));
}


/*
 * Class : java/lang/Class
 * Method : isAssignableFrom
 * Signature : (Ljava/lang/Class;)Z
 */
NS_EXPORT NS_NATIVECALL(uint32)  /* bool */
Netscape_Java_java_lang_Class_isAssignableFrom(Java_java_lang_Class * inClass,
					       Java_java_lang_Class * inToClass)
{
    if (!inToClass)
	sysThrowNullPointerException();

    return toType(*inClass).isAssignableFrom(toType(*inToClass));
}


/*
 * Class : java/lang/Class
 * Method : isInterface
 * Signature : ()Z
 */
NS_EXPORT NS_NATIVECALL(uint32 /* bool */)
Netscape_Java_java_lang_Class_isInterface(Java_java_lang_Class *inClass)
{
    Type &type = toType(*inClass);
    return type.isInterface();
}


/*
 * Class : java/lang/Class
 * Method : isArray
 * Signature : ()Z
 */
NS_EXPORT NS_NATIVECALL(uint32 /* bool */)
Netscape_Java_java_lang_Class_isArray(Java_java_lang_Class *inClass)
{
    Type &type = toType(*inClass);
    return type.isArray();
}


/*
 * Class : java/lang/Class
 * Method : isPrimitive
 * Signature : ()Z
 */
NS_EXPORT NS_NATIVECALL(uint32 /* bool */)
Netscape_Java_java_lang_Class_isPrimitive(Java_java_lang_Class *inClass)
{
    Type &type = toType(*inClass);
    return type.isPrimitive();
}


/*
 * Class : java/lang/Class
 * Method : getName
 * Signature : ()Ljava/lang/String;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_String *)
Netscape_Java_java_lang_Class_getName(Java_java_lang_Class *inClass)
{
    Type &type = toType(*inClass);
    const char *fullName = type.getName();
    
    JavaString &fullNameStr = VM::intern(fullName);
    return (Java_java_lang_String *) &fullNameStr;
}


/*
 * Class : java/lang/Class
 * Method : getClassLoader
 * Signature : ()Ljava/lang/ClassLoader;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_ClassLoader *)
Netscape_Java_java_lang_Class_getClassLoader(Java_java_lang_Class *)
{
#ifdef DEBUG_LOG
    PR_fprintf(PR_STDERR, "Netscape_Java_java_lang_Class_getClassLoader() not implemented");
#endif

    return 0;
}


/*
 * Class : java/lang/Class
 * Method : getSuperclass
 * Signature : ()Ljava/lang/Class;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Class *)
Netscape_Java_java_lang_Class_getSuperclass(Java_java_lang_Class *inClass)
{
    Type &type = toType(*inClass);
    const Type *superClass = type.getSuperClass();
    return (Java_java_lang_Class *) superClass;
}


/*
 * Class : java/lang/Class
 * Method : getInterfaces
 * Signature : ()[Ljava/lang/Class;
 */
NS_EXPORT NS_NATIVECALL(ArrayOf_Java_java_lang_Class *)
Netscape_Java_java_lang_Class_getInterfaces(Java_java_lang_Class *inClass)
{
  Type &type = toType(*inClass);

  if (isPrimitiveKind(type.typeKind))
    sysThrowNamedException("java/lang/IllegalAccessException");
  
  ClassOrInterface &clazz = *static_cast<ClassOrInterface *>(&type);
  Interface **interfaces;
  Int32 numInterfaces = clazz.getInterfaces(interfaces);
  
  JavaArray *arr = (JavaArray *) sysNewObjectArray(&VM::getStandardClass(cClass), numInterfaces);

  ArrayOf_Java_java_lang_Class *clazzArray = (ArrayOf_Java_java_lang_Class *) arr;
  for (Int32 i = 0; i < numInterfaces; i++)
    clazzArray->elements[i] = (Java_java_lang_Class *) interfaces[i];

  return clazzArray;
}


/*
 * Class : java/lang/Class
 * Method : getComponentType
 * Signature : ()Ljava/lang/Class;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Class *)
Netscape_Java_java_lang_Class_getComponentType(Java_java_lang_Class *inClass)
{
    Type &type = toType(*inClass);
    if (!type.isArray())
        return NULL;

    const Type &componentType = asArray(type).getComponentType();
    return (Java_java_lang_Class *) &componentType;
}


/*
 * Class : java/lang/Class
 * Method : getModifiers
 * Signature : ()I
 */
NS_EXPORT NS_NATIVECALL(int32)
Netscape_Java_java_lang_Class_getModifiers(Java_java_lang_Class *inClass)
{
    Type &type = toType(*inClass);
    return type.getModifiers();
}


/*
 * Class : java/lang/Class
 * Method : getSigners
 * Signature : ()[Ljava/lang/Object;
 */
NS_EXPORT NS_NATIVECALL(ArrayOf_Java_java_lang_Object *)
Netscape_Java_java_lang_Class_getSigners(Java_java_lang_Class *)
{
    printf("Netscape_Java_java_lang_Class_getSigners() not implemented");
    return 0;
}


/*
 * Class : java/lang/Class
 * Method : setSigners
 * Signature : ([Ljava/lang/Object;)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_Class_setSigners(Java_java_lang_Class *, ArrayOf_Java_java_lang_Object *)
{
    printf("Netscape_Java_java_lang_Class_setSigners() not implemented");
}


/*
 * Class : java/lang/Class
 * Method : getPrimitiveClass
 * Signature : (Ljava/lang/String;)Ljava/lang/Class;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Class *)
Netscape_Java_java_lang_Class_getPrimitiveClass(Java_java_lang_String *nameString)
{
    if (!nameString)
        return NULL;

    char *className = ((JavaString *) nameString)->convertUtf();
    const PrimitiveType *clazz = PrimitiveType::getClass(className);

    /* Free the memory we just allocated */
    JavaString::freeUtf(className);

    return (Java_java_lang_Class *)clazz;
}


/*
 * Class : java/lang/Class
 * Method : getFields0
 * Signature : (I)[Ljava/lang/reflect/Field;
 *
 * fieldCategory tells us what class of fields to return.
 * fieldCategory can have the values defined in enum FieldCategory,
 * defined above.
 */
NS_EXPORT NS_NATIVECALL(ArrayOf_Java_java_lang_reflect_Field *)
Netscape_Java_java_lang_Class_getFields0(Java_java_lang_Class *inClass, 
					 int32 category)
{
#if 0
    ClassOrInterface &clazz = *(ClassOrInterface *) &toType(*inClass);
    const Field **fields;
    Int32 numFields;
  
    if (category == fieldOrMethodCategoryPublic)
	numFields = clazz.getFields(fields);
    else
	numFields = clazz.getDeclaredFields(fields);
  
    const Type *classField = &VM::getStandardClass(cField);
    JavaArray *arr = (JavaArray *) sysNewObjectArray(classField, numFields);
  
    ArrayOf_Java_java_lang_reflect_Field *fieldArray = 
	(ArrayOf_Java_java_lang_reflect_Field *) arr;
  
    for (Int32 i = 0; i < numFields; i++)
	fieldArray->elements[i] = (Java_java_lang_reflect_Field *) fields[i];

    return fieldArray;
#else
    bool publik = (category == fieldOrMethodCategoryPublic);

    ClassOrInterface &clazz = *(ClassOrInterface *) &toType(*inClass);
    Int32 numFields = 0;

    if (publik) {
	Int32 i;
	
	/* Walk all interfaces that we implement */
	Int32 interfaceCount;
	Interface **interfaces;
	
	interfaceCount = clazz.getInterfaces(interfaces);
	
	for (i = 0; i < interfaceCount; i++) {
	    const Field **dummy;
	    numFields += interfaces[i]->getFields(dummy);
	}

	/* Walk our instance fields from child to parent */
	ClassOrInterface *tmp;
	for (tmp = &clazz; tmp; tmp = (ClassOrInterface *) tmp->getSuperClass()) {
	    const Field **fieldsInThisClass;
	    Int32 numFieldsInThisClass = tmp->getDeclaredFields(fieldsInThisClass);

	    for (i = 0; i < numFieldsInThisClass; i++)
		if (fieldsInThisClass[i]->getModifiers() & CR_FIELD_PUBLIC)
		    numFields++;
	}

	/* Allocate a JavaArray of the correct size */
	const Type *classField = &VM::getStandardClass(cField);
	JavaArray *arr = (JavaArray *) sysNewObjectArray(classField, numFields);
	
	ArrayOf_Java_java_lang_reflect_Field *fieldArray = 
	    (ArrayOf_Java_java_lang_reflect_Field *) arr;
	
	Int32 index = 0;
	
	/* Walk the fields again, this time copying them into our array */
	for (i = 0; i < interfaceCount; i++) {
	    const Field **interfaceFields;
	    Int32 numInterfaceFields = interfaces[i]->getFields(interfaceFields);
	    
	    for (Int32 j = 0; j < numInterfaceFields; j++)
		fieldArray->elements[index++] = (Java_java_lang_reflect_Field *) interfaceFields[j];
	}

	/* Add instance fields, starting with the topmost superclass */
	Vector<ClassOrInterface *> vec;
	for (tmp = &clazz; tmp; tmp = (ClassOrInterface *)(tmp->getSuperClass()))
	    vec.append(tmp);

	for (Int32 vectorIndex = vec.size(); vectorIndex > 0; vectorIndex--) {
	    tmp = vec[vectorIndex-1];
	    const Field **fieldsInThisClass;
	    Int32 numFieldsInThisClass = tmp->getDeclaredFields(fieldsInThisClass);
	    
	    for (i = 0; i < numFieldsInThisClass; i++)
		if (fieldsInThisClass[i]->getModifiers() & CR_FIELD_PUBLIC)
		    fieldArray->elements[index++] = (Java_java_lang_reflect_Field *) fieldsInThisClass[i];
	    
	}

	assert(index == numFields);

	return fieldArray;
    } else {
		const Field **fields;
	numFields = clazz.getDeclaredFields(fields);
	
	const Type *classField = &VM::getStandardClass(cField);
	JavaArray *arr = (JavaArray *) sysNewObjectArray(classField, numFields);
	
	ArrayOf_Java_java_lang_reflect_Field *fieldArray = 
	    (ArrayOf_Java_java_lang_reflect_Field *) arr;
	
	for (Int32 i = 0; i < numFields; i++)
	    fieldArray->elements[i] = (Java_java_lang_reflect_Field *) fields[i];
	
	return fieldArray;
    }
#endif
}


/*
 * Class : java/lang/Class
 * Method : getMethods0
 * Signature : (I)[Ljava/lang/reflect/Method;
 */
NS_EXPORT NS_NATIVECALL(ArrayOf_Java_java_lang_reflect_Method *)
Netscape_Java_java_lang_Class_getMethods0(Java_java_lang_Class *inClass, 
					  int32 category)
{
  ClassOrInterface &clazz = *(ClassOrInterface *) &toType(*inClass);
  const Method **methods;
  Int32 numMethods;
  
  if (category == fieldOrMethodCategoryPublic)
    numMethods = clazz.getMethods(methods);
  else
    numMethods = clazz.getDeclaredMethods(methods);
  
  const Type *classMethod = &VM::getStandardClass(cMethod);
  JavaArray *arr = (JavaArray *) sysNewObjectArray(classMethod, numMethods);
  
  ArrayOf_Java_java_lang_reflect_Method *methodArray = 
    (ArrayOf_Java_java_lang_reflect_Method *) arr;
  
  for (Int32 i = 0; i < numMethods; i++)
    methodArray->elements[i] = (Java_java_lang_reflect_Method *) methods[i];

  return methodArray;
  
}


/*
 * Class : java/lang/Class
 * Method : getConstructors0
 * Signature : (I)[Ljava/lang/reflect/Constructor;
 */
NS_EXPORT NS_NATIVECALL(ArrayOf_Java_java_lang_reflect_Constructor *)
Netscape_Java_java_lang_Class_getConstructors0(Java_java_lang_Class *inClass, 
					       int32 category)
{
  ClassOrInterface &clazz = *(ClassOrInterface *) &toType(*inClass);
  const Constructor **constructors;
  Int32 numConstructors;
  
  if (category == fieldOrMethodCategoryPublic)
    numConstructors = clazz.getConstructors(constructors);
  else
    numConstructors = clazz.getDeclaredConstructors(constructors);
  
  const Type *classConstructor = &VM::getStandardClass(cConstructor);
  JavaArray *arr = (JavaArray *) sysNewObjectArray(classConstructor, numConstructors);
  
  ArrayOf_Java_java_lang_reflect_Constructor *constructorArray = 
    (ArrayOf_Java_java_lang_reflect_Constructor *) arr;
  
  for (Int32 i = 0; i < numConstructors; i++)
    constructorArray->elements[i] = (Java_java_lang_reflect_Constructor *) constructors[i];

  return constructorArray;
}


/*
 * Class : java/lang/Class
 * Method : getField0
 * Signature : (Ljava/lang/String;I)Ljava/lang/reflect/Field;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_reflect_Field *)
Netscape_Java_java_lang_Class_getField0(Java_java_lang_Class *inClass, 
					Java_java_lang_String *inName, 
					int32 category)
{
    if (!inName || !inClass)
		sysThrowNullPointerException();
  
    char *name = ((JavaString *) inName)->convertUtf();
    ClassOrInterface &clazz = *(ClassOrInterface *) &toType(*inClass);

    const Field *field;
    try {
	if (category == fieldOrMethodCategoryPublic)
	    field = &clazz.getField(name);
	else
	    field = &clazz.getDeclaredField(name);
    } catch (RuntimeError) {
	sysThrowNamedException("java/lang/NoSuchFieldException");
    }
    
    JavaString::freeUtf(name);
    return (Java_java_lang_reflect_Field *)field;
}


/*
 * Class : java/lang/Class
 * Method : getMethod0
 * Signature : (Ljava/lang/String;[Ljava/lang/Class;I)Ljava/lang/reflect/Method;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_reflect_Method *)
Netscape_Java_java_lang_Class_getMethod0(Java_java_lang_Class *inClass, 
					 Java_java_lang_String *inName, 
					 ArrayOf_Java_java_lang_Class *paramTypes, 
					 int32 category)
{
    if (!inName)
      sysThrowNullPointerException();
  
    Int32 numParams;
    const Type **params;

    if (!paramTypes) {
      Type *dummy[1];

      params = (const Type **) dummy;
      numParams = 0;
    } else {
      params = (const Type **) paramTypes->elements;
      numParams = paramTypes->length;
    }

    ClassOrInterface &clazz = *(ClassOrInterface *) &toType(*inClass);
    
    char *name = ((JavaString *) inName)->convertUtf();
        
    const Method *method;
    
    try {
	if (category == fieldOrMethodCategoryPublic)
	    method = &clazz.getMethod(name, params, numParams);
	else
	    method = &clazz.getDeclaredMethod(name, params, numParams);
    } catch (RuntimeError) {
	sysThrowNamedException("java/lang/NoSuchMethodException");
    }
	
    JavaString::freeUtf(name);
    return (Java_java_lang_reflect_Method *) method;
}


/*
 * Class : java/lang/Class
 * Method : getConstructor0
 * Signature : ([Ljava/lang/Class;I)Ljava/lang/reflect/Constructor;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_reflect_Constructor *)
Netscape_Java_java_lang_Class_getConstructor0(Java_java_lang_Class *inClass, 
					      ArrayOf_Java_java_lang_Class *paramTypes,
					      int32 category)
{
  ClassOrInterface &clazz = *(ClassOrInterface *) &toType(*inClass);
  
  const Type **params;
  Uint32 nParams;

  if (paramTypes == 0) {
	  params = (const Type **) 0;
	  nParams = 0;
  } else {
	  params = (const Type **) paramTypes->elements;
	  nParams = paramTypes->length;
  }

  Constructor *constructor;

  try {
      if (category == fieldOrMethodCategoryPublic)
	  constructor = &clazz.getConstructor(params, nParams);
      else
	  constructor = &clazz.getDeclaredConstructor(params, nParams);
  } catch (RuntimeError) {
      sysThrowNamedException("java/lang/NoSuchMethodException");
  }

  return (Java_java_lang_reflect_Constructor *) constructor;
}

} /* extern "C" */

