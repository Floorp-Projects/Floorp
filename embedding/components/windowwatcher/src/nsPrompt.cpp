/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIServiceManager.h"
#include "nsIAuthPromptWrapper.h"
#include "nsPrompt.h"
#include "nsReadableUtils.h"
#include "nsDependentString.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsEmbedCID.h"
#include "nsPIDOMWindow.h"

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
  // wrap the base prompt in an nsIAuthPromptWrapper, if available
  // the impl used here persists prompt data and pre-fills the dialogs
  nsCOMPtr<nsIAuthPromptWrapper> siPrompt =
    do_CreateInstance("@mozilla.org/wallet/single-sign-on-prompt;1");
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
#ifdef DEBUG
  {
    nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(aParent));

    NS_ASSERTION(!win || win->IsOuterWindow(),
                 "Inner window passed as nsPrompt parent!");
  }
#endif
}

nsresult
nsPrompt::Init()
{
  mPromptService = do_GetService(NS_PROMPTSERVICE_CONTRACTID);
  return mPromptService ? NS_OK : NS_ERROR_FAILURE;
}

//*****************************************************************************
// nsPrompt::nsIPrompt
//*****************************************************************************

class nsAutoDOMEventDispatcher
{
public:
  nsAutoDOMEventDispatcher(nsIDOMWindow *aWindow);
  ~nsAutoDOMEventDispatcher();

  PRBool DefaultEnabled()
  {
    return mDefaultEnabled;
  }

protected:
  PRBool DispatchCustomEvent(const char *aEventName);

  nsIDOMWindow *mWindow;
  PRBool mDefaultEnabled;
};

nsAutoDOMEventDispatcher::nsAutoDOMEventDispatcher(nsIDOMWindow *aWindow)
  : mWindow(aWindow),
    mDefaultEnabled(DispatchCustomEvent("DOMWillOpenModalDialog"))
{
}

nsAutoDOMEventDispatcher::~nsAutoDOMEventDispatcher()
{
  if (mDefaultEnabled) {
    DispatchCustomEvent("DOMModalDialogClosed");
  }
}

PRBool
nsAutoDOMEventDispatcher::DispatchCustomEvent(const char *aEventName)
{
  if (!mWindow) {
    return PR_TRUE;
  }

#ifdef DEBUG
  {
    nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(mWindow));

    NS_ASSERTION(window->GetExtantDocument() != nsnull,
                 "nsPrompt used too early on window object!");
  }
#endif

  nsCOMPtr<nsIDOMDocument> domdoc;
  mWindow->GetDocument(getter_AddRefs(domdoc));

  nsCOMPtr<nsIDOMDocumentEvent> docevent(do_QueryInterface(domdoc));
  nsCOMPtr<nsIDOMEvent> event;

  PRBool defaultActionEnabled = PR_TRUE;

  if (docevent) {
    docevent->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));

    nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(event));
    if (privateEvent) {
      event->InitEvent(NS_ConvertASCIItoUTF16(aEventName), PR_TRUE, PR_TRUE);

      privateEvent->SetTrusted(PR_TRUE);

      nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mWindow));

      target->DispatchEvent(event, &defaultActionEnabled);
    }
  }

  return defaultActionEnabled;
}


NS_IMETHODIMP
nsPrompt::Alert(const PRUnichar* dialogTitle, 
                const PRUnichar* text)
{
  nsAutoDOMEventDispatcher autoDOMEventDispatcher(mParent);

  if (!autoDOMEventDispatcher.DefaultEnabled()) {
    return NS_OK;
  }

  return mPromptService->Alert(mParent, dialogTitle, text);
}

NS_IMETHODIMP
nsPrompt::AlertCheck(const PRUnichar* dialogTitle, 
                     const PRUnichar* text,
                     const PRUnichar* checkMsg,
                     PRBool *checkValue)
{
  nsAutoDOMEventDispatcher autoDOMEventDispatcher(mParent);

  if (!autoDOMEventDispatcher.DefaultEnabled()) {
    return NS_OK;
  }

  return mPromptService->AlertCheck(mParent, dialogTitle, text, checkMsg,
                                    checkValue);
}

NS_IMETHODIMP
nsPrompt::Confirm(const PRUnichar* dialogTitle, 
                  const PRUnichar* text,
                  PRBool *_retval)
{
  nsAutoDOMEventDispatcher autoDOMEventDispatcher(mParent);

  if (!autoDOMEventDispatcher.DefaultEnabled()) {
    return NS_OK;
  }

  return mPromptService->Confirm(mParent, dialogTitle, text, _retval);
}

NS_IMETHODIMP
nsPrompt::ConfirmCheck(const PRUnichar* dialogTitle, 
                       const PRUnichar* text,
                       const PRUnichar* checkMsg,
                       PRBool *checkValue,
                       PRBool *_retval)
{
  nsAutoDOMEventDispatcher autoDOMEventDispatcher(mParent);

  if (!autoDOMEventDispatcher.DefaultEnabled()) {
    return NS_OK;
  }

  return mPromptService->ConfirmCheck(mParent, dialogTitle, text, checkMsg,
                                      checkValue, _retval);
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
  nsAutoDOMEventDispatcher autoDOMEventDispatcher(mParent);

  if (!autoDOMEventDispatcher.DefaultEnabled()) {
    return NS_OK;
  }

  return mPromptService->ConfirmEx(mParent, dialogTitle, text, buttonFlags,
                                   button0Title, button1Title, button2Title,
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
  nsAutoDOMEventDispatcher autoDOMEventDispatcher(mParent);

  if (!autoDOMEventDispatcher.DefaultEnabled()) {
    return NS_OK;
  }

  return mPromptService->Prompt(mParent, dialogTitle, text, answer, checkMsg,
                                checkValue, _retval);
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
  nsAutoDOMEventDispatcher autoDOMEventDispatcher(mParent);

  if (!autoDOMEventDispatcher.DefaultEnabled()) {
    return NS_OK;
  }

  return mPromptService->PromptUsernameAndPassword(mParent, dialogTitle, text,
                                                   username, password,
                                                   checkMsg, checkValue,
                                                   _retval);
}

NS_IMETHODIMP
nsPrompt::PromptPassword(const PRUnichar *dialogTitle,
                         const PRUnichar *text,
                         PRUnichar **password,
                         const PRUnichar *checkMsg,
                         PRBool *checkValue,
                         PRBool *_retval)
{
  nsAutoDOMEventDispatcher autoDOMEventDispatcher(mParent);

  if (!autoDOMEventDispatcher.DefaultEnabled()) {
    return NS_OK;
  }

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
  nsAutoDOMEventDispatcher autoDOMEventDispatcher(mParent);

  if (!autoDOMEventDispatcher.DefaultEnabled()) {
    return NS_OK;
  }

  return mPromptService->Select(mParent, dialogTitle, inMsg, inCount, inList,
                                outSelection, _retval);
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
  nsAutoDOMEventDispatcher autoDOMEventDispatcher(mParent);

  if (!autoDOMEventDispatcher.DefaultEnabled()) {
    return NS_OK;
  }

  // Ignore passwordRealm and savePassword
  if (defaultText) {
    *result = ToNewUnicode(nsDependentString(defaultText));

    if (!*result) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return mPromptService->Prompt(mParent, dialogTitle, text, result, nsnull,
                                nsnull, _retval);
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
  nsAutoDOMEventDispatcher autoDOMEventDispatcher(mParent);

  if (!autoDOMEventDispatcher.DefaultEnabled()) {
    return NS_OK;
  }

  // Ignore passwordRealm and savePassword
  return mPromptService->PromptUsernameAndPassword(mParent, dialogTitle, text,
                                                   user, pwd, nsnull, nsnull,
                                                   _retval);
}

NS_IMETHODIMP
nsPrompt::PromptPassword(const PRUnichar* dialogTitle, 
                         const PRUnichar* text,
                         const PRUnichar* passwordRealm,
                         PRUint32 savePassword,
                         PRUnichar* *pwd,
                         PRBool *_retval)
{
  nsAutoDOMEventDispatcher autoDOMEventDispatcher(mParent);

  if (!autoDOMEventDispatcher.DefaultEnabled()) {
    return NS_OK;
  }

  // Ignore passwordRealm and savePassword
  return mPromptService->PromptPassword(mParent, dialogTitle, text, pwd,
                                        nsnull, nsnull, _retval);
}
