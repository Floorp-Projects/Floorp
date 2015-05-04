/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is the base class for all link classes.
 */

#ifndef mozilla_dom_Link_h__
#define mozilla_dom_Link_h__

#include "mozilla/IHistory.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/URLSearchParams.h"
#include "nsIContent.h" // for nsLinkState

namespace mozilla {

class EventStates;

namespace dom {

class Element;

#define MOZILLA_DOM_LINK_IMPLEMENTATION_IID               \
{ 0xb25edee6, 0xdd35, 0x4f8b,                             \
  { 0xab, 0x90, 0x66, 0xd0, 0xbd, 0x3c, 0x22, 0xd5 } }

class Link : public URLSearchParamsObserver
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_DOM_LINK_IMPLEMENTATION_IID)

  /**
   * aElement is the element pointer corresponding to this link.
   */
  explicit Link(Element* aElement);
  virtual void SetLinkState(nsLinkState aState);

  /**
   * @return NS_EVENT_STATE_VISITED if this link is visited,
   *         NS_EVENT_STATE_UNVISTED if this link is not visited, or 0 if this
   *         link is not actually a link.
   */
  EventStates LinkState() const;

  /**
   * @return the URI this link is for, if available.
   */
  nsIURI* GetURI() const;
  virtual nsIURI* GetURIExternal() const {
    return GetURI();
  }

  /**
   * Helper methods for modifying and obtaining parts of the URI of the Link.
   */
  void SetProtocol(const nsAString &aProtocol, ErrorResult& aError);
  void SetUsername(const nsAString &aUsername, ErrorResult& aError);
  void SetPassword(const nsAString &aPassword, ErrorResult& aError);
  void SetHost(const nsAString &aHost, ErrorResult& aError);
  void SetHostname(const nsAString &aHostname, ErrorResult& aError);
  void SetPathname(const nsAString &aPathname, ErrorResult& aError);
  void SetSearch(const nsAString &aSearch, ErrorResult& aError);
  void SetSearchParams(mozilla::dom::URLSearchParams& aSearchParams);
  void SetPort(const nsAString &aPort, ErrorResult& aError);
  void SetHash(const nsAString &aHash, ErrorResult& aError);
  void GetOrigin(nsAString &aOrigin, ErrorResult& aError);
  void GetProtocol(nsAString &_protocol, ErrorResult& aError);
  void GetUsername(nsAString &aUsername, ErrorResult& aError);
  void GetPassword(nsAString &aPassword, ErrorResult& aError);
  void GetHost(nsAString &_host, ErrorResult& aError);
  void GetHostname(nsAString &_hostname, ErrorResult& aError);
  void GetPathname(nsAString &_pathname, ErrorResult& aError);
  void GetSearch(nsAString &_search, ErrorResult& aError);
  URLSearchParams* SearchParams();
  void GetPort(nsAString &_port, ErrorResult& aError);
  void GetHash(nsAString &_hash, ErrorResult& aError);

  /**
   * Invalidates any link caching, and resets the state to the default.
   *
   * @param aNotify
   *        true if ResetLinkState should notify the owning document about style
   *        changes or false if it should not.
   */
  void ResetLinkState(bool aNotify, bool aHasHref);
  
  // This method nevers returns a null element.
  Element* GetElement() const { return mElement; }

  /**
   * DNS prefetch has been deferred until later, e.g. page load complete.
   */
  virtual void OnDNSPrefetchDeferred() { /*do nothing*/ }
  
  /**
   * DNS prefetch has been submitted to Host Resolver.
   */
  virtual void OnDNSPrefetchRequested() { /*do nothing*/ }

  /**
   * Checks if DNS Prefetching is ok
   * 
   * @returns boolean
   *          Defaults to true; should be overridden for specialised cases
   */
  virtual bool HasDeferredDNSPrefetchRequest() { return true; }

  virtual size_t
    SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  bool ElementHasHref() const;

  // URLSearchParamsObserver
  void URLSearchParamsUpdated(URLSearchParams* aSearchParams) override;

protected:
  virtual ~Link();

  /**
   * Return true if the link has associated URI.
   */
  bool HasURI() const
  {
    if (HasCachedURI()) {
      return true;
    }

    return !!GetURI();
  }

  nsIURI* GetCachedURI() const { return mCachedURI; }
  bool HasCachedURI() const { return !!mCachedURI; }

  void UpdateURLSearchParams();

  // CC methods
  void Unlink();
  void Traverse(nsCycleCollectionTraversalCallback &cb);

private:
  /**
   * Unregisters from History so this node no longer gets notifications about
   * changes to visitedness.
   */
  void UnregisterFromHistory();

  already_AddRefed<nsIURI> GetURIToMutate();
  void SetHrefAttribute(nsIURI *aURI);

  void CreateSearchParamsIfNeeded();

  void SetSearchInternal(const nsAString& aSearch);

  mutable nsCOMPtr<nsIURI> mCachedURI;

  Element * const mElement;

  // Strong reference to History.  The link has to unregister before History
  // can disappear.
  nsCOMPtr<IHistory> mHistory;

  uint16_t mLinkState;

  bool mNeedsRegistration;

  bool mRegistered;

protected:
  nsRefPtr<URLSearchParams> mSearchParams;
};

NS_DEFINE_STATIC_IID_ACCESSOR(Link, MOZILLA_DOM_LINK_IMPLEMENTATION_IID)

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Link_h__
