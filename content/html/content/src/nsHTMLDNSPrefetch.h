/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHTMLDNSPrefetch_h___
#define nsHTMLDNSPrefetch_h___

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsString.h"

#include "nsIDNSListener.h"
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"
#include "nsIObserver.h"

class nsIDocument;
class nsITimer;
namespace mozilla {
namespace dom {
class Link;
} // namespace dom
} // namespace mozilla

namespace mozilla {
namespace net {
class NeckoParent;
} // namespace net
} // namespace mozilla

class nsHTMLDNSPrefetch 
{
public:
  // The required aDocument parameter is the context requesting the prefetch - under
  // certain circumstances (e.g. headers, or security context) associated with
  // the context the prefetch will not be performed. 
  static bool     IsAllowed(nsIDocument *aDocument);
 
  static nsresult Initialize();
  static nsresult Shutdown();
  
  // Call one of the Prefetch* methods to start the lookup.
  //
  // The URI versions will defer DNS lookup until pageload is
  // complete, while the string versions submit the lookup to 
  // the DNS system immediately. The URI version is somewhat lighter
  // weight, but its request is also more likely to be dropped due to a 
  // full queue and it may only be used from the main thread.

  static nsresult PrefetchHigh(mozilla::dom::Link *aElement);
  static nsresult PrefetchMedium(mozilla::dom::Link *aElement);
  static nsresult PrefetchLow(mozilla::dom::Link *aElement);
  static nsresult PrefetchHigh(const nsAString &host);
  static nsresult PrefetchMedium(const nsAString &host);
  static nsresult PrefetchLow(const nsAString &host);
  static nsresult CancelPrefetchLow(const nsAString &host, nsresult aReason);
  static nsresult CancelPrefetchLow(mozilla::dom::Link *aElement,
                                    nsresult aReason);

private:
  static nsresult Prefetch(const nsAString &host, uint16_t flags);
  static nsresult Prefetch(mozilla::dom::Link *aElement, uint16_t flags);
  static nsresult CancelPrefetch(const nsAString &hostname,
                                 uint16_t flags,
                                 nsresult aReason);
  static nsresult CancelPrefetch(mozilla::dom::Link *aElement,
                                 uint16_t flags,
                                 nsresult aReason);
  
public:
  class nsListener MOZ_FINAL : public nsIDNSListener
  {
    // This class exists to give a safe callback no-op DNSListener
  public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIDNSLISTENER

    nsListener()  {}
  private:
    ~nsListener() {}
  };
  
  class nsDeferrals MOZ_FINAL: public nsIWebProgressListener
                             , public nsSupportsWeakReference
                             , public nsIObserver
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBPROGRESSLISTENER
    NS_DECL_NSIOBSERVER

    nsDeferrals();
    
    void Activate();
    nsresult Add(uint16_t flags, mozilla::dom::Link *aElement);
    
  private:
    ~nsDeferrals();
    void Flush();
    
    void SubmitQueue();
    
    uint16_t                  mHead;
    uint16_t                  mTail;
    uint32_t                  mActiveLoaderCount;

    nsCOMPtr<nsITimer>        mTimer;
    bool                      mTimerArmed;
    static void Tick(nsITimer *aTimer, void *aClosure);
    
    static const int          sMaxDeferred = 512;  // keep power of 2 for masking
    static const int          sMaxDeferredMask = (sMaxDeferred - 1);
    
    struct deferred_entry
    {
      uint16_t                         mFlags;
      nsWeakPtr                        mElement;
    } mEntries[sMaxDeferred];
  };

  friend class mozilla::net::NeckoParent;
};

#endif 
