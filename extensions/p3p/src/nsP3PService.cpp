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
 * The Original Code is the Platform for Privacy Preferences.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Harish Dhurvasula <harishd@netscape.com>
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

#include "nsP3PService.h"
#include "nsCompactPolicy.h"
#include "nsIHttpChannel.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsCOMPtr.h"

/***********************************
 *   nsP3PService Implementation   *
 ***********************************/

NS_IMPL_ISUPPORTS1(nsP3PService, nsICookieConsent)

nsP3PService::nsP3PService() 
  : mCompactPolicy(nsnull)
{
}

nsP3PService::~nsP3PService() 
{
  delete mCompactPolicy;
}

nsresult
nsP3PService::ProcessResponseHeader(nsIHttpChannel* aHttpChannel) 
{
  nsresult result = NS_OK;
  
  nsCAutoString p3pHeader;
  aHttpChannel->GetResponseHeader(NS_LITERAL_CSTRING("P3P"), p3pHeader);

  if (!p3pHeader.IsEmpty()) {
    nsCOMPtr<nsIURI> uri;
    aHttpChannel->GetURI(getter_AddRefs(uri));
      
    if (uri) {
      if (!mCompactPolicy) {
        mCompactPolicy = new nsCompactPolicy();
        NS_ENSURE_TRUE(mCompactPolicy,NS_ERROR_OUT_OF_MEMORY);
      }

      nsCAutoString spec;
      uri->GetSpec(spec);

      result = mCompactPolicy->OnHeaderAvailable(p3pHeader.get(), spec.get());
    }
  }

  return result;
}

NS_IMETHODIMP
nsP3PService::GetConsent(const char* aURI, 
                         nsIHttpChannel* aHttpChannel, 
                         PRInt32* aConsent)
{
  nsresult result = NS_OK;

  if (aHttpChannel) {

#ifdef NS_DEBUG
    nsCOMPtr<nsIURI> uri;
    aHttpChannel->GetURI(getter_AddRefs(uri));  
    
    if (uri) {
      nsCAutoString spec;
      uri->GetSpec(spec);

      if (aURI) {
        NS_ASSERTION(spec.Equals(aURI), "URIs don't match");
      }
    }
#endif

    result = ProcessResponseHeader(aHttpChannel);
    NS_ENSURE_SUCCESS(result,result);
  }

  PRInt32 consent = NS_NO_POLICY;
  result = (mCompactPolicy)? mCompactPolicy->GetConsent(aURI,consent):NS_OK;

  // Map consent to cookies macro
  if (consent & NS_NO_POLICY) {
    *aConsent = 0;
  }
  else if (consent & (NS_NO_CONSENT|NS_INVALID_POLICY)) {
    *aConsent = 2;
  }
  else if (consent & NS_IMPLICIT_CONSENT) {
    *aConsent = 4;
  }
  else if (consent & NS_EXPLICIT_CONSENT) {
    *aConsent = 6;
  }
  else if (consent & NS_NON_PII_TOKEN) {
    *aConsent = 8; // PII not collected
  }  
  else {
    NS_WARNING("invalid consent");
    *aConsent = 0;
  }

  return result;
}
