/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCSPContext_h___
#define nsCSPContext_h___

#include "mozilla/dom/nsCSPUtils.h"
#include "mozilla/dom/SecurityPolicyViolationEvent.h"
#include "mozilla/StaticPrefs_security.h"
#include "nsIChannel.h"
#include "nsIChannelEventSink.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIInterfaceRequestor.h"
#include "nsIStreamListener.h"
#include "nsIWeakReferenceUtils.h"
#include "nsXPCOM.h"

#define NS_CSPCONTEXT_CONTRACTID "@mozilla.org/cspcontext;1"
// 09d9ed1a-e5d4-4004-bfe0-27ceb923d9ac
#define NS_CSPCONTEXT_CID                            \
  {                                                  \
    0x09d9ed1a, 0xe5d4, 0x4004, {                    \
      0xbf, 0xe0, 0x27, 0xce, 0xb9, 0x23, 0xd9, 0xac \
    }                                                \
  }

class nsINetworkInterceptController;
class nsIEventTarget;
struct ConsoleMsgQueueElem;

namespace mozilla {
namespace dom {
class Element;
}
namespace ipc {
class ContentSecurityPolicy;
}
}  // namespace mozilla

class nsCSPContext : public nsIContentSecurityPolicy {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTSECURITYPOLICY
  NS_DECL_NSISERIALIZABLE

 protected:
  virtual ~nsCSPContext();

 public:
  nsCSPContext();

  static bool Equals(nsIContentSecurityPolicy* aCSP,
                     nsIContentSecurityPolicy* aOtherCSP);

  // Init a CSP from a different CSP
  nsresult InitFromOther(nsCSPContext* otherContext);

  /**
   * SetRequestContextWithDocument() needs to be called before the
   * innerWindowID is initialized on the document. Use this function
   * to call back to flush queued up console messages and initialize
   * the innerWindowID. Node, If SetRequestContextWithPrincipal() was
   * called then we do not have a innerWindowID anyway and hence
   * we can not flush messages to the correct console.
   */
  void flushConsoleMessages();

  void logToConsole(const char* aName, const nsTArray<nsString>& aParams,
                    const nsAString& aSourceName, const nsAString& aSourceLine,
                    uint32_t aLineNumber, uint32_t aColumnNumber,
                    uint32_t aSeverityFlag);

  /**
   * Construct SecurityPolicyViolationEventInit structure.
   *
   * @param aBlockedURI
   *        A nsIURI: the source of the violation.
   * @param aOriginalUri
   *        The original URI if the blocked content is a redirect, else null
   * @param aViolatedDirective
   *        the directive that was violated (string).
   * @param aSourceFile
   *        name of the file containing the inline script violation
   * @param aScriptSample
   *        a sample of the violating inline script
   * @param aLineNum
   *        source line number of the violation (if available)
   * @param aColumnNum
   *        source column number of the violation (if available)
   * @param aViolationEventInit
   *        The output
   */
  nsresult GatherSecurityPolicyViolationEventData(
      nsIURI* aBlockedURI, const nsACString& aBlockedString,
      nsIURI* aOriginalURI, nsAString& aViolatedDirective,
      uint32_t aViolatedPolicyIndex, nsAString& aSourceFile,
      nsAString& aScriptSample, uint32_t aLineNum, uint32_t aColumnNum,
      mozilla::dom::SecurityPolicyViolationEventInit& aViolationEventInit);

  nsresult SendReports(
      const mozilla::dom::SecurityPolicyViolationEventInit& aViolationEventInit,
      uint32_t aViolatedPolicyIndex);

  nsresult FireViolationEvent(
      mozilla::dom::Element* aTriggeringElement,
      nsICSPEventListener* aCSPEventListener,
      const mozilla::dom::SecurityPolicyViolationEventInit&
          aViolationEventInit);

  enum BlockedContentSource {
    eUnknown,
    eInline,
    eEval,
    eSelf,
  };

  nsresult AsyncReportViolation(
      mozilla::dom::Element* aTriggeringElement,
      nsICSPEventListener* aCSPEventListener, nsIURI* aBlockedURI,
      BlockedContentSource aBlockedContentSource, nsIURI* aOriginalURI,
      const nsAString& aViolatedDirective, uint32_t aViolatedPolicyIndex,
      const nsAString& aObserverSubject, const nsAString& aSourceFile,
      const nsAString& aScriptSample, uint32_t aLineNum, uint32_t aColumnNum);

  // Hands off! Don't call this method unless you know what you
  // are doing. It's only supposed to be called from within
  // the principal destructor to avoid a tangling pointer.
  void clearLoadingPrincipal() { mLoadingPrincipal = nullptr; }

  nsWeakPtr GetLoadingContext() { return mLoadingContext; }

  static uint32_t ScriptSampleMaxLength() {
    return std::max(
        mozilla::StaticPrefs::security_csp_reporting_script_sample_max_length(),
        0);
  }

  void AddIPCPolicy(const mozilla::ipc::ContentSecurityPolicy& aPolicy);
  void SerializePolicies(
      nsTArray<mozilla::ipc::ContentSecurityPolicy>& aPolicies);

 private:
  void EnsureIPCPoliciesRead();

  bool permitsInternal(CSPDirective aDir,
                       mozilla::dom::Element* aTriggeringElement,
                       nsICSPEventListener* aCSPEventListener,
                       nsIURI* aContentLocation, nsIURI* aOriginalURIIfRedirect,
                       const nsAString& aNonce, bool aIsPreload, bool aSpecific,
                       bool aSendViolationReports,
                       bool aSendContentLocationInViolationReports,
                       bool aParserCreated);

  // helper to report inline script/style violations
  void reportInlineViolation(nsContentPolicyType aContentType,
                             mozilla::dom::Element* aTriggeringElement,
                             nsICSPEventListener* aCSPEventListener,
                             const nsAString& aNonce, const nsAString& aContent,
                             const nsAString& aViolatedDirective,
                             uint32_t aViolatedPolicyIndex,
                             uint32_t aLineNumber, uint32_t aColumnNumber);

  nsString mReferrer;
  uint64_t mInnerWindowID;          // used for web console logging
  bool mSkipAllowInlineStyleCheck;  // used to allow Devtools to edit styles
  // When deserializing an nsCSPContext instance, we initially just keep the
  // policies unparsed. We will only reconstruct actual CSP policy instances
  // when there's an attempt to use the CSP. Given a better way to serialize/
  // deserialize individual nsCSPPolicy objects, this performance
  // optimization could go away.
  nsTArray<mozilla::ipc::ContentSecurityPolicy> mIPCPolicies;
  nsTArray<nsCSPPolicy*> mPolicies;
  nsCOMPtr<nsIURI> mSelfURI;
  nsCOMPtr<nsILoadGroup> mCallingChannelLoadGroup;
  nsWeakPtr mLoadingContext;
  nsCOMPtr<nsIPrincipal> mLoadingPrincipal;

  // helper members used to queue up web console messages till
  // the windowID becomes available. see flushConsoleMessages()
  nsTArray<ConsoleMsgQueueElem> mConsoleMsgQueue;
  bool mQueueUpMessages;
  nsCOMPtr<nsIEventTarget> mEventTarget;
};

// Class that listens to violation report transmission and logs errors.
class CSPViolationReportListener : public nsIStreamListener {
 public:
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_ISUPPORTS

 public:
  CSPViolationReportListener();

 protected:
  virtual ~CSPViolationReportListener();
};

// The POST of the violation report (if it happens) should not follow
// redirects, per the spec. hence, we implement an nsIChannelEventSink
// with an object so we can tell XHR to abort if a redirect happens.
class CSPReportRedirectSink final : public nsIChannelEventSink,
                                    public nsIInterfaceRequestor {
 public:
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_ISUPPORTS

 public:
  CSPReportRedirectSink();

  void SetInterceptController(
      nsINetworkInterceptController* aInterceptController);

 protected:
  virtual ~CSPReportRedirectSink();

 private:
  nsCOMPtr<nsINetworkInterceptController> mInterceptController;
};

#endif /* nsCSPContext_h___ */
