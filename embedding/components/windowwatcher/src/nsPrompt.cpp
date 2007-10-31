/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
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
 *     Google Inc.
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
#include "nsIAuthInformation.h"
#include "nsPrompt.h"
#include "nsReadableUtils.h"
#include "nsDependentString.h"
#include "nsIStringBundle.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsEmbedCID.h"
#include "nsNetCID.h"
#include "nsPIDOMWindow.h"
#include "nsIPromptFactory.h"
#include "nsIProxiedChannel.h"
#include "nsIProxyInfo.h"
#include "nsIIDNService.h"
#include "nsNetUtil.h"
#include "nsPromptUtils.h"

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
  nsCOMPtr<nsIPromptFactory> factory =
    do_GetService(NS_PWMGR_AUTHPROMPTFACTORY);
  if (factory) {
    // We just delegate everything to the pw mgr if we can
    rv = factory->GetPrompt(aParent,
                            NS_GET_IID(nsIAuthPrompt),
                            reinterpret_cast<void**>(result));
    // If the password manager doesn't support the interface, fall back to the
    // old way of doing things. This will allow older apps that haven't updated
    // to work still.
    if (rv != NS_NOINTERFACE)
      return rv;
  }

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

nsresult
NS_NewAuthPrompter2(nsIAuthPrompt2 **result, nsIDOMWindow *aParent)
{
  nsresult rv;

  nsCOMPtr<nsIPromptFactory> factory =
    do_GetService(NS_PWMGR_AUTHPROMPTFACTORY);
  if (factory) {
    // We just delegate everything to the pw mgr.
    rv = factory->GetPrompt(aParent,
                              NS_GET_IID(nsIAuthPrompt2),
                              reinterpret_cast<void**>(result));
    if (NS_SUCCEEDED(rv))
        return rv;
  }

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

NS_IMPL_THREADSAFE_ISUPPORTS3(nsPrompt, nsIPrompt, nsIAuthPrompt, nsIAuthPrompt2)

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
  mPromptService2 = do_QueryInterface(mPromptService);
  // A null mPromptService2 is not fatal, we have to deal with that
  // (for compatibility with embeddors who only implement the old version)
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
  return mPromptService->AlertCheck(mParent, dialogTitle, text, checkMsg,
                                    checkValue);
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
  // Ignore passwordRealm and savePassword
  return mPromptService->PromptPassword(mParent, dialogTitle, text, pwd,
                                        nsnull, nsnull, _retval);
}
NS_IMETHODIMP
nsPrompt::PromptAuth(nsIChannel* aChannel,
                     PRUint32 aLevel,
                     nsIAuthInformation* aAuthInfo,
                     PRBool* retval)
{
  if (mPromptService2) {
    return mPromptService2->PromptAuth(mParent, aChannel,
                                       aLevel, aAuthInfo,
                                       nsnull, nsnull, retval);
  }

  return PromptPasswordAdapter(mPromptService, mParent, aChannel,
                               aLevel, aAuthInfo, nsnull, nsnull, retval);
}

NS_IMETHODIMP
nsPrompt::AsyncPromptAuth(nsIChannel* aChannel,
                          nsIAuthPromptCallback* aCallback,
                          nsISupports* aContext,
                          PRUint32 aLevel,
                          nsIAuthInformation* aAuthInfo,
                          nsICancelable** retval)
{
  if (mPromptService2) {
    return mPromptService2->AsyncPromptAuth(mParent, aChannel,
                                            aCallback, aContext,
                                            aLevel, aAuthInfo,
                                            nsnull, nsnull, retval);
  }

  // Tell the caller to use the sync version
  return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult
MakeDialogText(nsIChannel* aChannel, nsIAuthInformation* aAuthInfo,
               nsXPIDLString& message)
{
  nsresult rv;
  nsCOMPtr<nsIStringBundleService> bundleSvc =
    do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStringBundle> bundle;
  rv = bundleSvc->CreateBundle("chrome://global/locale/prompts.properties",
                               getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, rv);

  // figure out what message to display...
  nsCAutoString host;
  PRInt32 port;
  NS_GetAuthHostPort(aChannel, aAuthInfo, PR_FALSE, host, &port);

  nsAutoString displayHost;
  CopyUTF8toUTF16(host, displayHost);

  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(uri));

  nsCAutoString scheme;
  uri->GetScheme(scheme);

  nsAutoString username;
  aAuthInfo->GetUsername(username);

  PRUint32 flags;
  aAuthInfo->GetFlags(&flags);
  PRBool proxyAuth = (flags & nsIAuthInformation::AUTH_PROXY) != 0;

  nsAutoString realm;
  aAuthInfo->GetRealm(realm);

  // Append the port if it was specified
  if (port != -1) {
    displayHost.Append(PRUnichar(':'));
    displayHost.AppendInt(port);
  }

  NS_NAMED_LITERAL_STRING(proxyText, "EnterUserPasswordForProxy");
  NS_NAMED_LITERAL_STRING(originText, "EnterUserPasswordForRealm");
  NS_NAMED_LITERAL_STRING(noRealmText, "EnterUserPasswordFor");
  NS_NAMED_LITERAL_STRING(passwordText, "EnterPasswordFor");

  const PRUnichar *text;
  if (proxyAuth) {
    text = proxyText.get();
  } else {
    text = originText.get();

    // prepend "scheme://"
    nsAutoString schemeU; 
    CopyASCIItoUTF16(scheme, schemeU);
    schemeU.AppendLiteral("://");
    displayHost.Insert(schemeU, 0);
  }

  const PRUnichar *strings[] = { realm.get(), displayHost.get() };
  PRUint32 count = NS_ARRAY_LENGTH(strings);

  if (flags & nsIAuthInformation::ONLY_PASSWORD) {
    text = passwordText.get();
    strings[0] = username.get();
  } else if (!proxyAuth && realm.IsEmpty()) {
    text = noRealmText.get();
    count--;
    strings[0] = strings[1];
  }

  rv = bundle->FormatStringFromName(text, strings, count, getter_Copies(message));
  return rv;
}

/* static */ nsresult
nsPrompt::PromptPasswordAdapter(nsIPromptService* aService,
                                nsIDOMWindow* aParent,
                                nsIChannel* aChannel,
                                PRUint32 aLevel,
                                nsIAuthInformation* aAuthInfo,
                                const PRUnichar* aCheckLabel,
                                PRBool* aCheckValue,
                                PRBool* retval)
{
  // construct the message string
  nsXPIDLString message;
  MakeDialogText(aChannel, aAuthInfo, message);

  nsAutoString defaultUser, defaultDomain, defaultPass;
  aAuthInfo->GetUsername(defaultUser);
  aAuthInfo->GetDomain(defaultDomain);
  aAuthInfo->GetPassword(defaultPass);

  PRUint32 flags;
  aAuthInfo->GetFlags(&flags);

  if ((flags & nsIAuthInformation::NEED_DOMAIN) && !defaultDomain.IsEmpty()) {
    defaultDomain.Append(PRUnichar('\\'));
    defaultUser.Insert(defaultDomain, 0);
  }

  // NOTE: Allocation failure is not fatal here (just default to empty string
  // if allocation fails)
  PRUnichar *user = ToNewUnicode(defaultUser),
            *pass = ToNewUnicode(defaultPass);
  nsresult rv;
  if (flags & nsIAuthInformation::ONLY_PASSWORD)
    rv = aService->PromptPassword(aParent, nsnull, message.get(),
                                  &pass, aCheckLabel,
                                  aCheckValue, retval);
  else
    rv = aService->PromptUsernameAndPassword(aParent, nsnull, message.get(),
                                             &user, &pass, aCheckLabel,
                                             aCheckValue, retval);

  nsAdoptingString userStr(user);
  nsAdoptingString passStr(pass);
  NS_SetAuthInfo(aAuthInfo, userStr, passStr);

  return rv;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(AuthPromptWrapper, nsIAuthPrompt2)

NS_IMETHODIMP
AuthPromptWrapper::PromptAuth(nsIChannel* aChannel,
                              PRUint32 aLevel,
                              nsIAuthInformation* aAuthInfo,
                              PRBool* retval)
{
  nsCAutoString keyUTF8;
  NS_GetAuthKey(aChannel, aAuthInfo, keyUTF8);

  NS_ConvertUTF8toUTF16 key(keyUTF8);

  nsXPIDLString text;
  MakeDialogText(aChannel, aAuthInfo, text);

  PRUint32 flags;
  aAuthInfo->GetFlags(&flags);

  nsresult rv;
  nsXPIDLString user, password;
  if (flags & nsIAuthInformation::ONLY_PASSWORD) {
    rv = mAuthPrompt->PromptPassword(nsnull, text.get(), key.get(),
                                     nsIAuthPrompt::SAVE_PASSWORD_PERMANENTLY,
                                     getter_Copies(password), retval);
    if (NS_SUCCEEDED(rv) && *retval) {
      NS_ASSERTION(password, "password must not be null if retval is true");
      aAuthInfo->SetPassword(password);
    }
  } else {
    rv = mAuthPrompt->PromptUsernameAndPassword(nsnull, text.get(), key.get(),
                                                nsIAuthPrompt::SAVE_PASSWORD_PERMANENTLY,
                                                getter_Copies(user),
                                                getter_Copies(password),
                                                retval);
    if (NS_SUCCEEDED(rv) && *retval) {
      NS_ASSERTION(user && password, "out params must be nonnull");
      NS_SetAuthInfo(aAuthInfo, user, password);
    }
  }
  return rv;
}

NS_IMETHODIMP
AuthPromptWrapper::AsyncPromptAuth(nsIChannel*,
                                   nsIAuthPromptCallback*,
                                   nsISupports*,
                                   PRUint32,
                                   nsIAuthInformation*,
                                   nsICancelable**)
{
  // There is no way to implement this here. Just tell the caller
  // to fall back to the synchronous version.
  return NS_ERROR_NOT_IMPLEMENTED;
}



