/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Preferences.h"
#include "nsAsyncRedirectVerifyHelper.h"
#include "nsChannelProperties.h"
#include "nsCOMPtr.h"
#include "nsContentPolicyUtils.h"
#include "nsContentUtils.h"
#include "nsCSPContext.h"
#include "nsCSPParser.h"
#include "nsCSPService.h"
#include "nsError.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIChannelEventSink.h"
#include "nsIChannelPolicy.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIHttpChannel.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIPrincipal.h"
#include "nsIPropertyBag2.h"
#include "nsIScriptError.h"
#include "nsIWritablePropertyBag2.h"
#include "nsString.h"
#include "prlog.h"

using namespace mozilla;

/* =====  nsIContentSecurityPolicy impl ====== */

NS_IMETHODIMP
nsCSPContext::ShouldLoad(nsContentPolicyType aContentType,
                         nsIURI*             aContentLocation,
                         nsIURI*             aRequestOrigin,
                         nsISupports*        aRequestContext,
                         const nsACString&   aMimeTypeGuess,
                         nsISupports*        aExtra,
                         int16_t*            outDecision)
{
  *outDecision = nsIContentPolicy::ACCEPT;
  return NS_OK;
}

NS_IMETHODIMP
nsCSPContext::ShouldProcess(nsContentPolicyType aContentType,
                            nsIURI*             aContentLocation,
                            nsIURI*             aRequestOrigin,
                            nsISupports*        aRequestContext,
                            const nsACString&   aMimeType,
                            nsISupports*        aExtra,
                            int16_t*            outDecision)
{
  *outDecision = nsIContentPolicy::ACCEPT;
  return NS_OK;
}

/* ===== nsISupports implementation ========== */

NS_IMPL_ISUPPORTS(nsCSPContext,
                  nsIContentSecurityPolicy,
                  nsISerializable)

nsCSPContext::nsCSPContext()
{
}

nsCSPContext::~nsCSPContext()
{
  for (uint32_t i = 0; i < mPolicies.Length(); i++) {
    delete mPolicies[i];
  }
}

NS_IMETHODIMP
nsCSPContext::GetIsInitialized(bool *outIsInitialized)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCSPContext::GetPolicy(uint32_t aIndex, nsAString& outStr)
{
  if (aIndex >= mPolicies.Length()) {
    return NS_ERROR_FAILURE;
  }
  mPolicies[aIndex]->toString(outStr);
  return NS_OK;
}

NS_IMETHODIMP
nsCSPContext::GetPolicyCount(uint32_t *outPolicyCount)
{
  *outPolicyCount = mPolicies.Length();
  return NS_OK;
}

NS_IMETHODIMP
nsCSPContext::RemovePolicy(uint32_t aIndex)
{
  if (aIndex >= mPolicies.Length()) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  mPolicies.RemoveElementAt(aIndex);
  return NS_OK;
}

NS_IMETHODIMP
nsCSPContext::AppendPolicy(const nsAString& aPolicyString,
                           nsIURI* aSelfURI,
                           bool aReportOnly,
                           bool aSpecCompliant)
{
  NS_ASSERTION(aSelfURI, "aSelfURI required for AppendPolicy");
  nsCSPPolicy* policy = nsCSPParser::parseContentSecurityPolicy(aPolicyString, aSelfURI, aReportOnly, 0);
  if (policy) {
    mPolicies.AppendElement(policy);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSPContext::GetAllowsInlineScript(bool* outShouldReportViolations,
                                    bool* outAllowsInlineScript)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCSPContext::GetAllowsEval(bool* outShouldReportViolations,
                            bool* outAllowsEval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCSPContext::GetAllowsInlineStyle(bool* outShouldReportViolations,
                                   bool* outAllowsInlineStyle)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCSPContext::GetAllowsNonce(const nsAString& aNonce,
                             uint32_t aContentType,
                             bool* outShouldReportViolation,
                             bool* outAllowsNonce)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCSPContext::GetAllowsHash(const nsAString& aContent,
                            uint16_t aContentType,
                            bool* outShouldReportViolation,
                            bool* outAllowsHash)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCSPContext::LogViolationDetails(uint16_t aViolationType,
                                  const nsAString& aSourceFile,
                                  const nsAString& aScriptSample,
                                  int32_t aLineNum,
                                  const nsAString& aNonce,
                                  const nsAString& aContent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCSPContext::SetRequestContext(nsIURI* aSelfURI,
                                nsIURI* aReferrer,
                                nsIPrincipal* aDocumentPrincipal,
                                nsIChannel* aChannel)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCSPContext::PermitsAncestry(nsIDocShell* aDocShell, bool* outPermitsAncestry)
{
  // For now, we allows permitsAncestry, this will be fixed in Bug 994320
  *outPermitsAncestry = true;
  return NS_OK;
}

/* ===== nsISerializable implementation ====== */
NS_IMETHODIMP
nsCSPContext::Read(nsIObjectInputStream* aStream)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCSPContext::Write(nsIObjectOutputStream* aStream)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
