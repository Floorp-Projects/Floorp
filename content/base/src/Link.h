/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is the base class for all link classes.
 */

#ifndef mozilla_dom_Link_h__
#define mozilla_dom_Link_h__

#include "mozilla/IHistory.h"
#include "nsIContent.h"

namespace mozilla {
namespace dom {

class Element;

#define MOZILLA_DOM_LINK_IMPLEMENTATION_IID \
  { 0x7EA57721, 0xE373, 0x458E, \
    {0x8F, 0x44, 0xF8, 0x96, 0x56, 0xB4, 0x14, 0xF5 } }

class Link : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_DOM_LINK_IMPLEMENTATION_IID)

  /**
   * aElement is the element pointer corresponding to this link.
   */
  Link(Element* aElement);
  nsLinkState GetLinkState() const;
  virtual void SetLinkState(nsLinkState aState);

  /**
   * @return NS_EVENT_STATE_VISITED if this link is visited,
   *         NS_EVENT_STATE_UNVISTED if this link is not visited, or 0 if this
   *         link is not actually a link.
   */
  nsEventStates LinkState() const;

  /**
   * @return the URI this link is for, if available.
   */
  already_AddRefed<nsIURI> GetURI() const;
  virtual already_AddRefed<nsIURI> GetURIExternal() const {
    return GetURI();
  }

  /**
   * Helper methods for modifying and obtaining parts of the URI of the Link.
   */
  nsresult SetProtocol(const nsAString &aProtocol);
  nsresult SetHost(const nsAString &aHost);
  nsresult SetHostname(const nsAString &aHostname);
  nsresult SetPathname(const nsAString &aPathname);
  nsresult SetSearch(const nsAString &aSearch);
  nsresult SetPort(const nsAString &aPort);
  nsresult SetHash(const nsAString &aHash);
  nsresult GetProtocol(nsAString &_protocol);
  nsresult GetHost(nsAString &_host);
  nsresult GetHostname(nsAString &_hostname);
  nsresult GetPathname(nsAString &_pathname);
  nsresult GetSearch(nsAString &_search);
  nsresult GetPort(nsAString &_port);
  nsresult GetHash(nsAString &_hash);

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
    SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

  bool ElementHasHref() const;

protected:
  virtual ~Link();

  /**
   * Return true if the link has associated URI.
   */
  bool HasURI() const
  {
    if (mCachedURI)
      return true;

    nsCOMPtr<nsIURI> uri(GetURI());
    return !!uri;
  }

  nsIURI* GetCachedURI() const { return mCachedURI; }
  bool HasCachedURI() const { return !!mCachedURI; }

private:
  /**
   * Unregisters from History so this node no longer gets notifications about
   * changes to visitedness.
   */
  void UnregisterFromHistory();

  already_AddRefed<nsIURI> GetURIToMutate();
  void SetHrefAttribute(nsIURI *aURI);

  mutable nsCOMPtr<nsIURI> mCachedURI;

  Element * const mElement;

  // Strong reference to History.  The link has to unregister before History
  // can disappear.
  nsCOMPtr<IHistory> mHistory;

  uint16_t mLinkState;

  bool mNeedsRegistration;

  bool mRegistered;
};

NS_DEFINE_STATIC_IID_ACCESSOR(Link, MOZILLA_DOM_LINK_IMPLEMENTATION_IID)

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Link_h__
