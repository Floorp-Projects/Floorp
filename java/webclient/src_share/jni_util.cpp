/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s): Kirk Baker <kbaker@eb.com>
 *               Ian Wilkinson <iw@ennoble.com>
 *               Mark Lin <mark.lin@eng.sun.com>
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 *               Ashutosh Kulkarni <ashuk@eng.sun.com>
 *               Ann Sunhachawee
 */

#include "jni_util.h"

#include <string.h>

JavaVM *gVm = nsnull; // declared in ns_globals.h, which is included in
                      // jni_util.h

//
// Local cache variables of JNI data items
//

static jmethodID gPropertiesInitMethodID = nsnull;
static jmethodID gPropertiesSetPropertyMethodID = nsnull;
static jmethodID gPropertiesClearMethodID = nsnull;


//
// Listener data
//

jboolean STRING_CONSTANTS_INITED = JNI_FALSE;

jobject SCREEN_X_KEY;
jobject SCREEN_Y_KEY;
jobject CLIENT_X_KEY;
jobject CLIENT_Y_KEY;
jobject ALT_KEY;
jobject CTRL_KEY;
jobject SHIFT_KEY;
jobject META_KEY;
jobject BUTTON_KEY;
jobject CLICK_COUNT_KEY;
jobject TRUE_VALUE;
jobject FALSE_VALUE;
jobject ONE_VALUE;
jobject TWO_VALUE;

jstring DOCUMENT_LOAD_LISTENER_CLASSNAME;
jstring MOUSE_LISTENER_CLASSNAME;

jlong DocumentLoader_maskValues[] = { -1L };
char * DocumentLoader_maskNames[] = {
  START_DOCUMENT_LOAD_EVENT_MASK_VALUE,
  END_DOCUMENT_LOAD_EVENT_MASK_VALUE,
  START_URL_LOAD_EVENT_MASK_VALUE,
  END_URL_LOAD_EVENT_MASK_VALUE,
  PROGRESS_URL_LOAD_EVENT_MASK_VALUE,
  STATUS_URL_LOAD_EVENT_MASK_VALUE,
  UNKNOWN_CONTENT_EVENT_MASK_VALUE,
  FETCH_INTERRUPT_EVENT_MASK_VALUE,
  nsnull
};

jlong DOMMouseListener_maskValues[] = { -1L };
char *DOMMouseListener_maskNames[] = {
    MOUSE_DOWN_EVENT_MASK_VALUE,
    MOUSE_UP_EVENT_MASK_VALUE,
    MOUSE_CLICK_EVENT_MASK_VALUE,
    MOUSE_DOUBLE_CLICK_EVENT_MASK_VALUE,
    MOUSE_OVER_EVENT_MASK_VALUE,
    MOUSE_OUT_EVENT_MASK_VALUE,
    nsnull
};

void    util_InitializeShareInitContext(void *yourInitContext)
{
    ShareInitContext *initContext = (ShareInitContext *) yourInitContext;
    initContext->propertiesClass = nsnull;
}

void    util_DeallocateShareInitContext(void *yourInitContext)
{
    // right now there is nothing to deallocate
}

jboolean util_InitStringConstants(JNIEnv *env)
{
	if (nsnull == gVm) { // declared in jni_util.h
        ::util_GetJavaVM(env, &gVm);  // save this vm reference away for the callback!
    }
    
    if (nsnull == (SCREEN_X_KEY = 
                   ::util_NewGlobalRef(env, (jobject) 
                                       ::util_NewStringUTF(env, "ScreenX")))) {
        return JNI_FALSE;
    }
    if (nsnull == (SCREEN_Y_KEY = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, "ScreenY")))) {
        return JNI_FALSE;
    }
    if (nsnull == (CLIENT_X_KEY = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, "ClientX")))) {
        return JNI_FALSE;
    }
    if (nsnull == (CLIENT_Y_KEY = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, "ClientY")))) {
        return JNI_FALSE;
    }
    if (nsnull == (ALT_KEY = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, "Alt")))) {
        return JNI_FALSE;
    }
    if (nsnull == (CTRL_KEY = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, "Ctrl")))) {
        return JNI_FALSE;
    }
    if (nsnull == (SHIFT_KEY = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, "Shift")))) {
        return JNI_FALSE;
    }
    if (nsnull == (META_KEY = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, "Meta")))) {
        return JNI_FALSE;
    }
    if (nsnull == (BUTTON_KEY = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, "Button")))) {
        return JNI_FALSE;
    }
    if (nsnull == (CLICK_COUNT_KEY = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, 
                                                           "ClickCount")))) {
        return JNI_FALSE;
    }
    if (nsnull == (TRUE_VALUE = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, "true")))) {
        return JNI_FALSE;
    }
    if (nsnull == (FALSE_VALUE = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, "false")))) {
        return JNI_FALSE;
    }
    if (nsnull == (ONE_VALUE = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, "1")))) {
        return JNI_FALSE;
    }
    if (nsnull == (TWO_VALUE = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, "2")))) {
        return JNI_FALSE;
    }
    if (nsnull == (DOCUMENT_LOAD_LISTENER_CLASSNAME = (jstring)
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, 
                                                           DOCUMENT_LOAD_LISTENER_CLASSNAME_VALUE)))) {
        return JNI_FALSE;
    }
    if (nsnull == (MOUSE_LISTENER_CLASSNAME = (jstring)
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, 
                                                           MOUSE_LISTENER_CLASSNAME_VALUE)))) {
        return JNI_FALSE;
    }

    return STRING_CONSTANTS_INITED = JNI_TRUE;
}

jboolean util_StringConstantsAreInitialized()
{
    return STRING_CONSTANTS_INITED;
}

void util_ThrowExceptionToJava (JNIEnv * env, const char * message)
{
    if (env->ExceptionOccurred()) {
      env->ExceptionClear();
    }
    jclass excCls = env->FindClass("java/lang/Exception");
    
    if (excCls == 0) { // Unable to find the exception class, give up.
	if (env->ExceptionOccurred())
	    env->ExceptionClear();
	return;
    }
    
    // Throw the exception with the error code and description
    jmethodID jID = env->GetMethodID(excCls, "<init>", "(Ljava/lang/String;)V");		// void Exception(String)
    
    if (jID != nsnull) {
	jstring	exceptionString = env->NewStringUTF(message);
	jthrowable newException = (jthrowable) env->NewObject(excCls, jID, exceptionString);
        
        if (newException != nsnull) {
	    env->Throw(newException);
	}
	else {
	    if (env->ExceptionOccurred())
		env->ExceptionClear();
	}
	} else {
	    if (env->ExceptionOccurred())
		env->ExceptionClear();
	}
} // ThrowExceptionToJava()

void util_SendEventToJava(JNIEnv *yourEnv, jobject nativeEventThread,
                          jobject webclientEventListener, 
                          jstring eventListenerClassName,
                          jlong eventType, jobject eventData)
{
#ifdef BAL_INTERFACE
    if (nsnull != externalEventOccurred) {
        externalEventOccurred(yourEnv, nativeEventThread,
                              webclientEventListener, eventType, eventData);
    }
#else
    if (nsnull == gVm) {
        return;
    }

	JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION_1_2);

    if (nsnull == env) {
      return;
    }

    jthrowable exception;

    if (nsnull != (exception = env->ExceptionOccurred())) {
        env->ExceptionDescribe();
    }

    jclass clazz = env->GetObjectClass(nativeEventThread);
    jmethodID mid = env->GetMethodID(clazz, "nativeEventOccurred", 
                                     "(Lorg/mozilla/webclient/WebclientEventListener;Ljava/lang/String;JLjava/lang/Object;)V");
    if ( mid != nsnull) {
        env->CallVoidMethod(nativeEventThread, mid, webclientEventListener,
                            eventListenerClassName,
                            eventType, eventData);
    } else {
        util_LogMessage(3, "cannot call the Java Method!\n");
    }
#endif
}

/**

 * @return: the string name of the current java thread.  Must be freed!

 */

char *util_GetCurrentThreadName(JNIEnv *env)
{
	jclass threadClass = env->FindClass("java/lang/Thread");
	jobject currentThread;
	jstring name;
	char *result = nsnull;
	const char *cstr;
	jboolean isCopy;

	if (nsnull != threadClass) {
		jmethodID currentThreadID = 
			env->GetStaticMethodID(threadClass,"currentThread",
								   "()Ljava/lang/Thread;");
		currentThread = env->CallStaticObjectMethod(threadClass, 
													currentThreadID);
		if (nsnull != currentThread) {
			jmethodID getNameID = env->GetMethodID(threadClass,
												   "getName",
												   "()Ljava/lang/String;");
			name = (jstring) env->CallObjectMethod(currentThread, getNameID, 
												   nsnull);
			result = strdup(cstr = env->GetStringUTFChars(name, &isCopy));
			if (JNI_TRUE == isCopy) {
				env->ReleaseStringUTFChars(name, cstr);
			}
		}
	}
	return result;
}

// static
void util_DumpJavaStack(JNIEnv *env)
{
	jclass threadClass = env->FindClass("java/lang/Thread");
	if (nsnull != threadClass) {
		jmethodID dumpStackID = env->GetStaticMethodID(threadClass,
													   "dumpStack",
													   "()V");
		env->CallStaticVoidMethod(threadClass, dumpStackID);
	}
}

jobject util_NewGlobalRef(JNIEnv *env, jobject obj)
{
    jobject result = nsnull;
#ifdef BAL_INTERFACE
    // PENDING(edburns): do we need to do anything here?
    result = obj;
#else
    result = env->NewGlobalRef(obj);
#endif
    return result;
}

void util_DeleteGlobalRef(JNIEnv *env, jobject obj)
{
#ifdef BAL_INTERFACE
#else
    env->DeleteGlobalRef(obj);
#endif
}

jthrowable util_ExceptionOccurred(JNIEnv *env)
{
    jthrowable result = nsnull;
#ifdef BAL_INTERFACE
#else
    result = env->ExceptionOccurred();
#endif
    return result;
}

jint util_GetJavaVM(JNIEnv *env, JavaVM **vm)
{
    int result = -1;
#ifdef BAL_INTERFACE
#else
    result = env->GetJavaVM(vm);
#endif
    return result;
}

jclass util_FindClass(JNIEnv *env, const char *fullyQualifiedClassName)
{
    jclass result = nsnull;
#ifdef BAL_INTERFACE
    result = util_GetClassMapping(fullyQualifiedClassName);
#else
    result = env->FindClass(fullyQualifiedClassName);
#endif
    return result;
}

jfieldID util_GetStaticFieldID(JNIEnv *env, jclass clazz, 
                               const char *fieldName, 
                               const char *signature)
{
    jfieldID result = nsnull;
#ifdef BAL_INTERFACE
#else
    result = env->GetStaticFieldID(clazz, fieldName, signature);
#endif
    return result;
}

jlong util_GetStaticLongField(JNIEnv *env, jclass clazz, jfieldID id)
{
    jlong result = -1;
#ifdef BAL_INTERFACE
#else
    result = env->GetStaticLongField(clazz, id);
#endif
    return result;
}

jboolean util_IsInstanceOf(JNIEnv *env, jobject obj, jclass clazz)
{
    jboolean result = JNI_FALSE;
#ifdef BAL_INTERFACE
    if (nsnull != externalInstanceOf) {
        result = externalInstanceOf(env, obj, clazz);
    }
#else
    result = env->IsInstanceOf(obj, clazz);
#endif
    return result;
}

jint util_GetIntValueFromInstance(JNIEnv *env, jobject obj,
                                  const char *fieldName)
{
    int result = -1;
#ifdef BAL_INTERFACE
#else
    jclass objClass = env->GetObjectClass(obj);
    if (nsnull == objClass) {
        util_LogMessage(3, 
                        "util_GetIntValueFromInstance: Can't get object class from instance.\n");
        return result;
    }

    jfieldID theFieldID = env->GetFieldID(objClass, fieldName, "I");
    if (nsnull == theFieldID) {
        util_LogMessage(3, 
                        "util_GetIntValueFromInstance: Can't get fieldID for fieldName.\n");
        return result;
    }

    result = env->GetIntField(obj, theFieldID);
#endif
    return result;
}

void util_SetIntValueForInstance(JNIEnv *env, jobject obj,
                                 const char *fieldName, jint newValue)
{
#ifdef BAL_INTERFACE
#else
    jclass objClass = env->GetObjectClass(obj);
    if (nsnull == objClass) {
        util_LogMessage(3, 
                        "util_SetIntValueForInstance: Can't get object class from instance.\n");
        return;
    }

    jfieldID fieldID = env->GetFieldID(objClass, fieldName, "I");
    if (nsnull == fieldID) {
        util_LogMessage(3, 
                        "util_SetIntValueForInstance: Can't get fieldID for fieldName.\n");
        return;
    }
    
    env->SetIntField(obj, fieldID, newValue);
#endif;
}

jobject util_CreatePropertiesObject(JNIEnv *env, jobject initContextObj)
{
    jobject result = nsnull;
#ifdef BAL_INTERFACE
    if (nsnull != externalCreatePropertiesObject) {
        result = externalCreatePropertiesObject(env, initContextObj);
    }
#else
    util_Assert(initContextObj);
    ShareInitContext *initContext = (ShareInitContext *) initContextObj;

    if (nsnull == initContext->propertiesClass) {
        if (nsnull == (initContext->propertiesClass =
                       ::util_FindClass(env, "java/util/Properties"))) {
            return result;
        }
    }

    if (nsnull == gPropertiesInitMethodID) {
        util_Assert(initContext->propertiesClass);
        if (nsnull == (gPropertiesInitMethodID = 
                       env->GetMethodID(initContext->propertiesClass, 
                                        "<init>", "()V"))) {
            return result;
        }
    }
    util_Assert(gPropertiesInitMethodID);
    
    result = ::util_NewGlobalRef(env, 
                                 env->NewObject(initContext->propertiesClass, 
                                                gPropertiesInitMethodID));

#endif
    return result;
}

void util_DestroyPropertiesObject(JNIEnv *env, jobject propertiesObject,
                                  jobject reserved_NotUsed)
{
#ifdef BAL_INTERFACE
    if (nsnull != externalDestroyPropertiesObject) {
        externalDestroyPropertiesObject(env, propertiesObject, 
                                        reserved_NotUsed);
    }
#else
    ::util_DeleteGlobalRef(env, propertiesObject);
#endif
}

void util_ClearPropertiesObject(JNIEnv *env, jobject propertiesObject,
                                jobject initContextObj)
{
#ifdef BAL_INTERFACE
    if (nsnull != externalClearPropertiesObject) {
        externalClearPropertiesObject(env, propertiesObject, initContextObj);
    }
#else
    util_Assert(initContextObj);
    ShareInitContext *initContext = (ShareInitContext *) initContextObj;
    
    if (nsnull == gPropertiesClearMethodID) {
        util_Assert(initContext->propertiesClass);
        if (nsnull == (gPropertiesClearMethodID = 
                       env->GetMethodID(initContext->propertiesClass, "clear", "()V"))) {
            return;
        }
    }
    util_Assert(gPropertiesClearMethodID);
    env->CallVoidMethod(propertiesObject, gPropertiesClearMethodID);
    
    return;
#endif
}

void util_StoreIntoPropertiesObject(JNIEnv *env, jobject propertiesObject,
                                    jobject name, jobject value, 
                                    jobject initContextObj)
{
#ifdef BAL_INTERFACE
    if (nsnull != externalStoreIntoPropertiesObject) {
        externalStoreIntoPropertiesObject(env, propertiesObject, name, value,
                                          initContextObj);
    }
#else
    util_Assert(initContextObj);
    ShareInitContext *initContext = (ShareInitContext *) initContextObj;
    
    if (nsnull == gPropertiesSetPropertyMethodID) {
        util_Assert(initContext->propertiesClass);
        if (nsnull == (gPropertiesSetPropertyMethodID = 
                       env->GetMethodID(initContext->propertiesClass, 
                                        "setProperty",
                                        "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/Object;"))) {
            return;
        }
    }
    util_Assert(gPropertiesSetPropertyMethodID);

    env->CallObjectMethod(propertiesObject, gPropertiesSetPropertyMethodID,
                          name, value);
                       

    
#endif
}

JNIEXPORT jvalue JNICALL
JNU_CallMethodByName(JNIEnv *env, 
					 jboolean *hasException,
					 jobject obj, 
					 const char *name,
					 const char *signature,
					 ...)
{
    jvalue result;
    va_list args;
	
    va_start(args, signature);
    result = JNU_CallMethodByNameV(env, hasException, obj, name, signature, 
								   args); 
    va_end(args);
	
    return result;    
}

JNIEXPORT jvalue JNICALL
JNU_CallMethodByNameV(JNIEnv *env, 
					  jboolean *hasException,
					  jobject obj, 
					  const char *name,
					  const char *signature, 
					  va_list args)
{
    jclass clazz;
    jmethodID mid;
    jvalue result;
    const char *p = signature;
    /* find out the return type */
    while (*p && *p != ')')
        p++;
    p++;
	
    result.i = 0;
	
    if (env->EnsureLocalCapacity(3) < 0)
        goto done2;
    clazz = env->GetObjectClass(obj);
    mid = env->GetMethodID(clazz, name, signature);
    if (mid == 0)
        goto done1;
    switch (*p) {
    case 'V':
        env->CallVoidMethodV(obj, mid, args);
		break;
    case '[':
    case 'L':
        result.l = env->CallObjectMethodV(obj, mid, args);
		break;
    case 'Z':
        result.z = env->CallBooleanMethodV(obj, mid, args);
		break;
    case 'B':
        result.b = env->CallByteMethodV(obj, mid, args);
		break;
    case 'C':
        result.c = env->CallCharMethodV(obj, mid, args);
		break;
    case 'S':
        result.s = env->CallShortMethodV(obj, mid, args);
		break;
    case 'I':
        result.i = env->CallIntMethodV(obj, mid, args);
		break;
    case 'J':
        result.j = env->CallLongMethodV(obj, mid, args);
		break;
    case 'F':
        result.f = env->CallFloatMethodV(obj, mid, args);
		break;
    case 'D':
        result.d = env->CallDoubleMethodV(obj, mid, args);
		break;
    default:
        env->FatalError("JNU_CallMethodByNameV: illegal signature");
    }
  done1:
    env->DeleteLocalRef(clazz);
  done2:
    if (hasException) {
        *hasException = env->ExceptionCheck();
    }
    return result;    
}

JNIEXPORT void * JNICALL
JNU_GetEnv(JavaVM *vm, jint version)
{
    //    void *result;
    //vm->GetEnv(&result, version);
    JNIEnv *result = nsnull;
#ifdef BAL_INTERFACE
#else
    vm->AttachCurrentThread((void **)&result, (void *) version);
#endif
    return result;
}

