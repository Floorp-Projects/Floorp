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
 */

/*
 * PreferencesActionEvents.cpp
 */

#include "PreferencesActionEvents.h"

#include "nsIPref.h"
#include "nsCRT.h" // for memset


static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

//
// Local functions
//

void prefEnumerator(const char *name, void *closure);

static int PR_CALLBACK prefChanged(const char *name, void *closure);

void prefEnumerator(const char *name, void *closure)
{
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    if (nsnull == closure) {
        return;
    }
    peStruct *pes = (peStruct *) closure;
    WebShellInitContext *mInitContext =  pes->cx;
    jobject props = pes->obj;
    PRInt32 prefType, intVal;
    PRBool boolVal;
    nsresult rv = NS_ERROR_FAILURE;
    jstring prefName = nsnull;
    jstring prefValue = nsnull;
    PRUnichar *prefValueUni = nsnull;
    nsAutoString prefValueAuto;
    const PRInt32 bufLen = 20;
    char buf[bufLen];
    nsCRT::memset(buf, 0, bufLen);
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID));

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
                                     (jobject) &(mInitContext->shareContext));
}

static int PR_CALLBACK prefChanged(const char *name, void *closure)
{
    if (nsnull == name || nsnull == closure) {
        return NS_ERROR_NULL_POINTER;
    }
    nsresult rv;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    wsPrefChangedEvent *actionEvent = nsnull;
    peStruct *pes = (peStruct *) closure;
    void *voidResult = nsnull;
    jstring prefName;

    if (!(prefName = ::util_NewStringUTF(env, name))) {
        rv = NS_ERROR_NULL_POINTER;
        goto PC_CLEANUP;
    }

    if (!(actionEvent = new wsPrefChangedEvent(name, (peStruct *) closure))) {
        rv = NS_ERROR_NULL_POINTER;
        goto PC_CLEANUP;
    }
    
    voidResult = ::util_PostSynchronousEvent(pes->cx,(PLEvent *) *actionEvent);
    rv = (nsresult) voidResult;
 PC_CLEANUP:

    return rv;
}

wsPrefChangedEvent::wsPrefChangedEvent(const char *prefName, 
                                       peStruct *yourPeStruct) :
    mPrefName(prefName), mPeStruct(yourPeStruct)
{
    
}

void *wsPrefChangedEvent::handleEvent()
{
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    jint result;
    jstring prefName;
#ifdef BAL_INTERFACE
#else
    jclass pcClass = nsnull;
    jmethodID pcMID = nsnull;
    
    if (!(pcClass = env->GetObjectClass(mPeStruct->callback))) {
        return (void*) NS_ERROR_FAILURE;
    }
    if (!(pcMID =env->GetMethodID(pcClass, "prefChanged",
                                  "(Ljava/lang/String;Ljava/lang/Object;)I"))){
        return (void*) NS_ERROR_FAILURE;
    }
    if (!(prefName = ::util_NewStringUTF(env, mPrefName))) {
        return (void*) NS_ERROR_FAILURE;
    }
    result = env->CallIntMethod(mPeStruct->callback, pcMID, prefName, 
                                mPeStruct->obj);
    
#endif
    return (void *) result;
}

wsSetUnicharPrefEvent::wsSetUnicharPrefEvent(const char *prefName, 
					     const jchar *yourPrefValue) :
    mPrefName(prefName), mPrefValue(yourPrefValue)
{
    
}

void *wsSetUnicharPrefEvent::handleEvent()
{
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID));

    nsresult rv = NS_ERROR_FAILURE;
    
    if (!prefs) {
        return (void *) rv;
    }

    rv = prefs->SetUnicharPref(mPrefName, (const PRUnichar *) mPrefValue);

    return (void *) rv;
}


wsSetIntPrefEvent::wsSetIntPrefEvent(const char *prefName, 
				     PRInt32 yourPrefValue) :
    mPrefName(prefName), mPrefValue(yourPrefValue)
{
    
}

void *wsSetIntPrefEvent::handleEvent()
{
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID));

    nsresult rv = NS_ERROR_FAILURE;
    
    if (!prefs) {
        return (void *) rv;
    }

    rv = prefs->SetIntPref(mPrefName, mPrefValue);

    return (void *) rv;
}

wsSetBoolPrefEvent::wsSetBoolPrefEvent(const char *prefName, 
				       PRBool yourPrefValue) :
    mPrefName(prefName), mPrefValue(yourPrefValue)
{
    
}

void *wsSetBoolPrefEvent::handleEvent()
{
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID));

    nsresult rv = NS_ERROR_FAILURE;
    
    if (!prefs) {
        return (void *) rv;
    }

    rv = prefs->SetBoolPref(mPrefName, mPrefValue);

    return (void *) rv;
}

wsGetPrefsEvent::wsGetPrefsEvent(peStruct *yourPeStruct) :
  mPeStruct(yourPeStruct)
{
    
}

void *wsGetPrefsEvent::handleEvent()
{
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID));

    nsresult rv = NS_ERROR_FAILURE;
    
    if (!prefs) {
        return (void *) rv;
    }

    rv = prefs->EnumerateChildren("", prefEnumerator, mPeStruct);

    return (void *) rv;
}


wsRegisterPrefCallbackEvent::wsRegisterPrefCallbackEvent(const char *yourPrefName,
							 peStruct *yourPeStruct) :
  mPrefName(yourPrefName), mPeStruct(yourPeStruct)
{
    
}

void *wsRegisterPrefCallbackEvent::handleEvent()
{
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID));

    nsresult rv = NS_ERROR_FAILURE;
    
    if (!prefs) {
        return (void *) rv;
    }

    rv = prefs->RegisterCallback(mPrefName, prefChanged, mPeStruct);

    return (void *) rv;
}
