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
 *      Jason Mawdsley <jason@macadamian.com>
 *      Louis-Philippe Gagnon <louisphilippe@macadamian.com>
 *               
 */

#include "jni_util_export.h"
#include "jni_util.h"

#include "bal_util.h"

fpEventOccurredType externalEventOccurred = nsnull; // jni_util_export.h

fpInstanceOfType externalInstanceOf = nsnull; // jni_util_export.h

fpInitializeEventMaskType externalInitializeEventMask = nsnull; // jni_util_export.h

fpCreatePropertiesObjectType externalCreatePropertiesObject = nsnull; // jni_util_export.h

fpDestroyPropertiesObjectType externalDestroyPropertiesObject = nsnull; // jni_util_export.h

fpClearPropertiesObjectType externalClearPropertiesObject = nsnull; // jni_util_export.h

fpStoreIntoPropertiesObjectType externalStoreIntoPropertiesObject = nsnull; // jni_util_export.h

JNIEXPORT const char * JNICALL util_GetStringUTFChars(JNIEnv *env, 
                                                      jstring inString)
{
    const char *result = nsnull;
    
#ifdef BAL_INTERFACE
    char *nonConstResult;
    bal_str_newFromJstring(&nonConstResult, inString);
    result = (const char *)nonConstResult;
#else
    result = (const char *) env->GetStringUTFChars(inString, 0);
#endif    
    
    return result;
}

JNIEXPORT void JNICALL util_ReleaseStringUTFChars(JNIEnv *env, 
                                                  jstring inString,
                                                  const char *stringFromGet)
{

#ifdef BAL_INTERFACE
    bal_str_release(stringFromGet);
#else
    env->ReleaseStringUTFChars(inString, stringFromGet);
#endif

}

JNIEXPORT const jchar * JNICALL util_GetStringChars(JNIEnv *env, 
                                                    jstring inString)
{
    const jchar *result = nsnull;

#ifdef BAL_INTERFACE
    // ASSUMES typedef wchar_t	        jchar;
    // ASSUMES typedef wchar_t *jstring;
    result = (const jchar *) inString;
#else
    result = (const jchar *) env->GetStringChars(inString, 0);
#endif    

    return result;
}

JNIEXPORT void JNICALL util_ReleaseStringChars(JNIEnv *env, jstring inString,
                                               const jchar *stringFromGet)
{

#ifdef BAL_INTERFACE
    // NO action necessary, see NewStringChars
#else
    env->ReleaseStringChars(inString, stringFromGet);
#endif

}

JNIEXPORT jsize  JNICALL util_GetStringLength(JNIEnv *env, 
                                                    jstring inString)
{
    jsize result = 0;
#ifdef BAL_INTERFACE
    result = bal_jstring_getLength(inString);
#else
    result = env->GetStringLength(inString);
#endif
    return result;
}

JNIEXPORT jstring JNICALL util_NewStringUTF(JNIEnv *env, const char *inString)
{
    jstring result = nsnull;
#ifdef BAL_INTERFACE
    bal_jstring_newFromAscii(&result, inString);
#else
    result = env->NewStringUTF(inString);
#endif

    return result;
}

JNIEXPORT  void JNICALL util_DeleteStringUTF(JNIEnv *env, jstring toDelete)
{
#ifdef BAL_INTERFACE
    util_DeleteString(env, toDelete);
#else
    // do nothing, java takes care of it
#endif

}

JNIEXPORT jstring JNICALL util_NewString(JNIEnv *env, const jchar *inString, 
                                         jsize len)
{
    jstring result = nsnull;
#ifdef BAL_INTERFACE
    bal_jstring_newFromJcharArray(&result, inString, len);
#else
    result = env->NewString(inString, len);
#endif

    return result;
}

JNIEXPORT  void JNICALL util_DeleteString(JNIEnv *env, jstring toDelete)
{
#ifdef BAL_INTERFACE
    bal_jstring_release(toDelete);
#else
    // do nothing, java takes care of it
#endif

}

JNIEXPORT void JNICALL util_SetEventOccurredFunction(fpEventOccurredType fp)
{
    externalEventOccurred = fp;
}

JNIEXPORT void JNICALL util_SetInstanceOfFunction(fpInstanceOfType fp)
{
    externalInstanceOf = fp;
}

JNIEXPORT void JNICALL util_SetInitializeEventMaskFunction(fpInitializeEventMaskType fp)
{
    externalInitializeEventMask = fp;
}

JNIEXPORT void JNICALL util_SetCreatePropertiesObjectFunction(fpCreatePropertiesObjectType fp)
{
    externalCreatePropertiesObject = fp;
}

JNIEXPORT void JNICALL util_SetDestroyPropertiesObjectFunction(fpDestroyPropertiesObjectType fp)
{
    externalDestroyPropertiesObject = fp;
}

JNIEXPORT void JNICALL util_SetClearPropertiesObjectFunction(fpClearPropertiesObjectType fp)
{
    externalClearPropertiesObject = fp;
}

JNIEXPORT void JNICALL util_SetStoreIntoPropertiesObjectFunction(fpStoreIntoPropertiesObjectType fp)
{
    externalStoreIntoPropertiesObject = fp;
}

JNIEXPORT void JNICALL 
util_InitializeEventMaskValuesFromClass(const char *className,
                                        char *maskNames[], 
                                        jlong maskValues[])
{
    int i = 0;
    JNIEnv *env = nsnull;
    if (nsnull == className) {
        return;
    }

    if (nsnull != gVm) {
        env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    }

    jclass clazz = ::util_FindClass(env, className);
    
    if (nsnull == clazz) {
        return;
    }

#ifdef BAL_INTERFACE
    if (nsnull != externalInitializeEventMask) {
        externalInitializeEventMask(env, clazz,
                                    (const char **) maskNames, maskValues);
    }
#else
    if (nsnull == env) {
        return;
    }
    
    jfieldID fieldID;
    
    while (nsnull != maskNames[i]) {
        fieldID = ::util_GetStaticFieldID(env, clazz, 
                                          maskNames[i], "J");
        
        if (nsnull == fieldID) {
            return;
        }
        
        maskValues[i] = ::util_GetStaticLongField(env, clazz, 
                                                  fieldID);
        i++;
    }
    
#endif
}
