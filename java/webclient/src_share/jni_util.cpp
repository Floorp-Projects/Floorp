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
 *      Jason Mawdsley <jason@macadamian.com>
 *      Louis-Philippe Gagnon <louisphilippe@macadamian.com>
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
static jmethodID gPropertiesGetPropertyMethodID = nsnull;
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
jobject USER_NAME_KEY;
jobject PASSWORD_KEY;
jobject EDIT_FIELD_1_KEY;
jobject EDIT_FIELD_2_KEY;
jobject CHECKBOX_STATE_KEY;
jobject BUTTON_PRESSED_KEY;
jobject TRUE_VALUE;
jobject FALSE_VALUE;
jobject ONE_VALUE;
jobject TWO_VALUE;
jobject URI_VALUE;
jobject HEADERS_VALUE;
jobject MESSAGE_VALUE;
jobject BM_ADD_DATE_VALUE;
jobject BM_LAST_MODIFIED_DATE_VALUE;
jobject BM_LAST_VISIT_DATE_VALUE;
jobject BM_NAME_VALUE;
jobject BM_URL_VALUE;
jobject BM_DESCRIPTION_VALUE;
jobject BM_IS_FOLDER_VALUE;


jstring DOCUMENT_LOAD_LISTENER_CLASSNAME;
jstring MOUSE_LISTENER_CLASSNAME;
jstring NEW_WINDOW_LISTENER_CLASSNAME;

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

void    util_InitializeShareInitContext(JNIEnv *env, 
                                        void *yourInitContext)
{
    ShareInitContext *initContext = (ShareInitContext *) yourInitContext;
    initContext->propertiesClass = nsnull;
    initContext->propertiesClass = 
        ::util_FindClass(env, "java/util/Properties");
    util_Assert(initContext->propertiesClass);
}

void    util_DeallocateShareInitContext(JNIEnv *env,
                                        void *yourInitContext)
{
    // right now there is nothing to deallocate
}

jboolean util_InitStringConstants()
{
    util_Assert(gVm);
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    
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
    if (nsnull == (USER_NAME_KEY = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, 
                                                           "userName")))) {
        return JNI_FALSE;
    }
    if (nsnull == (PASSWORD_KEY =  
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, 
                                                           "password")))) {
        return JNI_FALSE;
    }
    if (nsnull == (EDIT_FIELD_1_KEY = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, 
                                                           "editfield1Value")))) {
        return JNI_FALSE;
    }
    if (nsnull == (EDIT_FIELD_2_KEY = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, 
                                                           "editfield12Value")))) {
        return JNI_FALSE;
    }
    if (nsnull == (CHECKBOX_STATE_KEY = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, 
                                                           "checkboxState")))) {
        return JNI_FALSE;
    }
    if (nsnull == (BUTTON_PRESSED_KEY = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, 
                                                           "buttonPressed")))) {
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
    if (nsnull == (URI_VALUE = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, "URI")))) {
        return JNI_FALSE;
    }
    if (nsnull == (HEADERS_VALUE = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, "headers")))) {
        return JNI_FALSE;
    }
    if (nsnull == (MESSAGE_VALUE = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, "message")))) {
        return JNI_FALSE;
    }
    if (nsnull == (BM_ADD_DATE_VALUE = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, 
                                                           BM_ADD_DATE)))) {
        return JNI_FALSE;
    }
    if (nsnull == (BM_LAST_MODIFIED_DATE_VALUE = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, 
                                                           BM_LAST_MODIFIED_DATE)))) {
        return JNI_FALSE;
    }
    if (nsnull == (BM_LAST_VISIT_DATE_VALUE = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, 
                                                           BM_LAST_VISIT_DATE)))) {
        return JNI_FALSE;
    }
    if (nsnull == (BM_NAME_VALUE = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, 
                                                           BM_NAME)))) {
        return JNI_FALSE;
    }
    if (nsnull == (BM_URL_VALUE = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, 
                                                           BM_URL)))) {
        return JNI_FALSE;
    }
    if (nsnull == (BM_DESCRIPTION_VALUE = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, 
                                                           BM_DESCRIPTION)))) {
        return JNI_FALSE;
    }
    if (nsnull == (BM_IS_FOLDER_VALUE = 
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, 
                                                           BM_IS_FOLDER)))) {
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
    if (nsnull == (NEW_WINDOW_LISTENER_CLASSNAME = (jstring)
                   ::util_NewGlobalRef(env, (jobject)
                                       ::util_NewStringUTF(env, 
                                                           NEW_WINDOW_LISTENER_CLASSNAME_VALUE)))) {
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
    jclass excCls = env->FindClass("java/lang/RuntimeException");
    
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

void util_SendEventToJava(JNIEnv *yourEnv, jobject eventRegistrationImpl,
                          jstring eventListenerClassName,
                          jlong eventType, jobject eventData)
{
#ifdef BAL_INTERFACE
    if (nsnull != externalEventOccurred) {
        externalEventOccurred(yourEnv, eventRegistrationImpl,
                              eventType, eventData);
    }
#else
    if (nsnull == gVm) {
        return;
    }

	JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

    if (nsnull == env) {
      return;
    }

    jthrowable exception;

    if (nsnull != (exception = env->ExceptionOccurred())) {
        env->ExceptionDescribe();
    }

    jclass clazz = env->GetObjectClass(eventRegistrationImpl);
    jmethodID mid = env->GetMethodID(clazz, "nativeEventOccurred", 
                                   "(Ljava/lang/String;JLjava/lang/Object;)V");
    if ( mid != nsnull) {
        env->CallVoidMethod(eventRegistrationImpl, mid,
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

void util_DeleteLocalRef(JNIEnv *env, jobject obj)
{
#ifdef BAL_INTERFACE
#else
    env->DeleteLocalRef(obj);
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


jobject util_CallStaticObjectMethodlongArg(JNIEnv *env, jclass clazz, 
                                    jmethodID methodID, jlong longArg)
{
    jobject result = nsnull;
#ifdef BAL_INTERFACE
#else
    result = env->CallStaticObjectMethod(clazz, methodID, longArg);
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
#endif
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
    jclass propertiesClass = nsnull;

    if (nsnull == (propertiesClass =
                   ::util_FindClass(env, "java/util/Properties"))) {
        return result;
    }

    if (nsnull == gPropertiesInitMethodID) {
        util_Assert(propertiesClass);
        if (nsnull == (gPropertiesInitMethodID = 
                       env->GetMethodID(propertiesClass, 
                                        "<init>", "()V"))) {
            return result;
        }
    }
    util_Assert(gPropertiesInitMethodID);
    
    result = ::util_NewGlobalRef(env, 
                                 env->NewObject(propertiesClass, 
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
    jclass propertiesClass = nsnull;

    if (nsnull == (propertiesClass =
                   ::util_FindClass(env, "java/util/Properties"))) {
        return;
    }
    
    if (nsnull == gPropertiesClearMethodID) {
        if (nsnull == (gPropertiesClearMethodID = 
                       env->GetMethodID(propertiesClass, "clear", "()V"))) {
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
    jclass propertiesClass = nsnull;

    if (nsnull == (propertiesClass =
                   ::util_FindClass(env, "java/util/Properties"))) {
        return;
    }
    
    if (nsnull == gPropertiesSetPropertyMethodID) {
        if (nsnull == (gPropertiesSetPropertyMethodID = 
                       env->GetMethodID(propertiesClass, 
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

jobject util_GetFromPropertiesObject(JNIEnv *env, jobject propertiesObject,
                                     jobject name, jobject initContextObj)
{
    jobject result = nsnull;
#ifdef BAL_INTERFACE
    if (nsnull != externalGetFromPropertiesObject) {
        result = externalGetFromPropertiesObject(env, propertiesObject, name,
                                                 initContextObj);
    }
#else
    util_Assert(initContextObj);
    ShareInitContext *initContext = (ShareInitContext *) initContextObj;
    jclass propertiesClass = nsnull;

    if (nsnull == (propertiesClass =
                   ::util_FindClass(env, "java/util/Properties"))) {
        return result;
    }
    
    if (nsnull == gPropertiesGetPropertyMethodID) {
        if (nsnull == (gPropertiesGetPropertyMethodID = 
                       env->GetMethodID(propertiesClass, 
                                        "getProperty",
                                        "(Ljava/lang/String;)Ljava/lang/String;"))) {
            return result;
        }
    }
    util_Assert(gPropertiesGetPropertyMethodID);

    result = env->CallObjectMethod(propertiesObject, 
                                   gPropertiesGetPropertyMethodID, name);
#endif
    return result;
}

jboolean util_GetBoolFromPropertiesObject(JNIEnv *env, 
                                          jobject propertiesObject,
                                          jobject name, jobject reserved)
{
    jboolean result = JNI_FALSE;
    jstring stringResult = nsnull;
    
    if (nsnull == (stringResult = (jstring) 
                   util_GetFromPropertiesObject(env, propertiesObject, 
                                                name, reserved))) {
        return result;
    }
#ifdef BAL_INTERFACE
        // PENDING(edburns): make it so there is a way to convert from
        // string to boolean
#else
    jclass clazz;
    jmethodID valueOfMethodID;
    jmethodID booleanValueMethodID;
    jobject boolInstance;
    if (nsnull == (clazz = ::util_FindClass(env, "java/lang/Boolean"))) {
        return result;
    }
    if (nsnull == (valueOfMethodID = 
                   env->GetStaticMethodID(clazz,"valueOf",
                                          "(Ljava/lang/String;)Ljava/lang/Boolean;"))) {
        return result;
    }
    if (nsnull == (boolInstance = env->CallStaticObjectMethod(clazz, 
                                                      valueOfMethodID, 
                                                      stringResult))) {
        return result;
    }
    // now call booleanValue
    if (nsnull == (booleanValueMethodID = env->GetMethodID(clazz, 
                                                           "booleanValue",
                                                           "()Z"))) {
        return result;
    }
    result = env->CallBooleanMethod(boolInstance, booleanValueMethodID);
#endif        
    
    return result;
}

jint util_GetIntFromPropertiesObject(JNIEnv *env, jobject propertiesObject,
                                     jobject name, jobject reserved)
{
    jint result = -1;
    jstring stringResult = nsnull;

    if (nsnull == (stringResult = (jstring) 
                   util_GetFromPropertiesObject(env, propertiesObject, name,
                                                reserved))) {
        return result;
    }
#ifdef BAL_INTERFACE
        // PENDING(edburns): make it so there is a way to convert from
        // string to int
#else
    jclass clazz;
    jmethodID valueOfMethodID, intValueMethodID;
    jobject integer;
    if (nsnull == (clazz = ::util_FindClass(env, "java/lang/Integer"))) {
        return result;
    }
    if (nsnull == (valueOfMethodID = 
                   env->GetStaticMethodID(clazz, "valueOf",
                                          "(Ljava/lang/String;)Ljava/lang/Integer;"))) {
        return result;
    }
    if (nsnull == (integer = env->CallStaticObjectMethod(clazz, 
                                                         valueOfMethodID, 
                                                         stringResult))) {
        return result;
    }
    // now call intValue
    if (nsnull == (intValueMethodID = env->GetMethodID(clazz, "intValue",
                                                       "()I"))) {
        return result;
    }
    result = env->CallIntMethod(integer, intValueMethodID);
#endif        

    return result;
}

void util_getSystemProperty(JNIEnv *env, 
                            const char *propName,
                            char *propValue, jint propValueLen)
{
    jstring 
        resultJstr = nsnull,
        propNameJstr = ::util_NewStringUTF(env, propName);
    jclass clazz = nsnull;
    jmethodID getPropertyMethodId;
    const char * result = nsnull;
    int i = 0;

    memset(propValue, nsnull, propValueLen);
    
    if (nsnull == (clazz = ::util_FindClass(env, "java/lang/System"))) {
        return;
    }
    
    if (nsnull == (getPropertyMethodId = 
                   env->GetStaticMethodID(clazz, "getProperty",
                                          "(Ljava/lang/String;)Ljava/lang/String;"))) {
        return;
    }
    if (nsnull == 
        (resultJstr = (jstring) env->CallStaticObjectMethod(clazz, 
                                                            getPropertyMethodId, 
                                                            propNameJstr))) {
        return;
    }
    ::util_DeleteStringUTF(env, propNameJstr);
    result = ::util_GetStringUTFChars(env, resultJstr);
    strncpy(propValue, result, propValueLen - 1);
    ::util_ReleaseStringUTFChars(env, resultJstr, result);

    return;
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
	
#ifdef JNI_VERSION_1_2

    if (env->EnsureLocalCapacity(3) < 0)
        goto done2;

#endif

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
#ifdef JNI_VERSION_1_2
        *hasException = env->ExceptionCheck();
#else
        jthrowable exception = env->ExceptionOccurred();
        if ( exception != NULL ) {
            *hasException = true;
            env->DeleteLocalRef( exception );
        }
        else {
            *hasException = false;
        }
#endif
    } // END
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

#ifdef JNI_VERSION_1_2
    vm->AttachCurrentThread((void **)&result, (void *) version);
#else
    vm->AttachCurrentThread( &result, ( void * )version);
#endif

#endif
    return result;
}

