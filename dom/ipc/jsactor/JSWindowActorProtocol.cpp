/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/JSActorBinding.h"
#include "mozilla/dom/JSActorService.h"
#include "mozilla/dom/JSWindowActorBinding.h"
#include "mozilla/dom/JSWindowActorChild.h"
#include "mozilla/dom/JSWindowActorProtocol.h"
#include "mozilla/dom/PContent.h"
#include "mozilla/dom/WindowGlobalChild.h"

#include "nsContentUtils.h"
#include "JSActorProtocolUtils.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(JSWindowActorProtocol)
NS_IMPL_CYCLE_COLLECTING_RELEASE(JSWindowActorProtocol)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(JSWindowActorProtocol)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(JSWindowActorProtocol, mURIMatcher)

/* static */ already_AddRefed<JSWindowActorProtocol>
JSWindowActorProtocol::FromIPC(const JSWindowActorInfo& aInfo) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsContentProcess());

  RefPtr<JSWindowActorProtocol> proto = new JSWindowActorProtocol(aInfo.name());
  JSActorProtocolUtils::FromIPCShared(proto, aInfo);

  // Content processes cannot load chrome browsing contexts, so this flag is
  // irrelevant and not propagated.
  proto->mIncludeChrome = false;
  proto->mAllFrames = aInfo.allFrames();
  proto->mMatches = aInfo.matches().Clone();
  proto->mMessageManagerGroups = aInfo.messageManagerGroups().Clone();

  proto->mChild.mEvents.SetCapacity(aInfo.events().Length());
  for (auto& ipc : aInfo.events()) {
    auto event = proto->mChild.mEvents.AppendElement();
    event->mName.Assign(ipc.name());
    event->mFlags.mCapture = ipc.capture();
    event->mFlags.mInSystemGroup = ipc.systemGroup();
    event->mFlags.mAllowUntrustedEvents = ipc.allowUntrusted();
    if (ipc.passive()) {
      event->mPassive.Construct(ipc.passive().value());
    }
    event->mCreateActor = ipc.createActor();
  }

  return proto.forget();
}

JSWindowActorInfo JSWindowActorProtocol::ToIPC() {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());

  JSWindowActorInfo info;
  JSActorProtocolUtils::ToIPCShared(info, this);

  info.allFrames() = mAllFrames;
  info.matches() = mMatches.Clone();
  info.messageManagerGroups() = mMessageManagerGroups.Clone();

  info.events().SetCapacity(mChild.mEvents.Length());
  for (auto& event : mChild.mEvents) {
    auto ipc = info.events().AppendElement();
    ipc->name().Assign(event.mName);
    ipc->capture() = event.mFlags.mCapture;
    ipc->systemGroup() = event.mFlags.mInSystemGroup;
    ipc->allowUntrusted() = event.mFlags.mAllowUntrustedEvents;
    if (event.mPassive.WasPassed()) {
      ipc->passive() = Some(event.mPassive.Value());
    }
    ipc->createActor() = event.mCreateActor;
  }

  return info;
}

already_AddRefed<JSWindowActorProtocol>
JSWindowActorProtocol::FromWebIDLOptions(const nsACString& aName,
                                         const WindowActorOptions& aOptions,
                                         ErrorResult& aRv) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());

  RefPtr<JSWindowActorProtocol> proto = new JSWindowActorProtocol(aName);
  if (!JSActorProtocolUtils::FromWebIDLOptionsShared(proto, aOptions, aRv)) {
    return nullptr;
  }

  proto->mAllFrames = aOptions.mAllFrames;
  proto->mIncludeChrome = aOptions.mIncludeChrome;

  if (aOptions.mMatches.WasPassed()) {
    MOZ_ASSERT(aOptions.mMatches.Value().Length());
    proto->mMatches = aOptions.mMatches.Value();
  }

  if (aOptions.mMessageManagerGroups.WasPassed()) {
    proto->mMessageManagerGroups = aOptions.mMessageManagerGroups.Value();
  }

  // For each event declared in the source dictionary, initialize the
  // corresponding event declaration entry in the JSWindowActorProtocol.
  if (aOptions.mChild.WasPassed() &&
      aOptions.mChild.Value().mEvents.WasPassed()) {
    auto& entries = aOptions.mChild.Value().mEvents.Value().Entries();
    proto->mChild.mEvents.SetCapacity(entries.Length());

    for (auto& entry : entries) {
      // We don't support the mOnce field, as it doesn't work well in this
      // environment. For now, throw an error in that case.
      if (entry.mValue.mOnce) {
        aRv.ThrowNotSupportedError("mOnce is not supported");
        return nullptr;
      }

      // Add the EventDecl to our list of events.
      EventDecl* evt = proto->mChild.mEvents.AppendElement();
      evt->mName = entry.mKey;
      evt->mFlags.mCapture = entry.mValue.mCapture;
      evt->mFlags.mInSystemGroup = entry.mValue.mMozSystemGroup;
      evt->mFlags.mAllowUntrustedEvents =
          entry.mValue.mWantUntrusted.WasPassed()
              ? entry.mValue.mWantUntrusted.Value()
              : false;
      if (entry.mValue.mPassive.WasPassed()) {
        evt->mPassive.Construct(entry.mValue.mPassive.Value());
      }
      evt->mCreateActor = entry.mValue.mCreateActor;
    }
  }

  return proto.forget();
}

/**
 * This listener only listens for events for the child side of the protocol.
 * This will work in both content and parent processes.
 */
NS_IMETHODIMP JSWindowActorProtocol::HandleEvent(Event* aEvent) {
  MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());

  // Determine which inner window we're associated with, and get its
  // WindowGlobalChild actor.
  EventTarget* target = aEvent->GetOriginalTarget();
  if (NS_WARN_IF(!target)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsPIDOMWindowInner> inner =
      do_QueryInterface(target->GetOwnerGlobal());
  if (NS_WARN_IF(!inner)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<WindowGlobalChild> wgc = inner->GetWindowGlobalChild();
  if (NS_WARN_IF(!wgc)) {
    return NS_ERROR_FAILURE;
  }

  if (aEvent->ShouldIgnoreChromeEventTargetListener()) {
    return NS_OK;
  }

  // Ensure our actor is present.
  RefPtr<JSActor> actor = wgc->GetExistingActor(mName);
  if (!actor) {
    // Check if we're supposed to create the actor when this event is fired.
    bool createActor = true;
    nsAutoString typeStr;
    aEvent->GetType(typeStr);
    for (auto& event : mChild.mEvents) {
      if (event.mName == typeStr) {
        createActor = event.mCreateActor;
        break;
      }
    }

    // If we're supposed to create the actor, call GetActor to cause it to be
    // created.
    if (createActor) {
      AutoJSAPI jsapi;
      jsapi.Init();
      actor = wgc->GetActor(jsapi.cx(), mName, IgnoreErrors());
    }
  }
  if (!actor || NS_WARN_IF(!actor->GetWrapperPreserveColor())) {
    return NS_OK;
  }

  // Build our event listener & call it.
  JS::Rooted<JSObject*> global(RootingCx(),
                               JS::GetNonCCWObjectGlobal(actor->GetWrapper()));
  RefPtr<EventListener> eventListener =
      new EventListener(actor->GetWrapper(), global, nullptr, nullptr);
  eventListener->HandleEvent(*aEvent, "JSWindowActorProtocol::HandleEvent");
  return NS_OK;
}

NS_IMETHODIMP JSWindowActorProtocol::Observe(nsISupports* aSubject,
                                             const char* aTopic,
                                             const char16_t* aData) {
  MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());

  nsCOMPtr<nsPIDOMWindowInner> inner = do_QueryInterface(aSubject);
  RefPtr<WindowGlobalChild> wgc;

  if (!inner) {
    nsCOMPtr<nsPIDOMWindowOuter> outer = do_QueryInterface(aSubject);
    if (NS_WARN_IF(!outer)) {
      nsContentUtils::LogSimpleConsoleError(
          NS_ConvertUTF8toUTF16(nsPrintfCString(
              "JSWindowActor %s: expected window subject for topic '%s'.",
              mName.get(), aTopic)),
          "JSActor"_ns,
          /* aFromPrivateWindow */ false,
          /* aFromChromeContext */ true);
      return NS_ERROR_FAILURE;
    }
    if (NS_WARN_IF(!outer->GetCurrentInnerWindow())) {
      return NS_ERROR_FAILURE;
    }
    wgc = outer->GetCurrentInnerWindow()->GetWindowGlobalChild();
  } else {
    wgc = inner->GetWindowGlobalChild();
  }

  if (NS_WARN_IF(!wgc)) {
    return NS_ERROR_FAILURE;
  }

  // Ensure our actor is present.
  AutoJSAPI jsapi;
  jsapi.Init();
  RefPtr<JSActor> actor = wgc->GetActor(jsapi.cx(), mName, IgnoreErrors());
  if (!actor || NS_WARN_IF(!actor->GetWrapperPreserveColor())) {
    return NS_OK;
  }

  // Build a observer callback.
  JS::Rooted<JSObject*> global(jsapi.cx(),
                               JS::GetNonCCWObjectGlobal(actor->GetWrapper()));
  RefPtr<MozObserverCallback> observerCallback =
      new MozObserverCallback(actor->GetWrapper(), global, nullptr, nullptr);
  observerCallback->Observe(aSubject, nsDependentCString(aTopic),
                            aData ? nsDependentString(aData) : VoidString());
  return NS_OK;
}

void JSWindowActorProtocol::RegisterListenersFor(EventTarget* aTarget) {
  EventListenerManager* elm = aTarget->GetOrCreateListenerManager();

  for (auto& event : mChild.mEvents) {
    elm->AddEventListenerByType(EventListenerHolder(this), event.mName,
                                event.mFlags, event.mPassive);
  }
}

void JSWindowActorProtocol::UnregisterListenersFor(EventTarget* aTarget) {
  EventListenerManager* elm = aTarget->GetOrCreateListenerManager();

  for (auto& event : mChild.mEvents) {
    elm->RemoveEventListenerByType(EventListenerHolder(this), event.mName,
                                   event.mFlags);
  }
}

void JSWindowActorProtocol::AddObservers() {
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  for (auto& topic : mChild.mObservers) {
    // This makes the observer service hold an owning reference to the
    // JSWindowActorProtocol. The JSWindowActorProtocol objects will be living
    // for the full lifetime of the content process, thus the extra strong
    // referencec doesn't have a negative impact.
    os->AddObserver(this, topic.get(), false);
  }
}

void JSWindowActorProtocol::RemoveObservers() {
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  for (auto& topic : mChild.mObservers) {
    os->RemoveObserver(this, topic.get());
  }
}

extensions::MatchPatternSet* JSWindowActorProtocol::GetURIMatcher() {
  // If we've already created the pattern set, return it.
  if (mURIMatcher || mMatches.IsEmpty()) {
    return mURIMatcher;
  }

  // Constructing the MatchPatternSet requires a JS environment to be run in.
  // We can construct it here in the JSM scope, as we will be keeping it around.
  AutoJSAPI jsapi;
  MOZ_ALWAYS_TRUE(jsapi.Init(xpc::PrivilegedJunkScope()));
  GlobalObject global(jsapi.cx(), xpc::PrivilegedJunkScope());

  nsTArray<OwningStringOrMatchPattern> patterns;
  patterns.SetCapacity(mMatches.Length());
  for (nsString& s : mMatches) {
    auto entry = patterns.AppendElement();
    entry->SetAsString() = s;
  }

  MatchPatternOptions matchPatternOptions;
  // Make MatchPattern's mSchemes create properly.
  matchPatternOptions.mRestrictSchemes = false;
  mURIMatcher = extensions::MatchPatternSet::Constructor(
      global, patterns, matchPatternOptions, IgnoreErrors());
  return mURIMatcher;
}

bool JSWindowActorProtocol::RemoteTypePrefixMatches(
    const nsDependentCSubstring& aRemoteType) {
  for (auto& remoteType : mRemoteTypes) {
    if (StringBeginsWith(aRemoteType, remoteType)) {
      return true;
    }
  }
  return false;
}

bool JSWindowActorProtocol::MessageManagerGroupMatches(
    BrowsingContext* aBrowsingContext) {
  BrowsingContext* top = aBrowsingContext->Top();
  for (auto& group : mMessageManagerGroups) {
    if (group == top->GetMessageManagerGroup()) {
      return true;
    }
  }
  return false;
}

bool JSWindowActorProtocol::Matches(BrowsingContext* aBrowsingContext,
                                    nsIURI* aURI, const nsACString& aRemoteType,
                                    ErrorResult& aRv) {
  MOZ_ASSERT(aBrowsingContext, "DocShell without a BrowsingContext!");
  MOZ_ASSERT(aURI, "Must have URI!");

  if (!mAllFrames && aBrowsingContext->GetParent()) {
    aRv.ThrowNotSupportedError(nsPrintfCString(
        "Window protocol '%s' doesn't match subframes", mName.get()));
    return false;
  }

  if (!mIncludeChrome && !aBrowsingContext->IsContent()) {
    aRv.ThrowNotSupportedError(nsPrintfCString(
        "Window protocol '%s' doesn't match chrome browsing contexts",
        mName.get()));
    return false;
  }

  if (!mRemoteTypes.IsEmpty() &&
      !RemoteTypePrefixMatches(RemoteTypePrefix(aRemoteType))) {
    aRv.ThrowNotSupportedError(
        nsPrintfCString("Window protocol '%s' doesn't match remote type '%s'",
                        mName.get(), PromiseFlatCString(aRemoteType).get()));
    return false;
  }

  if (!mMessageManagerGroups.IsEmpty() &&
      !MessageManagerGroupMatches(aBrowsingContext)) {
    aRv.ThrowNotSupportedError(nsPrintfCString(
        "Window protocol '%s' doesn't match message manager group",
        mName.get()));
    return false;
  }

  if (extensions::MatchPatternSet* uriMatcher = GetURIMatcher()) {
    if (!uriMatcher->Matches(aURI)) {
      aRv.ThrowNotSupportedError(nsPrintfCString(
          "Window protocol '%s' doesn't match uri %s", mName.get(),
          nsContentUtils::TruncatedURLForDisplay(aURI).get()));
      return false;
    }
  }

  return true;
}

}  // namespace mozilla::dom
