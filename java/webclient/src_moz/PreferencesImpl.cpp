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
 * Contributor(s):  Ed Burns <edburns@acm.org>
 */

#include "org_mozilla_webclient_impl_wrapper_0005fnative_PreferencesImpl.h"

#include <nsIPref.h>
#include <nsIServiceManager.h> // for do_getService
#include <nsDataHashtable.h>
#include <nsHashKeys.h> // for nsStringHashKey
#include "ns_util.h"
//
// Local Data
// 

#include "ns_util.h"

typedef struct _peStruct {
    NativeWrapperFactory *cx;
    jobject obj;
    jobject callback;
} peStruct;

nsDataHashtable<nsCStringHashKey,peStruct *> closures;

//
// Local functions
//

void prefEnumerator(const char *name, void *closure);
static int PR_CALLBACK prefChanged(const char *name, void *closure);

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_PreferencesImpl_nativeStartup
(JNIEnv *env, jobject obj, jint nativeContext)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("PreferencesImpl_nativeStartup: entering\n"));
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    
    PR_ASSERT(wcContext);
    
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
    
    if (!prefs) {
        ::util_ThrowExceptionToJava(env, "Can't get the preferences.");
        return;
    }

    wcContext->sPrefs = prefs.get();
    NS_ADDREF(wcContext->sPrefs);

    closures.Init();
    
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("PreferencesImpl_nativeStartup: exiting\n"));
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_PreferencesImpl_nativeShutdown
(JNIEnv *env, jobject obj, jint nativeContext)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("PreferencesImpl_nativeShutdown: entering\n"));
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;

    PR_ASSERT(wcContext);

    closures.Clear();

    NS_RELEASE(wcContext->sPrefs);
    wcContext->sPrefs = nsnull;


    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("PreferencesImpl_nativeShutdown: exiting\n"));
}


JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_PreferencesImpl_nativeSetUnicharPref
(JNIEnv *env, jobject obj, jint nativeContext, jstring prefName, 
 jstring prefValue)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("PreferencesImpl_nativeSetUnicharPref: entering\n"));
    
    nsresult rv = NS_ERROR_FAILURE;
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    nsCOMPtr<nsIPref> prefs = nsnull;
    PR_ASSERT(wcContext);
    
    prefs = wcContext->sPrefs;
    PR_ASSERT(prefs);
    
    const char	*	prefNameChars = (char *)::util_GetStringUTFChars(env, 
                                                                     prefName);
    const jchar	*	prefValueChars = nsnull;
    if (nsnull != prefValue) {
        prefValueChars = 
            (jchar *)::util_GetStringChars(env, prefValue);
        if (nsnull == prefNameChars) {
            ::util_ThrowExceptionToJava(env, "nativeSetUnicharPref: unable to extract Java string for pref name");
            rv = NS_ERROR_NULL_POINTER;
            goto OMWIWNPINSUP_CLEANUP;
        }
        if (nsnull == prefValueChars) {
            ::util_ThrowExceptionToJava(env, "nativeSetUnicharPref: unable to extract Java string for pref value");
            rv = NS_ERROR_NULL_POINTER;
            goto OMWIWNPINSUP_CLEANUP;
        }
        
        rv = prefs->SetUnicharPref(prefNameChars, 
                                   (const PRUnichar *) prefValueChars);
    }
    else {
        rv = prefs->ClearUserPref(prefNameChars);
    }

 OMWIWNPINSUP_CLEANUP:
    
    ::util_ReleaseStringUTFChars(env, prefName, prefNameChars);
    ::util_ReleaseStringChars(env, prefValue, prefValueChars);
    
    if (NS_FAILED(rv)) {
        // PENDING(edburns): set a more specific kind of pref
        ::util_ThrowExceptionToJava(env, "nativeSetUnicharPref: can't set pref");
    }

    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("PreferencesImpl_nativeSetUnicharPref: exiting\n"));
    
    return;
}


JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_PreferencesImpl_nativeSetIntPref
(JNIEnv *env, jobject obj, jint nativeContext, jstring prefName, 
 jint prefValue)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("PreferencesImpl_nativeSetIntPref: entering\n"));
    
    nsresult rv = NS_ERROR_FAILURE;
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    nsCOMPtr<nsIPref> prefs = nsnull;
    PR_ASSERT(wcContext);
    
    prefs = wcContext->sPrefs;
    PR_ASSERT(prefs);

    const char	*	prefNameChars = (char *) ::util_GetStringUTFChars(env, 
                                                                      prefName);
    if (nsnull == prefNameChars) {
        ::util_ThrowExceptionToJava(env, "nativeSetIntPref: unable to extract Java string");
        return;
    }

    rv = prefs->SetIntPref(prefNameChars, (PRInt32) prefValue);
    
    ::util_ReleaseStringUTFChars(env, prefName, prefNameChars);
    
    if (NS_FAILED(rv)) {
        // PENDING(edburns): set a more specific kind of pref
        ::util_ThrowExceptionToJava(env, "nativeSetIntPref: can't set pref");
    }
    
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("PreferencesImpl_nativeSetIntPref: exiting\n"));
    return;
}


JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_PreferencesImpl_nativeSetBoolPref
(JNIEnv *env, jobject obj, jint nativeContext, jstring prefName, 
 jboolean prefValue)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("PreferencesImpl_nativeSetIntPref: entering\n"));
    
    nsresult rv = NS_ERROR_FAILURE;
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    nsCOMPtr<nsIPref> prefs = nsnull;
    PR_ASSERT(wcContext);
    
    prefs = wcContext->sPrefs;
    PR_ASSERT(prefs);

    const char	*	prefNameChars = (char *)::util_GetStringUTFChars(env, 
                                                                     prefName);
    if (nsnull == prefNameChars) {
        ::util_ThrowExceptionToJava(env, "nativeSetBoolPref: unable to extract Java string");
        return;
    }
    
    rv = prefs->SetBoolPref(prefNameChars, prefValue);

    ::util_ReleaseStringUTFChars(env, prefName, prefNameChars);

    if (NS_FAILED(rv)) {
        // PENDING(edburns): set a more specific kind of pref
        ::util_ThrowExceptionToJava(env, "nativeSetBoolPref: can't set pref");
    }

    return;

    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("PreferencesImpl_nativeSetIntPref: exiting\n"));

}

JNIEXPORT jobject JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_PreferencesImpl_nativeGetPrefs
(JNIEnv *env, jobject obj, jint nativeContext, jobject props)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("PreferencesImpl_nativeGetIntPref: entering\n"));
    
    nsresult rv = NS_ERROR_FAILURE;
    jobject newProps = nsnull;
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    nsCOMPtr<nsIPref> prefs = nsnull;
    PR_ASSERT(wcContext);
    
    prefs = wcContext->sPrefs;
    PR_ASSERT(prefs);

    // step one: create or clear props
    if (nsnull == props) {
        if (nsnull == 
            (newProps = 
             ::util_CreatePropertiesObject(env, (jobject)
                                           &(wcContext->shareContext)))) {
            ::util_ThrowExceptionToJava(env, "Exception: nativeGetPrefs: can't create prefs.");
            return props;
        }
        if (nsnull == (props = ::util_NewGlobalRef(env, newProps))) {
            ::util_ThrowExceptionToJava(env, "Exception: nativeGetPrefs: can't create global ref for prefs.");
            return props;
        }
    }
    else {
        ::util_ClearPropertiesObject(env, props, (jobject) 
                                     &(wcContext->shareContext));
        
    }
    PR_ASSERT(props);

    // step two, call the magic enumeration function, to populate the
    // properties
    peStruct pes;
    pes.cx = wcContext;
    pes.obj = props;

    rv = prefs->EnumerateChildren("", prefEnumerator, &pes);

    if (NS_FAILED(rv)) {
        // PENDING(edburns): set a more specific kind of pref
        ::util_ThrowExceptionToJava(env, "nativeGetPrefs: can't get prefs");
    }

    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("PreferencesImpl_nativeGetIntPref: exiting\n"));
    
    return props;
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_PreferencesImpl_nativeRegisterPrefChangedCallback
(JNIEnv *env, jobject obj, jint nativeContext, 
 jobject callback, jstring prefName, jobject closure)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("PreferencesImpl_nativeRegisterPrefChangedCallback: entering\n"));
    
    nsresult rv = NS_ERROR_FAILURE;
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    const char *prefNameChars = nsnull;
    nsCOMPtr<nsIPref> prefs = nsnull;
    PR_ASSERT(wcContext);
    
    prefs = wcContext->sPrefs;
    PR_ASSERT(prefs);

    // step one: create a const char * from the prefName
    if (nsnull == (prefNameChars = ::util_GetStringUTFChars(env, prefName))) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeRegisterPrefChangedCallback: can't get string for prefName");
        return;
    }

    // step two, set up our struct
    peStruct *pes = new peStruct();
    
    pes->cx = wcContext;
    pes->obj = ::util_NewGlobalRef(env, closure);
    pes->callback = ::util_NewGlobalRef(env, callback);
    closures.Put(nsDependentCString(prefNameChars), pes);

    rv = prefs->RegisterCallback(prefNameChars, prefChanged, pes);

    if (NS_FAILED(rv)) {
        // PENDING(edburns): set a more specific kind of pref
        ::util_ThrowExceptionToJava(env, "nativeRegisterPrefChangedCallback: can't set callback");
    }
    
    ::util_ReleaseStringUTFChars(env, prefName, prefNameChars);
    
    return;
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_PreferencesImpl_nativeUnregisterPrefChangedCallback
(JNIEnv *env, jobject obj, jint nativeContext, 
 jobject callback, jstring prefName, jobject closure)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("PreferencesImpl_nativeRegisterPrefChangedCallback: entering\n"));
    
    nsresult rv = NS_ERROR_FAILURE;
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    const char *prefNameChars = nsnull;
    nsCOMPtr<nsIPref> prefs = nsnull;
    PR_ASSERT(wcContext);
    
    prefs = wcContext->sPrefs;
    PR_ASSERT(prefs);

    // step one, set up our struct
    peStruct *pes = nsnull;
    

    // step two: create a const char * from the prefName
    if (nsnull == (prefNameChars = ::util_GetStringUTFChars(env, prefName))) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeRegisterPrefChangedCallback: can't get string for prefName");
        return;
    }
    
    nsDependentCString prefNameCString(prefNameChars);
    
    if (closures.Get(prefNameCString, &pes)) {
        closures.Remove(prefNameCString);

        rv = prefs->UnregisterCallback(prefNameChars, prefChanged, pes);
        
        if (NS_FAILED(rv)) {
            // PENDING(edburns): set a more specific kind of pref
            ::util_ThrowExceptionToJava(env, "nativeRegisterPrefChangedCallback: can't set callback");
        }
        
        ::util_DeleteGlobalRef(env, pes->obj);
        ::util_DeleteGlobalRef(env, pes->callback);
        delete pes;
    }
    
    ::util_ReleaseStringUTFChars(env, prefName, prefNameChars);
    
    return;
}

// 
// Helper functions
//

void prefEnumerator(const char *name, void *closure)
{
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    if (nsnull == closure) {
        return;
    }
    peStruct *pes = (peStruct *) closure;
    NativeWrapperFactory *wcContext =  pes->cx;
    jobject props = pes->obj;
    PRInt32 prefType, intVal;
    PRBool boolVal;
    jstring prefName = nsnull;
    jstring prefValue = nsnull;
    PRUnichar *prefValueUni = nsnull;
    nsAutoString prefValueAuto;
    const PRInt32 bufLen = 20;
    char buf[bufLen];
    memset(buf, 0, bufLen);
    nsCOMPtr<nsIPref> prefs = wcContext->sPrefs;
    
    if (nsnull == props || !prefs) {
        return;
    }
    if (NS_FAILED(prefs->GetPrefType(name, &prefType))) {
        return;
    }
    
    if (nsnull == (prefName = ::util_NewStringUTF(env, name))) {
        return;
    }
    
    switch(prefType) {
    case nsIPref::ePrefInt:
        if (NS_SUCCEEDED(prefs->GetIntPref(name, &intVal))) {
            WC_ITOA(intVal, buf, 10);
            prefValue = ::util_NewStringUTF(env, buf);
        }
        break;
    case nsIPref::ePrefBool:
        if (NS_SUCCEEDED(prefs->GetBoolPref(name, &boolVal))) {
            if (boolVal) {
                prefValue = ::util_NewStringUTF(env, "true");
            }
            else {
                prefValue = ::util_NewStringUTF(env, "false");
            }
        }
        break;
    case nsIPref::ePrefString:
        if (NS_SUCCEEDED(prefs->CopyUnicharPref(name, &prefValueUni))) {
            prefValueAuto = prefValueUni;
            prefValue = ::util_NewString(env, (const jchar *) prefValueUni,
                                         prefValueAuto.Length());
            delete [] prefValueUni;
        }
        break;
    default:
        PR_ASSERT(PR_TRUE);
        break;
    }
    if (nsnull == prefValue) {
        prefValue = ::util_NewStringUTF(env, "");
    }
    ::util_StoreIntoPropertiesObject(env, props, prefName, prefValue, 
                                     (jobject) &(wcContext->shareContext));
    ::util_DeleteLocalRef(env, prefName);
    ::util_DeleteLocalRef(env, prefValue);
}

static int PR_CALLBACK prefChanged(const char *name, void *closure)
{
    if (nsnull == name || nsnull == closure) {
        return NS_ERROR_NULL_POINTER;
    }
    int result;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    peStruct *pes = (peStruct *) closure;
    jstring prefName;
    
    if (!(prefName = ::util_NewStringUTF(env, name))) {
        return NS_ERROR_NULL_POINTER;
    }
    
#ifdef BAL_INTERFACE
#else
    jclass pcClass = nsnull;
    jmethodID pcMID = nsnull;
    
    if (!(pcClass = env->GetObjectClass(pes->callback))) {
        return NS_ERROR_FAILURE;
    }
    if (!(pcMID =env->GetMethodID(pcClass, "prefChanged",
                                  "(Ljava/lang/String;Ljava/lang/Object;)I"))){
        return NS_ERROR_FAILURE;
    }
    result = env->CallIntMethod(pes->callback, pcMID, prefName, 
                                pes->obj);
    
#endif
    
    ::util_DeleteStringUTF(env, prefName);

    return result;
}
 
