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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2003 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author: Kyle Yuan <kyle.yuan@sun.com>
 *
 * Contributor(s):
 *
 */

#include "PromptService.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIServiceManager.h"
#include "nsIWebBrowserChrome.h"
#include "nsIEmbeddingSiteWindow.h"
#include "CBrowserContainer.h"
#include "PromptActionEvents.h"

int processEventLoop(WebShellInitContext * initContext);

//*****************************************************************************
// PromptService
//*****************************************************************************   

NS_IMPL_ISUPPORTS1(PromptService, nsIPromptService)

PromptService::PromptService() :
    mWWatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID))
{
}

PromptService::~PromptService()
{
}

CBrowserContainer *
PromptService::BrowserContainerForDOMWindow(nsIDOMWindow *aWindow)
{
    nsCOMPtr<nsIWebBrowserChrome> chrome;
    CBrowserContainer *val = 0;
    
    if (mWWatch) {
        nsCOMPtr<nsIDOMWindow> fosterParent;
        if (!aWindow) { // it will be a dependent window. try to find a foster parent.
            mWWatch->GetActiveWindow(getter_AddRefs(fosterParent));
            aWindow = fosterParent;
        }
        mWWatch->GetChromeForWindow(aWindow, getter_AddRefs(chrome));
    }
    
    if (chrome) {
        nsCOMPtr<nsIEmbeddingSiteWindow> site(do_QueryInterface(chrome));
        if (site) {
            void *ret;
            site->GetSiteWindow(&ret);
            val = (CBrowserContainer *)ret;
        }
    }
    
    return val;
}

nsresult 
PromptService::PromptUniversalDialog(nsIDOMWindow *parent, 
                                     const PRUnichar *dialogTitle, 
                                     const PRUnichar *text, 
                                     const PRUnichar *checkMsg, 
                                     const PRUnichar *button0Title, 
                                     const PRUnichar *button1Title, 
                                     const PRUnichar *button2Title, 
                                     const PRUnichar *button3Title, 
                                     const PRUnichar *editfield1Msg, 
                                     const PRUnichar *editfield2Msg, 
                                     PRUnichar **editfield1Value, 
                                     PRUnichar **editfield2Value, 
                                     PRInt32 numButtons, 
                                     PRInt32 numFields, 
                                     PRInt32 fieldIsPasswd,
                                     PRBool *checkState,
                                     PRInt32 *buttonPressed)
{
    nsresult rv = NS_ERROR_FAILURE;

    // if the user hasn't given us a prompt, oh well
    CBrowserContainer* bc = BrowserContainerForDOMWindow(parent);

    // PENDING(edburns): uniformly apply checks for this throughout the
    // code
    PR_ASSERT(bc->mInitContext);
    PR_ASSERT(bc->mInitContext->initComplete); 
    
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

    // try to initialize the properties object for basic auth and cookies
    if (!gPromptProperties) {
        gPromptProperties = 
            ::util_CreatePropertiesObject(env, (jobject)
                                          &(bc->mInitContext->shareContext));
        if (!gPromptProperties) {
            printf("Error: can't create properties object for authentitication");
            return NS_ERROR_NULL_POINTER;
        }
    }
    else {
        ::util_ClearPropertiesObject(env, gPromptProperties, (jobject) 
                                     &(bc->mInitContext->shareContext));
    }

    wsStringStruct strings[10] = { 
        {text, nsnull},
        {dialogTitle, nsnull},
        {nsnull, nsnull},
        {checkMsg, nsnull},
        {button0Title, nsnull},
        {button1Title, nsnull},
        {button2Title, nsnull},
        {button3Title, nsnull},
        {editfield1Msg, nsnull},
        {editfield2Msg, nsnull} };

    rv = ::util_CreateJstringsFromUnichars(strings, 10);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: UniversalDialog: can't create jstrings from Unichars");
        ::util_DeleteJstringsFromUnichars(strings, 10);
        return rv;
    }
    
    jstring defaultValue = nsnull;
    if (editfield1Value) {
        nsAutoString autoStr(*editfield1Value);
        defaultValue = ::util_NewString(env, (const jchar *) autoStr.get(), autoStr.Length());
        ::util_StoreIntoPropertiesObject(env, gPromptProperties, EDIT_FIELD_1_KEY, defaultValue, 
                                     (jobject) &(bc->mInitContext->shareContext));
    }
    if (editfield2Value) {
        nsAutoString autoStr(*editfield2Value);
        defaultValue = ::util_NewString(env, (const jchar *) autoStr.get(), autoStr.Length());
        ::util_StoreIntoPropertiesObject(env, gPromptProperties, EDIT_FIELD_2_KEY, defaultValue, 
                                     (jobject) &(bc->mInitContext->shareContext));
    }

    wsPromptUniversalDialogEvent *actionEvent = nsnull;
    if (!(actionEvent = new wsPromptUniversalDialogEvent(bc->mInitContext, 
                                                         bc->mPrompt, 
                                                         strings, 
                                                         editfield1Value,
                                                         editfield2Value,
                                                         numButtons, 
                                                         numFields, 
                                                         fieldIsPasswd))) {
        ::util_ThrowExceptionToJava(env, "Exception: UniversalDialog: can't create wsPromptUniversalDialogEvent");
        ::util_DeleteJstringsFromUnichars(strings, 10);
        return NS_ERROR_NULL_POINTER;
    }
    // the out params to this method are set in wsPromptUsernameAndPasswordEvent::handleEvent()
    ::util_PostSynchronousEvent(bc->mInitContext, (PLEvent *) *actionEvent);
    ::util_DeleteJstringsFromUnichars(strings, 10);

    while (1) 
    {
        processEventLoop(bc->mInitContext);
        ::PR_Sleep(PR_INTERVAL_NO_WAIT);

        jstring finished = (jstring) ::util_GetFromPropertiesObject(env, 
                                                             gPromptProperties,
                                                             FINISHED_KEY, 
                                                             (jobject)
                                                             &(bc->mInitContext->shareContext));
        if (finished) {
            const jchar *finishedJchar = ::util_GetStringChars(env, finished);
            nsAutoString strValue((const PRUnichar *) finishedJchar);
            ::util_ReleaseStringChars(env, finished, finishedJchar);
            if (strValue.Equals(NS_LITERAL_STRING("true"))) {
                printf("***** Finished!\n");
                break;
            }
        }
    }

    // pull entries out of the properties table
    // editfield1Value, editfield2Value, checkboxState, buttonPressed
    jstring edit1 = nsnull;
    jstring edit2 = nsnull;
    const jchar *edit1Jchar = nsnull;
    const jchar *edit2Jchar = nsnull;
    nsAutoString autoEdit1;
    nsAutoString autoEdit2;
    
    if (editfield1Value) {
        edit1 = (jstring) ::util_GetFromPropertiesObject(env, 
                                                         gPromptProperties,
                                                         EDIT_FIELD_1_KEY, 
                                                         (jobject)
                                                         &(bc->mInitContext->shareContext));
        edit1Jchar = ::util_GetStringChars(env, edit1);
        autoEdit1 = (PRUnichar *) edit1Jchar;
        *editfield1Value = ToNewUnicode(autoEdit1);
        ::util_ReleaseStringChars(env, edit1, edit1Jchar);
    }
    
    if (editfield2Value) {
        edit2 = (jstring) ::util_GetFromPropertiesObject(env, 
                                                         gPromptProperties,
                                                         EDIT_FIELD_2_KEY, 
                                                         (jobject)
                                                         &(bc->mInitContext->shareContext));
        edit2Jchar = ::util_GetStringChars(env, edit2);
        autoEdit2 = (PRUnichar *) edit2Jchar;
        *editfield2Value = ToNewUnicode(autoEdit2);
        ::util_ReleaseStringChars(env, edit2, edit2Jchar);
    }

    if (checkState) {
        *checkState = 
            (JNI_TRUE == ::util_GetBoolFromPropertiesObject(env, 
                                                            gPromptProperties,
                                                            CHECKBOX_STATE_KEY, 
                                                            (jobject)
                                                            &(bc->mInitContext->shareContext)))
            ? PR_TRUE : PR_FALSE;
    }
    
    if (buttonPressed) {
        *buttonPressed = (PRInt32) 
            ::util_GetIntFromPropertiesObject(env, gPromptProperties,
                                              BUTTON_PRESSED_KEY, 
                                              (jobject)
                                              &(bc->mInitContext->shareContext));
    }

    return NS_OK;
}

NS_IMETHODIMP PromptService::Alert(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
                                    const PRUnichar *text)
{
    nsXPIDLString buttonOKStr;
    util_GetLocaleString("OK", getter_Copies(buttonOKStr));

    nsresult rv = PromptUniversalDialog(parent, 
        dialogTitle, 
        text, 
        nsnull, 
        buttonOKStr.get(), nsnull, nsnull, nsnull, 
        nsnull, nsnull, 
        nsnull, nsnull, 
        1, 
        0, 
        PR_FALSE,
        nsnull, 
        nsnull);

    return rv;
}

NS_IMETHODIMP PromptService::AlertCheck(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
                                         const PRUnichar *text,
                                         const PRUnichar *checkMsg, PRBool *checkValue)
{
    nsXPIDLString buttonOKStr;
    util_GetLocaleString("OK", getter_Copies(buttonOKStr));

    nsresult rv = PromptUniversalDialog(parent, 
        dialogTitle, 
        text, 
        checkMsg, 
        buttonOKStr.get(), nsnull, nsnull, nsnull, 
        nsnull, nsnull, 
        nsnull, nsnull, 
        1, 
        0, 
        PR_FALSE,
        checkValue, 
        nsnull);

    return rv;
}

NS_IMETHODIMP PromptService::Confirm(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
                                      const PRUnichar *text,
                                      PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    nsXPIDLString buttonOKStr, buttonCancelStr;
    util_GetLocaleString("OK", getter_Copies(buttonOKStr));
    util_GetLocaleString("Cancel", getter_Copies(buttonCancelStr));
    
    PRInt32 buttonPressed;
    nsresult rv = PromptUniversalDialog(parent, 
        dialogTitle, 
        text, 
        nsnull, 
        buttonOKStr.get(), buttonCancelStr.get(), nsnull, nsnull, 
        nsnull, nsnull, 
        nsnull, nsnull, 
        2, 
        0, 
        PR_FALSE,
        nsnull, 
        &buttonPressed);

    *_retval = (buttonPressed == 0) ? PR_TRUE : PR_FALSE;    

    return rv;
}

NS_IMETHODIMP PromptService::ConfirmCheck(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
                                           const PRUnichar *text,
                                           const PRUnichar *checkMsg, PRBool *checkValue,
                                           PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(checkValue);
    NS_ENSURE_ARG_POINTER(_retval);

    nsXPIDLString buttonOKStr, buttonCancelStr;
    util_GetLocaleString("OK", getter_Copies(buttonOKStr));
    util_GetLocaleString("Cancel", getter_Copies(buttonCancelStr));
    
    PRInt32 buttonPressed;
    nsresult rv = PromptUniversalDialog(parent, 
        dialogTitle, 
        text, 
        checkMsg, 
        buttonOKStr.get(), buttonCancelStr.get(), nsnull, nsnull, 
        nsnull, nsnull, 
        nsnull, nsnull, 
        2, 
        0, 
        PR_FALSE,
        checkValue, 
        &buttonPressed);

    *_retval = (buttonPressed == 0) ? PR_TRUE : PR_FALSE;    

    return rv;
}

NS_IMETHODIMP PromptService::ConfirmEx(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
                                        const PRUnichar *text,
                                        PRUint32 buttonFlags,
                                        const PRUnichar *button0Title,
                                        const PRUnichar *button1Title,
                                        const PRUnichar *button2Title,
                                        const PRUnichar *checkMsg, PRBool *checkValue,
                                        PRInt32 *buttonPressed)
{
    NS_ENSURE_ARG_POINTER(buttonPressed);

    // the maximum number of buttons we expect
    const PRInt32 maxButtons = 3;
    const PRUnichar* buttonStrings[maxButtons] =
        { button0Title, button1Title, button2Title };
    const PRUnichar* buttonTitles[maxButtons];

    PRInt32 numberButtons = 0;
    // this code cribbed from nsPromptService.cpp::ConfirmEx

    nsXPIDLString buttonTextStr[maxButtons];

    for (PRInt32 i = 0; i < maxButtons; i++) { 
        
        nsXPIDLString buttonTextStr;
        const PRUnichar* buttonText = 0;
        switch (buttonFlags & 0xff) {
        case BUTTON_TITLE_OK:
            util_GetLocaleString("OK", getter_Copies(buttonTextStr));
            break;
        case BUTTON_TITLE_CANCEL:
            util_GetLocaleString("Cancel", getter_Copies(buttonTextStr));
            break;
        case BUTTON_TITLE_YES:
            util_GetLocaleString("Yes", getter_Copies(buttonTextStr));
            break;
        case BUTTON_TITLE_NO:
            util_GetLocaleString("No", getter_Copies(buttonTextStr));
            break;
        case BUTTON_TITLE_SAVE:
            util_GetLocaleString("Save", getter_Copies(buttonTextStr));
            break;
        case BUTTON_TITLE_DONT_SAVE:
            util_GetLocaleString("DontSave", getter_Copies(buttonTextStr));
            break;
        case BUTTON_TITLE_REVERT:
            util_GetLocaleString("Revert", getter_Copies(buttonTextStr));
            break;
        case BUTTON_TITLE_IS_STRING:
            buttonText = buttonStrings[i];
            break;
        }
        if (!buttonText)
            buttonText = buttonTextStr.get();

        if (buttonText) {
            buttonTitles[numberButtons] = buttonText;
            ++numberButtons;
        }
        buttonFlags >>= 8;
    }
    
    nsresult rv = PromptUniversalDialog(parent, 
        dialogTitle, 
        text, 
        checkMsg, 
        buttonTitles[0], buttonTitles[1], buttonTitles[2], nsnull, 
        nsnull, nsnull, 
        nsnull, nsnull, 
        numberButtons, 
        0, 
        PR_FALSE,
        checkValue, 
        buttonPressed);

    return rv;
}

NS_IMETHODIMP PromptService::Prompt(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
                                     const PRUnichar *text, PRUnichar **value,
                                     const PRUnichar *checkMsg, PRBool *checkValue,
                                     PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    nsXPIDLString buttonOKStr, buttonCancelStr;
    util_GetLocaleString("OK", getter_Copies(buttonOKStr));
    util_GetLocaleString("Cancel", getter_Copies(buttonCancelStr));

    PRInt32 buttonPressed;
    nsresult rv = PromptUniversalDialog(parent, 
        dialogTitle, 
        text, 
        checkMsg, 
        buttonOKStr.get(), buttonCancelStr.get(), nsnull, nsnull, 
        nsnull, nsnull, 
        value, nsnull, 
        2, 
        1, 
        PR_FALSE,
        checkValue, 
        &buttonPressed);

    *_retval = (buttonPressed == 0) ? PR_TRUE : PR_FALSE;    

    return NS_OK;
}

NS_IMETHODIMP PromptService::PromptUsernameAndPassword(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
                                                        const PRUnichar *text,
                                                        PRUnichar **username, PRUnichar **password,
                                                        const PRUnichar *checkMsg, PRBool *checkValue,
                                                        PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    nsresult rv = NS_ERROR_FAILURE;

    // if the user hasn't given us a prompt, oh well
    CBrowserContainer* bc = BrowserContainerForDOMWindow(parent);

    // PENDING(edburns): uniformly apply checks for this throughout the
    // code
    PR_ASSERT(bc->mInitContext);
    PR_ASSERT(bc->mInitContext->initComplete); 

    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    wsPromptUsernameAndPasswordEvent *actionEvent = nsnull;

    wsStringStruct strings[3] = { 
        {dialogTitle, nsnull},
        {text, nsnull},
        {nsnull, nsnull} };

    rv = ::util_CreateJstringsFromUnichars(strings, 3);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: PromptUserNameAndPassword: can't create jstrings from Unichars");
        goto PUAP_CLEANUP;
    }

    // try to initialize the properties object for basic auth and cookies
    if (!gPromptProperties) {
        gPromptProperties = 
            ::util_CreatePropertiesObject(env, (jobject)
                                          &(bc->mInitContext->shareContext));
        if (!gPromptProperties) {
            printf("Error: can't create properties object for authentitication");
            rv = NS_ERROR_NULL_POINTER;
            goto PUAP_CLEANUP;
        }
    }
    else {
        ::util_ClearPropertiesObject(env, gPromptProperties, (jobject) 
                                     &(bc->mInitContext->shareContext));
    }
    
    if (!(actionEvent = new wsPromptUsernameAndPasswordEvent(bc->mInitContext, bc->mPrompt, 
                                          strings, username, password, _retval))) {
        ::util_ThrowExceptionToJava(env, "Exception: PromptUserNameAndPassword: can't create wsPromptUsernameAndPasswordEvent");
        rv = NS_ERROR_NULL_POINTER;
        goto PUAP_CLEANUP;
    }
    // the out params to this method are set in wsPromptUsernameAndPasswordEvent::handleEvent()
    ::util_PostSynchronousEvent(bc->mInitContext, 
                                (PLEvent *) *actionEvent);
    rv = NS_OK;
 PUAP_CLEANUP:

    ::util_DeleteJstringsFromUnichars(strings, 3);

    return rv;
}

NS_IMETHODIMP PromptService::PromptPassword(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
                                             const PRUnichar *text,
                                             PRUnichar **password,
                                             const PRUnichar *checkMsg, PRBool *checkValue,
                                             PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    
    printf("###PromptPassword\n");
    *_retval = PR_TRUE;        

    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PromptService::Select(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
                                     const PRUnichar *text, PRUint32 count,
                                     const PRUnichar **selectList, PRInt32 *outSelection,
                                     PRBool *_retval)
{
    printf("###Select\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}
