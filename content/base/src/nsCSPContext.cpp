/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsContentPolicyUtils.h"
#include "nsContentUtils.h"
#include "nsCSPContext.h"
#include "nsCSPParser.h"
#include "nsCSPService.h"
#include "nsError.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIChannelPolicy.h"
#include "nsIClassInfoImpl.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsIHttpChannel.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIPrincipal.h"
#include "nsIPropertyBag2.h"
#include "nsIScriptError.h"
#include "nsIWritablePropertyBag2.h"
#include "nsString.h"
#include "prlog.h"

using namespace mozilla;

#if defined(PR_LOGGING)
static PRLogModuleInfo *
GetCspContextLog()
{
  static PRLogModuleInfo *gCspContextPRLog;
  if (!gCspContextPRLog)
    gCspContextPRLog = PR_NewLogModule("CSPContext");
  return gCspContextPRLog;
}
#endif

#define CSPCONTEXTLOG(args) PR_LOG(GetCspContextLog(), 4, args)

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
#ifdef PR_LOGGING
  {
  nsAutoCString spec;
  aContentLocation->GetSpec(spec);
  CSPCONTEXTLOG(("nsCSPContext::ShouldLoad, aContentLocation: %s", spec.get()));
  }
#endif

  nsresult rv = NS_OK;

  // This ShouldLoad function is called from nsCSPService::ShouldLoad,
  // which already checked a number of things, including:
  // * aContentLocation is not null; we can consume this without further checks
  // * scheme is not a whitelisted scheme (about: chrome:, etc).
  // * CSP is enabled
  // * Content Type is not whitelisted (CSP Reports, TYPE_DOCUMENT, etc).
  // * Fast Path for Apps

  // Default decision, CSP can revise it if there's a policy to enforce
  *outDecision = nsIContentPolicy::ACCEPT;

  // This may be a load or a preload. If it is a preload, the document will
  // not have been fully parsed yet, and aRequestContext will be an
  // nsIDOMHTMLDocument rather than the nsIDOMHTMLElement associated with the
  // resource. As a result, we cannot extract the element's corresponding
  // nonce attribute, and so we cannot correctly check the nonce on a preload.
  //
  // Therefore, the decision returned here for a preload may be *incorrect* as
  // it cannot take the nonce into account. We will still check the load, but
  // we will not cache the result or report a violation. When the "real load"
  // happens subsequently, we will re-check with the additional context to
  // make a final decision.
  //
  // We don't just return false because that would block all preloads and
  // degrade performance. However, we do want to block preloads that are
  // clearly blocked (their urls are not whitelisted) by CSP.

  nsCOMPtr<nsIDOMHTMLDocument> doc = do_QueryInterface(aRequestContext);
  bool isPreload = doc &&
                   (aContentType == nsIContentPolicy::TYPE_SCRIPT ||
                    aContentType == nsIContentPolicy::TYPE_STYLESHEET);

  nsAutoString nonce;
  if (!isPreload) {
    nsCOMPtr<nsIDOMHTMLElement> htmlElement = do_QueryInterface(aRequestContext);
    if (htmlElement) {
      rv = htmlElement->GetAttribute(NS_LITERAL_STRING("nonce"), nonce);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  nsAutoString violatedDirective;
  for (uint32_t p = 0; p < mPolicies.Length(); p++) {
    if (!mPolicies[p]->permits(aContentType,
                               aContentLocation,
                               nonce,
                               violatedDirective)) {
      // If the policy is violated and not report-only, reject the load and
      // report to the console
      if (!mPolicies[p]->getReportOnlyFlag()) {
        CSPCONTEXTLOG(("nsCSPContext::ShouldLoad, nsIContentPolicy::REJECT_SERVER"));
        *outDecision = nsIContentPolicy::REJECT_SERVER;
      }

      // Do not send a report or notify observers if this is a preload - the
      // decision may be wrong due to the inability to get the nonce, and will
      // incorrectly fail the unit tests.
      if (!isPreload) {
        nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
        NS_ASSERTION(observerService, "CSP requires observer service.");

        observerService->NotifyObservers(aContentLocation,
                                         CSP_VIOLATION_TOPIC,
                                         violatedDirective.get());
      }

      // TODO: future patches fix:
      // * AsyncReportViolation, bug 994322
      // * Console error reporting, bug 994322
    }
  }
#ifdef PR_LOGGING
  {
  nsAutoCString spec;
  aContentLocation->GetSpec(spec);
  CSPCONTEXTLOG(("nsCSPContext::ShouldLoad, decision: %s, aContentLocation: %s", *outDecision ? "load" : "deny", spec.get()));
  }
#endif
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

NS_IMPL_CLASSINFO(nsCSPContext,
                  nullptr,
                  nsIClassInfo::MAIN_THREAD_ONLY,
                  NS_CSPCONTEXT_CID)

NS_IMPL_ISUPPORTS_CI(nsCSPContext,
                     nsIContentSecurityPolicy,
                     nsISerializable)

nsCSPContext::nsCSPContext()
  : mSelfURI(nullptr)
{
  CSPCONTEXTLOG(("nsCSPContext::nsCSPContext"));
}

nsCSPContext::~nsCSPContext()
{
  CSPCONTEXTLOG(("nsCSPContext::~nsCSPContext"));
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
    return NS_ERROR_ILLEGAL_VALUE;
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
  CSPCONTEXTLOG(("nsCSPContext::AppendPolicy: %s",
                 NS_ConvertUTF16toUTF8(aPolicyString).get()));

  if (aSelfURI) {
    // aSelfURI will be disregarded since we will remove it with bug 991474
    NS_WARNING("aSelfURI should be a nullptr in AppendPolicy and removed in bug 991474");
  }

  // Use the mSelfURI from setRequestContext, see bug 991474
  NS_ASSERTION(mSelfURI, "mSelfURI required for AppendPolicy, but not set");
  nsCSPPolicy* policy = nsCSPParser::parseContentSecurityPolicy(aPolicyString, mSelfURI, aReportOnly, 0);
  if (policy) {
    mPolicies.AppendElement(policy);
  }
  return NS_OK;
}

// aNonceOrContent either holds the nonce-value or otherwise the content
// of the element to be hashed.
NS_IMETHODIMP
nsCSPContext::getAllowsInternal(nsContentPolicyType aContentType,
                                enum CSPKeyword aKeyword,
                                const nsAString& aNonceOrContent,
                                bool* outShouldReportViolation,
                                bool* outIsAllowed) const
{
  *outShouldReportViolation = false;
  *outIsAllowed = true;

  // Skip things that aren't hash/nonce compatible
  if (aKeyword == CSP_NONCE || aKeyword == CSP_HASH) {
    if (!(aContentType == nsIContentPolicy::TYPE_SCRIPT ||
          aContentType == nsIContentPolicy::TYPE_STYLESHEET)) {
      *outIsAllowed = false;
      return NS_OK;
    }
  }

  for (uint32_t i = 0; i < mPolicies.Length(); i++) {
    if (!mPolicies[i]->allows(aContentType,
                              aKeyword,
                              aNonceOrContent)) {
      // policy is violated: must report the violation and allow the inline
      // script if the policy is report-only.
      *outShouldReportViolation = true;
      if (!mPolicies[i]->getReportOnlyFlag()) {
        *outIsAllowed = false;
      }
    }
  }
  CSPCONTEXTLOG(("nsCSPContext::getAllowsInternal, aContentType: %d, aKeyword: %s, aNonceOrContent: %s, isAllowed: %s",
                aContentType,
                aKeyword == CSP_HASH ? "hash" : CSP_EnumToKeyword(aKeyword),
                NS_ConvertUTF16toUTF8(aNonceOrContent).get(),
                *outIsAllowed ? "load" : "deny"));
  return NS_OK;
}

NS_IMETHODIMP
nsCSPContext::GetAllowsInlineScript(bool* outShouldReportViolation,
                                    bool* outAllowsInlineScript)
{
  return getAllowsInternal(nsIContentPolicy::TYPE_SCRIPT,
                           CSP_UNSAFE_INLINE,
                           EmptyString(),
                           outShouldReportViolation,
                           outAllowsInlineScript);
}

NS_IMETHODIMP
nsCSPContext::GetAllowsEval(bool* outShouldReportViolation,
                            bool* outAllowsEval)
{
  return getAllowsInternal(nsIContentPolicy::TYPE_SCRIPT,
                           CSP_UNSAFE_EVAL,
                           EmptyString(),
                           outShouldReportViolation,
                           outAllowsEval);
}

NS_IMETHODIMP
nsCSPContext::GetAllowsInlineStyle(bool* outShouldReportViolation,
                                   bool* outAllowsInlineStyle)
{
  return getAllowsInternal(nsIContentPolicy::TYPE_STYLESHEET,
                           CSP_UNSAFE_INLINE,
                           EmptyString(),
                           outShouldReportViolation,
                           outAllowsInlineStyle);
}

NS_IMETHODIMP
nsCSPContext::GetAllowsNonce(const nsAString& aNonce,
                             uint32_t aContentType,
                             bool* outShouldReportViolation,
                             bool* outAllowsNonce)
{
  return getAllowsInternal(aContentType,
                           CSP_NONCE,
                           aNonce,
                           outShouldReportViolation,
                           outAllowsNonce);
}

NS_IMETHODIMP
nsCSPContext::GetAllowsHash(const nsAString& aContent,
                            uint16_t aContentType,
                            bool* outShouldReportViolation,
                            bool* outAllowsHash)
{
  return getAllowsInternal(aContentType,
                           CSP_HASH,
                           aContent,
                           outShouldReportViolation,
                           outAllowsHash);
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
  if (!aSelfURI && !aChannel) {
    CSPCONTEXTLOG(("nsCSPContext::SetRequestContext: !selfURI && !aChannel provided"));
    return NS_ERROR_FAILURE;
  }

  mSelfURI = aSelfURI;

  if (!mSelfURI) {
    nsresult rv = aChannel->GetURI(getter_AddRefs(mSelfURI));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ASSERTION(mSelfURI, "No mSelfURI in SetRequestContext, can not translate 'self' into actual URI");

  // aDocumentPrincipal will be removed in bug 994872
  // aReferrer will be used in the patch for bug 994322

  return NS_OK;
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
  nsresult rv;
  nsCOMPtr<nsISupports> supports;

  rv = NS_ReadOptionalObject(aStream, true, getter_AddRefs(supports));
  NS_ENSURE_SUCCESS(rv, rv);

  mSelfURI = do_QueryInterface(supports);
  NS_ASSERTION(mSelfURI, "need a self URI to de-serialize");

  uint32_t numPolicies;
  rv = aStream->Read32(&numPolicies);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString policyString;

  while (numPolicies > 0) {
    numPolicies--;

    rv = aStream->ReadString(policyString);
    NS_ENSURE_SUCCESS(rv, rv);

    bool reportOnly = false;
    rv = aStream->ReadBoolean(&reportOnly);
    NS_ENSURE_SUCCESS(rv, rv);

    bool specCompliant = false;
    rv = aStream->ReadBoolean(&specCompliant);
    NS_ENSURE_SUCCESS(rv, rv);

    // Using the new backend, we don't support non-spec-compliant policies, so
    // skip any of those, will be fixed in bug 991466
    if (!specCompliant) {
      continue;
    }

    nsCSPPolicy* policy = nsCSPParser::parseContentSecurityPolicy(policyString,
                                                                  mSelfURI,
                                                                  reportOnly,
                                                                  0);
    if (policy) {
      mPolicies.AppendElement(policy);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCSPContext::Write(nsIObjectOutputStream* aStream)
{
  nsresult rv = NS_WriteOptionalCompoundObject(aStream,
                                               mSelfURI,
                                               NS_GET_IID(nsIURI),
                                               true);
  NS_ENSURE_SUCCESS(rv, rv);

  // Serialize all the policies.
  aStream->Write32(mPolicies.Length());

  nsAutoString polStr;
  for (uint32_t p = 0; p < mPolicies.Length(); p++) {
    mPolicies[p]->toString(polStr);
    aStream->WriteWStringZ(polStr.get());
    aStream->WriteBoolean(mPolicies[p]->getReportOnlyFlag());
    // Setting specCompliant boolean for backwards compatibility (fix in bug 991466)
    aStream->WriteBoolean(true);
  }
  return NS_OK;
}
