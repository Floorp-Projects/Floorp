/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Link.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/SVGAElement.h"
#include "mozilla/dom/HTMLDNSPrefetch.h"
#include "mozilla/IHistory.h"
#include "mozilla/StaticPrefs_layout.h"
#include "nsLayoutUtils.h"
#include "nsIURL.h"
#include "nsIURIMutator.h"
#include "nsISizeOf.h"

#include "nsEscape.h"
#include "nsGkAtoms.h"
#include "nsString.h"
#include "mozAutoDocUpdate.h"

#include "mozilla/Components.h"
#include "nsAttrValueInlines.h"
#include "HTMLLinkElement.h"

namespace mozilla::dom {

Link::Link(Element* aElement)
    : mElement(aElement),
      mState(State::NotLink),
      mNeedsRegistration(false),
      mRegistered(false),
      mHasPendingLinkUpdate(false),
      mHistory(true) {
  MOZ_ASSERT(mElement, "Must have an element");
}

Link::Link()
    : mElement(nullptr),
      mState(State::NotLink),
      mNeedsRegistration(false),
      mRegistered(false),
      mHasPendingLinkUpdate(false),
      mHistory(false) {}

Link::~Link() {
  // !mElement is for mock_Link.
  MOZ_ASSERT(!mElement || !mElement->IsInComposedDoc());
  UnregisterFromHistory();
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

void Link::VisitedQueryFinished(bool aVisited) {
  MOZ_ASSERT(mRegistered, "Setting the link state of an unregistered Link!");

  auto newState = aVisited ? State::Visited : State::Unvisited;

  // Set our current state as appropriate.
  mState = newState;

  MOZ_ASSERT(LinkState() == ElementState::VISITED ||
                 LinkState() == ElementState::UNVISITED,
             "Unexpected state obtained from LinkState()!");

  // Tell the element to update its visited state.
  mElement->UpdateState(true);

  // Even if the state didn't actually change, we need to repaint in order for
  // the visited state not to be observable.
  nsLayoutUtils::PostRestyleEvent(GetElement(), RestyleHint::RestyleSubtree(),
                                  nsChangeHint_RepaintFrame);
}

ElementState Link::LinkState() const {
  // We are a constant method, but we are just lazily doing things and have to
  // track that state.  Cast away that constness!
  //
  // XXX(emilio): that's evil.
  Link* self = const_cast<Link*>(this);

  Element* element = self->mElement;

  // If we have not yet registered for notifications and need to,
  // due to our href changing, register now!
  if (!mRegistered && mNeedsRegistration && element->IsInComposedDoc() &&
      !HasPendingLinkUpdate()) {
    // Only try and register once.
    self->mNeedsRegistration = false;

    nsCOMPtr<nsIURI> hrefURI(GetURI());

    // Assume that we are not visited until we are told otherwise.
    self->mState = State::Unvisited;

    // Make sure the href attribute has a valid link (bug 23209).
    // If we have a good href, register with History if available.
    if (mHistory && hrefURI) {
      if (nsCOMPtr<IHistory> history = components::History::Service()) {
        self->mRegistered = true;
        history->RegisterVisitedCallback(hrefURI, self);
        // And make sure we are in the document's link map.
        element->GetComposedDoc()->AddStyleRelevantLink(self);
      }
    }
  }

  // Otherwise, return our known state.
  if (mState == State::Visited) {
    return ElementState::VISITED;
  }

  if (mState == State::Unvisited) {
    return ElementState::UNVISITED;
  }

  return ElementState();
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

void Link::SetProtocol(const nsAString& aProtocol) {
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

void Link::SetPassword(const nsAString& aPassword) {
  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  nsresult rv = NS_MutateURI(uri)
                    .SetPassword(NS_ConvertUTF16toUTF8(aPassword))
                    .Finalize(uri);
  if (NS_SUCCEEDED(rv)) {
    SetHrefAttribute(uri);
  }
}

void Link::SetUsername(const nsAString& aUsername) {
  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  nsresult rv = NS_MutateURI(uri)
                    .SetUsername(NS_ConvertUTF16toUTF8(aUsername))
                    .Finalize(uri);
  if (NS_SUCCEEDED(rv)) {
    SetHrefAttribute(uri);
  }
}

void Link::SetHost(const nsAString& aHost) {
  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  nsresult rv =
      NS_MutateURI(uri).SetHostPort(NS_ConvertUTF16toUTF8(aHost)).Finalize(uri);
  if (NS_FAILED(rv)) {
    return;
  }
  SetHrefAttribute(uri);
}

void Link::SetHostname(const nsAString& aHostname) {
  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  nsresult rv =
      NS_MutateURI(uri).SetHost(NS_ConvertUTF16toUTF8(aHostname)).Finalize(uri);
  if (NS_FAILED(rv)) {
    return;
  }
  SetHrefAttribute(uri);
}

void Link::SetPathname(const nsAString& aPathname) {
  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  nsresult rv = NS_MutateURI(uri)
                    .SetFilePath(NS_ConvertUTF16toUTF8(aPathname))
                    .Finalize(uri);
  if (NS_FAILED(rv)) {
    return;
  }
  SetHrefAttribute(uri);
}

void Link::SetSearch(const nsAString& aSearch) {
  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  auto encoding = mElement->OwnerDoc()->GetDocumentCharacterSet();
  nsresult rv =
      NS_MutateURI(uri)
          .SetQueryWithEncoding(NS_ConvertUTF16toUTF8(aSearch), encoding)
          .Finalize(uri);
  if (NS_FAILED(rv)) {
    return;
  }
  SetHrefAttribute(uri);
}

void Link::SetPort(const nsAString& aPort) {
  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  nsresult rv;
  nsAutoString portStr(aPort);

  // nsIURI uses -1 as default value.
  int32_t port = -1;
  if (!aPort.IsEmpty()) {
    port = portStr.ToInteger(&rv);
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

void Link::SetHash(const nsAString& aHash) {
  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  nsresult rv =
      NS_MutateURI(uri).SetRef(NS_ConvertUTF16toUTF8(aHash)).Finalize(uri);
  if (NS_FAILED(rv)) {
    return;
  }

  SetHrefAttribute(uri);
}

void Link::GetOrigin(nsAString& aOrigin) {
  aOrigin.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    return;
  }

  nsString origin;
  nsContentUtils::GetWebExposedOriginSerialization(uri, origin);
  aOrigin.Assign(origin);
}

void Link::GetProtocol(nsAString& _protocol) {
  nsCOMPtr<nsIURI> uri(GetURI());
  if (uri) {
    nsAutoCString scheme;
    (void)uri->GetScheme(scheme);
    CopyASCIItoUTF16(scheme, _protocol);
  }
  _protocol.Append(char16_t(':'));
}

void Link::GetUsername(nsAString& aUsername) {
  aUsername.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    return;
  }

  nsAutoCString username;
  uri->GetUsername(username);
  CopyASCIItoUTF16(username, aUsername);
}

void Link::GetPassword(nsAString& aPassword) {
  aPassword.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    return;
  }

  nsAutoCString password;
  uri->GetPassword(password);
  CopyASCIItoUTF16(password, aPassword);
}

void Link::GetHost(nsAString& _host) {
  _host.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Do not throw!  Not having a valid URI should result in an empty string.
    return;
  }

  nsAutoCString hostport;
  nsresult rv = uri->GetHostPort(hostport);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(hostport, _host);
  }
}

void Link::GetHostname(nsAString& _hostname) {
  _hostname.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Do not throw!  Not having a valid URI should result in an empty string.
    return;
  }

  nsContentUtils::GetHostOrIPv6WithBrackets(uri, _hostname);
}

void Link::GetPathname(nsAString& _pathname) {
  _pathname.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Do not throw!  Not having a valid URI should result in an empty string.
    return;
  }

  nsAutoCString file;
  nsresult rv = uri->GetFilePath(file);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(file, _pathname);
  }
}

void Link::GetSearch(nsAString& _search) {
  _search.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Do not throw!  Not having a valid URI or URL should result in an empty
    // string.
    return;
  }

  nsAutoCString search;
  nsresult rv = uri->GetQuery(search);
  if (NS_SUCCEEDED(rv) && !search.IsEmpty()) {
    _search.Assign(u'?');
    AppendUTF8toUTF16(search, _search);
  }
}

void Link::GetPort(nsAString& _port) {
  _port.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Do not throw!  Not having a valid URI should result in an empty string.
    return;
  }

  int32_t port;
  nsresult rv = uri->GetPort(&port);
  // Note that failure to get the port from the URI is not necessarily a bad
  // thing.  Some URIs do not have a port.
  if (NS_SUCCEEDED(rv) && port != -1) {
    nsAutoString portStr;
    portStr.AppendInt(port, 10);
    _port.Assign(portStr);
  }
}

void Link::GetHash(nsAString& _hash) {
  _hash.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Do not throw!  Not having a valid URI should result in an empty
    // string.
    return;
  }

  nsAutoCString ref;
  nsresult rv = uri->GetRef(ref);
  if (NS_SUCCEEDED(rv) && !ref.IsEmpty()) {
    _hash.Assign(char16_t('#'));
    AppendUTF8toUTF16(ref, _hash);
  }
}

void Link::ResetLinkState(bool aNotify, bool aHasHref) {
  // If !mNeedsRegstration, then either we've never registered, or we're
  // currently registered; in either case, we should remove ourself
  // from the doc and the history.
  if (!mNeedsRegistration && mState != State::NotLink) {
    Document* doc = mElement->GetComposedDoc();
    if (doc && (mRegistered || mState == State::Visited)) {
      // Tell the document to forget about this link if we've registered
      // with it before.
      doc->ForgetLink(this);
    }
  }

  // If we have an href, we should register with the history.
  //
  // FIXME(emilio): Do we really want to allow all MathML elements to be
  // :visited? That seems not great.
  mNeedsRegistration = aHasHref;

  // If we've cached the URI, reset always invalidates it.
  UnregisterFromHistory();
  mCachedURI = nullptr;

  // Update our state back to the default; the default state for links with an
  // href is unvisited.
  mState = aHasHref ? State::Unvisited : State::NotLink;

  // We have to be very careful here: if aNotify is false we do NOT
  // want to call UpdateState, because that will call into LinkState()
  // and try to start off loads, etc.  But ResetLinkState is called
  // with aNotify false when things are in inconsistent states, so
  // we'll get confused in that situation.  Instead, just silently
  // update the link state on mElement. Since we might have set the
  // link state to unvisited, make sure to update with that state if
  // required.
  if (aNotify) {
    mElement->UpdateState(aNotify);
  } else {
    if (mState == State::Unvisited) {
      mElement->UpdateLinkState(ElementState::UNVISITED);
    } else {
      mElement->UpdateLinkState(ElementState());
    }
  }
}

void Link::UnregisterFromHistory() {
  // If we are not registered, we have nothing to do.
  if (!mRegistered) {
    return;
  }

  // And tell History to stop tracking us.
  if (mHistory && mCachedURI) {
    if (nsCOMPtr<IHistory> history = components::History::Service()) {
      history->UnregisterVisitedCallback(mCachedURI, this);
      mRegistered = false;
    }
  }
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
