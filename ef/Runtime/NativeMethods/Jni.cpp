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
#include "jni.h"
#include "FieldOrMethod.h"
#include "JavaVM.h"
#include "BufferedFileReader.h"
#include "SysCallsRuntime.h"

#include "prprf.h"

/* Implementation of the Java Native Interface */

/* Some explanation about how JNI works with the native method invoker.
 * What is passed in as the jobject argument in JNI methods is a pointer to a JavaObject.
 * Likewise, what is passed in as the jclass argument for static JNI methods is
 * a pointer to a Class (see JavaObject.h); a jfieldID is a Field; a jmethodID is a Method.
 * A local reference, for now, is the same as a pointer to the actual object. We may need to
 * change this, so make sure we use accessor methods to convert from objects to local-
 * references.
 */

/* Accessor methods to convert JNI-style objects to internal objects
 * and vice-versa
 */
static JavaObject *asJavaObject(jobject obj) {
    return (JavaObject *) obj;
}

static JavaObject *asJavaObject(jthrowable obj) {
    /* Make sure object is a sub-class of throwable.. */
    const Class &throwableClazz = Standard::get(cThrowable);

    if (!throwableClazz.isAssignableFrom(asClass(asJavaObject(obj)->getType())))
	return 0;
    
    return (JavaObject *) obj;
}

static Type *asJavaType(jclass clz) {
    return (Type *) clz;
}

static Class *asJavaClass(jclass clz) {
    if (!clz)
	return 0;

    Type *typ = asJavaType(clz);
    
    if (typ->typeKind != tkObject)
	return 0;

    return static_cast<Class *>(typ);
}

static Field *asJavaField(jfieldID fld) {
    return (Field *) fld;
}

static Method *asJavaMethod(jmethodID method) {
    return (Method *) method;
}

static JavaString *asJavaString(jstring obj) {
    if (!obj) return 0;

    /* Ensure that the object being passed in is a string */
    if (&asJavaObject(obj)->getType() != &asType(Standard::get(cString)))
		return 0;
    
    return (JavaString *) obj;
}

static jobject asJNIObject(JavaObject *obj) {
    return (jobject) obj;
}

static jthrowable asJNIThrowable(JavaObject *obj) {
    return (jthrowable) obj;
}

static jclass asJNIType(Type *clz) {
    return (jclass) clz;
}

static jfieldID asJNIField(Field *fld) {
    return (jfieldID) fld;
}

static jmethodID asJNIMethod(Method *m) {
    return (jmethodID) m;
}

static jstring asJNIString(JavaString *obj) {
    return (jstring) obj;
}

/* JNI State structure and JNIEnv. There is currently a global JNIState
 * and a global JNIEnv, eventually we must consider the possibility of having one of
 * these per thread
 */
JNIEnv *jniEnv = 0;

class JNIState {
    static JNIState *jniState;
    
    /* Set to true if an exception occurred in the current JNI call, or in one
     * of its callees. The exception should actually be thrown on exit from the
     * JNI call.
     */
    bool exceptionOccurred;       
    
    /* Pointer to the exception object if exceptionOccurred is true. */
    JavaObject *exceptionObject;

    /* JNI Env and JNI native function structure passed to JNI methods */
    static struct JNIEnv_ theEnv;
    static const struct JNINativeInterface_ jniNativeInterface;

    /* Initialize the JNI environment pointer passed to JNI methods. */
    static void initializeEnv() {
		theEnv.functions = &jniNativeInterface;
		jniEnv = &theEnv;
    }

public:
    JNIState() : exceptionOccurred(false), exceptionObject(0) { }
    
    /* Initialize the JNI State */
    static void staticInit() {
		jniState = new JNIState();
		initializeEnv();
    }
    
    /* Cleanup JNI subsystem */
    static void cleanup() { if (jniState) delete jniState; }
    
    /* Get the static JNIState object */
    static JNIState &getJniState() { assert(jniState); return *jniState; }
    
    /* Allocate a new object whose type is type. */
    jobject allocObject(const Type &type) {
		if (type.typeKind == tkObject)
		    return asJNIObject(&const_cast<Class *>(static_cast<const Class *>(&type))->newInstance());
		else {
		    trespass("JNI alloc of non-objects not implemented!");
		    return 0;
		}
    }
	
    /* Allocate and return a new local reference to the given object */
    jobject newLocalObject(JavaObject *obj) {
		if (!obj) return 0;

		return asJNIObject(obj);
    }

    /* Allocate and return a new global reference to the given object */
    jobject newGlobalObject(JavaObject *obj) {
		if (!obj) return 0;
	    
		return asJNIObject(obj);
    }

    /* Delete a local reference */
    void deleteLocalObject(jobject) { } 

    /* Delete a global reference */
    void deleteGlobalObject(jobject) {  }

    /* Set obj to be the current exception object, turn on exception flag.
     * It is currently an error to set an exception without handling an
     * already pending exception.
     * XXX What does the JNI spec say about this?
     */
    void setException(JavaObject *obj) {
		if (exceptionOccurred)
		    trespass("Attempt to throw exception before handling previous one");

		exceptionOccurred = true;
		exceptionObject = obj;
    }

    /* Returns true if an exception occurred */
    bool didExceptionOccur() { return exceptionOccurred; }

    /* Return the current exception object */
    JavaObject *getExceptionObject() {
	return exceptionObject;
    }

    /* Return the current exception message */
    const char *getExceptionMessage() {
		if (!exceptionOccurred)
		    return 0;
		
		return "VerifyError";
    }
  
    /* Clear the exception flags */
    void exceptionClear() {
		exceptionOccurred = false;
    }
};
    
JNIState *JNIState::jniState = 0;

/** Support functions for JNI Stubs ***/
void throwJNIExceptionIfNeeded();
void JNIInitialize();
void JNICleanup() ;

/* Check to see if there is a pending exception; if so, throw it. */
void throwJNIExceptionIfNeeded()
{
    JNIState &jniState = JNIState::getJniState();

    if (jniState.didExceptionOccur()) {
		JavaObject *obj = jniState.getExceptionObject();
		
		jniState.exceptionClear();

		if (obj)
		    sysThrow(*obj);
		else {
		    assert(0);
		}
    }
}

    
/*** Interface functions between the JNI and the VM ***/

/* Initialize the JNI sub-system */
void JNIInitialize()
{
    JNIState::staticInit();
}
   
/* Shutdown and cleanup the JNI sub-system */ 
void JNICleanup() 
{
    JNIState::cleanup();
}


/*** Utility functions used by the jni functions ***/

/* Check that type is the type of an object, and 
 * is a sub-class of java/lang/Throwable. If true,
 * return the type cast into a Class. If false, 
 * return NULL.
 */
static Class *getExceptionClass(Type *javaType)
{
    /* The type of an exception object must be a class */
    if (javaType->typeKind != tkObject)
	return 0;

    Class *javaClazz = static_cast<Class *>(javaType);
    
    /* javaClazz must be a sub-class of java/lang/Throwable */
    const Class &throwableClazz = Standard::get(cThrowable);

    if (!javaClazz->implements(throwableClazz))
	return 0;

    return javaClazz;
}

// FIX-ME
// This should go away once the full JNI implementation
// is finished.
#ifdef XP_MAC
#pragma warn_unusedarg  off
#endif

/*** The following are implementations of the JNI interface methods ***/

/* returns the version of the VM */
static JNICALL(jint) jniGetVersion(JNIEnv *env)
{
    /* XXX Need to return the right version */
    return 1;
}



/* Generates a class from the fully qualified class name and 
 * returns the class object corresponding to the class
 */
static JNICALL(jclass) jniDefineClass(JNIEnv *env, const char *name, 
		   jobject loader, 
		   const jbyte *buf, 
		   jsize len)
{
    if (!name || !buf || !len)
	return 0;

    Pool p(10);   /* This will not be used anyway */
    BufferedFileReader bf((char *) buf, len, p);
    
    ClassFileSummary *summ;

    try {
	UtfClassName utfClassName(name);
	//summ = &VM::getCentral().addClass((char *) utfClassName, bf);

    } catch (VerifyError) {
	JNIState &jniState = JNIState::getJniState();
	jniState.setException(0);
	return 0;
    }
    
    return asJNIType(summ->getThisClass());
    
}

/* If the class corresponding to the fully qualified classname 
 * name has been loaded, returns a reference to the class.
 * If not, returns null.
 */
static JNICALL(jclass) jniFindClass(JNIEnv *env, const char *name)
{
    if (!name)
      return 0;

    ClassFileSummary *summ;
    
    try {
	UtfClassName utfClassName(name);

	// XXX Need to replace this with central.findClass()
        summ = &VM::getCentral().addClass((char *) utfClassName);
    } catch (VerifyError) {
	JNIState &jniState = JNIState::getJniState();
	jniState.setException(0);
	return 0;
    }

    return asJNIType(summ->getThisClass());
}

static JNICALL(jmethodID) jniFromReflectedMethod       (JNIEnv *env, jobject method)
{
  PR_fprintf(PR_STDERR, "jnifromreflectedmethod Not implemented\n");
  return 0;
}

static JNICALL(jfieldID) jniFromReflectedField       (JNIEnv *env, jobject field)
{
  PR_fprintf(PR_STDERR, "jniFromReflectedField() Not implemented\n");
  return 0;
}


static JNICALL(jobject) jniToReflectedMethod      (JNIEnv *env, jclass cls, jmethodID methodID)
{
  PR_fprintf(PR_STDERR, "Not implemented\n");
  return 0;
}


static JNICALL(jclass) jniGetSuperclass(JNIEnv *env, jclass sub)
{
    if (!sub)
	return 0;
    
    Class *subClass = asJavaClass(sub);

    if (!subClass)
	return 0;
    
    return asJNIType(const_cast<Type *>(subClass->getSuperClass()));  
}

static JNICALL(jboolean) jniIsAssignableFrom(JNIEnv *env, jclass sub, jclass sup)
{
    if (!sub || !sup)
	return false;

    Class *subClazz = asJavaClass(sub);
    Class *supClazz = asJavaClass(sup);

    if (!subClazz || !supClazz)
	return false;

    return supClazz->isAssignableFrom(*subClazz);
}

static JNICALL (jobject) jniToReflectedField      (JNIEnv *env, jclass cls, jfieldID fieldID)
{
  PR_fprintf(PR_STDERR, "jniToReflectedField() Not implemented\n");
  return 0;
}


static JNICALL(jint) jniThrow(JNIEnv *env, jthrowable obj)
{
    if (!obj)
	return JNI_ERR;
    
    JavaObject *jobj = asJavaObject(obj);
 
    if (!getExceptionClass(const_cast<Type *>(&jobj->getType())))
	return JNI_ERR;

    JNIState &jniState = JNIState::getJniState();   
    jniState.setException(jobj);

    return JNI_OK;
}

static JNICALL(jint) jniThrowNew(JNIEnv *env, jclass clazz, const char *msg)
{
    if (!clazz)
	return JNI_ERR;

    Type *javaType = asJavaType(clazz);

    Class *javaClass = getExceptionClass(javaType);
    
    if (!javaClass)
	return JNI_ERR;

    // FIXME msg argument not passed on to exception 
    JavaObject &obj = javaClass->newInstance();

    JNIState &jniState = JNIState::getJniState();
    jniState.setException(&obj);

    return JNI_OK;
}

static JNICALL(jthrowable) jniExceptionOccurred(JNIEnv *env)
{
    JNIState &jniState = JNIState::getJniState();

    if (jniState.didExceptionOccur())
	return asJNIThrowable(jniState.getExceptionObject());
    else
	return 0;
}

static JNICALL(void) jniExceptionDescribe(JNIEnv *env)
{
    JNIState &jniState = JNIState::getJniState();

    printf("Error occurred: %s\n", jniState.getExceptionMessage());
}

static JNICALL(void) jniExceptionClear(JNIEnv *env)
{
    JNIState &jniState = JNIState::getJniState();
    jniState.exceptionClear();
}

static JNICALL(void) jniFatalError(JNIEnv *env, const char *msg)
{
    assert(false);
}

static JNICALL(jint) jniPushLocalFrame
      (JNIEnv *env, jint capacity)
{
  PR_fprintf(PR_STDERR, "jniPushLocalFrame() not implemented\n");
  return 0;
}

static JNICALL(jobject) jniPopLocalFrame
      (JNIEnv *env, jobject result)
{
  PR_fprintf(PR_STDERR, "jniPopLocalFrame() not implemented\n");
  return 0;
}


static JNICALL(jobject) jniNewGlobalRef(JNIEnv *env, jobject lobj)
{
    JNIState &jniState = JNIState::getJniState();
    return jniState.newGlobalObject(asJavaObject(lobj));
}

static JNICALL(void) jniDeleteGlobalRef(JNIEnv *env, jobject gref)
{
    JNIState &jniState = JNIState::getJniState();
    jniState.deleteGlobalObject(gref);
}

static JNICALL(void) jniDeleteLocalRef(JNIEnv *env, jobject obj)
{
    JNIState &jniState = JNIState::getJniState();
    jniState.deleteLocalObject(obj);
}

static JNICALL(jboolean) jniIsSameObject(JNIEnv *env, jobject obj1, jobject obj2)
{
    return (asJavaObject(obj1) == asJavaObject(obj2));
}

static JNICALL(jobject) jniNewLocalRef
      (JNIEnv *env, jobject ref)
{
    PR_fprintf(PR_STDERR, "jniNewLocalRef() not implemented\n");
    return 0;
}

static JNICALL(jint) jniEnsureLocalCapacity
      (JNIEnv *env, jint capacity)
{
  PR_fprintf(PR_STDERR, "jniEnsureLocalCapacity() not implemented\n");
  return 0;
}


static JNICALL(jobject) jniAllocObject(JNIEnv *env, jclass jniClazz)
{
    Class *clazz;

    if (!(clazz = asJavaClass(jniClazz)))
	return 0;

    JNIState &jniState = JNIState::getJniState();
    return jniState.allocObject(*clazz);
}

static JNICALL(jobject) jniNewObject(JNIEnv *env, jclass jniClazz, jmethodID methodID, ...)
{
    Class *clazz;

    if (!(clazz = asJavaClass(jniClazz)))
	return 0;

    Method *method = asJavaMethod(methodID);

    if (!method)
	return 0;

    /* Ensure that the method is a public constructor of the class */
    if (method->getDeclaringClass() != clazz ||
	PL_strcmp(method->getName(), "<init>") != 0 ||
	!(method->getModifiers() & CR_METHOD_STATIC))
	return 0;
    
    JavaObject &obj = clazz->newInstance();

    const Signature &sig = method->getSignature();
    
    Uint32 *argsToMethod = new Uint32[sig.nArguments*2];
    
    Uint32 nWords = 0;

    va_list args;
    va_start(args, methodID);
    
    for (Uint32 i = 0; i < sig.nArguments; i++) {
	void *rawArg = va_arg(args, void *);
	
	if (isDoublewordKind(sig.argumentTypes[i]->typeKind)) {
	    // XXX We need to keep byte order in mind here!
	    argsToMethod[nWords++] = (Uint32) rawArg;
	    argsToMethod[nWords++] = (Uint32) va_arg(args, void *);
	} else
	    argsToMethod[nWords++] = (Uint32) rawArg;
    }

    method->invokeRaw(&obj, argsToMethod, nWords);

    return asJNIObject(&obj);
}

static JNICALL(jobject) jniNewObjectV(JNIEnv *env, jclass clazz, jmethodID methodID, 
				      va_list args)
{
    PR_fprintf(PR_STDERR, "jni.NewObjectV() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jobject) jniNewObjectA(JNIEnv *env, jclass clazz, jmethodID methodID, 
		   jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.NewObjectA() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jclass) jniGetObjectClass(JNIEnv *env, jobject obj)
{
    PR_fprintf(PR_STDERR, "jni.GetObjectClass() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jboolean) jniIsInstanceOf(JNIEnv *env, jobject obj, jclass clazz)
{
    if (!obj || !clazz)
	return false;

    JavaObject *jobj = asJavaObject(obj);
    Type *type = asJavaType(clazz);

    return type->isInstance(*jobj);
}


static JNICALL(jmethodID) jniGetMethodID(JNIEnv *env, jclass clazz, const char *name, 
					 const char *sig)
{
    if (!clazz)
		return 0;
	
    Type *javaType = asJavaType(clazz);

    if (javaType->typeKind != tkObject && javaType->typeKind != tkInterface)
		return 0;

    ClassOrInterface *javaClass = static_cast<ClassOrInterface *>(javaType);

    Method *method = javaClass->getMethod(name, sig, false);
	
    if (method)
		return asJNIMethod(method);
    else if (javaClass->typeKind == tkObject) {
		for (const Class *parentClass = static_cast<Class *>(javaClass)->getParent(); parentClass; parentClass = parentClass->getParent())
		    if ((method = ((ClassOrInterface *)(parentClass))->getMethod(name, sig, true)) != NULL)
				return asJNIMethod(method);

		return 0;
    } else
		return 0;
}


static JNICALL(jobject) jniCallObjectMethod (JNIEnv *env, jobject obj, 
			  jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallObjectMethod() not implemented\n");
    assert(false);
    return 0;  
}

static JNICALL(jobject) jniCallObjectMethodV(JNIEnv *env, jobject obj, 
			  jmethodID methodID, va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallObjectMethodV() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jobject) jniCallObjectMethodA(JNIEnv *env, jobject obj, jmethodID methodID, 
			  jvalue * args)
{
    PR_fprintf(PR_STDERR, "jni.CallObjectMethodA() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jboolean) jniCallBooleanMethod      (JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallBooleanMethod() not implemented\n");
    assert(false);
    return false;
}

static JNICALL(jboolean) jniCallBooleanMethodV      (JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallBooleanMethodV() not implemented\n");
    assert(false);
    return false;
}

static JNICALL(jboolean) jniCallBooleanMethodA      (JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args)
{
    PR_fprintf(PR_STDERR, "jni.CallBooleanMethodA() not implemented\n");
    assert(false);
    return false;
}


static JNICALL(jbyte) jniCallByteMethod      (JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallByteMethod() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jbyte) jniCallByteMethodV      (JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallByteMethodV() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jbyte) jniCallByteMethodA      (JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.CallByteMethodA() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jchar) jniCallCharMethod      (JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallCharMethod() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jchar) jniCallCharMethodV      (JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallCharMethodV() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jchar) jniCallCharMethodA      (JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.CallCharMethodA() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jshort) jniCallShortMethod      (JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallShortMethod() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jshort) jniCallShortMethodV      (JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallShortMethodV() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jshort) jniCallShortMethodA      (JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.CallShortMethodA() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jint) jniCallIntMethod      (JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallIntMethod() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jint) jniCallIntMethodV      (JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallIntMethodV() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jint) jniCallIntMethodA      (JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.CallIntMethodA() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jlong) jniCallLongMethod      (JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallLongMethod() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jlong) jniCallLongMethodV      (JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallLongMethodV() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jlong) jniCallLongMethodA      (JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.CallLongMethodA() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jfloat) jniCallFloatMethod      (JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallFloatMethod() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jfloat) jniCallFloatMethodV      (JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallFloatMethodV() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jfloat) jniCallFloatMethodA      (JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.CallFloatMethodA() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jdouble) jniCallDoubleMethod      (JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallDoubleMethod() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jdouble) jniCallDoubleMethodV      (JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallDoubleMethodV() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jdouble) jniCallDoubleMethodA      (JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.CallDoubleMethodA() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(void) jniCallVoidMethod      (JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallVoidMethod() not implemented\n");
    assert(false);
}

static JNICALL(void) jniCallVoidMethodV      (JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallVoidMethodV() not implemented\n");
    assert(false);
}

static JNICALL(void) jniCallVoidMethodA      (JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args)
{
    PR_fprintf(PR_STDERR, "jni.CallVoidMethodA() not implemented\n");
    assert(false);
}


static JNICALL(jobject) jniCallNonvirtualObjectMethod      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualObjectMethod() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jobject) jniCallNonvirtualObjectMethodV      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, 
					  va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualObjectMethodV() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jobject) jniCallNonvirtualObjectMethodA      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, 
					  jvalue * args)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualObjectMethodA() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jboolean) jniCallNonvirtualBooleanMethod      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualBooleanMethod() not implemented\n");
    assert(false);
    return false;
}

static JNICALL(jboolean) jniCallNonvirtualBooleanMethodV      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID,
					    va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualBooleanMethodV() not implemented\n");
    assert(false);
    return false;
}

static JNICALL(jboolean) jniCallNonvirtualBooleanMethodA      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID,
					    jvalue * args)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualBooleanMethodA() not implemented\n");
    assert(false);
    return false;
}


static JNICALL(jbyte) jniCallNonvirtualByteMethod      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualByteMethod() not implemented\n");
    assert(false);
    return false;
}

static JNICALL(jbyte) jniCallNonvirtualByteMethodV      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID,
				      va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualByteMethodV() not implemented\n");
    assert(false);
    return false;
}

static JNICALL(jbyte) jniCallNonvirtualByteMethodA      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, 
				      jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualByteMethodA() not implemented\n");
    assert(false);
    return false;
}


static JNICALL(jchar) jniCallNonvirtualCharMethod      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualCharMethod() not implemented\n");
    assert(false);
    return false;
}

static JNICALL(jchar) jniCallNonvirtualCharMethodV      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID,
				      va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualCharMethodV() not implemented\n");
    assert(false);
    return false;
}

static JNICALL(jchar) jniCallNonvirtualCharMethodA      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID,
				      jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualCharMethodA() not implemented\n");
    assert(false);
    return false;
}


static JNICALL(jshort) jniCallNonvirtualShortMethod      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualShortMethod() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jshort) jniCallNonvirtualShortMethodV      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID,
					va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualShortMethodV() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jshort) jniCallNonvirtualShortMethodA      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID,
					jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualShortMethodA() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jint) jniCallNonvirtualIntMethod      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualIntMethod() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jint) jniCallNonvirtualIntMethodV      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID,
				    va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualIntMethodV() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jint) jniCallNonvirtualIntMethodA      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID,
				    jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualIntMethodA() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jlong) jniCallNonvirtualLongMethod      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualLongMethod() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jlong) jniCallNonvirtualLongMethodV      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID,
				      va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualLongMethodV() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jlong) jniCallNonvirtualLongMethodA      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, 
				      jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualLongMethodA() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jfloat) jniCallNonvirtualFloatMethod      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualFloatMethod() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jfloat) jniCallNonvirtualFloatMethodV      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID,
					va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualFloatMethodV() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jfloat) jniCallNonvirtualFloatMethodA      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID,
					jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualFloatMethodA() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jdouble) jniCallNonvirtualDoubleMethod      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualDoubleMethod() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jdouble) jniCallNonvirtualDoubleMethodV      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID,
					  va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualDoubleMethodV() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jdouble) jniCallNonvirtualDoubleMethodA      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID,
					  jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualDoubleMethodA() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(void) jniCallNonvirtualVoidMethod      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualVoidMethod() not implemented\n");
    assert(false);
}

static JNICALL(void) jniCallNonvirtualVoidMethodV      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID,
				     va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualVoidMethodV() not implemented\n");
    assert(false);
    
}

static JNICALL(void) jniCallNonvirtualVoidMethodA      (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID,
				     jvalue * args)
{
    PR_fprintf(PR_STDERR, "jni.CallNonvirtualVoidMethodA() not implemented\n");
    assert(false);
}


static JNICALL(jfieldID) jniGetFieldID      (JNIEnv *env, jclass clazz, const char *name, const char *sig)
{
    PR_fprintf(PR_STDERR, "jni.GetFieldID() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jobject) jniGetObjectField      (JNIEnv *env, jobject obj, jfieldID fieldID)
{
    PR_fprintf(PR_STDERR, "jni.GetObjectField() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jboolean) jniGetBooleanField      (JNIEnv *env, jobject obj, jfieldID fieldID)
{
    PR_fprintf(PR_STDERR, "jni.GetBooleanField() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jbyte) jniGetByteField      (JNIEnv *env, jobject obj, jfieldID fieldID)
{
    PR_fprintf(PR_STDERR, "jni.GetByteField() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jchar) jniGetCharField      (JNIEnv *env, jobject obj, jfieldID fieldID)
{
    PR_fprintf(PR_STDERR, "jni.GetCharField() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jshort) jniGetShortField      (JNIEnv *env, jobject obj, jfieldID fieldID)
{
    PR_fprintf(PR_STDERR, "jni.GetShortField() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jint) jniGetIntField      (JNIEnv *env, jobject obj, jfieldID fieldID)
{
    PR_fprintf(PR_STDERR, "jni.GetIntField() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jlong) jniGetLongField      (JNIEnv *env, jobject obj, jfieldID fieldID)
{
    PR_fprintf(PR_STDERR, "jni.GetLongField() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jfloat) jniGetFloatField      (JNIEnv *env, jobject obj, jfieldID fieldID)
{
    PR_fprintf(PR_STDERR, "jni.GetFloatField() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jdouble) jniGetDoubleField      (JNIEnv *env, jobject obj, jfieldID fieldID)
{
    PR_fprintf(PR_STDERR, "jni.GetDoubleField() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(void) jniSetObjectField      (JNIEnv *env, jobject obj, jfieldID fieldID, jobject val)
{
    PR_fprintf(PR_STDERR, "jni.SetObjectField() not implemented\n");
    assert(false);
}

static JNICALL(void) jniSetBooleanField      (JNIEnv *env, jobject obj, jfieldID fieldID, jboolean val)
{
    PR_fprintf(PR_STDERR, "jni.SetBooleanField() not implemented\n");
    assert(false);
}

static JNICALL(void) jniSetByteField      (JNIEnv *env, jobject obj, jfieldID fieldID, jbyte val)
{
    PR_fprintf(PR_STDERR, "jni.SetByteField() not implemented\n");
    assert(false);
}

static JNICALL(void) jniSetCharField      (JNIEnv *env, jobject obj, jfieldID fieldID, jchar val)
{
    PR_fprintf(PR_STDERR, "jni.SetCharField() not implemented\n");
    assert(false);
}

static JNICALL(void) jniSetShortField      (JNIEnv *env, jobject obj, jfieldID fieldID, jshort val)
{
    PR_fprintf(PR_STDERR, "jni.SetShortField() not implemented\n");
    assert(false);
}

static JNICALL(void) jniSetIntField      (JNIEnv *env, jobject obj, jfieldID fieldID, jint val)
{
    PR_fprintf(PR_STDERR, "jni.SetIntField() not implemented\n");
    assert(false);
}

static JNICALL(void) jniSetLongField      (JNIEnv *env, jobject obj, jfieldID fieldID, jlong val)
{
    PR_fprintf(PR_STDERR, "jni.SetLongField() not implemented\n");
    assert(false);
}

static JNICALL(void) jniSetFloatField      (JNIEnv *env, jobject obj, jfieldID fieldID, jfloat val)
{
    PR_fprintf(PR_STDERR, "jni.SetFloatField() not implemented\n");
    assert(false);
}

static JNICALL(void) jniSetDoubleField      (JNIEnv *env, jobject obj, jfieldID fieldID, jdouble val)
{
    PR_fprintf(PR_STDERR, "jni.SetDoubleField() not implemented\n");
    assert(false);
}


static JNICALL(jmethodID) jniGetStaticMethodID(JNIEnv *env, jclass clazz, const char *name, const char *sig)
{
    PR_fprintf(PR_STDERR, "jni.GetStaticMethodID() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jobject) jniCallStaticObjectMethod      (JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticObjectMethod() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jobject) jniCallStaticObjectMethodV      (JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticObjectMethodV() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jobject) jniCallStaticObjectMethodA      (JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticObjectMethodA() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jboolean) jniCallStaticBooleanMethod      (JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticBooleanMethod() not implemented\n");
    assert(false);
    return false;
}

static JNICALL(jboolean) jniCallStaticBooleanMethodV      (JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticBooleanMethodV() not implemented\n");
    assert(false);
    return false;
}

static JNICALL(jboolean) jniCallStaticBooleanMethodA      (JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticBooleanMethodA() not implemented\n");
    assert(false);
    return false;
}


static JNICALL(jbyte) jniCallStaticByteMethod      (JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticByteMethod() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jbyte) jniCallStaticByteMethodV      (JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticByteMethodV() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jbyte) jniCallStaticByteMethodA      (JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticByteMethodA() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jchar) jniCallStaticCharMethod      (JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticCharMethod() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jchar) jniCallStaticCharMethodV      (JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticCharMethodV() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jchar) jniCallStaticCharMethodA      (JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticCharMethodA() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jshort) jniCallStaticShortMethod      (JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticShortMethod() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jshort) jniCallStaticShortMethodV      (JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticShortMethodV() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jshort) jniCallStaticShortMethodA      (JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticShortMethodA() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jint) jniCallStaticIntMethod      (JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticIntMethod() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jint) jniCallStaticIntMethodV      (JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticIntMethodV() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jint) jniCallStaticIntMethodA      (JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticIntMethodA() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jlong) jniCallStaticLongMethod      (JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticLongMethod() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jlong) jniCallStaticLongMethodV      (JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticLongMethodV() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jlong) jniCallStaticLongMethodA      (JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticLongMethodA() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jfloat) jniCallStaticFloatMethod      (JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticFloatMethod() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jfloat) jniCallStaticFloatMethodV      (JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticFloatMethodV() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jfloat) jniCallStaticFloatMethodA      (JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticFloatMethodA() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jdouble) jniCallStaticDoubleMethod      (JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticDoubleMethod() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jdouble) jniCallStaticDoubleMethodV      (JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticDoubleMethodV() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jdouble) jniCallStaticDoubleMethodA      (JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticDoubleMethodA() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(void) jniCallStaticVoidMethod      (JNIEnv *env, jclass cls, jmethodID methodID, ...)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticVoidMethod() not implemented\n");
    assert(false);
}

static JNICALL(void) jniCallStaticVoidMethodV      (JNIEnv *env, jclass cls, jmethodID methodID, va_list args)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticVoidMethodV() not implemented\n");
    assert(false);
}

static JNICALL(void) jniCallStaticVoidMethodA      (JNIEnv *env, jclass cls, jmethodID methodID, jvalue * args)
{
    PR_fprintf(PR_STDERR, "jni.CallStaticVoidMethodA() not implemented\n");
    assert(false);
}


static JNICALL(jfieldID) jniGetStaticFieldID      (JNIEnv *env, jclass clazz, const char *name, const char *sig)
{
    PR_fprintf(PR_STDERR, "jni.GetStaticFieldID() not implemented\n");
    assert(false);
	return 0;
}

static JNICALL(jobject) jniGetStaticObjectField      (JNIEnv *env, jclass clazz, jfieldID fieldID)
{
    PR_fprintf(PR_STDERR, "jni.GetStaticObjectField() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jboolean) jniGetStaticBooleanField      (JNIEnv *env, jclass clazz, jfieldID fieldID)
{
    PR_fprintf(PR_STDERR, "jni.GetStaticBooleanField() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jbyte) jniGetStaticByteField      (JNIEnv *env, jclass clazz, jfieldID fieldID)
{
    PR_fprintf(PR_STDERR, "jni.GetStaticByteField() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jchar) jniGetStaticCharField      (JNIEnv *env, jclass clazz, jfieldID fieldID)
{
    PR_fprintf(PR_STDERR, "jni.GetStaticCharField() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jshort) jniGetStaticShortField      (JNIEnv *env, jclass clazz, jfieldID fieldID)
{
    PR_fprintf(PR_STDERR, "jni.GetStaticShortField() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jint) jniGetStaticIntField      (JNIEnv *env, jclass clazz, jfieldID fieldID)
{
    PR_fprintf(PR_STDERR, "jni.GetStaticIntField() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jlong) jniGetStaticLongField      (JNIEnv *env, jclass clazz, jfieldID fieldID)
{
    PR_fprintf(PR_STDERR, "jni.GetStaticLongField() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jfloat) jniGetStaticFloatField      (JNIEnv *env, jclass clazz, jfieldID fieldID)
{
    PR_fprintf(PR_STDERR, "jni.GetStaticFloatField() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jdouble) jniGetStaticDoubleField      (JNIEnv *env, jclass clazz, jfieldID fieldID)
{
    PR_fprintf(PR_STDERR, "jni.GetStaticDoubleField() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(void) jniSetStaticObjectField      (JNIEnv *env, jclass clazz, jfieldID fieldID, jobject value)
{
    PR_fprintf(PR_STDERR, "jni.SetStaticObjectField() not implemented\n");
    assert(false);
}

static JNICALL(void) jniSetStaticBooleanField      (JNIEnv *env, jclass clazz, jfieldID fieldID, jboolean value)
{
    PR_fprintf(PR_STDERR, "jni.SetStaticBooleanField() not implemented\n");
    assert(false);
}

static JNICALL(void) jniSetStaticByteField      (JNIEnv *env, jclass clazz, jfieldID fieldID, jbyte value)
{
    PR_fprintf(PR_STDERR, "jni.SetStaticByteField() not implemented\n");
    assert(false);
}

static JNICALL(void) jniSetStaticCharField      (JNIEnv *env, jclass clazz, jfieldID fieldID, jchar value)
{
    PR_fprintf(PR_STDERR, "jni.SetStaticCharField() not implemented\n");
    assert(false);
}

static JNICALL(void) jniSetStaticShortField      (JNIEnv *env, jclass clazz, jfieldID fieldID, jshort value)
{
    PR_fprintf(PR_STDERR, "jni.SetStaticShortField() not implemented\n");
    assert(false);
}

static JNICALL(void) jniSetStaticIntField      (JNIEnv *env, jclass clazz, jfieldID fieldID, jint value)
{
    PR_fprintf(PR_STDERR, "jni.SetStaticIntField() not implemented\n");
    assert(false);
}

static JNICALL(void) jniSetStaticLongField      (JNIEnv *env, jclass clazz, jfieldID fieldID, jlong value)
{
    PR_fprintf(PR_STDERR, "jni.SetStaticLongField() not implemented\n");
    assert(false);
}

static JNICALL(void) jniSetStaticFloatField      (JNIEnv *env, jclass clazz, jfieldID fieldID, jfloat value)
{
    PR_fprintf(PR_STDERR, "jni.SetStaticFloatField() not implemented\n");
    assert(false);
}

static JNICALL(void) jniSetStaticDoubleField      (JNIEnv *env, jclass clazz, jfieldID fieldID, jdouble value)
{
    PR_fprintf(PR_STDERR, "jni.SetStaticDoubleField() not implemented\n");
    assert(false);
}


static JNICALL(jstring) jniNewString      (JNIEnv *env, const jchar *unicode, jsize len)
{
    PR_fprintf(PR_STDERR, "jni.NewString() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jsize) jniGetStringLength      (JNIEnv *env, jstring str)
{
    PR_fprintf(PR_STDERR, "jni.GetStringLength() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(const jchar *) jniGetStringChars      (JNIEnv *env, jstring str, jboolean *isCopy)
{
    PR_fprintf(PR_STDERR, "jni.GetStringChars() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(void) jniReleaseStringChars      (JNIEnv *env, jstring str, const jchar *chars)
{
    PR_fprintf(PR_STDERR, "jni.ReleaseStringChars() not implemented\n");
    assert(false);
}

  
static JNICALL(jstring) jniNewStringUTF      (JNIEnv *env, const char *utf)
{
    PR_fprintf(PR_STDERR, "jni.NewStringUTF() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jsize) jniGetStringUTFLength(JNIEnv *env, jstring str)
{
    JavaString *jstr = asJavaString(str);

    if (!jstr)
	return 0;

    // XXX This returns the length of the string; not sure if
    // this is what they mean.
    return jstr->getLength();
}

static JNICALL(const char*) jniGetStringUTFChars(JNIEnv *env, jstring str, jboolean *isCopy)
{
    JavaString *jstr = asJavaString(str);

    if (!jstr) return 0;

    if (isCopy) *isCopy = true;

    return jstr->convertUtf();
}

static JNICALL(void) jniReleaseStringUTFChars(JNIEnv *env, jstring str, const char *chars)
{
    if (!chars)
	return;

    JavaString::freeUtf((char *) chars);
}

  

static JNICALL(jsize) jniGetArrayLength(JNIEnv *env, jarray array)
{
    PR_fprintf(PR_STDERR, "jni.GetArrayLength() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jobjectArray) jniNewObjectArray      (JNIEnv *env, jsize len, jclass clazz, jobject init)
{
    PR_fprintf(PR_STDERR, "jni.NewObjectArray() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jobject) jniGetObjectArrayElement      (JNIEnv *env, jobjectArray array, jsize index)
{
    PR_fprintf(PR_STDERR, "jni.GetObjectArrayElement() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(void) jniSetObjectArrayElement      (JNIEnv *env, jobjectArray array, jsize index, jobject val)
{
    PR_fprintf(PR_STDERR, "jni.SetObjectArrayElement() not implemented\n");
    assert(false);
}


static JNICALL(jbooleanArray) jniNewBooleanArray      (JNIEnv *env, jsize len)
{
    PR_fprintf(PR_STDERR, "jni.NewBooleanArray() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jbyteArray) jniNewByteArray      (JNIEnv *env, jsize len)
{
    PR_fprintf(PR_STDERR, "jni.NewByteArray() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jcharArray) jniNewCharArray      (JNIEnv *env, jsize len)
{
    PR_fprintf(PR_STDERR, "jni.NewCharArray() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jshortArray) jniNewShortArray      (JNIEnv *env, jsize len)
{
    PR_fprintf(PR_STDERR, "jni.NewShortArray() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jintArray) jniNewIntArray      (JNIEnv *env, jsize len)
{
    PR_fprintf(PR_STDERR, "jni.NewIntArray() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jlongArray) jniNewLongArray      (JNIEnv *env, jsize len)
{
    PR_fprintf(PR_STDERR, "jni.NewLongArray() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jfloatArray) jniNewFloatArray      (JNIEnv *env, jsize len)
{
    PR_fprintf(PR_STDERR, "jni.NewFloatArray() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jdoubleArray) jniNewDoubleArray      (JNIEnv *env, jsize len)
{
    PR_fprintf(PR_STDERR, "jni.NewDoubleArray() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jboolean *) jniGetBooleanArrayElements      (JNIEnv *env, jbooleanArray array, jboolean *isCopy)
{
    PR_fprintf(PR_STDERR, "jni.GetBooleanArrayElements() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jbyte *) jniGetByteArrayElements      (JNIEnv *env, jbyteArray array, jboolean *isCopy)
{
    PR_fprintf(PR_STDERR, "jni.GetByteArrayElements() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jchar *) jniGetCharArrayElements      (JNIEnv *env, jcharArray array, jboolean *isCopy)
{
    PR_fprintf(PR_STDERR, "jni.GetCharArrayElements() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jshort *) jniGetShortArrayElements      (JNIEnv *env, jshortArray array, jboolean *isCopy)
{
    PR_fprintf(PR_STDERR, "jni.GetShortArrayElements() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jint *) jniGetIntArrayElements      (JNIEnv *env, jintArray array, jboolean *isCopy)
{
    PR_fprintf(PR_STDERR, "jni.GetIntArrayElements() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jlong *) jniGetLongArrayElements      (JNIEnv *env, jlongArray array, jboolean *isCopy)
{
    PR_fprintf(PR_STDERR, "jni.GetLongArrayElements() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jfloat *) jniGetFloatArrayElements      (JNIEnv *env, jfloatArray array, jboolean *isCopy)
{
    PR_fprintf(PR_STDERR, "jni.GetFloatArrayElements() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jdouble *) jniGetDoubleArrayElements      (JNIEnv *env, jdoubleArray array, jboolean *isCopy)
{
    PR_fprintf(PR_STDERR, "jni.GetDoubleArrayElements() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(void) jniReleaseBooleanArrayElements      (JNIEnv *env, jbooleanArray array, jboolean *elems, jint mode)
{
    PR_fprintf(PR_STDERR, "jni.ReleaseBooleanArrayElements() not implemented\n");
    assert(false);
}

static JNICALL(void) jniReleaseByteArrayElements      (JNIEnv *env, jbyteArray array, jbyte *elems, jint mode)
{
    PR_fprintf(PR_STDERR, "jni.ReleaseByteArrayElements() not implemented\n");
    assert(false);
}

static JNICALL(void) jniReleaseCharArrayElements      (JNIEnv *env, jcharArray array, jchar *elems, jint mode)
{
    assert(false);
}

static JNICALL(void) jniReleaseShortArrayElements      (JNIEnv *env, jshortArray array, jshort *elems, jint mode)
{
    PR_fprintf(PR_STDERR, "jni.ReleaseShortArrayElements() not implemented\n");
    assert(false);
}

static JNICALL(void) jniReleaseIntArrayElements      (JNIEnv *env, jintArray array, jint *elems, jint mode)
{
    PR_fprintf(PR_STDERR, "jni.ReleaseIntArrayElements() not implemented\n");
    assert(false);
}

static JNICALL(void) jniReleaseLongArrayElements      (JNIEnv *env, jlongArray array, jlong *elems, jint mode)
{
    PR_fprintf(PR_STDERR, "jni.ReleaseLongArrayElements() not implemented\n");
    assert(false);
}

static JNICALL(void) jniReleaseFloatArrayElements      (JNIEnv *env, jfloatArray array, jfloat *elems, jint mode)
{
    PR_fprintf(PR_STDERR, "jni.ReleaseFloatArrayElements() not implemented\n");
    assert(false);
}

static JNICALL(void) jniReleaseDoubleArrayElements      (JNIEnv *env, jdoubleArray array, jdouble *elems, jint mode)
{
    PR_fprintf(PR_STDERR, "jni.ReleaseDoubleArrayElements() not implemented\n");
    assert(false);
}


static JNICALL(void) jniGetBooleanArrayRegion      (JNIEnv *env, jbooleanArray array, jsize start, jsize l, jboolean *buf)
{
    PR_fprintf(PR_STDERR, "jni.GetBooleanArrayRegion() not implemented\n");
    assert(false);
}

static JNICALL(void) jniGetByteArrayRegion      (JNIEnv *env, jbyteArray array, jsize start, jsize len, jbyte *buf)
{
    PR_fprintf(PR_STDERR, "jni.GetByteArrayRegion() not implemented\n");
    assert(false);
}

static JNICALL(void) jniGetCharArrayRegion      (JNIEnv *env, jcharArray array, jsize start, jsize len, jchar *buf)
{
    PR_fprintf(PR_STDERR, "jni.GetCharArrayRegion() not implemented\n");
    assert(false);
}

static JNICALL(void) jniGetShortArrayRegion      (JNIEnv *env, jshortArray array, jsize start, jsize len, jshort *buf)
{
    PR_fprintf(PR_STDERR, "jni.GetShortArrayRegion() not implemented\n");
    assert(false);
}

static JNICALL(void) jniGetIntArrayRegion      (JNIEnv *env, jintArray array, jsize start, jsize len, jint *buf)
{
    PR_fprintf(PR_STDERR, "jni.GetIntArrayRegion() not implemented\n");
    assert(false);
}

static JNICALL(void) jniGetLongArrayRegion      (JNIEnv *env, jlongArray array, jsize start, jsize len, jlong *buf)
{
    PR_fprintf(PR_STDERR, "jni.GetLongArrayRegion() not implemented\n");
    assert(false);
}

static JNICALL(void) jniGetFloatArrayRegion      (JNIEnv *env, jfloatArray array, jsize start, jsize len, jfloat *buf)
{
    PR_fprintf(PR_STDERR, "jni.GetFloatArrayRegion() not implemented\n");
    assert(false);
}

static JNICALL(void) jniGetDoubleArrayRegion      (JNIEnv *env, jdoubleArray array, jsize start, jsize len, jdouble *buf)
{
    PR_fprintf(PR_STDERR, "jni.GetDoubleArrayRegion() not implemented\n");
    assert(false);
}


static JNICALL(void) jniSetBooleanArrayRegion      (JNIEnv *env, jbooleanArray array, jsize start, jsize l, jboolean *buf)
{
    PR_fprintf(PR_STDERR, "jni.SetBooleanArrayRegion() not implemented\n");
    assert(false);
}

static JNICALL(void) jniSetByteArrayRegion      (JNIEnv *env, jbyteArray array, jsize start, jsize len, jbyte *buf)
{
    PR_fprintf(PR_STDERR, "jni.SetByteArrayRegion() not implemented\n");
    assert(false);
}

static JNICALL(void) jniSetCharArrayRegion      (JNIEnv *env, jcharArray array, jsize start, jsize len, jchar *buf)
{
    PR_fprintf(PR_STDERR, "jni.SetCharArrayRegion() not implemented\n");
    assert(false);
}

static JNICALL(void) jniSetShortArrayRegion      (JNIEnv *env, jshortArray array, jsize start, jsize len, jshort *buf)
{
    PR_fprintf(PR_STDERR, "jni.SetShortArrayRegion() not implemented\n");
    assert(false);
}

static JNICALL(void) jniSetIntArrayRegion      (JNIEnv *env, jintArray array, jsize start, jsize len, jint *buf)
{
    PR_fprintf(PR_STDERR, "jni.SetIntArrayRegion() not implemented\n");
    assert(false);
}

static JNICALL(void) jniSetLongArrayRegion      (JNIEnv *env, jlongArray array, jsize start, jsize len, jlong *buf)
{
    PR_fprintf(PR_STDERR, "jni.SetLongArrayRegion() not implemented\n");
    assert(false);
}

static JNICALL(void) jniSetFloatArrayRegion      (JNIEnv *env, jfloatArray array, jsize start, jsize len, jfloat *buf)
{
    PR_fprintf(PR_STDERR, "jni.SetFloatArrayRegion() not implemented\n");
    assert(false);
}

static JNICALL(void) jniSetDoubleArrayRegion      (JNIEnv *env, jdoubleArray array, jsize start, jsize len, jdouble *buf)
{
    PR_fprintf(PR_STDERR, "jni.SetDoubleArrayRegion() not implemented\n");
    assert(false);
}


static JNICALL(jint) jniRegisterNatives      (JNIEnv *env, jclass clazz, const JNINativeMethod *methods, 
			   jint nMethods)
{
    PR_fprintf(PR_STDERR, "jni.RegisterNatives() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jint) jniUnregisterNatives      (JNIEnv *env, jclass clazz)
{
    PR_fprintf(PR_STDERR, "jni.UnregisterNatives() not implemented\n");
    assert(false);
    return 0;
}


static JNICALL(jint) jniMonitorEnter      (JNIEnv *env, jobject obj)
{
    PR_fprintf(PR_STDERR, "jni.MonitorEnter() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(jint) jniMonitorExit      (JNIEnv *env, jobject obj)
{
    PR_fprintf(PR_STDERR, "jni.MonitorExit() not implemented\n");
    assert(false);
    return 0;
}

 
static JNICALL(jint) jniGetJavaVM      (JNIEnv *env, JavaVM **vm)
{
    PR_fprintf(PR_STDERR, "jni.GetJavaVM() not implemented\n");
    assert(false);
    return 0;
}

static JNICALL(void ) jniGetStringRegion
      (JNIEnv *env, jstring str, jsize start, jsize len, jchar *buf)
{
    PR_fprintf(PR_STDERR, "jniGetStringRegion() not implemented\n");
    assert(false);
}

static JNICALL(void) jniGetStringUTFRegion(JNIEnv *env, jstring str, jsize start, 
					    jsize len, char *buf)
{
    JavaString *jstr = asJavaString(str);
   
    if (!jstr)
	return;

    // Buf is allocated by the user.
    char *utfString = jstr->convertUtf();
    Int32 stringLength = jstr->getLength();

    if ((start+len) >= stringLength)
	return;  // What should we do here?
    
    strncpy(buf, &utfString[start], len);
    buf[start+len] = 0;
}

static JNICALL(void * ) jniGetPrimitiveArrayCritical
      (JNIEnv *env, jarray array, jboolean *isCopy)
{
    PR_fprintf(PR_STDERR, "jnigetprimitivearraycritical() not implemented\n");
    return 0;
}

static JNICALL(void ) jniReleasePrimitiveArrayCritical
      (JNIEnv *env, jarray array, void *carray, jint mode)
{
    PR_fprintf(PR_STDERR, "jnireleaseprimitivearraycritical() not implemented\n");
}

static JNICALL(const jchar * ) jniGetStringCritical
      (JNIEnv *env, jstring string, jboolean *isCopy)
{
    PR_fprintf(PR_STDERR, "jnigetstringcritical() not implemented\n");
    return 0;
}

static JNICALL(void ) jniReleaseStringCritical
      (JNIEnv *env, jstring string, const jchar *cstring)
{
    PR_fprintf(PR_STDERR, "jnireleasestringcritical() not implemented\n");
}


static JNICALL(jweak ) jniNewWeakGlobalRef
       (JNIEnv *env, jobject obj)
{
    PR_fprintf(PR_STDERR, "jninewweakglobalref() not implemented\n");
    return 0;
}

static JNICALL(void ) jniDeleteWeakGlobalRef
       (JNIEnv *env, jweak ref)
{
    PR_fprintf(PR_STDERR, "jnideleteweakglobalref() not implemented\n");
}

static JNICALL(jboolean ) jniExceptionCheck
       (JNIEnv *env)
{
    PR_fprintf(PR_STDERR, "jniexceptioncheck() not implemented\n");
    return 0;
}

const struct JNINativeInterface_ JNIState::jniNativeInterface = {
    NULL,
    NULL,
    NULL,

    NULL,

    jniGetVersion,

    jniDefineClass,
    jniFindClass,

    jniFromReflectedMethod,
    jniFromReflectedField,

    jniToReflectedMethod,

    jniGetSuperclass,
    jniIsAssignableFrom,

    jniToReflectedField,

    jniThrow,
    jniThrowNew,
    jniExceptionOccurred,
    jniExceptionDescribe,
    jniExceptionClear,
    jniFatalError,

    jniPushLocalFrame,
    jniPopLocalFrame,

    jniNewGlobalRef,
    jniDeleteGlobalRef,
    jniDeleteLocalRef,
    jniIsSameObject,

    jniNewLocalRef,
    jniEnsureLocalCapacity,

    jniAllocObject,
    jniNewObject,
    jniNewObjectV,
    jniNewObjectA,

    jniGetObjectClass,
    jniIsInstanceOf,

    jniGetMethodID,

    jniCallObjectMethod,
    jniCallObjectMethodV,
    jniCallObjectMethodA,
    jniCallBooleanMethod,
    jniCallBooleanMethodV,
    jniCallBooleanMethodA,
    jniCallByteMethod,
    jniCallByteMethodV,
    jniCallByteMethodA,
    jniCallCharMethod,
    jniCallCharMethodV,
    jniCallCharMethodA,
    jniCallShortMethod,
    jniCallShortMethodV,
    jniCallShortMethodA,
    jniCallIntMethod,
    jniCallIntMethodV,
    jniCallIntMethodA,
    jniCallLongMethod,
    jniCallLongMethodV,
    jniCallLongMethodA,
    jniCallFloatMethod,
    jniCallFloatMethodV,
    jniCallFloatMethodA,
    jniCallDoubleMethod,
    jniCallDoubleMethodV,
    jniCallDoubleMethodA,
    jniCallVoidMethod,
    jniCallVoidMethodV,
    jniCallVoidMethodA,

    jniCallNonvirtualObjectMethod,
    jniCallNonvirtualObjectMethodV,
    jniCallNonvirtualObjectMethodA,
    jniCallNonvirtualBooleanMethod,
    jniCallNonvirtualBooleanMethodV,
    jniCallNonvirtualBooleanMethodA,
    jniCallNonvirtualByteMethod,
    jniCallNonvirtualByteMethodV,
    jniCallNonvirtualByteMethodA,
    jniCallNonvirtualCharMethod,
    jniCallNonvirtualCharMethodV,
    jniCallNonvirtualCharMethodA,
    jniCallNonvirtualShortMethod,
    jniCallNonvirtualShortMethodV,
    jniCallNonvirtualShortMethodA,
    jniCallNonvirtualIntMethod,
    jniCallNonvirtualIntMethodV,
    jniCallNonvirtualIntMethodA,
    jniCallNonvirtualLongMethod,
    jniCallNonvirtualLongMethodV,
    jniCallNonvirtualLongMethodA,
    jniCallNonvirtualFloatMethod,
    jniCallNonvirtualFloatMethodV,
    jniCallNonvirtualFloatMethodA,
    jniCallNonvirtualDoubleMethod,
    jniCallNonvirtualDoubleMethodV,
    jniCallNonvirtualDoubleMethodA,
    jniCallNonvirtualVoidMethod,
    jniCallNonvirtualVoidMethodV,
    jniCallNonvirtualVoidMethodA,

    jniGetFieldID,

    jniGetObjectField,
    jniGetBooleanField,
    jniGetByteField,
    jniGetCharField,
    jniGetShortField,
    jniGetIntField,
    jniGetLongField,
    jniGetFloatField,
    jniGetDoubleField,

    jniSetObjectField,
    jniSetBooleanField,
    jniSetByteField,
    jniSetCharField,
    jniSetShortField,
    jniSetIntField,
    jniSetLongField,
    jniSetFloatField,
    jniSetDoubleField,

    jniGetStaticMethodID,

    jniCallStaticObjectMethod,
    jniCallStaticObjectMethodV,
    jniCallStaticObjectMethodA,
    jniCallStaticBooleanMethod,
    jniCallStaticBooleanMethodV,
    jniCallStaticBooleanMethodA,
    jniCallStaticByteMethod,
    jniCallStaticByteMethodV,
    jniCallStaticByteMethodA,
    jniCallStaticCharMethod,
    jniCallStaticCharMethodV,
    jniCallStaticCharMethodA,
    jniCallStaticShortMethod,
    jniCallStaticShortMethodV,
    jniCallStaticShortMethodA,
    jniCallStaticIntMethod,
    jniCallStaticIntMethodV,
    jniCallStaticIntMethodA,
    jniCallStaticLongMethod,
    jniCallStaticLongMethodV,
    jniCallStaticLongMethodA,
    jniCallStaticFloatMethod,
    jniCallStaticFloatMethodV,
    jniCallStaticFloatMethodA,
    jniCallStaticDoubleMethod,
    jniCallStaticDoubleMethodV,
    jniCallStaticDoubleMethodA,
    jniCallStaticVoidMethod,
    jniCallStaticVoidMethodV,
    jniCallStaticVoidMethodA,

    jniGetStaticFieldID,

    jniGetStaticObjectField,
    jniGetStaticBooleanField,
    jniGetStaticByteField,
    jniGetStaticCharField,
    jniGetStaticShortField,
    jniGetStaticIntField,
    jniGetStaticLongField,
    jniGetStaticFloatField,
    jniGetStaticDoubleField,

    jniSetStaticObjectField,
    jniSetStaticBooleanField,
    jniSetStaticByteField,
    jniSetStaticCharField,
    jniSetStaticShortField,
    jniSetStaticIntField,
    jniSetStaticLongField,
    jniSetStaticFloatField,
    jniSetStaticDoubleField,

    jniNewString,
    jniGetStringLength,
    jniGetStringChars,
    jniReleaseStringChars,

    jniNewStringUTF,
    jniGetStringUTFLength,
    jniGetStringUTFChars,
    jniReleaseStringUTFChars,

    jniGetArrayLength,
 
    jniNewObjectArray,
    jniGetObjectArrayElement,
    jniSetObjectArrayElement,

    jniNewBooleanArray,
    jniNewByteArray,
    jniNewCharArray,
    jniNewShortArray,
    jniNewIntArray,
    jniNewLongArray,
    jniNewFloatArray,
    jniNewDoubleArray,

    jniGetBooleanArrayElements,
    jniGetByteArrayElements,
    jniGetCharArrayElements,
    jniGetShortArrayElements,
    jniGetIntArrayElements,
    jniGetLongArrayElements,
    jniGetFloatArrayElements,
    jniGetDoubleArrayElements,

    jniReleaseBooleanArrayElements,
    jniReleaseByteArrayElements,
    jniReleaseCharArrayElements,
    jniReleaseShortArrayElements,
    jniReleaseIntArrayElements,
    jniReleaseLongArrayElements,
    jniReleaseFloatArrayElements,
    jniReleaseDoubleArrayElements,

    jniGetBooleanArrayRegion,
    jniGetByteArrayRegion,
    jniGetCharArrayRegion,
    jniGetShortArrayRegion,
    jniGetIntArrayRegion,
    jniGetLongArrayRegion,
    jniGetFloatArrayRegion,
    jniGetDoubleArrayRegion,

    jniSetBooleanArrayRegion,
    jniSetByteArrayRegion,
    jniSetCharArrayRegion,
    jniSetShortArrayRegion,
    jniSetIntArrayRegion,
    jniSetLongArrayRegion,
    jniSetFloatArrayRegion,
    jniSetDoubleArrayRegion,

    jniRegisterNatives,
    jniUnregisterNatives,

    jniMonitorEnter,
    jniMonitorExit,

    jniGetJavaVM,

    jniGetStringRegion,
    jniGetStringUTFRegion,

    jniGetPrimitiveArrayCritical,
    jniReleasePrimitiveArrayCritical,

    jniGetStringChars,
    jniReleaseStringChars,

    jniNewWeakGlobalRef,
    jniDeleteWeakGlobalRef,

    jniExceptionCheck

};


struct JNIEnv_ JNIState::theEnv;

#ifdef XP_MAC
#pragma warn_unusedarg  reset
#endif
