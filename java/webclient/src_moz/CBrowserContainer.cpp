/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 *
 * Contributor(s): Ashutosh Kulkarni <ashuk@eng.sun.com>
 *                 Ed Burns <edburns@acm.org>
 *      Jason Mawdsley <jason@macadamian.com>
 *      Louis-Philippe Gagnon <louisphilippe@macadamian.com>
 */

#include <limits.h>
#include "CBrowserContainer.h"
#include "nsCWebBrowser.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserFocus.h"
#include "nsIRequest.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMWindow.h"
#include "nsIDocShellTreeItem.h"
#include "nsIPromptService.h"

#include "prprf.h" // for PR_snprintf
#include "nsReadableUtils.h" 
#include "nsXPIDLString.h" 
#include "nsFileSpec.h" // for nsAutoCString

#include "dom_util.h"

#include "PromptActionEvents.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

jobject gPromptProperties = nsnull;

PRInt32 CBrowserContainer::mInstanceCount = 0;

CBrowserContainer::CBrowserContainer(nsIWebBrowser *pOwner, JNIEnv *env,
                                     WebShellInitContext *yourInitContext) :
    m_pOwner(pOwner), mJNIEnv(env), mInitContext(yourInitContext), 
    mDocTarget(nsnull), mMouseTarget(nsnull), mPrompt(nsnull),
    mDomEventTarget(nsnull), inverseDepth(-1), 
    properties(nsnull), currentDOMEvent(nsnull)
{
  	NS_INIT_ISUPPORTS();
    // initialize the string constants (including properties keys)
    if (!util_StringConstantsAreInitialized()) {
        util_InitStringConstants();
    }
    mInstanceCount++;
}


CBrowserContainer::~CBrowserContainer()
{
    m_pOwner = nsnull;
    mJNIEnv = nsnull;
    mInitContext = nsnull;
    mDocTarget = nsnull;
    mMouseTarget = nsnull;
    mDomEventTarget = nsnull;
    inverseDepth = -1;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    if (properties) {
        ::util_DeleteGlobalRef(env, properties);
    }
    properties = nsnull;
    currentDOMEvent = nsnull;
    mInstanceCount--;
}


///////////////////////////////////////////////////////////////////////////////
// nsISupports implementation

NS_IMPL_ADDREF(CBrowserContainer)
NS_IMPL_RELEASE(CBrowserContainer)

NS_INTERFACE_MAP_BEGIN(CBrowserContainer)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserChrome)
	NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
	NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
	NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
	NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeOwner)
	NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
	NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
	NS_INTERFACE_MAP_ENTRY(nsIWebShellContainer)
    NS_INTERFACE_MAP_ENTRY(nsIAuthPrompt)
	NS_INTERFACE_MAP_ENTRY(nsIPrompt)
	NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
	NS_INTERFACE_MAP_ENTRY(nsIDOMMouseListener)
	NS_INTERFACE_MAP_ENTRY(wcIBrowserContainer)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END


///////////////////////////////////////////////////////////////////////////////
// nsIInterfaceRequestor

NS_IMETHODIMP CBrowserContainer::GetInterface(const nsIID & uuid, void * *result)
{
    return QueryInterface(uuid, result);
}


///////////////////////////////////////////////////////////////////////////////
// nsIAuthPrompt

/* boolean prompt (in wstring dialogTitle, in wstring text, in wstring passwordRealm, in PRUint32 savePassword, in wstring defaultText, out wstring result); */
NS_IMETHODIMP CBrowserContainer::Prompt(const PRUnichar *dialogTitle, 
                                        const PRUnichar *text, 
                                        const PRUnichar *passwordRealm, 
                                        PRUint32 savePassword, 
                                        const PRUnichar *defaultText, 
                                        PRUnichar **result, 
                                        PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean promptPassword (in wstring dialogTitle, in wstring text, in wstring passwordRealm, in PRUint32 savePassword, out wstring pwd); */
NS_IMETHODIMP CBrowserContainer::PromptPassword(const PRUnichar *dialogTitle, 
                                                const PRUnichar *text, 
                                                const PRUnichar *passwordRealm, 
                                                PRUint32 savePassword, 
                                                PRUnichar **pwd, 
                                                PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean promptUsernameAndPassword (in wstring dialogTitle, in wstring text, in wstring passwordRealm, in PRUint32 savePassword, out wstring user, out wstring pwd); */
NS_IMETHODIMP CBrowserContainer::PromptUsernameAndPassword(const PRUnichar *dialogTitle,
                                                           const PRUnichar *text, 
                                                           const PRUnichar *passwordRealm, 
                                                           PRUint32 savePassword, 
                                                           PRUnichar **user, 
                                                           PRUnichar **pwd, 
                                                           PRBool *_retval)
{
    nsresult rv = NS_ERROR_FAILURE;

    // if the user hasn't given us a prompt, oh well
    if	 (!mPrompt) {
        return NS_OK;
    }

    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    wsPromptUsernameAndPasswordEvent *actionEvent = nsnull;

    wsStringStruct strings[3] = { 
        {dialogTitle, nsnull},
        {text, nsnull},
        {passwordRealm, nsnull} };

    rv = ::util_CreateJstringsFromUnichars(strings, 3);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: PromptUserNameAndPassword: can't create jstrings from Unichars");
        goto PUAP_CLEANUP;
    }


    // PENDING(edburns): uniformly apply checks for this throughout the
    // code
    PR_ASSERT(mInitContext);
    PR_ASSERT(mInitContext->initComplete); 

    // try to initialize the properties object for basic auth and cookies
    if (!gPromptProperties) {
        gPromptProperties = 
            ::util_CreatePropertiesObject(env, (jobject)
                                          &(mInitContext->shareContext));
        if (!gPromptProperties) {
            printf("Error: can't create properties object for authentitication");
            rv = NS_ERROR_NULL_POINTER;
            goto PUAP_CLEANUP;
        }
    }
    else {
        ::util_ClearPropertiesObject(env, gPromptProperties, (jobject) 
                                     &(mInitContext->shareContext));
    }
    
    if (!(actionEvent = new wsPromptUsernameAndPasswordEvent(mInitContext, mPrompt, 
                                          strings, savePassword,
                                          user, pwd, _retval))) {
        ::util_ThrowExceptionToJava(env, "Exception: PromptUserNameAndPassword: can't create wsPromptUsernameAndPasswordEvent");
        rv = NS_ERROR_NULL_POINTER;
        goto PUAP_CLEANUP;
    }
    // the out params to this method are set in wsPromptUsernameAndPasswordEvent::handleEvent()
    ::util_PostSynchronousEvent(mInitContext, 
                                (PLEvent *) *actionEvent);
    rv = NS_OK;
 PUAP_CLEANUP:

    ::util_DeleteJstringsFromUnichars(strings, 3);

    return rv;
}


///////////////////////////////////////////////////////////////////////////////
// nsIPrompt

NS_IMETHODIMP CBrowserContainer::ConfirmEx(const PRUnichar *dialogTitle, 
                                           const PRUnichar *text, 
                                           PRUint32 buttonFlags, 
                                           const PRUnichar *button0Title, 
                                           const PRUnichar *button1Title, 
                                           const PRUnichar *button2Title, 
                                           const PRUnichar *checkMsg, 
                                           PRBool *checkValue, 
                                           PRInt32 *buttonPressed)
{
    /** dialogTitle: Confirm
        text: the site yahoo.com wants to set a cookie, blah blah
        buttonFlags: 1027
        button0Title: ""
        button1Title: ""
        button2Title: ""
        checkMsg: Remember This Decision
    */
    nsresult rv = NS_ERROR_FAILURE;
    printf("debug: edburns: CBrowserContainer::ConfirmEx()\n");
    
    // if the user hasn't given us a prompt, oh well
    if	 (!mPrompt) {
        return NS_OK;
    }
    
    PRInt32 i;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    wsPromptUniversalDialogEvent *actionEvent = nsnull;
    
    // the offset, into the following strings array, of the first button;
    const PRInt32 buttonOffset = 4; 
    // the maximum number of buttons we expect
    const PRInt32 maxButtons = 3;

    wsStringStruct strings[10] = { 
        {text, nsnull},
        {dialogTitle, nsnull},
        {nsnull, nsnull},
        {checkMsg, nsnull},
        {button0Title, nsnull},
        {button1Title, nsnull},
        {button2Title, nsnull},
        {nsnull, nsnull},
        {nsnull, nsnull},
        {nsnull, nsnull} };

    const PRUnichar* buttonStrings[] = { button0Title, button1Title, 
                                         button2Title };

    // This array contains the button strings that are dynamically
    // allocated and need to be freed.
    const PRUnichar *buttonStringsToFree[maxButtons];
    memset(buttonStringsToFree, nsnull, 
           maxButtons * sizeof(PRUnichar *));
    

    PRInt32 numberButtons = 0;
    // this code cribbed from nsPromptService.cpp::ConfirmEx

    for (i = 0; i < maxButtons; i++) { 
        
        nsXPIDLString buttonTextStr;
        nsString tempStr;
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
        if (!buttonText && buttonTextStr.get()) {
            // This means the button text did not come from the argument
            // list to this method.  Rather, it came from the
            // localization bundle.  In this case, we must copy the
            // string and free it.
            tempStr = buttonTextStr.get();
            strings[buttonOffset + i].uniStr = ToNewUnicode(tempStr);
            buttonStringsToFree[i] = strings[buttonOffset + i].uniStr;
        }
        else {
            // This means the button text DID come from the argument
            // list, and we do not need to free it!
            strings[buttonOffset + i].uniStr = buttonTextStr.get();
        }
        
        if (strings[buttonOffset + i].uniStr) {
            ++numberButtons;
        }
        buttonFlags >>= 8;
    }
    
    rv = ::util_CreateJstringsFromUnichars(strings, 10);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: ConfirmEx: can't create jstrings from Unichars");
        goto UD_CLEANUP;
    }


    // PENDING(edburns): uniformly apply checks for this throughout the
    // code
    PR_ASSERT(mInitContext);
    PR_ASSERT(mInitContext->initComplete); 

    // try to initialize the properties object for basic auth and cookies
    if (!gPromptProperties) {
        gPromptProperties = 
            ::util_CreatePropertiesObject(env, (jobject)
                                          &(mInitContext->shareContext));
        if (!gPromptProperties) {
            printf("Error: can't create properties object for ConfirmEx");
            rv = NS_ERROR_NULL_POINTER;
            goto UD_CLEANUP;
        }
    }
    else {
        ::util_ClearPropertiesObject(env, gPromptProperties, (jobject) 
                                     &(mInitContext->shareContext));
    }
    
    if (!(actionEvent = new wsPromptUniversalDialogEvent(mInitContext, 
                                                         mPrompt, 
                                                         strings, 
                                                         nsnull,
                                                         nsnull,
                                                         checkValue,
                                                         numberButtons, 
                                                         0, 
                                                         PR_FALSE,
                                                         buttonPressed))) {
        ::util_ThrowExceptionToJava(env, "Exception: ConfirmEx: can't create wsPromptUniversalDialogEvent");
        rv = NS_ERROR_NULL_POINTER;
        goto UD_CLEANUP;
    }
    // the out params to this method are set in wsPromptUsernameAndPasswordEvent::handleEvent()
    ::util_PostSynchronousEvent(mInitContext, (PLEvent *) *actionEvent);
    
    rv = NS_OK;
 UD_CLEANUP:
    
    ::util_DeleteJstringsFromUnichars(strings, 10);
    // free any button strings copied from localized strings
    for (i = 0; i < maxButtons; i++) { 
        if (buttonStringsToFree[i]) {
            nsMemory::Free((void *) buttonStringsToFree[i]);
        }
    }
    
    return rv;
}


/* void alert (in wstring text); */
NS_IMETHODIMP CBrowserContainer::Alert(const PRUnichar *dialogTitle, 
                                       const PRUnichar *text)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CBrowserContainer::AlertCheck(const PRUnichar *dialogTitle, 
                                            const PRUnichar *text, 
                                            const PRUnichar *checkMsg, 
                                            PRBool *checkValue)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean confirm (in wstring text); */
NS_IMETHODIMP CBrowserContainer::Confirm(const PRUnichar *dialogTitle,
                                         const PRUnichar *text, 
                                         PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean confirmCheck (in wstring text, in wstring checkMsg, out boolean checkValue); */
NS_IMETHODIMP CBrowserContainer::ConfirmCheck(const PRUnichar *dialogTitle,
                                              const PRUnichar *text, 
                                              const PRUnichar *checkMsg, 
                                              PRBool *checkValue, 
                                              PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean prompt (in wstring text, in wstring defaultText, out wstring result); */
NS_IMETHODIMP CBrowserContainer::Prompt(const PRUnichar *dialogTitle, 
                                        const PRUnichar *text, 
                                        PRUnichar **value, 
                                        const PRUnichar *checkMsg, 
                                        PRBool *checkValue, 
                                        PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean promptUsernameAndPassword (in wstring text, out wstring user, out wstring pwd); */
NS_IMETHODIMP CBrowserContainer::PromptUsernameAndPassword(const PRUnichar *dialogTitle, 
                                                           const PRUnichar *text, 
                                                           PRUnichar **username, 
                                                           PRUnichar **password, 
                                                           const PRUnichar *checkMsg, 
                                                           PRBool *checkValue, 
                                                           PRBool *_retval)
{
    // We Implement the PromptUsernameAndPassword as declared in nsIAuthPrompt
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* boolean promptPassword (in wstring text, in wstring title, out wstring pwd); */
NS_IMETHODIMP CBrowserContainer::PromptPassword(const PRUnichar *dialogTitle, 
                                                const PRUnichar *text, 
                                                PRUnichar **password, 
                                                const PRUnichar *checkMsg, 
                                                PRBool *checkValue, 
                                                PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean select (in wstring inDialogTitle, in wstring inMsg, in PRUint32 inCount, [array, size_is (inCount)] in wstring inList, out long outSelection); */
NS_IMETHODIMP CBrowserContainer::Select(const PRUnichar *inDialogTitle, const PRUnichar *inMsg, PRUint32 inCount, const PRUnichar **inList, PRInt32 *outSelection, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

#if 0
NS_IMETHODIMP CBrowserContainer::UniversalDialog(const PRUnichar *inTitleMessage, 
                                   const PRUnichar *inDialogTitle, 
                                   const PRUnichar *inMsg, 
                                   const PRUnichar *inCheckboxMsg, 
                                   const PRUnichar *inButton0Text, 
                                   const PRUnichar *inButton1Text, 
                                   const PRUnichar *inButton2Text, 
                                   const PRUnichar *inButton3Text, 
                                   const PRUnichar *inEditfield1Msg, 
                                   const PRUnichar *inEditfield2Msg, 
                                   PRUnichar **inoutEditfield1Value, 
                                   PRUnichar **inoutEditfield2Value, 
                                   const PRUnichar *inIConURL, 
                                   PRBool *inoutCheckboxState, 
                                   PRInt32 inNumberButtons, 
                                   PRInt32 inNumberEditfields, 
                                   PRInt32 inEditField1Password, 
                                   PRInt32 *outButtonPressed)
{
    nsresult rv = NS_ERROR_FAILURE;
    printf("debug: edburns: CBrowserContainer::UniversalDialog()\n");
    
    // if the user hasn't given us a prompt, oh well
    if	 (!mPrompt) {
        return NS_OK;
    }
    
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    wsPromptUniversalDialogEvent *actionEvent = nsnull;

    wsStringStruct strings[10] = { 
        {inTitleMessage, nsnull},
        {inDialogTitle, nsnull},
        {inMsg, nsnull},
        {inCheckboxMsg, nsnull},
        {inButton0Text, nsnull},
        {inButton1Text, nsnull},
        {inButton2Text, nsnull},
        {inButton3Text, nsnull},
        {inEditfield1Msg, nsnull},
        {inEditfield2Msg, nsnull} };

    rv = ::util_CreateJstringsFromUnichars(strings, 10);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: UniversalDialog: can't create jstrings from Unichars");
        goto UD_CLEANUP;
    }


    // PENDING(edburns): uniformly apply checks for this throughout the
    // code
    PR_ASSERT(mInitContext);
    PR_ASSERT(mInitContext->initComplete); 

    // try to initialize the properties object for basic auth and cookies
    if (!gPromptProperties) {
        gPromptProperties = 
            ::util_CreatePropertiesObject(env, (jobject)
                                          &(mInitContext->shareContext));
        if (!gPromptProperties) {
            printf("Error: can't create properties object for authentitication");
            rv = NS_ERROR_NULL_POINTER;
            goto UD_CLEANUP;
        }
    }
    else {
        ::util_ClearPropertiesObject(env, gPromptProperties, (jobject) 
                                     &(mInitContext->shareContext));
    }
    
    if (!(actionEvent = new wsPromptUniversalDialogEvent(mInitContext, 
                                                         mPrompt, 
                                                         strings, 
                                                         inoutEditfield1Value,
                                                         inoutEditfield2Value,
                                                         inoutCheckboxState,
                                                         inNumberButtons, 
                                                         inNumberEditfields, 
                                                         inEditField1Password,
                                                         outButtonPressed))) {
        ::util_ThrowExceptionToJava(env, "Exception: UniversalDialog: can't create wsPromptUniversalDialogEvent");
        rv = NS_ERROR_NULL_POINTER;
        goto UD_CLEANUP;
    }
    // the out params to this method are set in wsPromptUsernameAndPasswordEvent::handleEvent()
    ::util_PostSynchronousEvent(mInitContext, (PLEvent *) *actionEvent);

    rv = NS_OK;
 UD_CLEANUP:

    ::util_DeleteJstringsFromUnichars(strings, 10);

    return rv;
}
#endif

///////////////////////////////////////////////////////////////////////////////
// nsIWebProgressListener

NS_IMETHODIMP CBrowserContainer::OnStateChange(nsIWebProgress *aWebProgress, 
                                               nsIRequest *aRequest, 
                                               PRUint32 aStateFlags, 
                                               PRUint32 aStatus)
{
    nsCAutoString name;
    nsAutoString uname;
    nsCOMPtr<nsIDOMWindow> domWin;
    nsresult rv;

    if (NS_FAILED(rv = aRequest->GetName(name))) {
        return rv;
    }

    uname.AssignWithConversion(name.get());

    // 
    // document states
    //
    if ((aStateFlags & STATE_START) && (aStateFlags & STATE_IS_DOCUMENT)) {
        if (NS_FAILED(rv =aWebProgress->GetDOMWindow(getter_AddRefs(domWin)))||
            !domWin) {
            return rv;
        }
        
        domWin->GetDocument(getter_AddRefs(mInitContext->currentDocument));
        doStartDocumentLoad(uname.get());
    }
    if ((aStateFlags & STATE_STOP) && (aStateFlags & STATE_IS_DOCUMENT)) {
        doEndDocumentLoad(aWebProgress);
    }
    //
    // request states
    //
    if ((aStateFlags & STATE_START) && (aStateFlags & STATE_IS_REQUEST)) {
        doStartURLLoad(uname.get());
    }
    if ((aStateFlags & STATE_STOP) && (aStateFlags & STATE_IS_REQUEST)) {
        doEndURLLoad(uname.get());
    }
    if (aStateFlags & STATE_REDIRECTING) {
        printf("debug: edburns: STATE_REDIRECTING\n");
    }
    if (aStateFlags & STATE_TRANSFERRING) {
        printf("debug: edburns: STATE_TRANSFERRING\n");
    }
    if (aStateFlags & STATE_NEGOTIATING) {
        printf("debug: edburns: STATE_NEGOTIATING\n");
    }
    return NS_OK;
}

NS_IMETHODIMP CBrowserContainer::OnSecurityChange(nsIWebProgress *aWebProgress,
                                                  nsIRequest *aRequest, 
                                                  PRUint32 state)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onProgressChange (in nsIChannel channel, in long curSelfProgress, in long maxSelfProgress, in long curTotalProgress, in long maxTotalProgress); */
NS_IMETHODIMP CBrowserContainer::OnProgressChange(nsIWebProgress *aWebProgress, 
                                                  nsIRequest *aRequest, 
                                                  PRInt32 aCurSelfProgress, 
                                                  PRInt32 aMaxSelfProgress, 
                                                  PRInt32 curTotalProgress, 
                                                  PRInt32 maxTotalProgress)
{
    PRInt32 percentComplete = 0;
    nsCAutoString name;
    nsAutoString autoName;
    jobject msgJStr = nsnull;
    nsresult rv;
    
    if (!mDocTarget) {
        return NS_OK;
    }
    
    // PENDING(edburns): Allow per fetch progress reporting.  Right now
    // we only have coarse grained support.
    if (maxTotalProgress) {
        percentComplete = curTotalProgress / maxTotalProgress;
    }
    
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 4, 
               ("CBrowserContainer: OnProgressURLLoad\n"));
    }
#endif

    if (NS_FAILED(rv = aRequest->GetName(name))) {
        return rv;
    }
    autoName.AssignWithConversion(name.get());
    // build up the string to be sent
    autoName.AppendWithConversion(" ");
    autoName.AppendInt(percentComplete);
    autoName.AppendWithConversion("%");


    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    
    msgJStr = (jobject) ::util_NewString(env, autoName.get(), 
                                         autoName.Length());
    
    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, mDocTarget, 
                         DOCUMENT_LOAD_LISTENER_CLASSNAME,
                         DocumentLoader_maskValues[PROGRESS_URL_LOAD_EVENT_MASK], 
                         msgJStr);

    if (msgJStr) {
        ::util_DeleteString(mInitContext->env, (jstring) msgJStr);
    }

    
    return NS_OK;
}

/* void onStatusChange (in nsIChannel channel, in long progressStatusFlags); */
NS_IMETHODIMP CBrowserContainer::OnStatusChange(nsIWebProgress *aWebProgress, 
                                                nsIRequest *aRequest, 
                                                nsresult aStatus, 
                                                const PRUnichar *aMessage)
{
    if (!mDocTarget) {
        return NS_OK;
    }
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 4, 
               ("CBrowserContainer: OnStatusURLLoad\n"));
    }
#endif
    nsAutoString aMsg(aMessage);
    int length = aMsg.Length();
    
    // IMPORTANT: do not use initContext->env here since it comes
    // from another thread.  Use JNU_GetEnv instead.
    
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    jstring statusMessage = ::util_NewString(env, (const jchar *) 
                                             aMsg.get(), length);
    
    util_SendEventToJava(mInitContext->env, mInitContext->nativeEventThread, 
                         mDocTarget, 
                         DOCUMENT_LOAD_LISTENER_CLASSNAME,
                         DocumentLoader_maskValues[STATUS_URL_LOAD_EVENT_MASK], 
                         (jobject) statusMessage);
    
    if (statusMessage) {
        ::util_DeleteString(mInitContext->env, statusMessage);
    }
    
	return NS_OK; 
}

/* void onLocationChange (in nsIURI location); */
NS_IMETHODIMP CBrowserContainer::OnLocationChange(nsIWebProgress *aWebProgress, 
                                                  nsIRequest *aRequest, 
                                                  nsIURI *location)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//
// Helper methods for our nsIWebProgressListener implementation
//

nsresult JNICALL
CBrowserContainer::doStartDocumentLoad(const PRUnichar *aDocumentName)
{
    // remove the old mouse listener for the old document
    if (mDomEventTarget) {
        mDomEventTarget->RemoveEventListener(NS_LITERAL_STRING("mouseover"), 
                                             this, PR_FALSE);
        mDomEventTarget = nsnull;
    }

    if (!mDocTarget) {
        return NS_OK;
    }
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 4, 
               ("CBrowserContainer: OnStartDocumentLoad\n"));
    }
#endif
    jobject urlJStr = nsnull;
    if (nsnull != aDocumentName) {
        
        // IMPORTANT: do not use initContext->env here since it comes
        // from another thread.  Use JNU_GetEnv instead.
        
        JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
        
        urlJStr = (jobject) ::util_NewString(env, aDocumentName, 
                                             nsCRT::strlen(aDocumentName));
    }  
    
    // maskValues array comes from ../src_share/jni_util.h
    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, mDocTarget, 
                         DOCUMENT_LOAD_LISTENER_CLASSNAME,
                         DocumentLoader_maskValues[START_DOCUMENT_LOAD_EVENT_MASK], 
                         urlJStr);
    
    
    if (urlJStr) {
        ::util_DeleteString(mInitContext->env, (jstring) urlJStr);
    }
    
	return NS_OK; 
} 

// we need this to fire the document complete 
nsresult JNICALL
CBrowserContainer::doEndDocumentLoad(nsIWebProgress *aWebProgress)
{

    nsCOMPtr<nsIWebBrowserFocus> focus(do_GetInterface(mInitContext->webBrowser));
    focus->Activate();

    nsCOMPtr<nsIDOMWindow> domWin;
    
    if (nsnull != aWebProgress) {
        if (NS_SUCCEEDED(aWebProgress->GetDOMWindow(getter_AddRefs(domWin)))
            && domWin) {
            if (NS_SUCCEEDED(domWin->GetDocument(getter_AddRefs(mInitContext->currentDocument)))) {
                // if we have a mouse listener
                if (mMouseTarget) {
                    // turn the currentDocument into an nsIDOMEventTarget
                    mDomEventTarget = 
                        do_QueryInterface(mInitContext->currentDocument);
                    // if successful
                    if (mDomEventTarget) {
                        mDomEventTarget->AddEventListener(NS_LITERAL_STRING("mouseover"), 
                                                          this, PR_FALSE);
                    }
                }
            }
        }
    }
    if (!mDocTarget) {
        return NS_OK;
    }
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 4, 
               ("CBrowserContainer: OnEndDocumentLoad\n"));
    }
#endif

    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, mDocTarget, 
                         DOCUMENT_LOAD_LISTENER_CLASSNAME,
                         DocumentLoader_maskValues[END_DOCUMENT_LOAD_EVENT_MASK], 
                         nsnull);
    

	return NS_OK; 
} 

nsresult JNICALL
CBrowserContainer::doStartURLLoad(const PRUnichar *aDocumentName)
{
	//NOTE: This appears to get fired once for each individual item on a page.
    if (!mDocTarget) {
        return NS_OK;
    }
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 4, 
               ("CBrowserContainer: OnStartURLLoad\n"));
    }
#endif
    nsAutoString nameAutoStr(aDocumentName);
    jstring nameJStr = ::util_NewString(env, (const jchar *) 
                                             nameAutoStr.get(), 
                                             nameAutoStr.Length());
    
    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, mDocTarget, 
                         DOCUMENT_LOAD_LISTENER_CLASSNAME,
                         DocumentLoader_maskValues[START_URL_LOAD_EVENT_MASK], 
                         nameJStr);

    if (nameJStr) {
        ::util_DeleteString(mInitContext->env, nameJStr);
    }
    
	return NS_OK; 
}

nsresult JNICALL
CBrowserContainer::doEndURLLoad(const PRUnichar *aDocumentName)
{
	//NOTE: This appears to get fired once for each individual item on a page.
    if (!mDocTarget) {
        return NS_OK;
    }
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 4, 
               ("CBrowserContainer: OnStartURLLoad\n"));
    }
#endif
    nsAutoString nameAutoStr(aDocumentName);
    jstring nameJStr = ::util_NewString(env, (const jchar *) 
                                             nameAutoStr.get(), 
                                             nameAutoStr.Length());
    
    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, mDocTarget, 
                         DOCUMENT_LOAD_LISTENER_CLASSNAME,
                         DocumentLoader_maskValues[END_URL_LOAD_EVENT_MASK], 
                         nameJStr);

    if (nameJStr) {
        ::util_DeleteString(mInitContext->env, nameJStr);
    }
    
	return NS_OK; 
}



///////////////////////////////////////////////////////////////////////////////
// nsIURIContentListener

NS_IMETHODIMP CBrowserContainer::OnStartURIOpen(nsIURI *aURI, PRBool *_retval)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CBrowserContainer::DoContent(const char *aContentType, PRBool aIsContentPreferred, nsIRequest *aRequest, nsIStreamListener **aContentHandler, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP CBrowserContainer::IsPreferred(const char *aContentType, char **aDesiredContentType, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP CBrowserContainer::CanHandleContent(const char *aContentType, PRBool aIsContentPreferred, char **aDesiredContentType, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* attribute nsISupports loadCookie; */
NS_IMETHODIMP CBrowserContainer::GetLoadCookie(nsISupports * *aLoadCookie)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP CBrowserContainer::SetLoadCookie(nsISupports * aLoadCookie)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* attribute nsIURIContentListener parentContentListener; */
NS_IMETHODIMP CBrowserContainer::GetParentContentListener(nsIURIContentListener * *aParentContentListener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP CBrowserContainer::SetParentContentListener(nsIURIContentListener * aParentContentListener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


///////////////////////////////////////////////////////////////////////////////
// nsIDocShellTreeOwner

NS_IMETHODIMP
CBrowserContainer::SetPersistence(PRBool aPersistPosition, 
                                  PRBool aPersistSize, PRBool aPersistSizeMode)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CBrowserContainer::GetPersistence(PRBool *aPersistPosition, 
                                  PRBool *aPersistSize, 
                                  PRBool *aPersistSizeMode)
{
    return NS_ERROR_FAILURE;
}





NS_IMETHODIMP
CBrowserContainer::FindItemWithName(const PRUnichar* aName,
   nsIDocShellTreeItem* aRequestor, nsIDocShellTreeItem** aFoundItem)
{
	return NS_ERROR_FAILURE;

}


NS_IMETHODIMP
CBrowserContainer::ContentShellAdded(nsIDocShellTreeItem* aContentShell,
   PRBool aPrimary, const PRUnichar* aID)
{
	return NS_OK;
}


NS_IMETHODIMP
CBrowserContainer::GetPrimaryContentShell(nsIDocShellTreeItem** aShell)
{
	nsCOMPtr<nsIDocShell> docShell = do_GetInterface(m_pOwner);
	nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(docShell));
	*aShell = (nsIDocShellTreeItem *) docShellAsItem;
	return NS_OK;
}


NS_IMETHODIMP
CBrowserContainer::SizeShellTo(nsIDocShellTreeItem* aShell,
   PRInt32 aCX, PRInt32 aCY)
{
	// Ignore request to be resized
	return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// nsIBaseWindow

NS_IMETHODIMP CBrowserContainer::GetEnabled(PRBool *aEnabled)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CBrowserContainer::SetEnabled(PRBool aEnabled)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
CBrowserContainer::InitWindow(nativeWindow parentNativeWindow, nsIWidget * parentWidget, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::Create(void)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::Destroy(void)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::SetPosition(PRInt32 x, PRInt32 y)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::GetPosition(PRInt32 *x, PRInt32 *y)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::GetSize(PRInt32 *cx, PRInt32 *cy)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::GetPositionAndSize(PRInt32 *x, PRInt32 *y, PRInt32 *cx, PRInt32 *cy)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::Repaint(PRBool force)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::GetParentWidget(nsIWidget * *aParentWidget)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::SetParentWidget(nsIWidget * aParentWidget)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::GetParentNativeWindow(nativeWindow *aParentNativeWindow)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::GetVisibility(PRBool *aVisibility)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::SetVisibility(PRBool aVisibility)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::GetMainWidget(nsIWidget * *aMainWidget)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::SetFocus(void)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::GetTitle(PRUnichar * *aTitle)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::SetTitle(const PRUnichar * aTitle)
{
	return NS_ERROR_FAILURE;
}


///////////////////////////////////////////////////////////////////////////////
// nsIWebBrowserChrome implementation

NS_IMETHODIMP
CBrowserContainer::DestroyBrowserWindow()
{
    PR_ASSERT(PR_FALSE);
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CBrowserContainer::IsWindowModal(PRBool *retVal)
{
    if (!retVal) {
        return NS_ERROR_NULL_POINTER;
    }
    *retVal = PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP
CBrowserContainer::SetStatus(PRUint32 statusType, const PRUnichar *status)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CBrowserContainer::GetWebBrowser(nsIWebBrowser * *aWebBrowser)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CBrowserContainer::SetWebBrowser(nsIWebBrowser * aWebBrowser)
{
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CBrowserContainer::GetChromeFlags(PRUint32 *aChromeFlags)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CBrowserContainer::SetChromeFlags(PRUint32 aChromeFlags)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CBrowserContainer::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CBrowserContainer::ShowAsModal(void)
{
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CBrowserContainer::ExitModalEventLoop(nsresult aStatus)
{
    return NS_ERROR_FAILURE;
}

///////////////////////////////////////////////////////////////////////////////
// nsIDocumentLoaderObserver implementation 
#if 0

// PENDING(edburns): when you figure out how to do this, implement it
// with nsIWebProgressListener.

NS_IMETHODIMP
CBrowserContainer::OnStartURLLoad(nsIDocumentLoader* loader, 
                                  nsIRequest* aRequest)
{ 
} 

NS_IMETHODIMP
CBrowserContainer::OnEndURLLoad(nsIDocumentLoader* loader, nsIRequest* request, nsresult aStatus)
{
  //NOTE: This appears to get fired once for each individual item on a page.
    if (!mDocTarget) {
        return NS_OK;
    }
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 4, 
               ("CBrowserContainer: OnStartURLLoad\n"));
    }
#endif

    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, mDocTarget, 
                         DOCUMENT_LOAD_LISTENER_CLASSNAME,
                         DocumentLoader_maskValues[END_URL_LOAD_EVENT_MASK], nsnull);
    
	return NS_OK; 
} 

#endif

NS_IMETHODIMP
CBrowserContainer::HandleEvent(nsIDOMEvent* aEvent)
{
	return NS_OK; 
}


NS_IMETHODIMP
CBrowserContainer::MouseDown(nsIDOMEvent* aMouseEvent)
{
    if (!mMouseTarget) {
        return NS_OK;
    }
    PR_ASSERT(nsnull != aMouseEvent);
    
    getPropertiesFromEvent(aMouseEvent);
    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, 
                         mMouseTarget, 
                         MOUSE_LISTENER_CLASSNAME,
                         DOMMouseListener_maskValues[MOUSE_DOWN_EVENT_MASK], 
                         properties);
	return NS_OK; 
}


NS_IMETHODIMP
CBrowserContainer::MouseUp(nsIDOMEvent* aMouseEvent)
{
    if (!mMouseTarget) {
        return NS_OK;
    }
    PR_ASSERT(nsnull != aMouseEvent);
    
    getPropertiesFromEvent(aMouseEvent);
    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, 
                         mMouseTarget, 
                         MOUSE_LISTENER_CLASSNAME,
                         DOMMouseListener_maskValues[MOUSE_UP_EVENT_MASK], 
                         properties);
	return NS_OK; 
}

NS_IMETHODIMP
CBrowserContainer::MouseClick(nsIDOMEvent* aMouseEvent)
{
    if (!mMouseTarget) {
        return NS_OK;
    }
    PR_ASSERT(nsnull != aMouseEvent);
    
    getPropertiesFromEvent(aMouseEvent);

    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    ::util_StoreIntoPropertiesObject(env, properties,  CLICK_COUNT_KEY,
                                     ONE_VALUE, (jobject) 
                                     &(mInitContext->shareContext));

    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, 
                         mMouseTarget, 
                         MOUSE_LISTENER_CLASSNAME,
                         DOMMouseListener_maskValues[MOUSE_CLICK_EVENT_MASK], 
                         properties);
	return NS_OK; 
}

NS_IMETHODIMP
CBrowserContainer::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
    if (!mMouseTarget) {
        return NS_OK;
    }
    PR_ASSERT(nsnull != aMouseEvent);
    
    getPropertiesFromEvent(aMouseEvent);

    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    ::util_StoreIntoPropertiesObject(env, properties,  CLICK_COUNT_KEY,
                                     TWO_VALUE, (jobject)
                                     &(mInitContext->shareContext));

    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, 
                         mMouseTarget, 
                         MOUSE_LISTENER_CLASSNAME,
                         DOMMouseListener_maskValues[MOUSE_DOUBLE_CLICK_EVENT_MASK], 
                         properties);
	return NS_OK; 
}

NS_IMETHODIMP
CBrowserContainer::MouseOver(nsIDOMEvent* aMouseEvent)
{
    if (!mMouseTarget) {
        return NS_OK;
    }
    PR_ASSERT(nsnull != aMouseEvent);
    
    getPropertiesFromEvent(aMouseEvent);
    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, 
                         mMouseTarget, 
                         MOUSE_LISTENER_CLASSNAME,
                         DOMMouseListener_maskValues[MOUSE_OVER_EVENT_MASK], 
                         properties);
	return NS_OK; 
}

NS_IMETHODIMP
CBrowserContainer::MouseOut(nsIDOMEvent* aMouseEvent)
{
    if (!mMouseTarget) {
        return NS_OK;
    }
    PR_ASSERT(nsnull != aMouseEvent);
    
    getPropertiesFromEvent(aMouseEvent);
    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, 
                         mMouseTarget, 
                         MOUSE_LISTENER_CLASSNAME,
                         DOMMouseListener_maskValues[MOUSE_OUT_EVENT_MASK], 
                         properties);
	return NS_OK; 
}

//
// From wcIBrowserContainer
// 


NS_IMETHODIMP CBrowserContainer::AddMouseListener(jobject target)
{
    nsresult rv = NS_OK;
    if (!mInitContext->initComplete) {
        return NS_ERROR_FAILURE;
    }

    if (-1 == DOMMouseListener_maskValues[0]) {
        util_InitializeEventMaskValuesFromClass("org/mozilla/webclient/WCMouseEvent",
                                                DOMMouseListener_maskNames, 
                                                DOMMouseListener_maskValues);
    }
    mMouseTarget = target;

    return rv;
}

NS_IMETHODIMP CBrowserContainer::AddDocumentLoadListener(jobject target)
{
    nsresult rv = NS_OK;
    if (!mInitContext->initComplete) {
        return NS_ERROR_FAILURE;
    }

    if (-1 == DocumentLoader_maskValues[0]) {
        util_InitializeEventMaskValuesFromClass("org/mozilla/webclient/DocumentLoadEvent",
                                                DocumentLoader_maskNames, 
                                                DocumentLoader_maskValues);
    }
    mDocTarget = target;

    return rv;
}

NS_IMETHODIMP CBrowserContainer::SetPrompt(jobject target)
{
    nsresult rv = NS_OK;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

    if (mPrompt) {
        ::util_DeleteGlobalRef(env, mPrompt);
        mPrompt = nsnull;
    }
    if (nsnull == (mPrompt = ::util_NewGlobalRef(env, target))) {
        ::util_ThrowExceptionToJava(env, "Exception: Navigation.nativeSetPrompt(): can't create NewGlobalRef\n\tfor argument");
        rv = NS_ERROR_NULL_POINTER;
    }
    
    return rv;
}


NS_IMETHODIMP CBrowserContainer::RemoveMouseListener()
{
    nsresult rv = NS_OK;
    if (!mInitContext->initComplete) {
        return NS_ERROR_FAILURE;
    }


    ::util_DeleteGlobalRef((JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION), mMouseTarget);
    mMouseTarget = nsnull;
    return rv;
}

NS_IMETHODIMP CBrowserContainer::RemoveDocumentLoadListener()
{
    nsresult rv = NS_OK;
    if (!mInitContext->initComplete) {
        return NS_ERROR_FAILURE;
    }

    ::util_DeleteGlobalRef((JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION), mDocTarget);
    mDocTarget = nsnull;
    return rv;
}

NS_IMETHODIMP CBrowserContainer::GetInstanceCount(PRInt32 *outCount)
{
    PR_ASSERT(outCount);
    *outCount = mInstanceCount;
    return NS_OK;
}

NS_IMETHODIMP CBrowserContainer::RemoveAllListeners()
{
    if (mDomEventTarget) {
        mDomEventTarget->RemoveEventListener(NS_LITERAL_STRING("mouseover"), 
                                             this, PR_FALSE);
        mDomEventTarget = nsnull;
    }   

    ::util_DeleteGlobalRef((JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION), mDocTarget);
    ::util_DeleteGlobalRef((JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION), mMouseTarget);

    mMouseTarget = nsnull;
    mDocTarget = nsnull;

    return NS_OK;
}


//
// Methods from nsIWebShellContainer
//
 
NS_IMETHODIMP CBrowserContainer::WillLoadURL(nsIWebShell* aShell,
                                             const PRUnichar* aURL,
                                             nsLoadType aReason)
{
    return NS_OK;
}

NS_IMETHODIMP CBrowserContainer::BeginLoadURL(nsIWebShell* aShell,
                                              const PRUnichar* aURL)
{
    return NS_OK;
}

NS_IMETHODIMP CBrowserContainer::EndLoadURL(nsIWebShell* aShell,
                                            const PRUnichar* aURL,
                                            nsresult aStatus)
{
    return NS_OK;
}

jobject JNICALL CBrowserContainer::getPropertiesFromEvent(nsIDOMEvent *event)
{
    nsCOMPtr<nsIDOMEventTarget> eventTarget;
    nsCOMPtr<nsIDOMNode> currentNode;
    nsCOMPtr<nsIDOMEvent> aMouseEvent = event;
    nsresult rv = NS_OK;;

    rv = aMouseEvent->GetTarget(getter_AddRefs(eventTarget));
    if (NS_FAILED(rv)) {
        return properties;
    }
    currentNode = do_QueryInterface(eventTarget);
    if (!currentNode) {
        return properties;
    }
    inverseDepth = 0;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

    if (properties) {
        util_ClearPropertiesObject(env, properties, (jobject) 
                                   &(mInitContext->shareContext));
    }
    else {
        if (!(properties = 
              util_CreatePropertiesObject(env, (jobject)
                                          &(mInitContext->shareContext)))) {
            return properties;
        }
    }
    dom_iterateToRoot(currentNode, CBrowserContainer::takeActionOnNode, 
                      (void *)this);
    addMouseEventDataToProperties(aMouseEvent);

    return properties;
}

void JNICALL CBrowserContainer::addMouseEventDataToProperties(nsIDOMEvent *aMouseEvent)
{
    // if the initialization failed, don't modify the properties
    if (!properties || !util_StringConstantsAreInitialized()) {
        return;
    }
    nsresult rv;

    // Add modifiers, keys, mouse buttons, etc, to the properties table
    nsCOMPtr<nsIDOMMouseEvent> mouseEvent;
    
    rv = aMouseEvent->QueryInterface(nsIDOMMouseEvent::GetIID(),
                                     getter_AddRefs(mouseEvent));
    if (NS_FAILED(rv)) {
        return;
    }

    PRInt32 intVal;
    PRUint16 int16Val;
    PRBool boolVal;
    char buf[20];
    jstring strVal;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    
    // PENDING(edburns): perhaps use a macro to speed this up?
    rv = mouseEvent->GetScreenX(&intVal);
    if (NS_SUCCEEDED(rv)) {
        WC_ITOA(intVal, buf, 10);
        strVal = ::util_NewStringUTF(env, buf);
        ::util_StoreIntoPropertiesObject(env, properties, SCREEN_X_KEY,
                                         (jobject) strVal, 
                                         (jobject) 
                                         &(mInitContext->shareContext));
    }
    
    rv = mouseEvent->GetScreenY(&intVal);
    if (NS_SUCCEEDED(rv)) {
        WC_ITOA(intVal, buf, 10);
        strVal = ::util_NewStringUTF(env, buf);
        ::util_StoreIntoPropertiesObject(env, properties, SCREEN_Y_KEY,
                                         (jobject) strVal, 
                                         (jobject) 
                                         &(mInitContext->shareContext));
    }
    
    rv = mouseEvent->GetClientX(&intVal);
    if (NS_SUCCEEDED(rv)) {
        WC_ITOA(intVal, buf, 10);
        strVal = ::util_NewStringUTF(env, buf);
        ::util_StoreIntoPropertiesObject(env, properties, CLIENT_X_KEY,
                                         (jobject) strVal, 
                                         (jobject) 
                                         &(mInitContext->shareContext));
    }
    
    rv = mouseEvent->GetClientY(&intVal);
    if (NS_SUCCEEDED(rv)) {
        WC_ITOA(intVal, buf, 10);
        strVal = ::util_NewStringUTF(env, buf);
        ::util_StoreIntoPropertiesObject(env, properties, CLIENT_Y_KEY,
                                         (jobject) strVal, 
                                         (jobject) 
                                         &(mInitContext->shareContext));
    }
    
    int16Val = 0;
    rv = mouseEvent->GetButton(&int16Val);
    if (NS_SUCCEEDED(rv)) {
        WC_ITOA(int16Val, buf, 10);
        strVal = ::util_NewStringUTF(env, buf);
        ::util_StoreIntoPropertiesObject(env, properties, BUTTON_KEY,
                                         (jobject) strVal,
                                         (jobject) 
                                         &(mInitContext->shareContext));
    }
    
    rv = mouseEvent->GetAltKey(&boolVal);
    if (NS_SUCCEEDED(rv)) {
        strVal = boolVal ? (jstring) TRUE_VALUE : (jstring) FALSE_VALUE;
        ::util_StoreIntoPropertiesObject(env, properties, ALT_KEY,
                                         (jobject) strVal, 
                                         (jobject) 
                                         &(mInitContext->shareContext));
    }
    
    rv = mouseEvent->GetCtrlKey(&boolVal);
    if (NS_SUCCEEDED(rv)) {
        strVal = boolVal ? (jstring) TRUE_VALUE : (jstring) FALSE_VALUE;
        ::util_StoreIntoPropertiesObject(env, properties, CTRL_KEY,
                                         (jobject) strVal, 
                                         (jobject) 
                                         &(mInitContext->shareContext));
    }
    
    rv = mouseEvent->GetShiftKey(&boolVal);
    if (NS_SUCCEEDED(rv)) {
        strVal = boolVal ? (jstring) TRUE_VALUE : (jstring) FALSE_VALUE;
        ::util_StoreIntoPropertiesObject(env, properties, SHIFT_KEY,
                                         (jobject) strVal, 
                                         (jobject) 
                                         &(mInitContext->shareContext));
    }
    
    rv = mouseEvent->GetMetaKey(&boolVal);
    if (NS_SUCCEEDED(rv)) {
        strVal = boolVal ? (jstring) TRUE_VALUE : (jstring) FALSE_VALUE;
        ::util_StoreIntoPropertiesObject(env, properties, META_KEY,
                                         (jobject) strVal, 
                                         (jobject) 
                                         &(mInitContext->shareContext));
    }
}

nsresult JNICALL CBrowserContainer::takeActionOnNode(nsCOMPtr<nsIDOMNode> currentNode,
                                                        void *myObject)
{
    nsresult rv = NS_OK;
    nsString nodeInfo, nodeName, nodeValue; //, nodeDepth;
    jstring jNodeName, jNodeValue;
    PRUint32 depth = 0;
    CBrowserContainer *curThis = nsnull;
    //const PRUint32 depthStrLen = 20;
    //    char depthStr[depthStrLen];
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

    PR_ASSERT(nsnull != myObject);
    curThis = (CBrowserContainer *) myObject;
    depth = curThis->inverseDepth++; 

    if (!(curThis->properties)) {
        return rv;
    }
    // encode the depth of the node
    //    PR_snprintf(depthStr, depthStrLen, "depthFromLeaf:%d", depth);
    //    nodeDepth = (const char *) depthStr;

    // Store the name and the value of this node

    {
        // get the name and prepend the depth
        rv = currentNode->GetNodeName(nodeInfo);
        if (NS_FAILED(rv)) {
            return rv;
        }
        //        nodeName = nodeDepth;
        //        nodeName += nodeInfo;
        nodeName = nodeInfo;
        
        if (prLogModuleInfo) {
            char * nodeInfoCStr = ToNewCString(nodeName);
            PR_LOG(prLogModuleInfo, 4, ("%s", nodeInfoCStr));
            nsMemory::Free(nodeInfoCStr);
        }
        
        rv = currentNode->GetNodeValue(nodeInfo);
        if (NS_FAILED(rv)) {
            return rv;
        }
        //        nodeValue = nodeDepth;
        //        nodeValue += nodeInfo;
        nodeValue = nodeInfo;
        
        if (prLogModuleInfo) {
            char * nodeInfoCStr = ToNewCString(nodeName);
            PR_LOG(prLogModuleInfo, 4, ("%s", (const char *)nodeInfoCStr));
            nsMemory::Free(nodeInfoCStr);
        }
        
        jNodeName = ::util_NewString(env, nodeName.get(), 
                                     nodeName.Length());
        jNodeValue = ::util_NewString(env, nodeValue.get(), 
                                      nodeValue.Length());
        
        util_StoreIntoPropertiesObject(env, (jobject) curThis->properties,
                                       (jobject) jNodeName, 
                                       (jobject) jNodeValue, 
                                       (jobject) 
                                       &(curThis->mInitContext->shareContext));
        if (jNodeName) {
            ::util_DeleteString(env, jNodeName);
        }
        if (jNodeValue) {
            ::util_DeleteString(env, jNodeValue);
        }
    } // end of Storing the name and value of this node

    // store any attributes of this node
    {
        nsCOMPtr<nsIDOMNamedNodeMap> nodeMap;
        rv = currentNode->GetAttributes(getter_AddRefs(nodeMap));
        if (NS_FAILED(rv) || !nodeMap) {
            return rv;
        }
        PRUint32 length, i;
        rv = nodeMap->GetLength(&length);
        if (NS_FAILED(rv)) {
            return rv;
        }
        for (i = 0; i < length; i++) {
            rv = nodeMap->Item(i, getter_AddRefs(currentNode));
            
            if (nsnull == currentNode) {
                return rv;
            }
            
            rv = currentNode->GetNodeName(nodeInfo);
            if (NS_FAILED(rv)) {
                return rv;
            }
            //            nodeName = nodeDepth;
            //            nodeName += nodeInfo;
            nodeName = nodeInfo;

            if (prLogModuleInfo) {
                char * nodeInfoCStr = ToNewCString(nodeName);
                PR_LOG(prLogModuleInfo, 4, 
                       ("attribute[%d], %s", i, (const char *)nodeInfoCStr));
                nsMemory::Free(nodeInfoCStr);
            }
            
            rv = currentNode->GetNodeValue(nodeInfo);
            if (NS_FAILED(rv)) {
                return rv;
            }
            //            nodeValue = nodeDepth;
            //            nodeValue += nodeInfo;
            nodeValue = nodeInfo;
            
            if (prLogModuleInfo) {
                char * nodeInfoCStr = ToNewCString(nodeName);
                PR_LOG(prLogModuleInfo, 4, 
                       ("attribute[%d] %s", i,(const char *)nodeInfoCStr));
                nsMemory::Free(nodeInfoCStr);
            }
            jNodeName = ::util_NewString(env, nodeName.get(), 
                                         nodeName.Length());
            jNodeValue = ::util_NewString(env, nodeValue.get(), 
                                          nodeValue.Length());
            
            util_StoreIntoPropertiesObject(env, (jobject) curThis->properties,
                                           (jobject) jNodeName, 
                                           (jobject) jNodeValue, 
                                           (jobject) 
                                           &(curThis->mInitContext->shareContext));
            if (jNodeName) {
                ::util_DeleteString(env, jNodeName);
            }
            if (jNodeValue) {
                ::util_DeleteString(env, jNodeValue);
            }
        }
    } // end of storing the attributes
        
    return rv;
}

// 
// Local functions
//

