/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLDNSPrefetch_h___
#define mozilla_dom_HTMLDNSPrefetch_h___

#include "nsCOMPtr.h"
#include "nsIRequest.h"
#include "nsString.h"

class nsITimer;
class nsIURI;
namespace mozilla {

class OriginAttributes;

namespace net {
class NeckoParent;
}  // namespace net

namespace dom {
class Document;
class Element;

class SupportsDNSPrefetch;

class HTMLDNSPrefetch {
 public:
  // The required aDocument parameter is the context requesting the prefetch -
  // under certain circumstances (e.g. headers, or security context) associated
  // with the context the prefetch will not be performed.
  static bool IsAllowed(Document* aDocument);

  static nsresult Initialize();
  static nsresult Shutdown();

  // Call one of the Prefetch* methods to start the lookup.
  //
  // The URI versions will defer DNS lookup until pageload is
  // complete, while the string versions submit the lookup to
  // the DNS system immediately. The URI version is somewhat lighter
  // weight, but its request is also more likely to be dropped due to a
  // full queue and it may only be used from the main thread.
  //
  // If you are planning to use the methods with the OriginAttributes param, be
  // sure that you pass a partitioned one. See StoragePrincipalHelper.h to know
  // more.

  enum class Priority {
    Low,
    Medium,
    High,
  };
  static nsresult Prefetch(SupportsDNSPrefetch&, Element&, Priority);
  static nsresult Prefetch(
      const nsAString& host, bool isHttps,
      const OriginAttributes& aPartitionedPrincipalOriginAttributes,
      nsIRequest::TRRMode aTRRMode, Priority);
  static nsresult CancelPrefetch(
      const nsAString& host, bool isHttps,
      const OriginAttributes& aPartitionedPrincipalOriginAttributes,
      nsIRequest::TRRMode aTRRMode, Priority, nsresult aReason);
  static nsresult CancelPrefetch(SupportsDNSPrefetch&, Element&, Priority,
                                 nsresult aReason);
  static void ElementDestroyed(Element&, SupportsDNSPrefetch&);

 private:
  static uint32_t PriorityToDNSServiceFlags(Priority);

  static nsresult Prefetch(
      const nsAString& host, bool isHttps,
      const OriginAttributes& aPartitionedPrincipalOriginAttributes,
      uint32_t flags);
  static nsresult CancelPrefetch(
      const nsAString& hostname, bool isHttps,
      const OriginAttributes& aPartitionedPrincipalOriginAttributes,
      uint32_t flags, nsresult aReason);

  friend class net::NeckoParent;
};

// Elements that support DNS prefetch are expected to subclass this.
class SupportsDNSPrefetch {
 public:
  bool IsInDNSPrefetch() { return mInDNSPrefetch; }
  void SetIsInDNSPrefetch() { mInDNSPrefetch = true; }
  void ClearIsInDNSPrefetch() { mInDNSPrefetch = false; }

  void DNSPrefetchRequestStarted() {
    mDNSPrefetchDeferred = false;
    mDNSPrefetchRequested = true;
  }

  void DNSPrefetchRequestDeferred() {
    mDNSPrefetchDeferred = true;
    mDNSPrefetchRequested = false;
  }

  bool IsDNSPrefetchRequestDeferred() const { return mDNSPrefetchDeferred; }

  // This could be a virtual function or something like that, but that would
  // cause our subclasses to grow by two pointers, rather than just 1 byte at
  // most.
  nsIURI* GetURIForDNSPrefetch(Element& aOwner);

 protected:
  SupportsDNSPrefetch()
      : mInDNSPrefetch(false),
        mDNSPrefetchRequested(false),
        mDNSPrefetchDeferred(false),
        mDestroyedCalled(false) {}

  void CancelDNSPrefetch(Element&);
  void TryDNSPrefetch(Element&);

  // This MUST be called on the destructor of the Element subclass.
  // Our own destructor ensures that.
  void Destroyed(Element& aOwner) {
    MOZ_DIAGNOSTIC_ASSERT(!mDestroyedCalled,
                          "Multiple calls to SupportsDNSPrefetch::Destroyed?");
    mDestroyedCalled = true;
    if (mInDNSPrefetch) {
      HTMLDNSPrefetch::ElementDestroyed(aOwner, *this);
    }
  }

  ~SupportsDNSPrefetch() {
    MOZ_DIAGNOSTIC_ASSERT(mDestroyedCalled,
                          "Need to call SupportsDNSPrefetch::Destroyed "
                          "from the owner element");
  }

 private:
  bool mInDNSPrefetch : 1;
  bool mDNSPrefetchRequested : 1;
  bool mDNSPrefetchDeferred : 1;
  bool mDestroyedCalled : 1;
};

}  // namespace dom
}  // namespace mozilla

#endif
