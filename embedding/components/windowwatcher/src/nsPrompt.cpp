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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsILocale.h"
#include "nsIStringBundle.h"
#include "nsIServiceManager.h"
#include "nsIWalletService.h"
#include "nsPrompt.h"
#include "nsReadableUtils.h"

static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kSingleSignOnPromptCID, NS_SINGLESIGNONPROMPT_CID);

#define kWebShellLocaleProperties "chrome://global/locale/commonDialogs.properties"

class AutoStringFree {

public:
  AutoStringFree() : mString(0) { }
  ~AutoStringFree() { if (mString) nsCRT::free(mString); }
  void TakeString(PRUnichar *aString) {
         if (mString) nsCRT::free(mString);
         mString = aString;
       }

private:
  PRUnichar *mString;
};

nsresult
NS_NewPrompter(nsIPrompt **result, nsIDOMWindow *aParent)
{
  nsresult rv;
  *result = 0;

  nsPrompt *prompter = new nsPrompt(aParent);
  if (!prompter)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(prompter);
  rv = prompter->Init();
  if (NS_FAILED(rv)) {
    NS_RELEASE(prompter);
    return rv;
  }

  *result = prompter;
  return NS_OK;
}

nsresult
NS_NewAuthPrompter(nsIAuthPrompt **result, nsIDOMWindow *aParent)
{

  nsresult rv;
  *result = 0;

  nsPrompt *prompter = new nsPrompt(aParent);
  if (!prompter)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(prompter);
  rv = prompter->Init();
  if (NS_FAILED(rv)) {
    NS_RELEASE(prompter);
    return rv;
  }

  *result = prompter;
  // wrap the base prompt in an nsISingleSignOnPrompt, if available
  nsCOMPtr<nsISingleSignOnPrompt> siPrompt = do_CreateInstance(kSingleSignOnPromptCID);
  if (siPrompt) {
    // then single sign-on is installed
    rv = siPrompt->SetPromptDialogs(prompter);
    if (NS_SUCCEEDED(rv)) {
      *result = siPrompt;
      NS_RELEASE(prompter); // siPrompt is a strong owner
      NS_ADDREF(*result);
    }
  }
  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsPrompt, nsIPrompt, nsIAuthPrompt)

nsPrompt::nsPrompt(nsIDOMWindow *aParent)
  : mParent(aParent)
{
  NS_INIT_REFCNT();
}

nsresult
nsPrompt::Init()
{
  mPromptService = do_GetService("@mozilla.org/embedcomp/prompt-service;1");
  return mPromptService ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsPrompt::GetLocaleString(const PRUnichar *aKey, PRUnichar **aResult)
{
  nsresult rv;

  nsCOMPtr<nsIStringBundleService> stringService = do_GetService(kStringBundleServiceCID);
  nsCOMPtr<nsIStringBundle> stringBundle;
  nsILocale *locale = nsnull;
 
  rv = stringService->CreateBundle(kWebShellLocaleProperties, locale, getter_AddRefs(stringBundle));
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  rv = stringBundle->GetStringFromName(aKey, aResult);

  return rv;
}

//*****************************************************************************
// nsPrompt::nsIPrompt
//*****************************************************************************   

NS_IMETHODIMP
nsPrompt::Alert(const PRUnichar* dialogTitle, 
                const PRUnichar* text)
{
  nsresult rv;
  AutoStringFree stringOwner;
  PRUnichar *title = NS_CONST_CAST(PRUnichar *, dialogTitle);
 
  if (!title) {
    rv = GetLocaleString(NS_LITERAL_STRING("Alert").get(), &title);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    stringOwner.TakeString(title);
  }

  return mPromptService->Alert(mParent, title, text);
}

NS_IMETHODIMP
nsPrompt::AlertCheck(const PRUnichar* dialogTitle, 
                     const PRUnichar* text,
                     const PRUnichar* checkMsg,
                     PRBool *checkValue)
{
  nsresult rv;
  AutoStringFree stringOwner;
  PRUnichar *title = NS_CONST_CAST(PRUnichar *, dialogTitle);

  if (!title) {
    rv = GetLocaleString(NS_LITERAL_STRING("Alert").get(), &title);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    stringOwner.TakeString(title);
  }

  return mPromptService->AlertCheck(mParent, title, text, checkMsg, checkValue);
}

NS_IMETHODIMP
nsPrompt::Confirm(const PRUnichar* dialogTitle, 
                  const PRUnichar* text,
                  PRBool *_retval)
{
  nsresult rv;
  AutoStringFree stringOwner;
  PRUnichar *title = NS_CONST_CAST(PRUnichar *, dialogTitle);

  if (!title) {
    rv = GetLocaleString(NS_LITERAL_STRING("Confirm").get(), &title);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    stringOwner.TakeString(title);
  }

  return mPromptService->Confirm(mParent, title, text, _retval);
}

NS_IMETHODIMP
nsPrompt::ConfirmCheck(const PRUnichar* dialogTitle, 
                       const PRUnichar* text,
                       const PRUnichar* checkMsg,
                       PRBool *checkValue,
                       PRBool *_retval)
{
  nsresult rv;
  AutoStringFree stringOwner;
  PRUnichar *title = NS_CONST_CAST(PRUnichar *, dialogTitle);

  if (!title) {
    rv = GetLocaleString(NS_LITERAL_STRING("ConfirmCheck").get(), &title);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    stringOwner.TakeString(title);
  }

  return mPromptService->ConfirmCheck(mParent, title, text, checkMsg, checkValue, _retval);
}

NS_IMETHODIMP
nsPrompt::Prompt(const PRUnichar *dialogTitle,
                 const PRUnichar *text,
                 PRUnichar **answer,
                 const PRUnichar *checkMsg,
                 PRBool *checkValue,
                 PRBool *_retval)
{
  nsresult rv;
  AutoStringFree stringOwner;
  PRUnichar *title = NS_CONST_CAST(PRUnichar *, dialogTitle);

  if (!title) {
    rv = GetLocaleString(NS_LITERAL_STRING("Prompt").get(), &title);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    stringOwner.TakeString(title);
  }

  return mPromptService->Prompt(mParent, title, text, answer,
                                checkMsg, checkValue, _retval);
}

NS_IMETHODIMP
nsPrompt::PromptUsernameAndPassword(const PRUnichar *dialogTitle,
                                    const PRUnichar *text,
                                    PRUnichar **username,
                                    PRUnichar **password,
                                    const PRUnichar *checkMsg,
                                    PRBool *checkValue,
                                    PRBool *_retval)
{
  nsresult rv;
  AutoStringFree stringOwner;
  PRUnichar *title = NS_CONST_CAST(PRUnichar *, dialogTitle);
  
  if (!title) {
    rv = GetLocaleString(NS_LITERAL_STRING("PromptUsernameAndPassword").get(), &title);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    stringOwner.TakeString(title);
  }

  return mPromptService->PromptUsernameAndPassword(mParent, title, text, username, password,
                                                   checkMsg, checkValue, _retval);
}

NS_IMETHODIMP
nsPrompt::PromptPassword(const PRUnichar *dialogTitle,
                         const PRUnichar *text,
                         PRUnichar **password,
                         const PRUnichar *checkMsg,
                         PRBool *checkValue,
                         PRBool *_retval)
{
  nsresult rv;
  AutoStringFree stringOwner;
  PRUnichar *title = NS_CONST_CAST(PRUnichar *, dialogTitle);

  if (!title) {
    rv = GetLocaleString(NS_LITERAL_STRING("PromptPassword").get(), &title);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    stringOwner.TakeString(title);
  }

  return mPromptService->PromptPassword(mParent, title, text, password,
                                        checkMsg, checkValue, _retval);
}

NS_IMETHODIMP
nsPrompt::Select(const PRUnichar *dialogTitle,
                 const PRUnichar* inMsg,
                 PRUint32 inCount, 
                 const PRUnichar **inList,
                 PRInt32 *outSelection,
                 PRBool *_retval)
{
  nsresult rv; 
  AutoStringFree stringOwner;
  PRUnichar *title = NS_CONST_CAST(PRUnichar *, dialogTitle);

  if (!title) {
    rv = GetLocaleString(NS_LITERAL_STRING("Select").get(), &title);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    stringOwner.TakeString(title);
  }

  return mPromptService->Select(mParent, title, inMsg,
                                inCount, inList, outSelection, _retval);
}

NS_IMETHODIMP
nsPrompt::UniversalDialog(const PRUnichar *inTitleMessage,
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
  nsresult rv;
  NS_ASSERTION(inDialogTitle, "UniversalDialog must have a dialog title supplied");
  rv = mPromptService->UniversalDialog(mParent, 
                                       inTitleMessage,
                                       inDialogTitle,
                                       inMsg,
                                       inCheckboxMsg,
                                       inButton0Text,
                                       inButton1Text,
                                       inButton2Text,
                                       inButton3Text,
                                       inEditfield1Msg,
                                       inEditfield2Msg,
                                       inoutEditfield1Value,
                                       inoutEditfield2Value,
                                       inIConURL,
                                       inoutCheckboxState,
                                       inNumberButtons,
                                       inNumberEditfields,
                                       inEditField1Password,
                                       outButtonPressed);
  return rv;
}

//*****************************************************************************
// nsPrompt::nsIAuthPrompt
// This implementation serves as glue between nsIAuthPrompt and nsIPrompt
// when a single-signon was not available.
//*****************************************************************************   

NS_IMETHODIMP
nsPrompt::Prompt(const PRUnichar* dialogTitle,
                 const PRUnichar* text,
                 const PRUnichar* passwordRealm,
                 PRUint32 savePassword,
                 const PRUnichar* defaultText,
                 PRUnichar* *result,
                 PRBool *_retval)
{
  // Ignore passwordRealm and savePassword
  nsresult rv;
  AutoStringFree stringOwner;
  PRUnichar *title = NS_CONST_CAST(PRUnichar *, dialogTitle);

  if (!title) {
    rv = GetLocaleString(NS_LITERAL_STRING("Prompt").get(), &title);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    stringOwner.TakeString(title);
  }

  if (defaultText)
    *result = ToNewUnicode(nsLiteralString(defaultText));
  return mPromptService->Prompt(mParent, title, text,
                                result, nsnull, nsnull, _retval);
}

NS_IMETHODIMP
nsPrompt::PromptUsernameAndPassword(const PRUnichar* dialogTitle, 
                                    const PRUnichar* text,
                                    const PRUnichar* passwordRealm,
                                    PRUint32 savePassword,
                                    PRUnichar* *user,
                                    PRUnichar* *pwd,
                                    PRBool *_retval)
{
  // Ignore passwordRealm and savePassword
  nsresult rv;
  AutoStringFree stringOwner;
  PRUnichar *title = NS_CONST_CAST(PRUnichar *, dialogTitle);
  
  if (!title) {
    rv = GetLocaleString(NS_LITERAL_STRING("PromptUsernameAndPassword").get(), &title);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    stringOwner.TakeString(title);
  }

  return mPromptService->PromptUsernameAndPassword(mParent, title, text,
                                                   user, pwd, nsnull, nsnull, _retval);
}

NS_IMETHODIMP
nsPrompt::PromptPassword(const PRUnichar* dialogTitle, 
                         const PRUnichar* text,
                         const PRUnichar* passwordRealm,
                         PRUint32 savePassword,
                         PRUnichar* *pwd,
                         PRBool *_retval)
{
  // Ignore passwordRealm and savePassword
  nsresult rv;
  AutoStringFree stringOwner;
  PRUnichar *title = NS_CONST_CAST(PRUnichar *, dialogTitle);

  if (!title) {
    rv = GetLocaleString(NS_LITERAL_STRING("PromptPassword").get(), &title);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    stringOwner.TakeString(title);
  }

  return mPromptService->PromptPassword(mParent, title, text,
                                        pwd, nsnull, nsnull, _retval);
}
