/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsCCodebasePrincipal.h"
#include "nsPrincipal.h"
#include "xp.h"

NS_DEFINE_IID(kICodebasePrincipalIID, NS_ICODEBASEPRINCIPAL_IID);
////////////////////////////////////////////////////////////////////////////
// from nsISupports:

// These macros produce simple version of QueryInterface and AddRef.
// See the nsISupports.h header file for DETAILS.

NS_IMPL_ADDREF(nsCCodebasePrincipal)
NS_IMPL_RELEASE(nsCCodebasePrincipal)
NS_IMPL_QUERY_INTERFACE(nsCCodebasePrincipal, kICodebasePrincipalIID);

////////////////////////////////////////////////////////////////////////////
// from nsIPrincipal:

NS_METHOD
nsCCodebasePrincipal::IsTrusted(const char* scope, PRBool *pbIsTrusted)
{
   if(m_pNSPrincipal == NULL)
   {
      *pbIsTrusted = PR_FALSE;
      return NS_ERROR_ILLEGAL_VALUE;
   }
   *pbIsTrusted = m_pNSPrincipal->isSecurePrincipal();
   return NS_OK;
}
     


/**
 * Returns the codebase URL of the principal.
 *
 * @param result - the resulting codebase URL
 */
NS_METHOD
nsCCodebasePrincipal::GetURL(const char **ppCodeBaseURL)
{
   if(m_pNSPrincipal == NULL)
   {
      *ppCodeBaseURL = NULL;
      return NS_ERROR_ILLEGAL_VALUE;
   }
   *ppCodeBaseURL = m_pNSPrincipal->getKey();
   return NS_OK;
}

////////////////////////////////////////////////////////////////////////////
// from nsCCodebasePrincipal:

nsCCodebasePrincipal::nsCCodebasePrincipal(const char *codebaseURL, 
                                           nsresult *result)
{
   m_pNSPrincipal = new nsPrincipal(nsPrincipalType_CodebaseExact, 
                                    (void *)codebaseURL, 
                                    XP_STRLEN(codebaseURL));
   if(m_pNSPrincipal == NULL)
   {
      *result = NS_ERROR_OUT_OF_MEMORY;
      return;
   }
   *result = NS_OK;
}

nsCCodebasePrincipal::nsCCodebasePrincipal(nsPrincipal *pNSPrincipal)
{
   m_pNSPrincipal = pNSPrincipal;
}

nsCCodebasePrincipal::~nsCCodebasePrincipal(void)
{
   delete m_pNSPrincipal;
}

nsPrincipal*
nsCCodebasePrincipal::GetPeer(void)
{
   return m_pNSPrincipal;
}
