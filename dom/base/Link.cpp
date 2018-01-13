/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Link.h"

#include "mozilla/EventStates.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/Element.h"
#ifdef ANDROID
#include "mozilla/IHistory.h"
#else
#include "mozilla/places/History.h"
#endif
#include "nsIURL.h"
#include "nsISizeOf.h"
#include "nsIDocShell.h"
#include "nsIPrefetchService.h"
#include "nsCPrefetchService.h"
#include "nsStyleLinkElement.h"

#include "nsEscape.h"
#include "nsGkAtoms.h"
#include "nsHTMLDNSPrefetch.h"
#include "nsString.h"
#include "mozAutoDocUpdate.h"

#include "mozilla/Services.h"
#include "nsAttrValueInlines.h"

namespace mozilla {
namespace dom {

#ifndef ANDROID
using places::History;
#endif

Link::Link(Element *aElement)
  : mElement(aElement)
  , mLinkState(eLinkState_NotLink)
  , mNeedsRegistration(false)
  , mRegistered(false)
  , mHasPendingLinkUpdate(false)
  , mInDNSPrefetch(false)
  , mHistory(true)
{
  MOZ_ASSERT(mElement, "Must have an element");
}

Link::Link()
  : mElement(nullptr)
  , mLinkState(eLinkState_NotLink)
  , mNeedsRegistration(false)
  , mRegistered(false)
  , mHasPendingLinkUpdate(false)
  , mInDNSPrefetch(false)
  , mHistory(false)
{
}

Link::~Link()
{
  // !mElement is for mock_Link.
  MOZ_ASSERT(!mElement || !mElement->IsInComposedDoc());
  if (IsInDNSPrefetch()) {
    nsHTMLDNSPrefetch::LinkDestroyed(this);
  }
  UnregisterFromHistory();
}

bool
Link::ElementHasHref() const
{
  return mElement->HasAttr(kNameSpaceID_None, nsGkAtoms::href) ||
         (!mElement->IsHTMLElement() &&
          mElement->HasAttr(kNameSpaceID_XLink, nsGkAtoms::href));
}

void
Link::TryDNSPrefetch()
{
  MOZ_ASSERT(mElement->IsInComposedDoc());
  if (ElementHasHref() && nsHTMLDNSPrefetch::IsAllowed(mElement->OwnerDoc())) {
    nsHTMLDNSPrefetch::PrefetchLow(this);
  }
}

void
Link::CancelDNSPrefetch(nsWrapperCache::FlagsType aDeferredFlag,
                        nsWrapperCache::FlagsType aRequestedFlag)
{
  // If prefetch was deferred, clear flag and move on
  if (mElement->HasFlag(aDeferredFlag)) {
    mElement->UnsetFlags(aDeferredFlag);
    // Else if prefetch was requested, clear flag and send cancellation
  } else if (mElement->HasFlag(aRequestedFlag)) {
    mElement->UnsetFlags(aRequestedFlag);
    // Possible that hostname could have changed since binding, but since this
    // covers common cases, most DNS prefetch requests will be canceled
    nsHTMLDNSPrefetch::CancelPrefetchLow(this, NS_ERROR_ABORT);
  }
}

void
Link::GetContentPolicyMimeTypeMedia(nsAttrValue& aAsAttr,
                                    nsContentPolicyType& aPolicyType,
                                    nsString& aMimeType,
                                    nsAString& aMedia)
{
  nsAutoString as;
  mElement->GetAttr(kNameSpaceID_None, nsGkAtoms::as, as);
  Link::ParseAsValue(as, aAsAttr);
  aPolicyType = AsValueToContentPolicy(aAsAttr);

  nsAutoString type;
  mElement->GetAttr(kNameSpaceID_None, nsGkAtoms::type, type);
  nsAutoString notUsed;
  nsContentUtils::SplitMimeType(type, aMimeType, notUsed);

  mElement->GetAttr(kNameSpaceID_None, nsGkAtoms::media, aMedia);
}

void
Link::TryDNSPrefetchOrPreconnectOrPrefetchOrPreloadOrPrerender()
{
  MOZ_ASSERT(mElement->IsInComposedDoc());
  if (!ElementHasHref()) {
    return;
  }

  nsAutoString rel;
  if (!mElement->GetAttr(kNameSpaceID_None, nsGkAtoms::rel, rel)) {
    return;
  }

  if (!nsContentUtils::PrefetchPreloadEnabled(mElement->OwnerDoc()->GetDocShell())) {
    return;
  }

  uint32_t linkTypes = nsStyleLinkElement::ParseLinkTypes(rel);

  if ((linkTypes & nsStyleLinkElement::ePREFETCH) ||
      (linkTypes & nsStyleLinkElement::eNEXT) ||
      (linkTypes & nsStyleLinkElement::ePRELOAD)){
    nsCOMPtr<nsIPrefetchService> prefetchService(do_GetService(NS_PREFETCHSERVICE_CONTRACTID));
    if (prefetchService) {
      nsCOMPtr<nsIURI> uri(GetURI());
      if (uri) {
        nsCOMPtr<nsIDOMNode> domNode = GetAsDOMNode(mElement);
        if (linkTypes & nsStyleLinkElement::ePRELOAD) {
          nsAttrValue asAttr;
          nsContentPolicyType policyType;
          nsAutoString mimeType;
          nsAutoString media;
          GetContentPolicyMimeTypeMedia(asAttr, policyType, mimeType, media);

          if (policyType == nsIContentPolicy::TYPE_INVALID) {
            // Ignore preload with a wrong or empty as attribute.
            return;
          }

          if (!nsStyleLinkElement::CheckPreloadAttrs(asAttr, mimeType, media,
                                                     mElement->OwnerDoc())) {
            policyType = nsIContentPolicy::TYPE_INVALID;
          }

          prefetchService->PreloadURI(uri,
                                      mElement->OwnerDoc()->GetDocumentURI(),
                                      domNode, policyType);
        } else {
          prefetchService->PrefetchURI(uri,
                                       mElement->OwnerDoc()->GetDocumentURI(),
                                       domNode, linkTypes & nsStyleLinkElement::ePREFETCH);
        }
        return;
      }
    }
  }

  if (linkTypes & nsStyleLinkElement::ePRECONNECT) {
    nsCOMPtr<nsIURI> uri(GetURI());
    if (uri && mElement->OwnerDoc()) {
      mElement->OwnerDoc()->MaybePreconnect(uri,
        mElement->AttrValueToCORSMode(mElement->GetParsedAttr(nsGkAtoms::crossorigin)));
      return;
    }
  }

  if (linkTypes & nsStyleLinkElement::eDNS_PREFETCH) {
    if (nsHTMLDNSPrefetch::IsAllowed(mElement->OwnerDoc())) {
      nsHTMLDNSPrefetch::PrefetchLow(this);
    }
  }
}

void
Link::UpdatePreload(nsAtom* aName, const nsAttrValue* aValue,
                    const nsAttrValue* aOldValue)
{
  MOZ_ASSERT(mElement->IsInComposedDoc());

  if (!ElementHasHref()) {
     return;
  }

  nsAutoString rel;
  if (!mElement->GetAttr(kNameSpaceID_None, nsGkAtoms::rel, rel)) {
    return;
  }

  if (!nsContentUtils::PrefetchPreloadEnabled(mElement->OwnerDoc()->GetDocShell())) {
    return;
  }

  uint32_t linkTypes = nsStyleLinkElement::ParseLinkTypes(rel);

  if (!(linkTypes & nsStyleLinkElement::ePRELOAD)) {
    return;
  }

  nsCOMPtr<nsIPrefetchService> prefetchService(do_GetService(NS_PREFETCHSERVICE_CONTRACTID));
  if (!prefetchService) {
    return;
  }

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    return;
  }

  nsCOMPtr<nsIDOMNode> domNode = GetAsDOMNode(mElement);

  nsAttrValue asAttr;
  nsContentPolicyType asPolicyType;
  nsAutoString mimeType;
  nsAutoString media;
  GetContentPolicyMimeTypeMedia(asAttr, asPolicyType, mimeType, media);

  if (asPolicyType == nsIContentPolicy::TYPE_INVALID) {
    // Ignore preload with a wrong or empty as attribute, but be sure to cancel
    // the old one.
    prefetchService->CancelPrefetchPreloadURI(uri, domNode);
    return;
  }

  nsContentPolicyType policyType = asPolicyType;
  if (!nsStyleLinkElement::CheckPreloadAttrs(asAttr, mimeType, media,
                                             mElement->OwnerDoc())) {
    policyType = nsIContentPolicy::TYPE_INVALID;
  }

  if (aName == nsGkAtoms::crossorigin) {
    CORSMode corsMode = Element::AttrValueToCORSMode(aValue);
    CORSMode oldCorsMode = Element::AttrValueToCORSMode(aOldValue);
    if (corsMode != oldCorsMode) {
      prefetchService->CancelPrefetchPreloadURI(uri, domNode);
      prefetchService->PreloadURI(uri, mElement->OwnerDoc()->GetDocumentURI(),
                                  domNode, policyType);
    }
    return;
  }

  nsContentPolicyType oldPolicyType;

  if (aName == nsGkAtoms::as) {
    if (aOldValue) {
      oldPolicyType = AsValueToContentPolicy(*aOldValue);
      if (!nsStyleLinkElement::CheckPreloadAttrs(*aOldValue, mimeType, media,
                                                 mElement->OwnerDoc())) {
        oldPolicyType = nsIContentPolicy::TYPE_INVALID;
      }
    } else {
      oldPolicyType = nsIContentPolicy::TYPE_INVALID;
    }    
  } else if (aName == nsGkAtoms::type) {
    nsAutoString oldType;
    nsAutoString notUsed;
    if (aOldValue) {
      aOldValue->ToString(oldType);
    } else {
      oldType = EmptyString();
    }
    nsAutoString oldMimeType;
    nsContentUtils::SplitMimeType(oldType, oldMimeType, notUsed);
    if (nsStyleLinkElement::CheckPreloadAttrs(asAttr, oldMimeType, media,
                                              mElement->OwnerDoc())) {
      oldPolicyType = asPolicyType;
    } else {
      oldPolicyType = nsIContentPolicy::TYPE_INVALID;
    }
  } else {
    MOZ_ASSERT(aName == nsGkAtoms::media);
    nsAutoString oldMedia;
    if (aOldValue) {
      aOldValue->ToString(oldMedia);
    } else {
      oldMedia = EmptyString();
    }
    if (nsStyleLinkElement::CheckPreloadAttrs(asAttr, mimeType, oldMedia,
                                              mElement->OwnerDoc())) {
      oldPolicyType = asPolicyType;
    } else {
      oldPolicyType = nsIContentPolicy::TYPE_INVALID;
    }
  }

  if ((policyType != oldPolicyType) &&
      (oldPolicyType != nsIContentPolicy::TYPE_INVALID)) {
    prefetchService->CancelPrefetchPreloadURI(uri, domNode);

  }

  // Trigger a new preload if the policy type has changed.
  // Also trigger load if the new policy type is invalid, this will only
  // trigger an error event.
  if ((policyType != oldPolicyType) ||
      (policyType == nsIContentPolicy::TYPE_INVALID)) {
    prefetchService->PreloadURI(uri, mElement->OwnerDoc()->GetDocumentURI(),
                                domNode, policyType);
  }
}

void
Link::CancelPrefetchOrPreload()
{
  nsCOMPtr<nsIPrefetchService> prefetchService(do_GetService(NS_PREFETCHSERVICE_CONTRACTID));
  if (prefetchService) {
    nsCOMPtr<nsIURI> uri(GetURI());
    if (uri) {
      nsCOMPtr<nsIDOMNode> domNode = GetAsDOMNode(mElement);
      prefetchService->CancelPrefetchPreloadURI(uri, domNode);
    }
  }
}

void
Link::SetLinkState(nsLinkState aState)
{
  NS_ASSERTION(mRegistered,
               "Setting the link state of an unregistered Link!");
  NS_ASSERTION(mLinkState != aState,
               "Setting state to the currently set state!");

  // Set our current state as appropriate.
  mLinkState = aState;

  // Per IHistory interface documentation, we are no longer registered.
  mRegistered = false;

  MOZ_ASSERT(LinkState() == NS_EVENT_STATE_VISITED ||
             LinkState() == NS_EVENT_STATE_UNVISITED,
             "Unexpected state obtained from LinkState()!");

  // Tell the element to update its visited state
  mElement->UpdateState(true);
}

EventStates
Link::LinkState() const
{
  // We are a constant method, but we are just lazily doing things and have to
  // track that state.  Cast away that constness!
  Link *self = const_cast<Link *>(this);

  Element *element = self->mElement;

  // If we have not yet registered for notifications and need to,
  // due to our href changing, register now!
  if (!mRegistered && mNeedsRegistration && element->IsInComposedDoc() &&
      !HasPendingLinkUpdate()) {
    // Only try and register once.
    self->mNeedsRegistration = false;

    nsCOMPtr<nsIURI> hrefURI(GetURI());

    // Assume that we are not visited until we are told otherwise.
    self->mLinkState = eLinkState_Unvisited;

    // Make sure the href attribute has a valid link (bug 23209).
    // If we have a good href, register with History if available.
    if (mHistory && hrefURI) {
#ifdef ANDROID
      nsCOMPtr<IHistory> history = services::GetHistoryService();
#else
      History* history = History::GetService();
#endif
      if (history) {
        nsresult rv = history->RegisterVisitedCallback(hrefURI, self);
        if (NS_SUCCEEDED(rv)) {
          self->mRegistered = true;

          // And make sure we are in the document's link map.
          element->GetComposedDoc()->AddStyleRelevantLink(self);
        }
      }
    }
  }

  // Otherwise, return our known state.
  if (mLinkState == eLinkState_Visited) {
    return NS_EVENT_STATE_VISITED;
  }

  if (mLinkState == eLinkState_Unvisited) {
    return NS_EVENT_STATE_UNVISITED;
  }

  return EventStates();
}

nsIURI*
Link::GetURI() const
{
  // If we have this URI cached, use it.
  if (mCachedURI) {
    return mCachedURI;
  }

  // Otherwise obtain it.
  Link *self = const_cast<Link *>(this);
  Element *element = self->mElement;
  mCachedURI = element->GetHrefURI();

  return mCachedURI;
}

void
Link::SetProtocol(const nsAString &aProtocol)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  nsAString::const_iterator start, end;
  aProtocol.BeginReading(start);
  aProtocol.EndReading(end);
  nsAString::const_iterator iter(start);
  (void)FindCharInReadable(':', iter, end);
  (void)uri->SetScheme(NS_ConvertUTF16toUTF8(Substring(start, iter)));

  SetHrefAttribute(uri);
}

void
Link::SetPassword(const nsAString &aPassword)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  uri->SetPassword(NS_ConvertUTF16toUTF8(aPassword));
  SetHrefAttribute(uri);
}

void
Link::SetUsername(const nsAString &aUsername)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  uri->SetUsername(NS_ConvertUTF16toUTF8(aUsername));
  SetHrefAttribute(uri);
}

void
Link::SetHost(const nsAString &aHost)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  (void)uri->SetHostPort(NS_ConvertUTF16toUTF8(aHost));
  SetHrefAttribute(uri);
}

void
Link::SetHostname(const nsAString &aHostname)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  (void)uri->SetHost(NS_ConvertUTF16toUTF8(aHostname));
  SetHrefAttribute(uri);
}

void
Link::SetPathname(const nsAString &aPathname)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  if (!url) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  (void)url->SetFilePath(NS_ConvertUTF16toUTF8(aPathname));
  SetHrefAttribute(uri);
}

void
Link::SetSearch(const nsAString& aSearch)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  if (!url) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  auto encoding = mElement->OwnerDoc()->GetDocumentCharacterSet();
  (void)url->SetQueryWithEncoding(NS_ConvertUTF16toUTF8(aSearch), encoding);
  SetHrefAttribute(uri);
}

void
Link::SetPort(const nsAString &aPort)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
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

  (void)uri->SetPort(port);
  SetHrefAttribute(uri);
}

void
Link::SetHash(const nsAString &aHash)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  (void)uri->SetRef(NS_ConvertUTF16toUTF8(aHash));
  SetHrefAttribute(uri);
}

void
Link::GetOrigin(nsAString &aOrigin)
{
  aOrigin.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    return;
  }

  nsString origin;
  nsContentUtils::GetUTFOrigin(uri, origin);
  aOrigin.Assign(origin);
}

void
Link::GetProtocol(nsAString &_protocol)
{
  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    _protocol.AssignLiteral("http");
  }
  else {
    nsAutoCString scheme;
    (void)uri->GetScheme(scheme);
    CopyASCIItoUTF16(scheme, _protocol);
  }
  _protocol.Append(char16_t(':'));
}

void
Link::GetUsername(nsAString& aUsername)
{
  aUsername.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    return;
  }

  nsAutoCString username;
  uri->GetUsername(username);
  CopyASCIItoUTF16(username, aUsername);
}

void
Link::GetPassword(nsAString &aPassword)
{
  aPassword.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    return;
  }

  nsAutoCString password;
  uri->GetPassword(password);
  CopyASCIItoUTF16(password, aPassword);
}

void
Link::GetHost(nsAString &_host)
{
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

void
Link::GetHostname(nsAString &_hostname)
{
  _hostname.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Do not throw!  Not having a valid URI should result in an empty string.
    return;
  }

  nsContentUtils::GetHostOrIPv6WithBrackets(uri, _hostname);
}

void
Link::GetPathname(nsAString &_pathname)
{
  _pathname.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  if (!url) {
    // Do not throw!  Not having a valid URI or URL should result in an empty
    // string.
    return;
  }

  nsAutoCString file;
  nsresult rv = url->GetFilePath(file);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(file, _pathname);
  }
}

void
Link::GetSearch(nsAString &_search)
{
  _search.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  if (!url) {
    // Do not throw!  Not having a valid URI or URL should result in an empty
    // string.
    return;
  }

  nsAutoCString search;
  nsresult rv = url->GetQuery(search);
  if (NS_SUCCEEDED(rv) && !search.IsEmpty()) {
    CopyUTF8toUTF16(NS_LITERAL_CSTRING("?") + search, _search);
  }
}

void
Link::GetPort(nsAString &_port)
{
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

void
Link::GetHash(nsAString &_hash)
{
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

void
Link::ResetLinkState(bool aNotify, bool aHasHref)
{
  nsLinkState defaultState;

  // The default state for links with an href is unvisited.
  if (aHasHref) {
    defaultState = eLinkState_Unvisited;
  } else {
    defaultState = eLinkState_NotLink;
  }

  // If !mNeedsRegstration, then either we've never registered, or we're
  // currently registered; in either case, we should remove ourself
  // from the doc and the history.
  if (!mNeedsRegistration && mLinkState != eLinkState_NotLink) {
    nsIDocument *doc = mElement->GetComposedDoc();
    if (doc && (mRegistered || mLinkState == eLinkState_Visited)) {
      // Tell the document to forget about this link if we've registered
      // with it before.
      doc->ForgetLink(this);
    }
  }

  // If we have an href, we should register with the history.
  mNeedsRegistration = aHasHref;

  // If we've cached the URI, reset always invalidates it.
  UnregisterFromHistory();
  mCachedURI = nullptr;

  // Update our state back to the default.
  mLinkState = defaultState;

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
    if (mLinkState == eLinkState_Unvisited) {
      mElement->UpdateLinkState(NS_EVENT_STATE_UNVISITED);
    } else {
      mElement->UpdateLinkState(EventStates());
    }
  }
}

void
Link::UnregisterFromHistory()
{
  // If we are not registered, we have nothing to do.
  if (!mRegistered) {
    return;
  }

  // And tell History to stop tracking us.
  if (mHistory && mCachedURI) {
#ifdef ANDROID
    nsCOMPtr<IHistory> history = services::GetHistoryService();
#else
    History* history = History::GetService();
#endif
    if (history) {
      nsresult rv = history->UnregisterVisitedCallback(mCachedURI, this);
      NS_ASSERTION(NS_SUCCEEDED(rv), "This should only fail if we misuse the API!");
      if (NS_SUCCEEDED(rv)) {
        mRegistered = false;
      }
    }
  }
}

already_AddRefed<nsIURI>
Link::GetURIToMutate()
{
  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    return nullptr;
  }
  nsCOMPtr<nsIURI> clone;
  (void)uri->Clone(getter_AddRefs(clone));
  return clone.forget();
}

void
Link::SetHrefAttribute(nsIURI *aURI)
{
  NS_ASSERTION(aURI, "Null URI is illegal!");

  // if we change this code to not reserialize we need to do something smarter
  // in SetProtocol because changing the protocol of an URI can change the
  // "nature" of the nsIURL/nsIURI implementation.
  nsAutoCString href;
  (void)aURI->GetSpec(href);
  (void)mElement->SetAttr(kNameSpaceID_None, nsGkAtoms::href,
                          NS_ConvertUTF8toUTF16(href), true);
}

size_t
Link::SizeOfExcludingThis(mozilla::SizeOfState& aState) const
{
  size_t n = 0;

  if (mCachedURI) {
    nsCOMPtr<nsISizeOf> iface = do_QueryInterface(mCachedURI);
    if (iface) {
      n += iface->SizeOfIncludingThis(aState.mMallocSizeOf);
    }
  }

  // The following members don't need to be measured:
  // - mElement, because it is a pointer-to-self used to avoid QIs

  return n;
}

static const nsAttrValue::EnumTable kAsAttributeTable[] = {
  { "",              DESTINATION_INVALID       },
  { "audio",         DESTINATION_AUDIO         },
  { "font",          DESTINATION_FONT          },
  { "image",         DESTINATION_IMAGE         },
  { "script",        DESTINATION_SCRIPT        },
  { "style",         DESTINATION_STYLE         },
  { "track",         DESTINATION_TRACK         },
  { "video",         DESTINATION_VIDEO         },
  { "fetch",         DESTINATION_FETCH         },
  { nullptr,         0 }
};


/* static */ void
Link::ParseAsValue(const nsAString& aValue,
                   nsAttrValue& aResult)
{
  DebugOnly<bool> success =
  aResult.ParseEnumValue(aValue, kAsAttributeTable, false,
                         // default value is a empty string
                         // if aValue is not a value we
                         // understand
                         &kAsAttributeTable[0]);
  MOZ_ASSERT(success);
}

/* static */ nsContentPolicyType
Link::AsValueToContentPolicy(const nsAttrValue& aValue)
{
  switch(aValue.GetEnumValue()) {
  case DESTINATION_INVALID:
    return nsIContentPolicy::TYPE_INVALID;
  case DESTINATION_AUDIO:
  case DESTINATION_TRACK:
  case DESTINATION_VIDEO:
    return nsIContentPolicy::TYPE_MEDIA;
  case DESTINATION_FONT:
    return nsIContentPolicy::TYPE_FONT;
  case DESTINATION_IMAGE:
    return nsIContentPolicy::TYPE_IMAGE;
  case DESTINATION_SCRIPT:
    return nsIContentPolicy::TYPE_SCRIPT;
  case DESTINATION_STYLE:
    return nsIContentPolicy::TYPE_STYLESHEET;
  case DESTINATION_FETCH:
    return nsIContentPolicy::TYPE_OTHER;
  }
  return nsIContentPolicy::TYPE_INVALID;
}

} // namespace dom
} // namespace mozilla
