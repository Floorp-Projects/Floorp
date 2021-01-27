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
namespace mozilla {

class OriginAttributes;

namespace net {
class NeckoParent;
}  // namespace net

namespace dom {
class Document;
class Link;

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
  static nsresult Prefetch(Link* aElement, Priority);
  static nsresult Prefetch(
      const nsAString& host, bool isHttps,
      const OriginAttributes& aPartitionedPrincipalOriginAttributes,
      nsIRequest::TRRMode aTRRMode, Priority);
  static nsresult CancelPrefetch(
      const nsAString& host, bool isHttps,
      const OriginAttributes& aPartitionedPrincipalOriginAttributes,
      nsIRequest::TRRMode aTRRMode, Priority, nsresult aReason);
  static nsresult CancelPrefetch(Link* aElement, Priority, nsresult aReason);

  static void LinkDestroyed(Link* aLink);

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

}  // namespace dom
}  // namespace mozilla

#endif
