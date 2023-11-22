/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSPEvalChecker.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ErrorResult.h"
#include "nsIParentChannel.h"
#include "nsGlobalWindowInner.h"
#include "nsContentSecurityUtils.h"
#include "nsContentUtils.h"
#include "nsCOMPtr.h"
#include "nsJSUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace {

// We use the subjectPrincipal to assert that eval() is never
// executed in system privileged context.
nsresult CheckInternal(nsIContentSecurityPolicy* aCSP,
                       nsICSPEventListener* aCSPEventListener,
                       nsIPrincipal* aSubjectPrincipal,
                       const nsAString& aExpression,
                       const nsAString& aFileNameString, uint32_t aLineNum,
                       uint32_t aColumnNum, bool* aAllowed) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aAllowed);

  // The value is set at any "return", but better to have a default value here.
  *aAllowed = false;

  // This is the non-CSP check for gating eval() use in the SystemPrincipal
#if !defined(ANDROID)
  JSContext* cx = nsContentUtils::GetCurrentJSContext();
  if (!nsContentSecurityUtils::IsEvalAllowed(
          cx, aSubjectPrincipal->IsSystemPrincipal(), aExpression)) {
    *aAllowed = false;
    return NS_OK;
  }
#endif

  if (!aCSP) {
    *aAllowed = true;
    return NS_OK;
  }

  bool reportViolation = false;
  nsresult rv = aCSP->GetAllowsEval(&reportViolation, aAllowed);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    *aAllowed = false;
    return rv;
  }

  if (reportViolation) {
    aCSP->LogViolationDetails(nsIContentSecurityPolicy::VIOLATION_TYPE_EVAL,
                              nullptr,  // triggering element
                              aCSPEventListener, aFileNameString, aExpression,
                              aLineNum, aColumnNum, u""_ns, u""_ns);
  }

  return NS_OK;
}

class WorkerCSPCheckRunnable final : public WorkerMainThreadRunnable {
 public:
  WorkerCSPCheckRunnable(WorkerPrivate* aWorkerPrivate,
                         const nsAString& aExpression,
                         const nsAString& aFileNameString, uint32_t aLineNum,
                         uint32_t aColumnNum)
      : WorkerMainThreadRunnable(aWorkerPrivate, "CSP Eval Check"_ns),
        mExpression(aExpression),
        mFileNameString(aFileNameString),
        mLineNum(aLineNum),
        mColumnNum(aColumnNum),
        mEvalAllowed(false) {}

  bool MainThreadRun() override {
    mResult = CheckInternal(
        mWorkerPrivate->GetCsp(), mWorkerPrivate->CSPEventListener(),
        mWorkerPrivate->GetLoadingPrincipal(), mExpression, mFileNameString,
        mLineNum, mColumnNum, &mEvalAllowed);
    return true;
  }

  nsresult GetResult(bool* aAllowed) {
    MOZ_ASSERT(aAllowed);
    *aAllowed = mEvalAllowed;
    return mResult;
  }

 private:
  const nsString mExpression;
  const nsString mFileNameString;
  const uint32_t mLineNum;
  const uint32_t mColumnNum;
  bool mEvalAllowed;
  nsresult mResult;
};

}  // namespace

/* static */
nsresult CSPEvalChecker::CheckForWindow(JSContext* aCx,
                                        nsGlobalWindowInner* aWindow,
                                        const nsAString& aExpression,
                                        bool* aAllowEval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aAllowEval);

  // The value is set at any "return", but better to have a default value here.
  *aAllowEval = false;

  // if CSP is enabled, and setTimeout/setInterval was called with a string,
  // disable the registration and log an error
  nsCOMPtr<Document> doc = aWindow->GetExtantDoc();
  if (!doc) {
    // if there's no document, we don't have to do anything.
    *aAllowEval = true;
    return NS_OK;
  }

  nsresult rv = NS_OK;

  // Get the calling location.
  uint32_t lineNum = 0;
  uint32_t columnNum = 1;
  nsAutoString fileNameString;
  if (!nsJSUtils::GetCallingLocation(aCx, fileNameString, &lineNum,
                                     &columnNum)) {
    fileNameString.AssignLiteral("unknown");
  }

  nsCOMPtr<nsIContentSecurityPolicy> csp = doc->GetCsp();
  rv = CheckInternal(csp, nullptr /* no CSPEventListener for window */,
                     doc->NodePrincipal(), aExpression, fileNameString, lineNum,
                     columnNum, aAllowEval);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    *aAllowEval = false;
    return rv;
  }

  return NS_OK;
}

/* static */
nsresult CSPEvalChecker::CheckForWorker(JSContext* aCx,
                                        WorkerPrivate* aWorkerPrivate,
                                        const nsAString& aExpression,
                                        bool* aAllowEval) {
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(aAllowEval);

  // The value is set at any "return", but better to have a default value here.
  *aAllowEval = false;

  // Get the calling location.
  uint32_t lineNum = 0;
  uint32_t columnNum = 1;
  nsAutoString fileNameString;
  if (!nsJSUtils::GetCallingLocation(aCx, fileNameString, &lineNum,
                                     &columnNum)) {
    fileNameString.AssignLiteral("unknown");
  }

  RefPtr<WorkerCSPCheckRunnable> r = new WorkerCSPCheckRunnable(
      aWorkerPrivate, aExpression, fileNameString, lineNum, columnNum);
  ErrorResult error;
  r->Dispatch(Canceling, error);
  if (NS_WARN_IF(error.Failed())) {
    *aAllowEval = false;
    return error.StealNSResult();
  }

  nsresult rv = r->GetResult(aAllowEval);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    *aAllowEval = false;
    return rv;
  }

  return NS_OK;
}
