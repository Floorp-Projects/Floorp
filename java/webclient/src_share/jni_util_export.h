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
 * Contributor(s): Ed Burns <edburns@acm.org>
 *
 */


/**

 * Exported Util methods, called from webclient uno.

 */

#ifndef jni_util_export_h
#define jni_util_export_h

#ifdef __cplusplus
extern "C" {
#endif

#include <jni.h>


JNIEXPORT const char * JNICALL util_GetStringUTFChars(JNIEnv *env, 
                                                      jstring inString);

JNIEXPORT void JNICALL util_ReleaseStringUTFChars(JNIEnv *env, 
                                                  jstring inString, 
                                                  const char *stringFromGet);

JNIEXPORT const jchar * JNICALL util_GetStringChars(JNIEnv *env, 
                                                    jstring inString);

JNIEXPORT void JNICALL util_ReleaseStringChars(JNIEnv *env, jstring inString, 
                                               const jchar *stringFromGet);

JNIEXPORT jsize  JNICALL util_GetStringLength(JNIEnv *env, 
                                                    jstring inString);

JNIEXPORT jobjectArray util_GetJstringArrayFromJcharArray(JNIEnv *env, 
                                                          jint len,
                                                          jchar **strings,
                                                          jint *stringLengths);

JNIEXPORT jstring JNICALL util_NewStringUTF(JNIEnv *env, 
                                            const char * inString);

JNIEXPORT  void JNICALL util_DeleteStringUTF(JNIEnv *env, jstring toDelete); 

JNIEXPORT jstring JNICALL util_NewString(JNIEnv *env, const jchar *inString, 
                                         jsize len);

JNIEXPORT  void JNICALL util_DeleteString(JNIEnv *env, jstring toDelete); 

//
// BAL methods
//

/*

 * The following methods are used by non Java JNI clients, such as
 * StarOfficeDesktop.

 */


/**

 * Function declaration for the user defined InstanceOf function.  It
 * tells whether the second argument, which is an instance, is an
 * instance of the type in the third argument.

 * @see util_SetInstanceOfFunction

 */


typedef JNIEXPORT jboolean (JNICALL *fpInstanceOfType) (JNIEnv *env,
                                                        jobject obj,
                                                        jclass clazz);

/**

 * Function declaration for the user defined EventOccurred function.  It
 * is called when an event occurrs.  The second argument is the context
 * for the event, passed in by the user as the second argument to
 * NativeEventThreadImpl_nativeAddListener().  The third arcument is the
 * listener object, passed in as the last argument to
 * NativeEventThreadImpl_nativeAddListener().  The fourth argument is a
 * listener specific type field, to indicate what kind of sub-event
 * within the listener has occurred.  The last argument is a listener
 * sub-event specific argument.  For example, when the event class is
 * DocumentLoadListener, and the sub-event is "STATUS_URL_LOAD", the
 * last argument is a string with a status message, ie "Contacting host
 * blah...", etc.

 */

typedef JNIEXPORT void (JNICALL * fpEventOccurredType) (JNIEnv *env, 
                                                        jobject nativeEventThread,
                                                        jobject webclientEventListener,
                                                        jlong eventType,
                                                        jobject eventData);

/**

 * Called at app initialization to external user to provide a function
 * that will fill in the event mask values for the given listener class.

 * @param nullTermMaskNameArray a NULL terminated const char * array for
 * the mask names.

 * @param maskValueArray a parallel array for the values that match the
 * corresponding elements in nullTermMaskNameArray

 */

typedef JNIEXPORT void (JNICALL * fpInitializeEventMaskType) 
    (JNIEnv *env, 
     jclass listenerClass,
     const char **nullTermMaskNameArray,
     jlong *maskValueArray);

/**

 * Called when webclient wants to create a "Properties" object.  Right
 * now, no parameters are actually used.

 */

typedef JNIEXPORT jobject (JNICALL * fpCreatePropertiesObjectType) 
    (JNIEnv *env, jobject reserved_NotUsed);

/**

 * Called after webclient is done using a "Properties" object it created
 * with fpCreatePropertiesObject

 * @param propertiesObject the propertiesObject created with
 * fpCreatePropertiesObject

 */

typedef JNIEXPORT void (JNICALL * fpDestroyPropertiesObjectType) 
    (JNIEnv *env, jobject propertiesObject, jobject reserved_NotUsed);

/**

 * Called when webclient wants to clear a  "Properties" object it created
 * with fpCreatePropertiesObject

 * @param propertiesObject the propertiesObject created with
 * fpCreatePropertiesObject

 */

typedef JNIEXPORT void (JNICALL * fpClearPropertiesObjectType) 
    (JNIEnv *env, jobject propertiesObject, jobject reserved_NotUsed);

/**

 * Called after webclient has called fpCreatePropertiesObjectType when
 * webclient wants to store values into the properties object.

 * @param env not used

 * @param propertiesObject obtained from fpCreatePropertiesObjectType

 * @param name the name of the property

 * @param the value of the property

 */

typedef JNIEXPORT void (JNICALL * fpStoreIntoPropertiesObjectType) 
    (JNIEnv *env, jobject propertiesObject, jobject name, jobject value, 
     jobject reserved);

/**

 * Called after webclient has called fpCreatePropertiesObjectType when
 * webclient wants to get values from the properties object.

 * @param env not used

 * @param propertiesObject obtained from fpCreatePropertiesObjectType

 * @param name the name of the property

 * @returns the return value from the properties object

 */


typedef JNIEXPORT jobject (JNICALL * fpGetFromPropertiesObjectType) 
    (JNIEnv *env, jobject propertiesObject, jobject name, jobject reserved);


/**

 * This function must be called at app initialization.

 * @see fpInstanceOfType

 */

JNIEXPORT void JNICALL util_SetInstanceOfFunction(fpInstanceOfType fp);

/**

 * This function must be called at app initialization.

 * @see fpEventOccurredType

 */

JNIEXPORT void JNICALL util_SetEventOccurredFunction(fpEventOccurredType fp);

/**

 * This function must be called at app initialization.

 * @see fpInitializeEventMaskType

 */

JNIEXPORT void JNICALL util_SetInitializeEventMaskFunction(fpInitializeEventMaskType fp);

/**

 * This function must be called at app initialization.

 * @see fpCreatePropertiesObjectType

 */

JNIEXPORT void JNICALL util_SetCreatePropertiesObjectFunction(fpCreatePropertiesObjectType fp);

/**

 * This function must be called at app initialization.

 * @see fpDestroyPropertiesObjectType

 */

JNIEXPORT void JNICALL util_SetDestroyPropertiesObjectFunction(fpDestroyPropertiesObjectType fp);

/**

 * This function must be called at app initialization.

 * @see fpDestroyPropertiesObjectType

 */

JNIEXPORT void JNICALL util_SetClearPropertiesObjectFunction(fpDestroyPropertiesObjectType fp);

/**

 * This function must be called at app initialization.

 * @see fpStoreIntoPropertiesObjectType

 */

JNIEXPORT void JNICALL util_SetStoreIntoPropertiesObjectFunction(fpStoreIntoPropertiesObjectType fp);

/**

 * This function must be called at app initialization.

 * @see fpGetFromPropertiesObjectType

 */

JNIEXPORT void JNICALL util_SetGetFromPropertiesObjectFunction(fpGetFromPropertiesObjectType fp);



/**

 * defined in jni_util_export.cpp

 * The function pointer set with util_SetEventOccurredFunction.

 */

extern fpEventOccurredType externalEventOccurred;

/**

 * defined in jni_util_export.cpp

 * The function pointer set with util_SetInstanceOfFunction.

 */

extern fpInstanceOfType externalInstanceOf;

/**

 * defined in jni_util_export.cpp

 * The function pointer set with util_SetInitializeEventMaskFunction

 */

extern fpInitializeEventMaskType externalInitializeEventMask;

/**

 * defined in jni_util_export.cpp

 * The function pointer set with util_SetCreatePropertiesObjectFunction

 */

extern fpCreatePropertiesObjectType externalCreatePropertiesObject;

/**

 * defined in jni_util_export.cpp

 * The function pointer set with util_SetDestroyPropertiesObjectFunction

 */

extern fpDestroyPropertiesObjectType externalDestroyPropertiesObject;

/**

 * defined in jni_util_export.cpp

 * The function pointer set with util_SetClearPropertiesObjectFunction

 */

extern fpClearPropertiesObjectType externalClearPropertiesObject;

/**

 * defined in jni_util_export.cpp

 * The function pointer set with util_SetStoreIntoPropertiesObjectFunction

 */

extern fpStoreIntoPropertiesObjectType externalStoreIntoPropertiesObject;

/**

 * defined in jni_util_export.cpp

 * The function pointer set with util_SetGetFromPropertiesObjectFunction

 */

extern fpGetFromPropertiesObjectType externalGetFromPropertiesObject;


/**

 * Called by the mozilla event listener implementation class at
 * construction time.

 */

JNIEXPORT void JNICALL 
util_InitializeEventMaskValuesFromClass(const char *className,
                                        char *maskNames[], 
                                        jlong maskValues[]);

//
// Functions provided by browser-specific native code
//

/**

 * This method is used to store a mapping from a jniClass Name, such as
 * "org/mozilla/webclient/DocumentLoadListener" to some external class
 * type, such as
 * org::mozilla::webclient::wrapper_native::uno::DocumentLoadListener.

 * This table is used in util_IsInstanceOf.

 * @see util_SetInstanceOfFunction

 * @ret 0 on success

 */

JNIEXPORT jint JNICALL util_StoreClassMapping(const char* jniClassName,
                                              jclass yourClassType);

JNIEXPORT jclass JNICALL util_GetClassMapping(const char* jniClassName);



#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif // jni_util_export_h
