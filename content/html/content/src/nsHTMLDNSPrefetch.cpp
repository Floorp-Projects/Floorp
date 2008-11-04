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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick McManus <mcmanus@ducksong.com>
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

#include "nsHTMLDNSPrefetch.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsNetUtil.h"

#include "nsIDNSListener.h"
#include "nsIDNSRecord.h"
#include "nsIDNSService.h"
#include "nsICancelable.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"
#include "nsIDocument.h"

static NS_DEFINE_CID(kDNSServiceCID, NS_DNSSERVICE_CID);
static PRBool sDisablePrefetchHTTPSPref;
static PRBool sInitialized = PR_FALSE;
static nsIDNSService *sDNSService = nsnull;

nsresult
nsHTMLDNSPrefetch::Initialize()
{
  if (sInitialized) {
    NS_WARNING("Initialize() called twice");
    return NS_OK;
  }
  
  nsContentUtils::AddBoolPrefVarCache("network.dns.disablePrefetchFromHTTPS", 
                                      &sDisablePrefetchHTTPSPref);
  
  // Default is false, so we need an explicit call to prime the cache.
  sDisablePrefetchHTTPSPref = 
    nsContentUtils::GetBoolPref("network.dns.disablePrefetchFromHTTPS", PR_TRUE);
  
  NS_IF_RELEASE(sDNSService);
  nsresult rv;
  rv = CallGetService(kDNSServiceCID, &sDNSService);
  if (NS_FAILED(rv)) return rv;
  
  sInitialized = PR_TRUE;
  return NS_OK;
}

nsresult
nsHTMLDNSPrefetch::Shutdown()
{
  if (!sInitialized) {
    NS_WARNING("Not Initialized");
    return NS_OK;
  }
  sInitialized = PR_FALSE;
  NS_IF_RELEASE(sDNSService);
  return NS_OK;
}

nsHTMLDNSPrefetch::nsHTMLDNSPrefetch(nsAString &hostname, nsIDocument *aDocument)
{
  NS_ASSERTION(aDocument, "Document Required");
  NS_ASSERTION(sInitialized, "nsHTMLDNSPrefetch is not initialized");
  
  mAllowed = IsAllowed(aDocument);
  CopyUTF16toUTF8(hostname, mHostname);
}

nsHTMLDNSPrefetch::nsHTMLDNSPrefetch(nsIURI *aURI, nsIDocument *aDocument)
{
  NS_ASSERTION(aDocument, "Document Required");
  NS_ASSERTION(aURI, "URI Required");
  NS_ASSERTION(sInitialized, "nsHTMLDNSPrefetch is not initialized");

  mAllowed = IsAllowed(aDocument);
  aURI->GetAsciiHost(mHostname);
}

PRBool
nsHTMLDNSPrefetch::IsSecureBaseContext (nsIDocument *aDocument)
{
  nsIURI *docURI = aDocument->GetDocumentURI();
  nsCAutoString scheme;
  docURI->GetScheme(scheme);
  return scheme.EqualsLiteral("https");
}

PRBool
nsHTMLDNSPrefetch::IsAllowed (nsIDocument *aDocument)
{
  if (IsSecureBaseContext(aDocument) && sDisablePrefetchHTTPSPref)
      return PR_FALSE;
    
  // Check whether the x-dns-prefetch-control HTTP response header is set to override 
  // the default. This may also be set by meta tag. Chromium treats any value other
  // than 'on' (case insensitive) as 'off'.

  nsAutoString control;
  aDocument->GetHeaderData(nsGkAtoms::headerDNSPrefetchControl, control);
  
  if (!control.IsEmpty() && !control.LowerCaseEqualsLiteral("on"))
    return PR_FALSE;

  // There is no need to do prefetch on non UI scenarios such as XMLHttpRequest.
  if (!aDocument->GetWindow())
    return PR_FALSE;

  return PR_TRUE;
}

nsresult 
nsHTMLDNSPrefetch::Prefetch(PRUint16 flags)
{
  if (mHostname.IsEmpty())
    return NS_ERROR_NOT_AVAILABLE;
  
  if (!mAllowed)
    return NS_ERROR_NOT_AVAILABLE;

  if (!sDNSService)
    return NS_ERROR_NOT_AVAILABLE;
  
  nsCOMPtr<nsICancelable> tmpOutstanding;
  
  return sDNSService->AsyncResolve(mHostname, flags, this, nsnull,
                                   getter_AddRefs(tmpOutstanding));
}

nsresult
nsHTMLDNSPrefetch::PrefetchLow()
{
  return Prefetch(nsIDNSService::RESOLVE_PRIORITY_LOW);
}

nsresult
nsHTMLDNSPrefetch::PrefetchMedium()
{
  return Prefetch(nsIDNSService::RESOLVE_PRIORITY_MEDIUM);
}

nsresult
nsHTMLDNSPrefetch::PrefetchHigh()
{
  return Prefetch(0);
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsHTMLDNSPrefetch, nsIDNSListener)

NS_IMETHODIMP
nsHTMLDNSPrefetch::OnLookupComplete(nsICancelable *request,
                                    nsIDNSRecord  *rec,
                                    nsresult       status)
{
  return NS_OK;
}
