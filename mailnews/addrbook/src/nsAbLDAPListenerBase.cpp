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
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Paul Sandoz <paul.sandoz@sun.com>
 *   Dan Mosedale <dmose@mozilla.org>
 *   Mark Banner <mark@standard8.demon.co.uk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsAbLDAPListenerBase.h"
#include "nsIWindowWatcher.h"
#include "nsIDOMWindow.h"
#include "nsIAuthPrompt.h"
#include "nsIStringBundle.h"
#include "nsIServiceManager.h"
#include "nsIProxyObjectManager.h"
#include "nsILDAPMessage.h"
#include "nsILDAPErrors.h"
#include "nsCategoryManagerUtils.h"

nsAbLDAPListenerBase::nsAbLDAPListenerBase(nsILDAPURL* url,
                                           nsILDAPConnection* connection,
                                           const nsACString &login,
                                           const PRInt32 timeOut) :
  mDirectoryUrl(url), mConnection(connection), mLogin(login),
  mTimeOut(timeOut), mBound(PR_FALSE), mInitialized(PR_FALSE),
  mLock(nsnull)
{
}

nsAbLDAPListenerBase::~nsAbLDAPListenerBase()
{
  if (mLock)
    PR_DestroyLock(mLock);
}

nsresult nsAbLDAPListenerBase::Initiate()
{
  if (mInitialized)
    return NS_OK;

  mLock = PR_NewLock();
  if (!mLock)
    return NS_ERROR_OUT_OF_MEMORY;

  mInitialized = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP nsAbLDAPListenerBase::OnLDAPInit(nsILDAPConnection *aConn, nsresult aStatus)
{
  nsresult rv;
  nsXPIDLString passwd;

  // Make sure that the Init() worked properly
  NS_ENSURE_SUCCESS(aStatus, aStatus);

  // If mLogin is set, we're expected to use it to get a password.
  //
  if (!mLogin.IsEmpty())
  {
    PRBool status;

    // get the string bundle service
    //
    nsCOMPtr<nsIStringBundleService> stringBundleSvc = 
      do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
    {
      NS_ERROR("nsAbLDAPListenerBase::OnLDAPInit():"
               " error getting string bundle service");
      return rv;
    }

    // get the LDAP string bundle
    //
    nsCOMPtr<nsIStringBundle> ldapBundle;
    rv = stringBundleSvc->CreateBundle("chrome://mozldap/locale/ldap.properties",
                                       getter_AddRefs(ldapBundle));
    if (NS_FAILED(rv))
    {
      NS_ERROR("nsAbLDAPListenerBase::OnLDAPInit(): error creating string"
               "bundle chrome://mozldap/locale/ldap.properties");
      return rv;
    }

    // get the title for the authentication prompt
    //
    nsXPIDLString authPromptTitle;
    rv = ldapBundle->GetStringFromName(NS_LITERAL_STRING("authPromptTitle").get(),
                                       getter_Copies(authPromptTitle));
    if (NS_FAILED(rv))
    {
      NS_ERROR("nsAbLDAPListenerBase::OnLDAPInit(): error getting"
               "'authPromptTitle' string from bundle "
               "chrome://mozldap/locale/ldap.properties");
      return rv;
    }

    // get the host name for the auth prompt
    //
    nsCAutoString host;
    rv = mDirectoryUrl->GetAsciiHost(host);
    NS_ENSURE_SUCCESS(rv, rv);

    // hostTemp is only necessary to work around a code-generation 
    // bug in egcs 1.1.2 (the version of gcc that comes with Red Hat 6.2),
    // which is the default compiler for Mozilla on linux at the moment.
    //
    NS_ConvertASCIItoUTF16 hostTemp(host);
    const PRUnichar *hostArray[1] = { hostTemp.get() };

    // format the hostname into the authprompt text string
    //
    nsXPIDLString authPromptText;
    rv = ldapBundle->FormatStringFromName(NS_LITERAL_STRING("authPromptText").get(),
                                          hostArray,
                                          sizeof(hostArray) / sizeof(const PRUnichar *),
                                          getter_Copies(authPromptText));
    if (NS_FAILED(rv))
    {
      NS_ERROR("nsAbLDAPListenerBase::OnLDAPInit():"
               "error getting 'authPromptText' string from bundle "
               "chrome://mozldap/locale/ldap.properties");
      return rv;
    }

    // get the window watcher service, so we can get an auth prompter
    //
    nsCOMPtr<nsIWindowWatcher> windowWatcherSvc = 
      do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
    if (NS_FAILED(rv))
    {
      NS_ERROR("nsAbLDAPListenerBase::OnLDAPInit():"
               " couldn't get window watcher service.");
      return rv;
    }

    // get the addressbook window, as it will be used to parent the auth
    // prompter dialog
    //
    nsCOMPtr<nsIDOMWindow> abDOMWindow;
    rv = windowWatcherSvc->GetWindowByName(NS_LITERAL_STRING("addressbookWindow").get(),
                                           nsnull,
                                           getter_AddRefs(abDOMWindow));
    if (NS_FAILED(rv))
    {
      NS_ERROR("nsAbLDAPListenerBase::OnLDAPInit():"
               " error getting addressbook Window");
      return rv;
    }

    // get the auth prompter itself
    //
    nsCOMPtr<nsIAuthPrompt> authPrompter;
    rv = windowWatcherSvc->GetNewAuthPrompter(abDOMWindow,
                                              getter_AddRefs(authPrompter));
    if (NS_FAILED(rv))
    {
      NS_ERROR("nsAbLDAPMessageBase::OnLDAPInit():"
               " error getting auth prompter");
      return rv;
    }

    // get authentication password, prompting the user if necessary
    //
    // we're going to use the URL spec of the server as the "realm" for 
    // wallet to remember the password by / for.

    // Get the specification
    nsXPIDLCString spec;
    rv = mDirectoryUrl->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = authPrompter->PromptPassword(authPromptTitle.get(),
                                      authPromptText.get(),
                                      NS_ConvertUTF8toUTF16(spec).get(),
                                      nsIAuthPrompt::SAVE_PASSWORD_PERMANENTLY,
                                      getter_Copies(passwd),
                                      &status);
    if (NS_FAILED(rv) || !status)
      return NS_ERROR_FAILURE;
  }

  // Initiate the LDAP operation
  nsCOMPtr<nsILDAPOperation> ldapOperation =
    do_CreateInstance(NS_LDAPOPERATION_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILDAPMessageListener> proxyListener;
  rv = NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                            NS_GET_IID(nsILDAPMessageListener),
                            NS_STATIC_CAST(nsILDAPMessageListener *, this),
                            NS_PROXY_SYNC | NS_PROXY_ALWAYS,
                            getter_AddRefs(proxyListener));

  rv = ldapOperation->Init(mConnection, proxyListener, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  // Bind
  return ldapOperation->SimpleBind(NS_ConvertUTF16toUTF8(passwd));
}

nsresult nsAbLDAPListenerBase::OnLDAPMessageBind(nsILDAPMessage *aMessage)
{
  if (mBound)
    return NS_OK;

  // see whether the bind actually succeeded
  //
  PRInt32 errCode;
  nsresult rv = aMessage->GetErrorCode(&errCode);
  NS_ENSURE_SUCCESS(rv, rv);

  if (errCode != nsILDAPErrors::SUCCESS)
  {
    // if the login failed, tell the wallet to forget this password
    //
    if (errCode == nsILDAPErrors::INAPPROPRIATE_AUTH ||
        errCode == nsILDAPErrors::INVALID_CREDENTIALS)
    {
      // make sure the wallet service has been created, and in doing so,
      // pass in a login-failed message to tell it to forget this passwd.
      //
      // apparently getting passwords stored in the wallet
      // doesn't require the service to be running, which is why
      // this might not exist yet.
      //
      rv = NS_CreateServicesFromCategory("passwordmanager",
                                         mDirectoryUrl,
                                         "login-failed");
      if (NS_FAILED(rv))
      {
        NS_ERROR("nsLDAPAutoCompleteSession::ForgetPassword(): error"
                 " creating password manager service");
        // not much to do at this point, though conceivably we could 
        // pop up a dialog telling the user to go manually delete
        // this password in the password manager.
      }
    } 

    // XXX this error should be propagated back to the UI somehow
    return NS_ERROR_FAILURE;
  }

  mBound = PR_TRUE;
  return DoTask();
}
