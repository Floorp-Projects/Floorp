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
#include "nsIServiceManager.h"
#include "nsIHttpChannel.h"
#include "nsIURI.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"
#include "nsCRT.h"

// pref string constants
static const char kCookiesP3PStringPref[] = "network.cookie.p3p";
static const char kCookiesP3PStringDefault[] = "drdraaaa";

/***********************************
 *   nsP3PService Implementation   *
 ***********************************/

NS_IMPL_ISUPPORTS2(nsP3PService, nsICookieConsent, nsIObserver)

nsP3PService::nsP3PService() 
{
  // we can live without a prefservice, so errors here aren't fatal
  nsCOMPtr<nsIPrefBranchInternal> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefBranch) {
    prefBranch->AddObserver(kCookiesP3PStringPref, this, PR_FALSE);
  }
  PrefChanged(prefBranch);
}

nsP3PService::~nsP3PService() 
{
}

void
nsP3PService::PrefChanged(nsIPrefBranch *aPrefBranch)
{
  nsresult rv;
  if (aPrefBranch) {
    rv = aPrefBranch->GetCharPref(kCookiesP3PStringPref, getter_Copies(mCookiesP3PString));
  }

  // check for a malformed string, or no prefbranch
  if (!aPrefBranch || NS_FAILED(rv) || mCookiesP3PString.Length() != 8) {
    // reassign to default string
    mCookiesP3PString.AssignLiteral(kCookiesP3PStringDefault);
  }
}

NS_IMETHODIMP
nsP3PService::Observe(nsISupports     *aSubject,
                      const char      *aTopic,
                      const PRUnichar *aData)
{
  nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(aSubject);
  NS_ASSERTION(!nsCRT::strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic),
               "unexpected topic - we only deal with pref changes!");

  PrefChanged(prefBranch);
  return NS_OK;
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
nsP3PService::GetConsent(nsIURI         *aURI, 
                         nsIHttpChannel *aHttpChannel, 
                         PRBool          aIsForeign,
                         nsCookiePolicy *aPolicy,
                         nsCookieStatus *aStatus)
{
  *aPolicy = nsICookie::POLICY_UNKNOWN;

  nsCAutoString uriSpec;
  nsresult rv = aURI->GetSpec(uriSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aHttpChannel) {
#ifdef DEBUG
    nsCOMPtr<nsIURI> uri;
    aHttpChannel->GetURI(getter_AddRefs(uri));  

    PRBool equals;
    NS_ASSERTION(uri && NS_SUCCEEDED(uri->Equals(aURI, &equals)) && equals,
                 "URIs don't match");
#endif

    rv = ProcessResponseHeader(aHttpChannel);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  PRInt32 consent = NS_NO_POLICY;
  if (mCompactPolicy) {
    mCompactPolicy->GetConsent(uriSpec.get(), consent);
  }

  // Map consent to cookies macro
  if (consent & NS_NO_POLICY) {
    *aPolicy = nsICookie::POLICY_NONE;
  }
  else if (consent & (NS_NO_CONSENT|NS_INVALID_POLICY)) {
    *aPolicy = nsICookie::POLICY_NO_CONSENT;
  }
  else if (consent & NS_IMPLICIT_CONSENT) {
    *aPolicy = nsICookie::POLICY_IMPLICIT_CONSENT;
  }
  else if (consent & NS_EXPLICIT_CONSENT) {
    *aPolicy = nsICookie::POLICY_EXPLICIT_CONSENT;
  }
  else if (consent & NS_NON_PII_TOKEN) {
    *aPolicy = nsICookie::POLICY_NO_II;
  }  
  else {
    NS_WARNING("invalid consent");
  }

  // munge the policy into something we can get a status from
  nsCookiePolicy policyForStatus = *aPolicy;
  if (policyForStatus == nsICookie::POLICY_NO_II) {
    // the site does not collect identifiable info - treat it as if it did,
    // and asked for explicit consent
    policyForStatus = nsICookie::POLICY_EXPLICIT_CONSENT;
  }
  else if (policyForStatus == nsICookie::POLICY_UNKNOWN) {
    // default to no policy
    policyForStatus = nsICookie::POLICY_NONE;
  }

  // map the policy number into an index for the pref string.
  PRInt32 index = (policyForStatus - 1) * 2 + (aIsForeign != PR_FALSE);

  switch (mCookiesP3PString.CharAt(index)) {
    case 'a':
      *aStatus = nsICookie::STATUS_ACCEPTED;
      break;
    case 'd':
      *aStatus = nsICookie::STATUS_DOWNGRADED;
      break;
    case 'f':
      *aStatus = nsICookie::STATUS_FLAGGED;
      break;
    case 'r':
    default:
      *aStatus = nsICookie::STATUS_REJECTED;
  }

  return NS_OK;
}
