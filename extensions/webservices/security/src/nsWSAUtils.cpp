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
 * The Original Code is the web scripts access security code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
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


#include "nsWSAUtils.h"
#include "nsReadableUtils.h"
#include "nsIStringBundle.h"
#include "nsIConsoleService.h"
#include "nsIServiceManager.h"
#include "nsIDNSService.h"
#include "nsIRequest.h"
#include "nsEventQueueUtils.h"
#include "nsAutoPtr.h"
#include "nsNetCID.h"

static const char kSecurityProperties[] =
  "chrome://communicator/locale/webservices/security.properties";
static NS_DEFINE_CID(kDNSServiceCID, NS_DNSSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

class nsDNSListener : public nsIDNSListener
{
public:
  nsDNSListener();
  virtual ~nsDNSListener();
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDNSLISTENER

  nsCString mOfficialHostName;
  PRBool mLookupFinished;
};

#ifdef DEBUG

struct TestStruct {
  const char* lhs; // string that contains the wild char(s).
  const char* rhs; // string that is compared against lhs.
  PRBool equal;    // set to true if lhs and rhs are expected 
                   // to be equal else set false;
};

static 
const TestStruct kStrings[] = {
  { "f*o*bar", "foobar", PR_TRUE },
  { "foo*bar", "foofbar", PR_TRUE },
  { "*foo*bar", "ffoofoobbarbarbar", PR_TRUE },
  { "*foo*bar*barbar", "ffoofoobbarbarbar", PR_TRUE },
  { "http://*.*.*/*", "http://www.mozilla.org/", PR_TRUE},
  { "http://*/*", "http://www.mozilla.org/", PR_TRUE},
  { "http://*.mozilla.org/*/*", "http://www.mozilla.org/Projects/", PR_TRUE},
  { "http://www.m*zi*la.org/*", "http://www.mozilla.org/", PR_TRUE },
  { "http://www.mozilla.org/*.html", "http://www.mozilla.org/owners.html", PR_TRUE },
  { "http://www.mozilla.org/*.htm*", "http://www.mozilla.org/owners.html", PR_TRUE },
  { "http://www.mozilla.org/*rs.htm*", "http://www.mozilla.org/ownres.html", PR_FALSE },
  { "http://www.mozilla.org/a*c.html", "http://www.mozilla.org/abcd.html", PR_FALSE },
  { "https://www.mozilla.org/*", "http://www.mozilla.org/abcd.html", PR_FALSE },
};

void 
nsWSAUtils::VerifyIsEqual()
{
  static PRUint32 size = NS_ARRAY_LENGTH(kStrings);
  PRUint32 i;
  for (i = 0; i < size; ++i) {
    if (IsEqual(NS_ConvertUTF8toUCS2(kStrings[i].lhs), 
                NS_ConvertUTF8toUCS2(kStrings[i].rhs)) 
                != kStrings[i].equal) {
      const char* equal = 
        kStrings[i].equal ? "equivalent" : 
                            "not equivalent";
      printf("\nTest Failed: %s is %s to %s.\n", 
             kStrings[i].lhs, equal, kStrings[i].rhs);
    }
  }
}

#endif

/**
 *  This method compares two strings where the lhs string value may contain 
 *  asterisk. Therefore, an lhs with a value of "he*o" should be equal to a rhs
 *  value of "hello" or "hero" etc.. These strings are compared as follows:
 *  1) Characters before the first asterisk are compared from left to right.
 *     Thus if the lhs string did not contain an asterisk then we just do
 *     a simple string comparison.
 *  2) Match a pattern, found between asterisk. That is, if lhs and rhs were 
 *     "h*ll*" and "hello" respectively, then compare the pattern "ll".
 *  3) Characters after the last asterisk are compared from right to left.
 *     Thus, "*lo" == "hello" and != "blow"
 */
PRBool 
nsWSAUtils::IsEqual(const nsAString& aLhs, const nsAString& aRhs) 
{
  nsAString::const_iterator lhs_begin, lhs_end;
  nsAString::const_iterator rhs_begin, rhs_end;
 
  aLhs.BeginReading(lhs_begin);
  aLhs.EndReading(lhs_end);
  aRhs.BeginReading(rhs_begin);
  aRhs.EndReading(rhs_end);

  PRBool pattern_before_asterisk = PR_TRUE; 
  nsAString::const_iterator curr_posn = lhs_begin;
  while (curr_posn != lhs_end) {
    if (*lhs_begin == '*') {
      pattern_before_asterisk = PR_FALSE;
      ++lhs_begin; // Do this to not include '*' when pattern matching.
    }
    else if (pattern_before_asterisk) {
      // Match character by character to see if lhs and rhs are identical
      if (*curr_posn != *rhs_begin) {
        return PR_FALSE;
      }
      ++lhs_begin;
      ++curr_posn;
      ++rhs_begin;
      if (rhs_begin == rhs_end &&
          curr_posn == lhs_end) {
        return PR_TRUE; // lhs and rhs matched perfectly
      }
    }
    else if (++curr_posn == lhs_end) {
      if (curr_posn != lhs_begin) {
        // Here we're matching the last few characters to make sure
        // that lhs is actually equal to rhs. Ex. "a*c" != "abcd"
        // and "*xabcd" != "abcd".
        PRBool done = PR_FALSE;
        for (;;) {
          if (--curr_posn == lhs_begin)
            done = PR_TRUE;
          if (rhs_end == rhs_begin)
            return PR_FALSE;
          if (*(--rhs_end) != *curr_posn)
            return PR_FALSE;
          if (done)
            return PR_TRUE;
        }
      }
      // No discrepency between lhs and rhs
      return PR_TRUE;
    }
    else if (*curr_posn == '*') {
      // Matching pattern between asterisks. That is, in "h*ll*" we
      // check to see if "ll" exists in the rhs string.
      const nsAString& pattern = Substring(lhs_begin, curr_posn);

      nsAString::const_iterator tmp_end = rhs_end;
      if (!FindInReadable(pattern, rhs_begin, rhs_end)) {
         return PR_FALSE;
      }
      rhs_begin = rhs_end;
      rhs_end   = tmp_end;
      lhs_begin = curr_posn;
    }
  }

  return PR_FALSE;
}

nsresult
nsWSAUtils::ReportError(const PRUnichar* aMessageID, 
                        const PRUnichar** aInputs, 
                        const PRInt32 aLength)
{
  nsCOMPtr<nsIStringBundleService> bundleService
    = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  NS_ENSURE_TRUE(bundleService, NS_OK); // intentionally returning NS_OK;

  nsCOMPtr<nsIStringBundle> bundle;
  bundleService->CreateBundle(kSecurityProperties, getter_AddRefs(bundle));
  NS_ENSURE_TRUE(bundle, NS_OK);

  nsXPIDLString message;
  bundle->FormatStringFromName(aMessageID, aInputs, aLength,
                               getter_Copies(message));

  nsCOMPtr<nsIConsoleService> consoleService = 
    do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  NS_ENSURE_TRUE(consoleService, NS_OK); // intentionally returning NS_OK;
  
  return consoleService->LogStringMessage(message.get());
}

nsresult
nsWSAUtils::GetOfficialHostName(nsIURI* aServiceURI,
                                nsACString& aResult)
{
  NS_ASSERTION(aServiceURI, "Cannot get FQDN for a null URI!");
 
  if (!aServiceURI)
    return NS_ERROR_NULL_POINTER;

  nsresult rv;
  nsCOMPtr<nsIDNSService> dns(do_GetService(kDNSServiceCID, &rv));
  
  if (NS_FAILED(rv)) 
    return rv;
  
  nsCAutoString host;
  aServiceURI->GetHost(host);

  nsRefPtr<nsDNSListener> listener = new nsDNSListener();
  NS_ENSURE_TRUE(listener, NS_ERROR_OUT_OF_MEMORY);
    
  nsCOMPtr<nsIEventQueueService> eventQService = 
    do_GetService(kEventQueueServiceCID, &rv);
  
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIEventQueue> eventQ;
  rv = eventQService->PushThreadEventQueue(getter_AddRefs(eventQ));
  
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDNSRequest> dummy;
  rv = dns->AsyncResolve(host, nsIDNSService::RESOLVE_CANONICAL_NAME,
                         listener, eventQ, getter_AddRefs(dummy));
  
  PLEvent *ev;
  while (NS_SUCCEEDED(rv) && !listener->mLookupFinished) {
    rv = eventQ->WaitForEvent(&ev);
    NS_ASSERTION(NS_SUCCEEDED(rv), "WaitForEvent failed");
    if (NS_SUCCEEDED(rv)) {
      rv = eventQ->HandleEvent(ev);
      NS_ASSERTION(NS_SUCCEEDED(rv), "HandleEvent failed");
    }
  }

  aResult.Assign(listener->mOfficialHostName);

  eventQService->PopThreadEventQueue(eventQ);
   
  return rv;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDNSListener,
                              nsIDNSListener)

nsDNSListener::nsDNSListener()
  : mLookupFinished(PR_FALSE)
{
}

nsDNSListener::~nsDNSListener()
{
}

NS_IMETHODIMP
nsDNSListener::OnLookupComplete(nsIDNSRequest* aRequest, 
                                nsIDNSRecord* aRecord,
                                nsresult aStatus)
{
  if (aRecord)
    aRecord->GetCanonicalName(mOfficialHostName);

  mLookupFinished = PR_TRUE;
  return NS_OK;
}

