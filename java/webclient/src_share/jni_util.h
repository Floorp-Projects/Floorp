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


/**

 * Util methods

 */

#ifndef jni_util_h
#define jni_util_h

#include <jni.h>


//    
// added for 1.1.x compatibility
//
#ifdef JNI_VERSION_1_2

#ifndef JNI_VERSION
#define JNI_VERSION JNI_VERSION_1_2
#endif

#else

#ifndef JNI_VERSION_1_1
#define JNI_VERSION_1_1 0x00010001 
#endif

#ifndef JNI_VERSION
#define JNI_VERSION JNI_VERSION_1_1
#endif

#endif // END: JNI_VERSION_1_2

//
// String constants, defined in jni_util.cpp
//

extern jobject SCREEN_X_KEY;
extern jobject SCREEN_Y_KEY;
extern jobject CLIENT_X_KEY;
extern jobject CLIENT_Y_KEY;
extern jobject ALT_KEY;
extern jobject CTRL_KEY;
extern jobject SHIFT_KEY;
extern jobject META_KEY;
extern jobject BUTTON_KEY;
extern jobject CLICK_COUNT_KEY;
extern jobject USER_NAME_KEY;
extern jobject PASSWORD_KEY;
extern jobject EDIT_FIELD_1_KEY;
extern jobject EDIT_FIELD_2_KEY;
extern jobject CHECKBOX_STATE_KEY;
extern jobject BUTTON_PRESSED_KEY;
extern jobject TRUE_VALUE;
extern jobject FALSE_VALUE;
extern jobject ONE_VALUE;
extern jobject TWO_VALUE;
extern jobject BM_ADD_DATE_VALUE;
extern jobject BM_LAST_MODIFIED_DATE_VALUE;
extern jobject BM_LAST_VISIT_DATE_VALUE;
extern jobject BM_NAME_VALUE;
extern jobject BM_URL_VALUE;
extern jobject BM_DESCRIPTION_VALUE;
extern jobject BM_IS_FOLDER_VALUE;


/**

 * How to create a new listener type on the native side: <P>

 * 1. add an entry in the gSupportedListenerInterfaces array defined in
 * ns_util.cpp or ie_util.cpp <P>

 * 2. add a corresponding entry in the LISTENER_CLASSES enum in
 * jni_util.h <P>

 * 3. add a jstring to the string constant list in
 * jni_util.cpp, below.

 * 4. Initialize this jstring constant in jni_util.cpp
 * util_InitStringConstants() <P>

 * 5. add an entry to the switch statement in NativeEventThread.cpp
 * native{add,remove}Listener <P>

 */

/**

 * We need one of these for each of the classes in
 * gSupportedListenerInterfaces, defined in ns_util.cpp

 */
extern jstring DOCUMENT_LOAD_LISTENER_CLASSNAME;
extern jstring MOUSE_LISTENER_CLASSNAME;
extern jstring NEW_WINDOW_LISTENER_CLASSNAME;


// 
// Listener support
//

// these index into the gSupportedListenerInterfaces array

typedef enum {
    DOCUMENT_LOAD_LISTENER = 0,
    MOUSE_LISTENER,
    NEW_WINDOW_LISTENER,
    LISTENER_NOT_FOUND
} LISTENER_CLASSES;

// listener names and values, tightly correspond to java listener interfaces

#define DOCUMENT_LOAD_LISTENER_CLASSNAME_VALUE "org.mozilla.webclient.DocumentLoadListener"
#define MOUSE_LISTENER_CLASSNAME_VALUE "java.awt.event.MouseListener"
#define NEW_WINDOW_LISTENER_CLASSNAME_VALUE "org.mozilla.webclient.NewWindowListener"


#define START_DOCUMENT_LOAD_EVENT_MASK_VALUE "START_DOCUMENT_LOAD_EVENT_MASK"
#define END_DOCUMENT_LOAD_EVENT_MASK_VALUE "END_DOCUMENT_LOAD_EVENT_MASK"
#define START_URL_LOAD_EVENT_MASK_VALUE "START_URL_LOAD_EVENT_MASK"
#define END_URL_LOAD_EVENT_MASK_VALUE "END_URL_LOAD_EVENT_MASK"
#define PROGRESS_URL_LOAD_EVENT_MASK_VALUE "PROGRESS_URL_LOAD_EVENT_MASK"
#define STATUS_URL_LOAD_EVENT_MASK_VALUE "STATUS_URL_LOAD_EVENT_MASK"
#define UNKNOWN_CONTENT_EVENT_MASK_VALUE "UNKNOWN_CONTENT_EVENT_MASK"
#define FETCH_INTERRUPT_EVENT_MASK_VALUE "FETCH_INTERRUPT_EVENT_MASK"

#define MOUSE_DOWN_EVENT_MASK_VALUE "MOUSE_DOWN_EVENT_MASK"
#define MOUSE_UP_EVENT_MASK_VALUE "MOUSE_UP_EVENT_MASK"
#define MOUSE_CLICK_EVENT_MASK_VALUE "MOUSE_CLICK_EVENT_MASK"
#define MOUSE_DOUBLE_CLICK_EVENT_MASK_VALUE "MOUSE_DOUBLE_CLICK_EVENT_MASK"
#define MOUSE_OVER_EVENT_MASK_VALUE "MOUSE_OVER_EVENT_MASK"
#define MOUSE_OUT_EVENT_MASK_VALUE "MOUSE_OUT_EVENT_MASK"

typedef enum {
  START_DOCUMENT_LOAD_EVENT_MASK = 0,
  END_DOCUMENT_LOAD_EVENT_MASK,
  START_URL_LOAD_EVENT_MASK,
  END_URL_LOAD_EVENT_MASK,
  PROGRESS_URL_LOAD_EVENT_MASK,
  STATUS_URL_LOAD_EVENT_MASK,
  UNKNOWN_CONTENT_EVENT_MASK,
  FETCH_INTERRUPT_EVENT_MASK,
  NUMBER_OF_DOCUMENT_LOADER_MASK_NAMES
} DOCUMENT_LOADER_EVENT_MASK_NAMES;

typedef enum {
  MOUSE_DOWN_EVENT_MASK = 0,
  MOUSE_UP_EVENT_MASK,
  MOUSE_CLICK_EVENT_MASK,
  MOUSE_DOUBLE_CLICK_EVENT_MASK,
  MOUSE_OVER_EVENT_MASK,
  MOUSE_OUT_EVENT_MASK,
  NUMBER_OF_DOM_MOUSE_LISTENER_MASK_NAMES
} DOM_MOUSE_LISTENER_EVENT_MASK_NAMES;

// BookmarkEntry string constants, must coincide with java
#define BM_ADD_DATE "AddDate"
#define BM_LAST_MODIFIED_DATE "LastModifiedDate"
#define BM_LAST_VISIT_DATE "LastVisitDate"
#define BM_NAME "Name"
#define BM_URL "URL"
#define BM_DESCRIPTION "Description"
#define BM_IS_FOLDER "IsFolder"

extern jlong DocumentLoader_maskValues [NUMBER_OF_DOCUMENT_LOADER_MASK_NAMES];
extern char *DocumentLoader_maskNames [NUMBER_OF_DOCUMENT_LOADER_MASK_NAMES + 1];

extern jlong DOMMouseListener_maskValues [NUMBER_OF_DOM_MOUSE_LISTENER_MASK_NAMES];
extern char *DOMMouseListener_maskNames [NUMBER_OF_DOM_MOUSE_LISTENER_MASK_NAMES + 1];

/**

 * This struct contains all non browser specific per-window data.  The
 * WebShellInitContext struct, in the src_moz and src_ie directories
 * contains a pointer to a ShareInitContext struct.

 */

struct ShareInitContext {
    jclass propertiesClass;
};

extern JavaVM *gVm; // defined in jni_util.cpp

#ifndef nsnull
#define nsnull 0
#endif

void    util_InitializeShareInitContext(JNIEnv *env, void *initContext);
void    util_DeallocateShareInitContext(JNIEnv *env, void *initContext);

/**

 * Initialize the above extern jobject string constants as jstrings

 */ 

jboolean util_InitStringConstants();
jboolean util_StringConstantsAreInitialized();

void    util_ThrowExceptionToJava (JNIEnv * env, const char * message);

/**

 * simply call the java method nativeEventOccurred on
 * eventRegistrationImpl, passing webclientEventListener and eventType
 * as arguments.

 */

void    util_SendEventToJava(JNIEnv *env, jobject eventRegistrationImpl,
                             jstring eventListenerClassName,
                             jlong eventType,
                             jobject eventData);

char *util_GetCurrentThreadName(JNIEnv *env);

void util_DumpJavaStack(JNIEnv *env);

//
// Functions to wrap JNIEnv functions.
//

#include "jni_util_export.h"

jobject util_NewGlobalRef(JNIEnv *env, jobject toAddRef);

void util_DeleteGlobalRef(JNIEnv *env, jobject toDeleteRef);

void util_DeleteLocalRef(JNIEnv *env, jobject toDeleteRef);

jthrowable util_ExceptionOccurred(JNIEnv *env);

jint util_GetJavaVM(JNIEnv *env, JavaVM **vm);

jclass util_FindClass(JNIEnv *env, const char *fullyQualifiedClassName);

jobject util_CallStaticObjectMethodlongArg(JNIEnv *env, jclass clazz, 
                                    jmethodID methodID, jlong longArg);

jfieldID util_GetStaticFieldID(JNIEnv *env, jclass clazz, 
                               const char *fieldName, 
                               const char *signature);

jlong util_GetStaticLongField(JNIEnv *env, jclass clazz, jfieldID id);

jboolean util_IsInstanceOf(JNIEnv *env, jobject obj, jclass clazz);

jint util_GetIntValueFromInstance(JNIEnv *env, jobject instance,
                                  const char *fieldName);

void util_SetIntValueForInstance(JNIEnv *env, jobject instance,
                                 const char *fieldName, jint newValue);

/**

 * A JNI wrapper to create a java.util.Properties object, or the
 * equivalent object in the BAL case.

 */

jobject util_CreatePropertiesObject(JNIEnv *env, jobject reserved_NotUsed);

/**

 * A JNI wrapper to destroy the object from CreatePropertiesObject

 */

void util_DestroyPropertiesObject(JNIEnv *env, jobject propertiesObject,
                                  jobject reserved_NotUsed);

/**

 * A JNI wrapper to clear the object from CreatePropertiesObject

 */

void util_ClearPropertiesObject(JNIEnv *env, jobject propertiesObject,
                                jobject reserved_NotUsed);

/**

 * A JNI wrapper for storing a name/value pair into the Properties
 * object created by CreatePropertiesObject

 */

void util_StoreIntoPropertiesObject(JNIEnv *env, jobject propertiesObject,
                                    jobject name, jobject value, 
                                    jobject reserved);

/**

 * A JNI wrapper to get a value for a name out of a PropertiesObject
 * created by CreatePropertiesObject

 */

jobject util_GetFromPropertiesObject(JNIEnv *, jobject propertiesObject,
                                     jobject name, jobject reserved);

/**
   
 * A JNI wrapper to get a boolean value for a name out of a PropertiesObject
 * created by CreatePropertiesObject
 
 */

jboolean util_GetBoolFromPropertiesObject(JNIEnv *, jobject propertiesObject,
                                          jobject name, jobject reserved);

/**
   
 * A JNI wrapper to get an int value for a name out of a PropertiesObject
 * created by CreatePropertiesObject
 
 */

jint util_GetIntFromPropertiesObject(JNIEnv *, jobject propertiesObject,
                                     jobject name, jobject reserved);

void util_getSystemProperty(JNIEnv *env, 
                            const char *propName,
                            char *propValue, 
                            jint propValueLen);

//
// Functions provided by the browser specific native code
//

void util_LogMessage(int level, const char *fmt);

void util_Assert(void *test);

//
// Functions from secret JDK files
//

JNIEXPORT jvalue JNICALL
JNU_CallMethodByName(JNIEnv *env, 
					 jboolean *hasException,
					 jobject obj, 
					 const char *name,
					 const char *signature,
					 ...);

JNIEXPORT jvalue JNICALL
JNU_CallMethodByNameV(JNIEnv *env, 
					  jboolean *hasException,
					  jobject obj, 
					  const char *name,
					  const char *signature, 
					  va_list args);

JNIEXPORT void * JNICALL
JNU_GetEnv(JavaVM *vm, jint version);

#endif // jni_util_h
