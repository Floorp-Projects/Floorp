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
#include "java_lang_System.h"

#include "java_io_InputStream.h"
#include "java_io_PrintStream.h"
#include "java_util_Properties.h"

#include "FieldOrMethod.h"
#include "JavaVM.h"
#include "prprf.h"
#include "prtime.h"
#include "StackWalker.h"
#include "SysCallsRuntime.h"

extern "C" {
inline Field *getField(const char *className, const char *name) 
{
  ClassCentral &central = VM::getCentral();

  ClassFileSummary &summ = central.addClass(className);
  
  Class *clazz = const_cast<Class *>(static_cast<const Class *>(summ.getThisClass()));

  Field *field = &clazz->getDeclaredField(name);

  return field;
}

inline void setStaticField(const char *name, const char *className, JavaObject &val)
{
  Field *field = getField(className, name);

  field->set(0, val);
}

/*
 * Class : java/lang/System
 * Method : setIn0
 * Signature : (Ljava/io/InputStream;)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_System_setIn0(Java_java_io_InputStream *str)
{
  setStaticField("in", "java/lang/System", *str);
}


/*
 * Class : java/lang/System
 * Method : setOut0
 * Signature : (Ljava/io/PrintStream;)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_System_setOut0(Java_java_io_PrintStream *str)
{
  setStaticField("out", "java/lang/System", *str);
}


/*
 * Class : java/lang/System
 * Method : setErr0
 * Signature : (Ljava/io/PrintStream;)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_System_setErr0(Java_java_io_PrintStream *str)
{
  setStaticField("err", "java/lang/System", *str);
}


/*
 * Class : java/lang/System
 * Method : currentTimeMillis
 * Signature : ()J
 */
NS_EXPORT NS_NATIVECALL(int64)
Netscape_Java_java_lang_System_currentTimeMillis()
{
    PRInt64 thousand, milliseconds;
    PRTime now = PR_Now();
    LL_I2L(thousand, 1000);
    LL_DIV(milliseconds, now, thousand);
    return milliseconds;
}


/*
 * Class : java/lang/System
 * Method : arraycopy
 * Signature : (Ljava/lang/Object;ILjava/lang/Object;II)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_System_arraycopy(Java_java_lang_Object *srcObj, 
					 int32 srcPos, 
					 Java_java_lang_Object *destObj, 
					 int32 destPos, int32 len)
{
	// ChkNull.
	if ((srcObj == 0) || (destObj == 0)) {
		sysThrowNullPointerException();
		assert("never come back");		// FIX-ME assert(true) 
	}

	// ChkArrayStore.
    if ((srcObj->getType().typeKind != tkArray) || (destObj->getType().typeKind != tkArray)) {
		sysThrowArrayStoreException();
		assert("never come back");		// FIX-ME assert(1) 	
	}
    TypeKind tkSource = asArray(srcObj->getType()).componentType.typeKind;
    TypeKind tkDest = asArray(destObj->getType()).componentType.typeKind;
    if (tkSource != tkDest)
        sysThrowArrayStoreException();

	JavaArray& srcArray = *reinterpret_cast<JavaArray *>(srcObj);
	JavaArray& dstArray = *reinterpret_cast<JavaArray *>(destObj);

	// ChkLimit.
	if ((len < 0) || (srcPos < 0) || (destPos < 0) || (uint32(len + srcPos) > srcArray.length) || 
		(uint32(len + destPos) > dstArray.length)) {

		sysThrowArrayIndexOutOfBoundsException();
		assert("never come back");
	}

	// Do the copy.
    int offset = arrayEltsOffset(tkSource);
	char* srcPtr = ((char *) &srcArray) + offset;
	char* destPtr = ((char *) &dstArray) + offset;
	int8 shift = getLgTypeKindSize(tkSource);

	memmove(&destPtr[destPos << shift], &srcPtr[srcPos << shift], len << shift);
}

/*
 * Class : java/lang/System
 * Method : getCallerClass
 * Signature : ()Ljava/lang/Class;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Class *)
Netscape_Java_java_lang_System_getCallerClass()
{
	Frame	thisFrame;
	Method*	callerMethod;

	thisFrame.moveToPrevFrame();		// system guard frame
	thisFrame.moveToPrevFrame();		// java frame
	callerMethod = thisFrame.getMethod();
	
	// FIX-ME should call some RuntimeException here
	if (!callerMethod)
	{
		trespass("Caller was not a Java function!!");
		return 0;
	}
	else
	{
		ClassOrInterface*	potentialClass = callerMethod->getDeclaringClass();
		assert(potentialClass);	// FIX-ME this might be guaranteed to me non-NULL
		return ((Java_java_lang_Class*)&asClass(*potentialClass));	
	}
}


/*
 * Class : java/lang/System
 * Method : identityHashCode
 * Signature : (Ljava/lang/Object;)I
 */
NS_EXPORT NS_NATIVECALL(int32)
Netscape_Java_java_lang_System_identityHashCode(Java_java_lang_Object *)
{
#ifdef DEBUG_LOG
  PR_fprintf(PR_STDERR, "Warning: System::hashCode() not implemented\n");
#endif
  return 0;
}


/*
 * Class : java/lang/System
 * Method : initProperties
 * Signature : (Ljava/util/Properties;)Ljava/util/Properties;
 */
NS_EXPORT NS_NATIVECALL(Java_java_util_Properties *)
Netscape_Java_java_lang_System_initProperties(Java_java_util_Properties *props)
{
	// FIXME: should we check the security manager for checkPropertiesAccess ?

	const Type* parameterTypesForPropertyPut[2];

	parameterTypesForPropertyPut[0] = (Type *) &VM::getStandardClass(cObject);
	parameterTypesForPropertyPut[1] = (Type *) &VM::getStandardClass(cObject);

	Method& Property_put = const_cast<Class *>(&props->getClass())->getMethod("put", parameterTypesForPropertyPut, 2);
	JavaObject* arguments[2];

#define PUTPROP(key,value) { \
	arguments[0] = JavaString::make(key); \
	arguments[1] = JavaString::make(value); \
	Property_put.invoke(props, arguments, 2); \
	}


    /* java properties */
    PUTPROP("java-vm.specification.version", "?.?");
    PUTPROP("java-vm.specification.name", "???");
    PUTPROP("java-vm.specification.vendor", "???");
    PUTPROP("java-vm.version", "?.?");
    PUTPROP("java-vm.name", "Electrical Fire");
    PUTPROP("java-vm.vendor", "???");

    PUTPROP("java.specification.version", "?.?");
    PUTPROP("java.specification.name", "???");
    PUTPROP("java.specification.vendor", "???");

    PUTPROP("java.version", "?");
    PUTPROP("java.vendor", "???");
    PUTPROP("java.vendor.url", "home.netscape.com");
    PUTPROP("java.vendor.url.bug", "home.netscape.com/bugreport.cgi");
    PUTPROP("java.class.version", "?.?");

    /* Java2D properties */
    PUTPROP("java.awt.graphicsenv", "sun.awt.GraphicsEnvironment");
    PUTPROP("java.awt.fonts", "you wanna fonts?");

    /* os properties */
	PUTPROP("os.name", "???");
	PUTPROP("os.version", "???");
	PUTPROP("os.arch", "???");

    /* file system properties */
    PUTPROP("file.separator", "/ or \\");
    PUTPROP("path.separator", ": or ;");
    PUTPROP("line.separator", "\n");

    /*
     *  user.language
     *  user.region (if user's environmant specify this)
     *  file.encoding
     *  file.encoding.pkg
     */

	PUTPROP("user.language", "???");
	PUTPROP("file.encoding", "???");
	PUTPROP("user.region", "???");
    PUTPROP("file.encoding.pkg", "sun.io");
		
	/* system */
	PUTPROP("java.library.path", "???");
    PUTPROP("java.home", "???");
    PUTPROP("java.class.path", "\ns\dist\classes");
    PUTPROP("user.name", "???");
    PUTPROP("user.home", "???");
    PUTPROP("user.timezone", "???");
    PUTPROP("user.dir", "???");
	PUTPROP("java.compiler", "???");

	return props;
}




} /* extern "C" */

