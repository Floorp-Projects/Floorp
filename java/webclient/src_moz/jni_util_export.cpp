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

#include "bal_util.h"

#include "nscore.h" // for nsnull

#include "ns_globals.h" // for prLogModuleInfo

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
