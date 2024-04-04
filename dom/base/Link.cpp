/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Link.h"

#include "mozilla/dom/Element.h"
#include "mozilla/dom/BindContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/SVGAElement.h"
#include "mozilla/dom/HTMLDNSPrefetch.h"
#include "mozilla/IHistory.h"
#include "nsLayoutUtils.h"
#include "nsIURIMutator.h"
#include "nsISizeOf.h"

#include "nsGkAtoms.h"
#include "nsString.h"

#include "mozilla/Components.h"
#include "nsAttrValueInlines.h"

namespace mozilla::dom {

Link::Link(Element* aElement)
    : mElement(aElement),
      mNeedsRegistration(false),
      mRegistered(false),
      mHasPendingLinkUpdate(false),
      mHistory(true) {
  MOZ_ASSERT(mElement, "Must have an element");
}

Link::Link()
    : mElement(nullptr),
      mNeedsRegistration(false),
      mRegistered(false),
      mHasPendingLinkUpdate(false),
      mHistory(false) {}

Link::~Link() {
  // !mElement is for mock_Link.
  MOZ_ASSERT(!mElement || !mElement->IsInComposedDoc());
  Unregister();
}

bool Link::ElementHasHref() const {
  if (mElement->HasAttr(nsGkAtoms::href)) {
    return true;
  }
  if (const auto* svg = SVGAElement::FromNode(*mElement)) {
    // This can be a HasAttr(kNameSpaceID_XLink, nsGkAtoms::href) check once
    // SMIL is fixed to actually mutate DOM attributes rather than faking it.
    return svg->HasHref();
  }
  MOZ_ASSERT(!mElement->IsSVGElement(),
             "What other SVG element inherits from Link?");
  return false;
}

void Link::SetLinkState(State aState, bool aNotify) {
  Element::AutoStateChangeNotifier notifier(*mElement, aNotify);
  switch (aState) {
    case State::Visited:
      mElement->AddStatesSilently(ElementState::VISITED);
      mElement->RemoveStatesSilently(ElementState::UNVISITED);
      break;
    case State::Unvisited:
      mElement->AddStatesSilently(ElementState::UNVISITED);
      mElement->RemoveStatesSilently(ElementState::VISITED);
      break;
    case State::NotLink:
      mElement->RemoveStatesSilently(ElementState::VISITED_OR_UNVISITED);
      break;
  }
}

void Link::TriggerLinkUpdate(bool aNotify) {
  if (mRegistered || !mNeedsRegistration || mHasPendingLinkUpdate ||
      !mElement->IsInComposedDoc()) {
    return;
  }

  // Only try and register once.
  mNeedsRegistration = false;

  nsCOMPtr<nsIURI> hrefURI = GetURI();

  // Assume that we are not visited until we are told otherwise.
  SetLinkState(State::Unvisited, aNotify);

  // Make sure the href attribute has a valid link (bug 23209).
  // If we have a good href, register with History if available.
  if (mHistory && hrefURI) {
    if (nsCOMPtr<IHistory> history = components::History::Service()) {
      mRegistered = true;
      history->RegisterVisitedCallback(hrefURI, this);
      // And make sure we are in the document's link map.
      mElement->GetComposedDoc()->AddStyleRelevantLink(this);
    }
  }
}

void Link::VisitedQueryFinished(bool aVisited) {
  MOZ_ASSERT(mRegistered, "Setting the link state of an unregistered Link!");

  SetLinkState(aVisited ? State::Visited : State::Unvisited,
               /* aNotify = */ true);
  // Even if the state didn't actually change, we need to repaint in order for
  // the visited state not to be observable.
  nsLayoutUtils::PostRestyleEvent(GetElement(), RestyleHint::RestyleSubtree(),
                                  nsChangeHint_RepaintFrame);
}

nsIURI* Link::GetURI() const {
  // If we have this URI cached, use it.
  if (mCachedURI) {
    return mCachedURI;
  }

  // Otherwise obtain it.
  Link* self = const_cast<Link*>(this);
  Element* element = self->mElement;
  mCachedURI = element->GetHrefURI();

  return mCachedURI;
}

void Link::SetProtocol(const nsACString& aProtocol) {
  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }
  uri = net::TryChangeProtocol(uri, aProtocol);
  if (!uri) {
    return;
  }
  SetHrefAttribute(uri);
}

void Link::SetPassword(const nsACString& aPassword) {
  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  nsresult rv = NS_MutateURI(uri).SetPassword(aPassword).Finalize(uri);
  if (NS_SUCCEEDED(rv)) {
    SetHrefAttribute(uri);
  }
}

void Link::SetUsername(const nsACString& aUsername) {
  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  nsresult rv = NS_MutateURI(uri).SetUsername(aUsername).Finalize(uri);
  if (NS_SUCCEEDED(rv)) {
    SetHrefAttribute(uri);
  }
}

void Link::SetHost(const nsACString& aHost) {
  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  nsresult rv = NS_MutateURI(uri).SetHostPort(aHost).Finalize(uri);
  if (NS_FAILED(rv)) {
    return;
  }
  SetHrefAttribute(uri);
}

void Link::SetHostname(const nsACString& aHostname) {
  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  nsresult rv = NS_MutateURI(uri).SetHost(aHostname).Finalize(uri);
  if (NS_FAILED(rv)) {
    return;
  }
  SetHrefAttribute(uri);
}

void Link::SetPathname(const nsACString& aPathname) {
  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  nsresult rv = NS_MutateURI(uri).SetFilePath(aPathname).Finalize(uri);
  if (NS_FAILED(rv)) {
    return;
  }
  SetHrefAttribute(uri);
}

void Link::SetSearch(const nsACString& aSearch) {
  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  nsresult rv = NS_MutateURI(uri).SetQuery(aSearch).Finalize(uri);
  if (NS_FAILED(rv)) {
    return;
  }
  SetHrefAttribute(uri);
}

void Link::SetPort(const nsACString& aPort) {
  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  // nsIURI uses -1 as default value.
  nsresult rv;
  int32_t port = -1;
  if (!aPort.IsEmpty()) {
    port = aPort.ToInteger(&rv);
    if (NS_FAILED(rv)) {
      return;
    }
  }

  rv = NS_MutateURI(uri).SetPort(port).Finalize(uri);
  if (NS_FAILED(rv)) {
    return;
  }
  SetHrefAttribute(uri);
}

void Link::SetHash(const nsACString& aHash) {
  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  nsresult rv = NS_MutateURI(uri).SetRef(aHash).Finalize(uri);
  if (NS_FAILED(rv)) {
    return;
  }

  SetHrefAttribute(uri);
}

void Link::GetOrigin(nsACString& aOrigin) {
  aOrigin.Truncate();

  nsIURI* uri = GetURI();
  if (!uri) {
    return;
  }

  nsContentUtils::GetWebExposedOriginSerialization(uri, aOrigin);
}

void Link::GetProtocol(nsACString& aProtocol) {
  if (nsIURI* uri = GetURI()) {
    (void)uri->GetScheme(aProtocol);
  }
  aProtocol.Append(':');
}

void Link::GetUsername(nsACString& aUsername) {
  aUsername.Truncate();

  nsIURI* uri = GetURI();
  if (!uri) {
    return;
  }

  uri->GetUsername(aUsername);
}

void Link::GetPassword(nsACString& aPassword) {
  aPassword.Truncate();

  nsIURI* uri = GetURI();
  if (!uri) {
    return;
  }

  uri->GetPassword(aPassword);
}

void Link::GetHost(nsACString& aHost) {
  aHost.Truncate();

  nsIURI* uri = GetURI();
  if (!uri) {
    // Do not throw!  Not having a valid URI should result in an empty string.
    return;
  }

  uri->GetHostPort(aHost);
}

void Link::GetHostname(nsACString& aHostname) {
  aHostname.Truncate();

  nsIURI* uri = GetURI();
  if (!uri) {
    // Do not throw!  Not having a valid URI should result in an empty string.
    return;
  }

  nsContentUtils::GetHostOrIPv6WithBrackets(uri, aHostname);
}

void Link::GetPathname(nsACString& aPathname) {
  aPathname.Truncate();

  nsIURI* uri = GetURI();
  if (!uri) {
    // Do not throw!  Not having a valid URI should result in an empty string.
    return;
  }

  uri->GetFilePath(aPathname);
}

void Link::GetSearch(nsACString& aSearch) {
  aSearch.Truncate();

  nsIURI* uri = GetURI();
  if (!uri) {
    // Do not throw!  Not having a valid URI or URL should result in an empty
    // string.
    return;
  }

  nsresult rv = uri->GetQuery(aSearch);
  if (NS_SUCCEEDED(rv) && !aSearch.IsEmpty()) {
    aSearch.Insert('?', 0);
  }
}

void Link::GetPort(nsACString& aPort) {
  aPort.Truncate();

  nsIURI* uri = GetURI();
  if (!uri) {
    // Do not throw!  Not having a valid URI should result in an empty string.
    return;
  }

  int32_t port;
  nsresult rv = uri->GetPort(&port);
  // Note that failure to get the port from the URI is not necessarily a bad
  // thing.  Some URIs do not have a port.
  if (NS_SUCCEEDED(rv) && port != -1) {
    aPort.AppendInt(port, 10);
  }
}

void Link::GetHash(nsACString& aHash) {
  aHash.Truncate();

  nsIURI* uri = GetURI();
  if (!uri) {
    // Do not throw!  Not having a valid URI should result in an empty
    // string.
    return;
  }

  nsresult rv = uri->GetRef(aHash);
  if (NS_SUCCEEDED(rv) && !aHash.IsEmpty()) {
    aHash.Insert('#', 0);
  }
}

void Link::BindToTree(const BindContext& aContext) {
  if (aContext.InComposedDoc()) {
    aContext.OwnerDoc().RegisterPendingLinkUpdate(this);
  }
  ResetLinkState(false);
}

void Link::ResetLinkState(bool aNotify, bool aHasHref) {
  // If we have an href, we should register with the history.
  //
  // FIXME(emilio): Do we really want to allow all MathML elements to be
  // :visited? That seems not great.
  mNeedsRegistration = aHasHref;

  // If we've cached the URI, reset always invalidates it.
  Unregister();
  mCachedURI = nullptr;

  // Update our state back to the default; the default state for links with an
  // href is unvisited.
  SetLinkState(aHasHref ? State::Unvisited : State::NotLink, aNotify);
  TriggerLinkUpdate(aNotify);
}

void Link::Unregister() {
  // If we are not registered, we have nothing to do.
  if (!mRegistered) {
    return;
  }

  MOZ_ASSERT(mHistory);
  MOZ_ASSERT(mCachedURI, "Should unregister before invalidating the URI");

  // And tell History to stop tracking us.
  if (nsCOMPtr<IHistory> history = components::History::Service()) {
    history->UnregisterVisitedCallback(mCachedURI, this);
  }
  mElement->OwnerDoc()->ForgetLink(this);
  mRegistered = false;
}

void Link::SetHrefAttribute(nsIURI* aURI) {
  NS_ASSERTION(aURI, "Null URI is illegal!");

  // if we change this code to not reserialize we need to do something smarter
  // in SetProtocol because changing the protocol of an URI can change the
  // "nature" of the nsIURL/nsIURI implementation.
  nsAutoCString href;
  (void)aURI->GetSpec(href);
  (void)mElement->SetAttr(kNameSpaceID_None, nsGkAtoms::href,
                          NS_ConvertUTF8toUTF16(href), true);
}

size_t Link::SizeOfExcludingThis(mozilla::SizeOfState& aState) const {
  size_t n = 0;

  if (nsCOMPtr<nsISizeOf> iface = do_QueryInterface(mCachedURI)) {
    n += iface->SizeOfIncludingThis(aState.mMallocSizeOf);
  }

  // The following members don't need to be measured:
  // - mElement, because it is a pointer-to-self used to avoid QIs

  return n;
}

}  // namespace mozilla::dom
