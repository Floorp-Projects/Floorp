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
 * PromptActionEvents.cpp
 */

#include "PromptActionEvents.h"

#include "ns_util.h"
#include "nsReadableUtils.h"
#include "nsString2.h" // for nsAutoString

wsPromptUsernameAndPasswordEvent::wsPromptUsernameAndPasswordEvent(NativeBrowserControl *yourInitContext,
                             jobject yourPromptGlobalRef, 
			     wsStringStruct *inStrings,
                             PRUint32 savePassword, 
                             PRUnichar **outUser, 
                             PRUnichar **outPwd, 
                             PRBool *_retval) :
    mInitContext(yourInitContext), mPromptGlobalRef(yourPromptGlobalRef), 
    mInStrings(inStrings), mOutUser(outUser),
    mOutPwd(outPwd), mRetVal(_retval)
{
    
}

void *wsPromptUsernameAndPasswordEvent::handleEvent()
{
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    jboolean result = JNI_FALSE;
    jstring user = nsnull;
    jstring password = nsnull;
    const jchar *userJchar = nsnull;
    const jchar *passwordJchar = nsnull;
    jclass promptClass = nsnull;
    jmethodID mid = nsnull;
    nsAutoString autoUser;
    nsAutoString autoPassword;
    if (!gPromptProperties) {
        return (void *) NS_ERROR_FAILURE;
    }

#ifdef BAL_INTERFACE
#else
    // step two, call the java method.
    if (!(promptClass = env->GetObjectClass(mPromptGlobalRef))) {
        return (void *) NS_ERROR_FAILURE;
    }
    if (!(mid = env->GetMethodID(promptClass, "promptUsernameAndPassword", 
                                 "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;ILjava/util/Properties;)Z"))) {
        return (void *) NS_ERROR_FAILURE;
    }
    result = env->CallBooleanMethod(mPromptGlobalRef, mid, 
				    mInStrings[0].jStr, mInStrings[1].jStr, 
                                    mInStrings[2].jStr, (jint) mSavePassword, 
                                    gPromptProperties);
#endif
    
    // pull userName and password entries out of the properties table
    
    user = (jstring) ::util_GetFromPropertiesObject(env, gPromptProperties,
                                                    USER_NAME_KEY, (jobject)
                                                    &(mInitContext->shareContext));
    if (user) {
        userJchar = ::util_GetStringChars(env, user);
        autoUser = (PRUnichar *) userJchar;
        *mOutUser = ToNewUnicode(autoUser);
        ::util_ReleaseStringChars(env, user, userJchar);
    }
    
    password = (jstring) ::util_GetFromPropertiesObject(env, gPromptProperties,
                                                        PASSWORD_KEY, (jobject)
                                                        &(mInitContext->shareContext));
    if (password) {
        passwordJchar = ::util_GetStringChars(env, password);
        autoPassword = (PRUnichar *) passwordJchar;
        *mOutPwd = ToNewUnicode(autoPassword);
        ::util_ReleaseStringChars(env, password, passwordJchar);
    }
    
    *mRetVal = (result == JNI_TRUE) ? PR_TRUE : PR_FALSE;
    
    return (void *) NS_OK;
}

wsPromptUniversalDialogEvent::wsPromptUniversalDialogEvent(NativeBrowserControl *yourInitContext,
							   jobject yourPromptGlobalRef,
							   wsStringStruct *inStrings,
							   PRUnichar **fieldOne, 
							   PRUnichar **fieldTwo, 
							   PRBool *checkboxState,
							   PRInt32 numButtons,
							   PRInt32 numFields,
							   PRInt32 fieldIsPasswd,
							   PRInt32 *buttonPressed) :
  mInitContext(yourInitContext), mPromptGlobalRef(yourPromptGlobalRef), 
  mInStrings(inStrings), mFieldOne(fieldOne), mFieldTwo(fieldTwo),
  mCheckboxState(checkboxState), mNumButtons(numButtons),
  mNumFields(numFields), mFieldIsPasswd(fieldIsPasswd), 
  mButtonPressed(buttonPressed)
{
    
}

void *wsPromptUniversalDialogEvent::handleEvent()
{
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    jboolean result = JNI_FALSE;
    jclass promptClass = nsnull;
    jmethodID mid = nsnull;

    jstring edit1 = nsnull;
    jstring edit2 = nsnull;
    const jchar *edit1Jchar = nsnull;
    const jchar *edit2Jchar = nsnull;
    nsAutoString autoEdit1;
    nsAutoString autoEdit2;

    if (!gPromptProperties) {
        return (void *) NS_ERROR_FAILURE;
    }

#ifdef BAL_INTERFACE
#else
    // step two, call the java method.
    if (!(promptClass = env->GetObjectClass(mPromptGlobalRef))) {
        return (void *) NS_ERROR_FAILURE;
    }
    if (!(mid = env->GetMethodID(promptClass, "universalDialog", 
                                 "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;IIZLjava/util/Properties;)Z"))) {
        return (void *) NS_ERROR_FAILURE;
    }
    result = env->CallBooleanMethod(mPromptGlobalRef, mid, 
                                    mInStrings[0].jStr, 
                                    mInStrings[1].jStr, 
                                    mInStrings[2].jStr, 
                                    mInStrings[3].jStr, 
                                    mInStrings[4].jStr, 
                                    mInStrings[5].jStr, 
                                    mInStrings[6].jStr, 
                                    mInStrings[7].jStr, 
                                    mInStrings[8].jStr, 
                                    mInStrings[9].jStr, 
                                    (jint) mNumButtons, 
                                    (jint) mNumFields,
                                    (jboolean) mFieldIsPasswd,
                                    gPromptProperties);
#endif
    
    // pull entries out of the properties table
    // editfield1Value, editfield2Value, checkboxState, buttonPressed
    
    if (mFieldOne) {
        edit1 = (jstring) ::util_GetFromPropertiesObject(env, 
                                                         gPromptProperties,
                                                         EDIT_FIELD_1_KEY, 
                                                         (jobject)
                                                         &(mInitContext->shareContext));
        edit1Jchar = ::util_GetStringChars(env, edit1);
        autoEdit1 = (PRUnichar *) edit1Jchar;
        *mFieldOne = ToNewUnicode(autoEdit1);
        ::util_ReleaseStringChars(env, edit1, edit1Jchar);
    }
    
    if (mFieldTwo) {
        edit2 = (jstring) ::util_GetFromPropertiesObject(env, 
                                                         gPromptProperties,
                                                         EDIT_FIELD_2_KEY, 
                                                         (jobject)
                                                         &(mInitContext->shareContext));
        edit2Jchar = ::util_GetStringChars(env, edit2);
        autoEdit2 = (PRUnichar *) edit2Jchar;
        *mFieldTwo = ToNewUnicode(autoEdit2);
        ::util_ReleaseStringChars(env, edit2, edit2Jchar);
    }

    if (mCheckboxState) {
        *mCheckboxState = 
            (JNI_TRUE == ::util_GetBoolFromPropertiesObject(env, 
                                                            gPromptProperties,
                                                            CHECKBOX_STATE_KEY, 
                                                            (jobject)
                                                            &(mInitContext->shareContext)))
            ? PR_TRUE : PR_FALSE;
    }
    
    if (mButtonPressed) {
        *mButtonPressed = (PRInt32) 
            ::util_GetIntFromPropertiesObject(env, gPromptProperties,
                                              BUTTON_PRESSED_KEY, 
                                              (jobject)
                                              &(mInitContext->shareContext));
    }
    
    return (void *) NS_OK;
}
