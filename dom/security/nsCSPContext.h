/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCSPContext_h___
#define nsCSPContext_h___

#include "mozilla/dom/nsCSPUtils.h"
#include "nsDataHashtable.h"
#include "nsIChannel.h"
#include "nsIChannelEventSink.h"
#include "nsIClassInfo.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIInterfaceRequestor.h"
#include "nsISerializable.h"
#include "nsIStreamListener.h"
#include "nsWeakReference.h"
#include "nsXPCOM.h"

#define NS_CSPCONTEXT_CONTRACTID "@mozilla.org/cspcontext;1"
 // 09d9ed1a-e5d4-4004-bfe0-27ceb923d9ac
#define NS_CSPCONTEXT_CID \
{ 0x09d9ed1a, 0xe5d4, 0x4004, \
  { 0xbf, 0xe0, 0x27, 0xce, 0xb9, 0x23, 0xd9, 0xac } }

class nsINetworkInterceptController;
struct ConsoleMsgQueueElem;

class nsCSPContext : public nsIContentSecurityPolicy
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICONTENTSECURITYPOLICY
    NS_DECL_NSISERIALIZABLE

  protected:
    virtual ~nsCSPContext();

  public:
    nsCSPContext();

    /**
     * SetRequestContext() needs to be called before the innerWindowID
     * is initialized on the document. Use this function to call back to
     * flush queued up console messages and initalize the innerWindowID.
     */
    void flushConsoleMessages();

    void logToConsole(const char16_t* aName,
                      const char16_t** aParams,
                      uint32_t aParamsLength,
                      const nsAString& aSourceName,
                      const nsAString& aSourceLine,
                      uint32_t aLineNumber,
                      uint32_t aColumnNumber,
                      uint32_t aSeverityFlag);

    nsresult SendReports(nsISupports* aBlockedContentSource,
                         nsIURI* aOriginalURI,
                         nsAString& aViolatedDirective,
                         uint32_t aViolatedPolicyIndex,
                         nsAString& aSourceFile,
                         nsAString& aScriptSample,
                         uint32_t aLineNum);

    nsresult AsyncReportViolation(nsISupports* aBlockedContentSource,
                                  nsIURI* aOriginalURI,
                                  const nsAString& aViolatedDirective,
                                  uint32_t aViolatedPolicyIndex,
                                  const nsAString& aObserverSubject,
                                  const nsAString& aSourceFile,
                                  const nsAString& aScriptSample,
                                  uint32_t aLineNum);

    // Hands off! Don't call this method unless you know what you
    // are doing. It's only supposed to be called from within
    // the principal destructor to avoid a tangling pointer.
    void clearLoadingPrincipal() {
      mLoadingPrincipal = nullptr;
    }

    nsWeakPtr GetLoadingContext(){
      return mLoadingContext;
    }

  private:
    bool permitsInternal(CSPDirective aDir,
                         nsIURI* aContentLocation,
                         nsIURI* aOriginalURI,
                         const nsAString& aNonce,
                         bool aWasRedirected,
                         bool aIsPreload,
                         bool aSpecific,
                         bool aSendViolationReports,
                         bool aSendContentLocationInViolationReports,
                         bool aParserCreated);

    // helper to report inline script/style violations
    void reportInlineViolation(nsContentPolicyType aContentType,
                               const nsAString& aNonce,
                               const nsAString& aContent,
                               const nsAString& aViolatedDirective,
                               uint32_t aViolatedPolicyIndex,
                               uint32_t aLineNumber);

    nsString                                   mReferrer;
    uint64_t                                   mInnerWindowID; // used for web console logging
    nsTArray<nsCSPPolicy*>                     mPolicies;
    nsCOMPtr<nsIURI>                           mSelfURI;
    nsDataHashtable<nsCStringHashKey, int16_t> mShouldLoadCache;
    nsCOMPtr<nsILoadGroup>                     mCallingChannelLoadGroup;
    nsWeakPtr                                  mLoadingContext;
    // The CSP hangs off the principal, so let's store a raw pointer of the principal
    // to avoid memory leaks. Within the destructor of the principal we explicitly
    // set mLoadingPrincipal to null.
    nsIPrincipal*                              mLoadingPrincipal;

    // helper members used to queue up web console messages till
    // the windowID becomes available. see flushConsoleMessages()
    nsTArray<ConsoleMsgQueueElem>              mConsoleMsgQueue;
    bool                                       mQueueUpMessages;
};

// Class that listens to violation report transmission and logs errors.
class CSPViolationReportListener : public nsIStreamListener
{
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
                                    public nsIInterfaceRequestor
{
  public:
    NS_DECL_NSICHANNELEVENTSINK
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_ISUPPORTS

  public:
    CSPReportRedirectSink();

    void SetInterceptController(nsINetworkInterceptController* aInterceptController);

  protected:
    virtual ~CSPReportRedirectSink();

  private:
    nsCOMPtr<nsINetworkInterceptController> mInterceptController;
};

#endif /* nsCSPContext_h___ */
