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

#include "PreferencesImpl.h"
#include "PreferencesActionEvents.h"

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_PreferencesImpl_nativeSetUnicharPref
(JNIEnv *env, jobject obj, jint webShellPtr, jstring prefName, 
 jstring prefValue)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

    if (initContext == nsnull) {
      ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to netiveSetPref");
      return;
    }

    const char	*	prefNameChars = (char *) ::util_GetStringUTFChars(env, 
                                                                      prefName);
    const jchar	*	prefValueChars = (jchar *) ::util_GetStringChars(env, 
                                                                     prefValue);
    nsresult rv = NS_ERROR_FAILURE;
    wsSetUnicharPrefEvent *actionEvent = nsnull;
    void *voidResult = nsnull;
    
    if (nsnull == prefNameChars) {
        ::util_ThrowExceptionToJava(env, "nativeSetUnicharPref: unable to extract Java string for pref name");
        rv = NS_ERROR_NULL_POINTER;
        goto NSUP_CLEANUP;
    }
    if (nsnull == prefValueChars) {
        ::util_ThrowExceptionToJava(env, "nativeSetUnicharPref: unable to extract Java string for pref value");
        rv = NS_ERROR_NULL_POINTER;
        goto NSUP_CLEANUP;
    }

    if (!(actionEvent = new wsSetUnicharPrefEvent(prefNameChars, 
                                                  prefValueChars))) {
        ::util_ThrowExceptionToJava(env, "nativeSetPref: unable to create actionEvent");
        rv = NS_ERROR_NULL_POINTER;
        goto NSUP_CLEANUP;
    }
    
    voidResult = ::util_PostSynchronousEvent(initContext,
                                             (PLEvent *) *actionEvent);
    rv = (nsresult) voidResult;

 NSUP_CLEANUP:
    
    ::util_ReleaseStringUTFChars(env, prefName, prefNameChars);
    ::util_ReleaseStringChars(env, prefName, prefValueChars);
    
    if (NS_FAILED(rv)) {
        // PENDING(edburns): set a more specific kind of pref
        ::util_ThrowExceptionToJava(env, "nativeSetUnicharPref: can't set pref");
    }
    
    return;
}


JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_PreferencesImpl_nativeSetIntPref
(JNIEnv *env, jobject obj, jint webShellPtr, jstring prefName, 
 jint prefValue)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

    if (initContext == nsnull) {
      ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to netiveSetPref");
      return;
    }
    const char	*	prefNameChars = (char *) ::util_GetStringUTFChars(env, 
                                                                      prefName);
    nsresult rv = NS_ERROR_FAILURE;
    wsSetIntPrefEvent *actionEvent = nsnull;
    void *voidResult = nsnull;
    
    if (nsnull == prefNameChars) {
        ::util_ThrowExceptionToJava(env, "nativeSetIntPref: unable to extract Java string");
        return;
    }

    if (!(actionEvent = new wsSetIntPrefEvent(prefNameChars, 
                                              (PRInt32) prefValue))) {
        ::util_ThrowExceptionToJava(env, "nativeSetPref: unable to create actionEvent");
        rv = NS_ERROR_NULL_POINTER;
        goto NSIP_CLEANUP;
    }
    
    voidResult = ::util_PostSynchronousEvent(initContext,
                                             (PLEvent *) *actionEvent);
    rv = (nsresult) voidResult;

 NSIP_CLEANUP:

    ::util_ReleaseStringUTFChars(env, prefName, prefNameChars);
    
    if (NS_FAILED(rv)) {
        // PENDING(edburns): set a more specific kind of pref
        ::util_ThrowExceptionToJava(env, "nativeSetIntPref: can't set pref");
    }
    
    return;
}


JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_PreferencesImpl_nativeSetBoolPref
(JNIEnv *env, jobject obj, jint webShellPtr, jstring prefName, 
 jboolean prefValue)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

    if (initContext == nsnull) {
      ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to netiveSetPref");
      return;
    }

    const char	*	prefNameChars = (char *) ::util_GetStringUTFChars(env, 
                                                                      prefName);
    nsresult rv = NS_ERROR_FAILURE;
    wsSetBoolPrefEvent *actionEvent = nsnull;
    void *voidResult = nsnull;

    if (nsnull == prefNameChars) {
        ::util_ThrowExceptionToJava(env, "nativeSetBoolPref: unable to extract Java string");
        return;
    }

    if (!(actionEvent = new wsSetBoolPrefEvent(prefNameChars, 
                                               prefValue))) {
        ::util_ThrowExceptionToJava(env, "nativeSetPref: unable to create actionEvent");
        rv = NS_ERROR_NULL_POINTER;
        goto NSBP_CLEANUP;
    }
    
    voidResult = ::util_PostSynchronousEvent(initContext,
                                             (PLEvent *) *actionEvent);
    rv = (nsresult) voidResult;

 NSBP_CLEANUP:

    ::util_ReleaseStringUTFChars(env, prefName, prefNameChars);

    if (NS_FAILED(rv)) {
        // PENDING(edburns): set a more specific kind of pref
        ::util_ThrowExceptionToJava(env, "nativeSetBoolPref: can't set pref");
    }

    return;
}

JNIEXPORT jobject JNICALL 
Java_org_mozilla_webclient_wrapper_1native_PreferencesImpl_nativeGetPrefs
(JNIEnv *env, jobject obj, jint webShellPtr, jobject props)
{
    nsresult rv = NS_ERROR_FAILURE;
    jobject newProps;
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    wsGetPrefsEvent *actionEvent = nsnull;
    void *voidResult = nsnull;

    if (initContext == nsnull) {
      ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to netiveGetPrefs");
      return props;
    }

    PR_ASSERT(initContext->initComplete);

    // step one: create or clear props
    if (nsnull == props) {
        if (nsnull == 
            (newProps = 
             ::util_CreatePropertiesObject(env, (jobject)
                                           &(initContext->shareContext)))) {
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
                                     &(initContext->shareContext));
        
    }
    PR_ASSERT(props);

    // step two, call the magic enumeration function, to populate the
    // properties
    peStruct pes;
    pes.cx = initContext;
    pes.obj = props;
    if (!(actionEvent = new wsGetPrefsEvent(&pes))) {
        ::util_ThrowExceptionToJava(env, "nativeSetPref: unable to create actionEvent");
        return props;
    }
    
    voidResult = ::util_PostSynchronousEvent(initContext,
                                             (PLEvent *) *actionEvent);
    rv = (nsresult) voidResult;

    if (NS_FAILED(rv)) {
        // PENDING(edburns): set a more specific kind of pref
        ::util_ThrowExceptionToJava(env, "nativeGetPrefs: can't get prefs");
    }
    
    return props;
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_PreferencesImpl_nativeRegisterPrefChangedCallback
(JNIEnv *env, jobject obj, jint webShellPtr, 
 jobject callback, jstring prefName, jobject closure)
{
    nsresult rv = NS_ERROR_FAILURE;
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    const char *prefNameChars;
    wsRegisterPrefCallbackEvent *actionEvent = nsnull;
    void *voidResult = nsnull;
    
    if (initContext == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativeRegisterPrefChangedCallback");
        return;
    }
    
    PR_ASSERT(initContext->initComplete);

    if (nsnull == (callback = ::util_NewGlobalRef(env, callback))) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeRegisterPrefChangedCallback: can't global ref for callback");
        return;
    }
    
    if (nsnull == (closure = ::util_NewGlobalRef(env, closure))) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeRegisterPrefChangedCallback: can't global ref for closure");
        return;
    }
    
    // step one, set up our struct
    peStruct *pes;
    
    if (nsnull == (pes = new peStruct())) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeRegisterPrefChangedCallback: can't get peStruct");
        return;
    }
    
    pes->cx = initContext;
    pes->obj = closure;
    pes->callback = callback;

    // step two: create a const char * from the prefName
    if (nsnull == (prefNameChars = ::util_GetStringUTFChars(env, prefName))) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeRegisterPrefChangedCallback: can't get string for prefName");
        return;
    }

    if (!(actionEvent = new wsRegisterPrefCallbackEvent(prefNameChars, pes))) {
        ::util_ThrowExceptionToJava(env, "nativeSetPref: unable to create actionEvent");
        rv = NS_ERROR_NULL_POINTER;
        return;
    }
    
    voidResult = ::util_PostSynchronousEvent(initContext,
                                             (PLEvent *) *actionEvent);
    rv = (nsresult) voidResult;

    if (NS_FAILED(rv)) {
        // PENDING(edburns): set a more specific kind of pref
        ::util_ThrowExceptionToJava(env, "nativeRegisterPrefChangedCallback: can't set callback");
    }
    
    ::util_ReleaseStringUTFChars(env, prefName, prefNameChars);
            
    return;
}
