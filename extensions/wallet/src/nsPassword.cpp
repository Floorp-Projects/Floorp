/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 * Contributor(s): 
 */

#include "nsPassword.h"
#include "nsString.h"
#include "prmem.h"

// nsPassword Implementation

NS_IMPL_ISUPPORTS2(nsPassword, nsIPassword, nsISupportsWeakReference);

nsPassword::nsPassword() {
  NS_INIT_REFCNT();
}

nsPassword::nsPassword(char * host, PRUnichar * user, PRUnichar * pswd) {
  passwordHost = host;
  passwordUser = user;
  passwordPswd = pswd;
  NS_INIT_REFCNT();
}

nsPassword::~nsPassword(void) {
  CRTFREEIF(passwordHost);
  CRTFREEIF(passwordUser);
  CRTFREEIF(passwordPswd);
}

NS_IMETHODIMP nsPassword::GetHost(char * *aHost) {
  if (passwordHost) {
    *aHost = (char *) nsMemory::Clone(passwordHost, nsCRT::strlen(passwordHost) + 1);
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsPassword::GetUser(PRUnichar * *aUser) {
  if (passwordUser) {
    *aUser = (PRUnichar *) nsMemory::Clone(passwordUser, 2*(nsCRT::strlen(passwordUser) + 1));
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsPassword::GetPassword(PRUnichar * *aPswd) {
  if (passwordPswd) {
    *aPswd = (PRUnichar *) nsMemory::Clone(passwordPswd, 2*(nsCRT::strlen(passwordPswd) + 1));
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
}
