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

#include "jni_util_export.h"
#include "jni_util.h"

#include "bal_util.h"

#include "nscore.h" // for nsnull

#include "ns_globals.h" // for prLogModuleInfo

#include "nsHashtable.h" // PENDING(edburns): size optimization?

static nsHashtable *gClassMappingTable = nsnull;

fpEventOccurredType externalEventOccurred = nsnull; // jni_util_export.h

fpInstanceOfType externalInstanceOf = nsnull; // jni_util_export.h

fpInitializeEventMaskType externalInitializeEventMask = nsnull; // jni_util_export.h

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
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, 
               ("This shouldn't get called in a non-BAL context!\n"));
    }
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
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, 
               ("This shouldn't get called in a non-BAL context!\n"));
    }
#endif

}

JNIEXPORT jint JNICALL util_StoreClassMapping(const char* jniClassName,
                                              jclass yourClassType)
{
    if (nsnull == gClassMappingTable) {
        if (nsnull == (gClassMappingTable = new nsHashtable(20, PR_TRUE))) {
            return -1;
        }
    }

    nsStringKey key(jniClassName);
    gClassMappingTable->Put(&key, yourClassType);

    return 0;
}

JNIEXPORT jclass JNICALL util_GetClassMapping(const char* jniClassName)
{
    jclass result = nsnull;

    if (nsnull == gClassMappingTable) {
        return nsnull;
    }

    nsStringKey key(jniClassName);
    result = (jclass) gClassMappingTable->Get(&key);
    
    return result;
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
        env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION_1_2);
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
