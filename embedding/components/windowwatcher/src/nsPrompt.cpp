/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsIServiceManager.h"
#include "nsIWalletService.h"
#include "nsPrompt.h"
#include "nsReadableUtils.h"

static NS_DEFINE_CID(kSingleSignOnPromptCID, NS_SINGLESIGNONPROMPT_CID);

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

//*****************************************************************************
// nsPrompt::nsIPrompt
//*****************************************************************************   

NS_IMETHODIMP
nsPrompt::Alert(const PRUnichar* dialogTitle, 
                const PRUnichar* text)
{
  return mPromptService->Alert(mParent, dialogTitle, text);
}

NS_IMETHODIMP
nsPrompt::AlertCheck(const PRUnichar* dialogTitle, 
                     const PRUnichar* text,
                     const PRUnichar* checkMsg,
                     PRBool *checkValue)
{
  return mPromptService->AlertCheck(mParent, dialogTitle, text, checkMsg, checkValue);
}

NS_IMETHODIMP
nsPrompt::Confirm(const PRUnichar* dialogTitle, 
                  const PRUnichar* text,
                  PRBool *_retval)
{
  return mPromptService->Confirm(mParent, dialogTitle, text, _retval);
}

NS_IMETHODIMP
nsPrompt::ConfirmCheck(const PRUnichar* dialogTitle, 
                       const PRUnichar* text,
                       const PRUnichar* checkMsg,
                       PRBool *checkValue,
                       PRBool *_retval)
{
  return mPromptService->ConfirmCheck(mParent, dialogTitle, text, checkMsg, checkValue, _retval);
}

NS_IMETHODIMP
nsPrompt::ConfirmEx(const PRUnichar *dialogTitle,
                    const PRUnichar *text,
                    PRUint32 buttonFlags,
                    const PRUnichar *button0Title,
                    const PRUnichar *button1Title,
                    const PRUnichar *button2Title,
                    const PRUnichar *checkMsg,
                    PRBool *checkValue,
                    PRInt32 *buttonPressed)
{
  return mPromptService->ConfirmEx(mParent, dialogTitle, text,
                                   buttonFlags, button0Title, button1Title, button2Title,
                                   checkMsg, checkValue, buttonPressed);
}

NS_IMETHODIMP
nsPrompt::Prompt(const PRUnichar *dialogTitle,
                 const PRUnichar *text,
                 PRUnichar **answer,
                 const PRUnichar *checkMsg,
                 PRBool *checkValue,
                 PRBool *_retval)
{
  return mPromptService->Prompt(mParent, dialogTitle, text, answer,
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
  return mPromptService->PromptUsernameAndPassword(mParent, dialogTitle, text, username, password,
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
  return mPromptService->PromptPassword(mParent, dialogTitle, text, password,
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
  return mPromptService->Select(mParent, dialogTitle, inMsg,
                                inCount, inList, outSelection, _retval);
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
  if (defaultText)
    *result = ToNewUnicode(nsDependentString(defaultText));
  return mPromptService->Prompt(mParent, dialogTitle, text,
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
  return mPromptService->PromptUsernameAndPassword(mParent, dialogTitle, text,
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
  return mPromptService->PromptPassword(mParent, dialogTitle, text,
                                        pwd, nsnull, nsnull, _retval);
}
