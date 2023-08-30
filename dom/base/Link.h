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

#include "nsWrapperCache.h"  // For nsWrapperCache::FlagsType
#include "nsCOMPtr.h"
#include "mozilla/dom/RustTypes.h"

class nsIURI;

namespace mozilla {

class SizeOfState;

namespace dom {

class Document;
class Element;
struct BindContext;

#define MOZILLA_DOM_LINK_IMPLEMENTATION_IID          \
  {                                                  \
    0xb25edee6, 0xdd35, 0x4f8b, {                    \
      0xab, 0x90, 0x66, 0xd0, 0xbd, 0x3c, 0x22, 0xd5 \
    }                                                \
  }

class Link : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_DOM_LINK_IMPLEMENTATION_IID)

  enum class State : uint8_t {
    Unvisited = 0,
    Visited,
    NotLink,
  };

  /**
   * aElement is the element pointer corresponding to this link.
   */
  explicit Link(Element* aElement);

  /**
   * This constructor is only used for testing.
   */
  explicit Link();

  virtual void VisitedQueryFinished(bool aVisited);

  /**
   * @return the URI this link is for, if available.
   */
  nsIURI* GetURI() const;

  /**
   * Helper methods for modifying and obtaining parts of the URI of the Link.
   */
  void SetProtocol(const nsAString& aProtocol);
  void SetUsername(const nsAString& aUsername);
  void SetPassword(const nsAString& aPassword);
  void SetHost(const nsAString& aHost);
  void SetHostname(const nsAString& aHostname);
  void SetPathname(const nsAString& aPathname);
  void SetSearch(const nsAString& aSearch);
  void SetPort(const nsAString& aPort);
  void SetHash(const nsAString& aHash);
  void GetOrigin(nsAString& aOrigin);
  void GetProtocol(nsAString& _protocol);
  void GetUsername(nsAString& aUsername);
  void GetPassword(nsAString& aPassword);
  void GetHost(nsAString& _host);
  void GetHostname(nsAString& _hostname);
  void GetPathname(nsAString& _pathname);
  void GetSearch(nsAString& _search);
  void GetPort(nsAString& _port);
  void GetHash(nsAString& _hash);

  /**
   * Invalidates any link caching, and resets the state to the default.
   *
   * @param aNotify
   *        true if ResetLinkState should notify the owning document about style
   *        changes or false if it should not.
   */
  void ResetLinkState(bool aNotify, bool aHasHref);
  void ResetLinkState(bool aNotify) {
    ResetLinkState(aNotify, ElementHasHref());
  }
  void BindToTree(const BindContext&);
  void UnbindFromTree() { ResetLinkState(false); }

  // This method nevers returns a null element.
  Element* GetElement() const { return mElement; }

  virtual size_t SizeOfExcludingThis(mozilla::SizeOfState& aState) const;

  virtual bool ElementHasHref() const;

  bool HasPendingLinkUpdate() const { return mHasPendingLinkUpdate; }
  void SetHasPendingLinkUpdate() { mHasPendingLinkUpdate = true; }
  void ClearHasPendingLinkUpdate() { mHasPendingLinkUpdate = false; }
  void TriggerLinkUpdate(bool aNotify);

  // To ensure correct mHasPendingLinkUpdate handling, we have this method
  // similar to the one in Element. Overriders must call
  // ClearHasPendingLinkUpdate().
  // If you change this, change also the method in nsINode.
  virtual void NodeInfoChanged(Document* aOldDoc) = 0;

 protected:
  virtual ~Link();

  nsIURI* GetCachedURI() const { return mCachedURI; }
  bool HasCachedURI() const { return !!mCachedURI; }

 private:
  /**
   * Unregisters from History and the document so this node no longer gets
   * notifications about changes to visitedness.
   */
  void Unregister();
  void SetLinkState(State, bool aNotify);
  void SetHrefAttribute(nsIURI* aURI);

  mutable nsCOMPtr<nsIURI> mCachedURI;

  Element* const mElement;

  bool mNeedsRegistration : 1;
  bool mRegistered : 1;
  bool mHasPendingLinkUpdate : 1;
  const bool mHistory : 1;
};

NS_DEFINE_STATIC_IID_ACCESSOR(Link, MOZILLA_DOM_LINK_IMPLEMENTATION_IID)

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_Link_h__
