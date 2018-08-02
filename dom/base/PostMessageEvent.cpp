/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PostMessageEvent.h"

#include "MessageEvent.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileList.h"
#include "mozilla/dom/FileListBinding.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/PMessagePort.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "mozilla/dom/UnionConversions.h"
#include "mozilla/EventDispatcher.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsIPresShell.h"
#include "nsIPrincipal.h"
#include "nsIScriptError.h"
#include "nsPresContext.h"
#include "nsQueryObject.h"

namespace mozilla {
namespace dom {

PostMessageEvent::PostMessageEvent(nsGlobalWindowOuter* aSource,
                                   const nsAString& aCallerOrigin,
                                   nsGlobalWindowOuter* aTargetWindow,
                                   nsIPrincipal* aProvidedPrincipal,
                                   nsIDocument* aSourceDocument,
                                   bool aTrustedCaller)
  : Runnable("dom::PostMessageEvent")
  , StructuredCloneHolder(CloningSupported,
                          TransferringSupported,
                          StructuredCloneScope::SameProcessSameThread)
  , mSource(aSource)
  , mCallerOrigin(aCallerOrigin)
  , mTargetWindow(aTargetWindow)
  , mProvidedPrincipal(aProvidedPrincipal)
  , mSourceDocument(aSourceDocument)
  , mTrustedCaller(aTrustedCaller)
{
}

PostMessageEvent::~PostMessageEvent()
{
}

NS_IMETHODIMP
PostMessageEvent::Run()
{
  // Note: We don't init this AutoJSAPI with targetWindow, because we do not
  // want exceptions during message deserialization to trigger error events on
  // targetWindow.
  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();

  // The document is just used for the principal mismatch error message below.
  // Use a stack variable so mSourceDocument is not held onto after this method
  // finishes, regardless of the method outcome.
  nsCOMPtr<nsIDocument> sourceDocument;
  sourceDocument.swap(mSourceDocument);

  // If we bailed before this point we're going to leak mMessage, but
  // that's probably better than crashing.

  RefPtr<nsGlobalWindowInner> targetWindow;
  if (mTargetWindow->IsClosedOrClosing() ||
      !(targetWindow = mTargetWindow->GetCurrentInnerWindowInternal()) ||
      targetWindow->IsDying())
    return NS_OK;

  JSAutoRealm ar(cx, targetWindow->GetWrapper());

  // Ensure that any origin which might have been provided is the origin of this
  // window's document.  Note that we do this *now* instead of when postMessage
  // is called because the target window might have been navigated to a
  // different location between then and now.  If this check happened when
  // postMessage was called, it would be fairly easy for a malicious webpage to
  // intercept messages intended for another site by carefully timing navigation
  // of the target window so it changed location after postMessage but before
  // now.
  if (mProvidedPrincipal) {
    // Get the target's origin either from its principal or, in the case the
    // principal doesn't carry a URI (e.g. the system principal), the target's
    // document.
    nsIPrincipal* targetPrin = targetWindow->GetPrincipal();
    if (NS_WARN_IF(!targetPrin))
      return NS_OK;

    // Note: This is contrary to the spec with respect to file: URLs, which
    //       the spec groups into a single origin, but given we intentionally
    //       don't do that in other places it seems better to hold the line for
    //       now.  Long-term, we want HTML5 to address this so that we can
    //       be compliant while being safer.
    if (!targetPrin->Equals(mProvidedPrincipal)) {
      OriginAttributes sourceAttrs = mProvidedPrincipal->OriginAttributesRef();
      OriginAttributes targetAttrs = targetPrin->OriginAttributesRef();

      MOZ_DIAGNOSTIC_ASSERT(sourceAttrs.mAppId == targetAttrs.mAppId,
        "Target and source should have the same mAppId attribute.");
      MOZ_DIAGNOSTIC_ASSERT(sourceAttrs.mUserContextId == targetAttrs.mUserContextId,
        "Target and source should have the same userContextId attribute.");
      MOZ_DIAGNOSTIC_ASSERT(sourceAttrs.mInIsolatedMozBrowser == targetAttrs.mInIsolatedMozBrowser,
        "Target and source should have the same inIsolatedMozBrowser attribute.");

      if (!nsContentUtils::IsSystemOrExpandedPrincipal(targetPrin) &&
          !nsContentUtils::IsSystemOrExpandedPrincipal(mProvidedPrincipal) &&
          !mTrustedCaller) {
        MOZ_DIAGNOSTIC_ASSERT(sourceAttrs.mPrivateBrowsingId == targetAttrs.mPrivateBrowsingId,
          "Target and source should have the same mPrivateBrowsingId attribute.");
      }

      nsAutoString providedOrigin, targetOrigin;
      nsresult rv = nsContentUtils::GetUTFOrigin(targetPrin, targetOrigin);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = nsContentUtils::GetUTFOrigin(mProvidedPrincipal, providedOrigin);
      NS_ENSURE_SUCCESS(rv, rv);

      const char16_t* params[] = { providedOrigin.get(), targetOrigin.get() };

      nsContentUtils::ReportToConsole(nsIScriptError::errorFlag,
        NS_LITERAL_CSTRING("DOM Window"), sourceDocument,
        nsContentUtils::eDOM_PROPERTIES,
        "TargetPrincipalDoesNotMatch",
        params, ArrayLength(params));

      return NS_OK;
    }
  }

  IgnoredErrorResult rv;
  JS::Rooted<JS::Value> messageData(cx);
  nsCOMPtr<mozilla::dom::EventTarget> eventTarget = do_QueryObject(targetWindow);

  Read(targetWindow->AsInner(), cx, &messageData, rv);
  if (NS_WARN_IF(rv.Failed())) {
    DispatchError(cx, targetWindow, eventTarget);
    return NS_OK;
  }

  // Create the event
  RefPtr<MessageEvent> event = new MessageEvent(eventTarget, nullptr, nullptr);


  Nullable<WindowProxyOrMessagePortOrServiceWorker> source;
  source.SetValue().SetAsWindowProxy() = mSource ? mSource->AsOuter() : nullptr;

  Sequence<OwningNonNull<MessagePort>> ports;
  if (!TakeTransferredPortsAsSequence(ports)) {
    DispatchError(cx, targetWindow, eventTarget);
    return NS_OK;
  }

  event->InitMessageEvent(nullptr, NS_LITERAL_STRING("message"),
                          CanBubble::eNo, Cancelable::eNo,
                          messageData, mCallerOrigin,
                          EmptyString(), source, ports);

  Dispatch(targetWindow, event);
  return NS_OK;
}

void
PostMessageEvent::DispatchError(JSContext* aCx, nsGlobalWindowInner* aTargetWindow,
                                mozilla::dom::EventTarget* aEventTarget)
{
  RootedDictionary<MessageEventInit> init(aCx);
  init.mBubbles = false;
  init.mCancelable = false;
  init.mOrigin = mCallerOrigin;

  if (mSource) {
    init.mSource.SetValue().SetAsWindowProxy() = mSource->AsOuter();
  }

  RefPtr<Event> event =
    MessageEvent::Constructor(aEventTarget, NS_LITERAL_STRING("messageerror"),
                              init);
  Dispatch(aTargetWindow, event);
}

void
PostMessageEvent::Dispatch(nsGlobalWindowInner* aTargetWindow, Event* aEvent)
{
  // We can't simply call dispatchEvent on the window because doing so ends
  // up flipping the trusted bit on the event, and we don't want that to
  // happen because then untrusted content can call postMessage on a chrome
  // window if it can get a reference to it.

  RefPtr<nsPresContext> presContext =
    aTargetWindow->GetExtantDoc()->GetPresContext();

  aEvent->SetTrusted(mTrustedCaller);
  WidgetEvent* internalEvent = aEvent->WidgetEventPtr();

  nsEventStatus status = nsEventStatus_eIgnore;
  EventDispatcher::Dispatch(aTargetWindow->AsInner(),
                            presContext,
                            internalEvent,
                            aEvent,
                            &status);
}

} // namespace dom
} // namespace mozilla
