/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCSPContext_h___
#define nsCSPContext_h___

#include "nsCSPUtils.h"
#include "nsDataHashtable.h"
#include "nsIChannel.h"
#include "nsIChannelEventSink.h"
#include "nsIClassInfo.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIInterfaceRequestor.h"
#include "nsISerializable.h"
#include "nsIStreamListener.h"
#include "nsXPCOM.h"

#define NS_CSPCONTEXT_CONTRACTID "@mozilla.org/cspcontext;1"
 // 09d9ed1a-e5d4-4004-bfe0-27ceb923d9ac
#define NS_CSPCONTEXT_CID \
{ 0x09d9ed1a, 0xe5d4, 0x4004, \
  { 0xbf, 0xe0, 0x27, 0xce, 0xb9, 0x23, 0xd9, 0xac } }

class nsCSPContext : public nsIContentSecurityPolicy
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICONTENTSECURITYPOLICY
    NS_DECL_NSISERIALIZABLE

  public:
    nsCSPContext();
    virtual ~nsCSPContext();

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

  private:
    NS_IMETHODIMP getAllowsInternal(nsContentPolicyType aContentType,
                                    enum CSPKeyword aKeyword,
                                    const nsAString& aNonceOrContent,
                                    bool* outShouldReportViolations,
                                    bool* outIsAllowed) const;

    nsCOMPtr<nsIURI>                           mReferrer;
    uint64_t                                   mInnerWindowID; // used for web console logging
    nsTArray<nsCSPPolicy*>                     mPolicies;
    nsCOMPtr<nsIURI>                           mSelfURI;
    nsDataHashtable<nsCStringHashKey, int16_t> mShouldLoadCache;
    nsCOMPtr<nsILoadGroup>                     mCallingChannelLoadGroup;
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
    virtual ~CSPViolationReportListener();
};

// The POST of the violation report (if it happens) should not follow
// redirects, per the spec. hence, we implement an nsIChannelEventSink
// with an object so we can tell XHR to abort if a redirect happens.
class CSPReportRedirectSink MOZ_FINAL : public nsIChannelEventSink,
                                        public nsIInterfaceRequestor
{
  public:
    NS_DECL_NSICHANNELEVENTSINK
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_ISUPPORTS

  public:
    CSPReportRedirectSink();
    virtual ~CSPReportRedirectSink();
};

#endif /* nsCSPContext_h___ */
