/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PostMessageEvent.h"

#include "MessageEvent.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/PMessagePort.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "mozilla/EventDispatcher.h"
#include "nsGlobalWindow.h"
#include "nsIPresShell.h"
#include "nsIPrincipal.h"
#include "nsPresContext.h"

namespace mozilla {
namespace dom {

namespace {

struct StructuredCloneInfo
{
  PostMessageEvent* event;
  bool subsumes;
  nsPIDOMWindow* window;

  // This hashtable contains the transferred ports - used to avoid duplicates.
  nsTArray<nsRefPtr<MessagePortBase>> transferredPorts;

  // This array is populated when the ports are cloned.
  nsTArray<nsRefPtr<MessagePortBase>> clonedPorts;
};

} // anonymous namespace

const JSStructuredCloneCallbacks PostMessageEvent::sPostMessageCallbacks = {
  PostMessageEvent::ReadStructuredClone,
  PostMessageEvent::WriteStructuredClone,
  nullptr,
  PostMessageEvent::ReadTransferStructuredClone,
  PostMessageEvent::TransferStructuredClone,
  PostMessageEvent::FreeTransferStructuredClone
};

/* static */ JSObject*
PostMessageEvent::ReadStructuredClone(JSContext* cx,
                                      JSStructuredCloneReader* reader,
                                      uint32_t tag,
                                      uint32_t data,
                                      void* closure)
{
  StructuredCloneInfo* scInfo = static_cast<StructuredCloneInfo*>(closure);
  NS_ASSERTION(scInfo, "Must have scInfo!");

  if (tag == SCTAG_DOM_BLOB) {
    NS_ASSERTION(!data, "Data should be empty");

    // What we get back from the reader is a BlobImpl.
    // From that we create a new File.
    BlobImpl* blobImpl;
    if (JS_ReadBytes(reader, &blobImpl, sizeof(blobImpl))) {
      MOZ_ASSERT(blobImpl);

      // nsRefPtr<File> needs to go out of scope before toObjectOrNull() is
      // called because the static analysis thinks dereferencing XPCOM objects
      // can GC (because in some cases it can!), and a return statement with a
      // JSObject* type means that JSObject* is on the stack as a raw pointer
      // while destructors are running.
      JS::Rooted<JS::Value> val(cx);
      {
        nsRefPtr<Blob> blob = Blob::Create(scInfo->window, blobImpl);
        if (!ToJSValue(cx, blob, &val)) {
          return nullptr;
        }
      }

      return &val.toObject();
    }
  }

  if (tag == SCTAG_DOM_FILELIST) {
    NS_ASSERTION(!data, "Data should be empty");

    nsISupports* supports;
    if (JS_ReadBytes(reader, &supports, sizeof(supports))) {
      JS::Rooted<JS::Value> val(cx);
      if (NS_SUCCEEDED(nsContentUtils::WrapNative(cx, supports, &val))) {
        return val.toObjectOrNull();
      }
    }
  }

  const JSStructuredCloneCallbacks* runtimeCallbacks =
    js::GetContextStructuredCloneCallbacks(cx);

  if (runtimeCallbacks) {
    return runtimeCallbacks->read(cx, reader, tag, data, nullptr);
  }

  return nullptr;
}

/* static */ bool
PostMessageEvent::WriteStructuredClone(JSContext* cx,
                                       JSStructuredCloneWriter* writer,
                                       JS::Handle<JSObject*> obj,
                                       void *closure)
{
  StructuredCloneInfo* scInfo = static_cast<StructuredCloneInfo*>(closure);
  NS_ASSERTION(scInfo, "Must have scInfo!");

  // See if this is a File/Blob object.
  {
    Blob* blob = nullptr;
    if (scInfo->subsumes && NS_SUCCEEDED(UNWRAP_OBJECT(Blob, obj, blob))) {
      BlobImpl* blobImpl = blob->Impl();
      if (JS_WriteUint32Pair(writer, SCTAG_DOM_BLOB, 0) &&
          JS_WriteBytes(writer, &blobImpl, sizeof(blobImpl))) {
        scInfo->event->StoreISupports(blobImpl);
        return true;
      }
    }
  }

  nsCOMPtr<nsIXPConnectWrappedNative> wrappedNative;
  nsContentUtils::XPConnect()->
    GetWrappedNativeOfJSObject(cx, obj, getter_AddRefs(wrappedNative));
  if (wrappedNative) {
    uint32_t scTag = 0;
    nsISupports* supports = wrappedNative->Native();

    nsCOMPtr<nsIDOMFileList> list = do_QueryInterface(supports);
    if (list && scInfo->subsumes)
      scTag = SCTAG_DOM_FILELIST;

    if (scTag)
      return JS_WriteUint32Pair(writer, scTag, 0) &&
             JS_WriteBytes(writer, &supports, sizeof(supports)) &&
             scInfo->event->StoreISupports(supports);
  }

  const JSStructuredCloneCallbacks* runtimeCallbacks =
    js::GetContextStructuredCloneCallbacks(cx);

  if (runtimeCallbacks) {
    return runtimeCallbacks->write(cx, writer, obj, nullptr);
  }

  return false;
}

/* static */ bool
PostMessageEvent::ReadTransferStructuredClone(JSContext* aCx,
                                              JSStructuredCloneReader* reader,
                                              uint32_t tag, void* aData,
                                              uint64_t aExtraData,
                                              void* aClosure,
                                              JS::MutableHandle<JSObject*> returnObject)
{
  StructuredCloneInfo* scInfo = static_cast<StructuredCloneInfo*>(aClosure);
  NS_ASSERTION(scInfo, "Must have scInfo!");

  if (tag == SCTAG_DOM_MAP_MESSAGEPORT) {
    MOZ_ASSERT(!aData);
    // aExtraData is the index of this port identifier.
    ErrorResult rv;
    nsRefPtr<MessagePort> port =
      MessagePort::Create(scInfo->window,
                          scInfo->event->GetPortIdentifier(aExtraData),
                          rv);
    if (NS_WARN_IF(rv.Failed())) {
      return false;
    }

    scInfo->clonedPorts.AppendElement(port);

    JS::Rooted<JS::Value> value(aCx);
    if (!GetOrCreateDOMReflector(aCx, port, &value)) {
      JS_ClearPendingException(aCx);
      return false;
    }

    returnObject.set(&value.toObject());
    return true;
  }

  return false;
}

/* static */ bool
PostMessageEvent::TransferStructuredClone(JSContext* aCx,
                                          JS::Handle<JSObject*> aObj,
                                          void* aClosure,
                                          uint32_t* aTag,
                                          JS::TransferableOwnership* aOwnership,
                                          void** aContent,
                                          uint64_t* aExtraData)
{
  StructuredCloneInfo* scInfo = static_cast<StructuredCloneInfo*>(aClosure);
  NS_ASSERTION(scInfo, "Must have scInfo!");

  MessagePortBase* port = nullptr;
  nsresult rv = UNWRAP_OBJECT(MessagePort, aObj, port);
  if (NS_SUCCEEDED(rv)) {
    if (scInfo->transferredPorts.Contains(port)) {
      // No duplicates.
      return false;
    }

    // We use aExtraData to store the index of this new port identifier.
    MessagePortIdentifier* identifier =
      scInfo->event->NewPortIdentifier(aExtraData);

    if (!port->CloneAndDisentangle(*identifier)) {
      return false;
    }

    scInfo->transferredPorts.AppendElement(port);

    *aTag = SCTAG_DOM_MAP_MESSAGEPORT;
    *aOwnership = JS::SCTAG_TMO_CUSTOM;
    *aContent = nullptr;

    return true;
  }

  return false;
}

/* static */ void
PostMessageEvent::FreeTransferStructuredClone(uint32_t aTag,
                                              JS::TransferableOwnership aOwnership,
                                              void *aContent,
                                              uint64_t aExtraData,
                                              void* aClosure)
{
  if (aTag == SCTAG_DOM_MAP_MESSAGEPORT) {
    MOZ_ASSERT(aClosure);
    MOZ_ASSERT(!aContent);

    StructuredCloneInfo* scInfo = static_cast<StructuredCloneInfo*>(aClosure);
    MessagePort::ForceClose(scInfo->event->GetPortIdentifier(aExtraData));
  }
}

PostMessageEvent::PostMessageEvent(nsGlobalWindow* aSource,
                                   const nsAString& aCallerOrigin,
                                   nsGlobalWindow* aTargetWindow,
                                   nsIPrincipal* aProvidedPrincipal,
                                   bool aTrustedCaller)
: mSource(aSource),
  mCallerOrigin(aCallerOrigin),
  mTargetWindow(aTargetWindow),
  mProvidedPrincipal(aProvidedPrincipal),
  mTrustedCaller(aTrustedCaller)
{
  MOZ_COUNT_CTOR(PostMessageEvent);
}

PostMessageEvent::~PostMessageEvent()
{
  MOZ_COUNT_DTOR(PostMessageEvent);
}

const MessagePortIdentifier&
PostMessageEvent::GetPortIdentifier(uint64_t aId)
{
  MOZ_ASSERT(aId < mPortIdentifiers.Length());
  return mPortIdentifiers[aId];
}

MessagePortIdentifier*
PostMessageEvent::NewPortIdentifier(uint64_t* aPosition)
{
  *aPosition = mPortIdentifiers.Length();
  return mPortIdentifiers.AppendElement();
}

NS_IMETHODIMP
PostMessageEvent::Run()
{
  MOZ_ASSERT(mTargetWindow->IsOuterWindow(),
             "should have been passed an outer window!");
  MOZ_ASSERT(!mSource || mSource->IsOuterWindow(),
             "should have been passed an outer window!");

  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();

  // If we bailed before this point we're going to leak mMessage, but
  // that's probably better than crashing.

  nsRefPtr<nsGlobalWindow> targetWindow;
  if (mTargetWindow->IsClosedOrClosing() ||
      !(targetWindow = mTargetWindow->GetCurrentInnerWindowInternal()) ||
      targetWindow->IsClosedOrClosing())
    return NS_OK;

  MOZ_ASSERT(targetWindow->IsInnerWindow(),
             "we ordered an inner window!");
  JSAutoCompartment ac(cx, targetWindow->GetWrapperPreserveColor());

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
      return NS_OK;
    }
  }

  // Deserialize the structured clone data
  JS::Rooted<JS::Value> messageData(cx);
  StructuredCloneInfo scInfo;
  scInfo.event = this;
  scInfo.window = targetWindow;

  if (!mBuffer.read(cx, &messageData, &sPostMessageCallbacks, &scInfo)) {
    return NS_ERROR_DOM_DATA_CLONE_ERR;
  }

  // Create the event
  nsCOMPtr<mozilla::dom::EventTarget> eventTarget =
    do_QueryInterface(static_cast<nsPIDOMWindow*>(targetWindow.get()));
  nsRefPtr<MessageEvent> event =
    new MessageEvent(eventTarget, nullptr, nullptr);

  event->InitMessageEvent(NS_LITERAL_STRING("message"), false /*non-bubbling */,
                          false /*cancelable */, messageData, mCallerOrigin,
                          EmptyString(), mSource);

  event->SetPorts(new MessagePortList(static_cast<dom::Event*>(event.get()),
                                      scInfo.clonedPorts));

  // We can't simply call dispatchEvent on the window because doing so ends
  // up flipping the trusted bit on the event, and we don't want that to
  // happen because then untrusted content can call postMessage on a chrome
  // window if it can get a reference to it.

  nsIPresShell *shell = targetWindow->GetExtantDoc()->GetShell();
  nsRefPtr<nsPresContext> presContext;
  if (shell)
    presContext = shell->GetPresContext();

  event->SetTrusted(mTrustedCaller);
  WidgetEvent* internalEvent = event->GetInternalNSEvent();

  nsEventStatus status = nsEventStatus_eIgnore;
  EventDispatcher::Dispatch(static_cast<nsPIDOMWindow*>(mTargetWindow),
                            presContext,
                            internalEvent,
                            static_cast<dom::Event*>(event.get()),
                            &status);
  return NS_OK;
}

bool
PostMessageEvent::Write(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                        JS::Handle<JS::Value> aTransfer, bool aSubsumes,
                        nsPIDOMWindow* aWindow)
{
  // We *must* clone the data here, or the JS::Value could be modified
  // by script
  StructuredCloneInfo scInfo;
  scInfo.event = this;
  scInfo.window = aWindow;
  scInfo.subsumes = aSubsumes;

  return mBuffer.write(aCx, aMessage, aTransfer, &sPostMessageCallbacks,
                       &scInfo);
}

} // dom namespace
} // mozilla namespace
