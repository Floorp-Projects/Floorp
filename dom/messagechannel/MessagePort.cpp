/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MessagePort.h"

#include "MessageEvent.h"
#include "MessagePortChild.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/MessageChannel.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/MessagePortChild.h"
#include "mozilla/dom/MessagePortList.h"
#include "mozilla/dom/PMessagePort.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/unused.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsPresContext.h"
#include "ScriptSettings.h"
#include "SharedMessagePortMessage.h"

#include "nsIBFCacheEntry.h"
#include "nsIDocument.h"
#include "nsIDOMFileList.h"
#include "nsIPresShell.h"
#include "nsISupportsPrimitives.h"
#include "nsServiceManagerUtils.h"

#ifdef XP_WIN
#undef PostMessage
#endif

using namespace mozilla::dom::workers;

namespace mozilla {
namespace dom {

class DispatchEventRunnable final : public nsICancelableRunnable
{
  friend class MessagePort;

public:
  NS_DECL_ISUPPORTS

  explicit DispatchEventRunnable(MessagePort* aPort)
    : mPort(aPort)
  { }

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(mPort);
    MOZ_ASSERT(mPort->mDispatchRunnable == this);
    mPort->mDispatchRunnable = nullptr;
    mPort->Dispatch();

    return NS_OK;
  }

  NS_IMETHOD
  Cancel() override
  {
    mPort = nullptr;
    return NS_OK;
  }

private:
  ~DispatchEventRunnable()
  {}

  nsRefPtr<MessagePort> mPort;
};

NS_IMPL_ISUPPORTS(DispatchEventRunnable, nsICancelableRunnable, nsIRunnable)

class PostMessageRunnable final : public nsICancelableRunnable
{
public:
  NS_DECL_ISUPPORTS

  PostMessageRunnable(MessagePort* aPort, SharedMessagePortMessage* aData)
    : mPort(aPort)
    , mData(aData)
  {
    MOZ_ASSERT(aPort);
    MOZ_ASSERT(aData);
  }

  NS_IMETHOD
  Run() override
  {
    nsCOMPtr<nsIGlobalObject> globalObject;

    if (NS_IsMainThread()) {
      globalObject = do_QueryInterface(mPort->GetParentObject());
    } else {
      WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
      MOZ_ASSERT(workerPrivate);
      globalObject = workerPrivate->GlobalScope();
    }

    AutoJSAPI jsapi;
    if (!globalObject || !jsapi.Init(globalObject)) {
      NS_WARNING("Failed to initialize AutoJSAPI object.");
      return NS_ERROR_FAILURE;
    }

    JSContext* cx = jsapi.cx();

    nsTArray<nsRefPtr<MessagePort>> ports;
    nsCOMPtr<nsPIDOMWindow> window =
      do_QueryInterface(mPort->GetParentObject());

    JS::Rooted<JS::Value> value(cx);
    if (!mData->mData.IsEmpty()) {
      bool ok = ReadStructuredCloneWithTransfer(cx, mData->mData,
                                                mData->mClosure,
                                                &value, window, ports);
      FreeStructuredClone(mData->mData, mData->mClosure);

      if (!ok) {
        return NS_ERROR_FAILURE;
      }
    }

    // The data should be already be cleaned.
    MOZ_ASSERT(!mData->mData.Length());

    // Create the event
    nsCOMPtr<mozilla::dom::EventTarget> eventTarget =
      do_QueryInterface(mPort->GetOwner());
    nsRefPtr<MessageEvent> event =
      new MessageEvent(eventTarget, nullptr, nullptr);

    event->InitMessageEvent(NS_LITERAL_STRING("message"),
                            false /* non-bubbling */,
                            false /* cancelable */, value, EmptyString(),
                            EmptyString(), nullptr);
    event->SetTrusted(true);
    event->SetSource(mPort);

    nsTArray<nsRefPtr<MessagePortBase>> array;
    array.SetCapacity(ports.Length());
    for (uint32_t i = 0; i < ports.Length(); ++i) {
      array.AppendElement(ports[i]);
    }

    nsRefPtr<MessagePortList> portList =
      new MessagePortList(static_cast<dom::Event*>(event.get()), array);
    event->SetPorts(portList);

    bool dummy;
    mPort->DispatchEvent(static_cast<dom::Event*>(event.get()), &dummy);
    return NS_OK;
  }

  NS_IMETHOD
  Cancel() override
  {
    mPort = nullptr;
    mData = nullptr;
    return NS_OK;
  }

private:
  ~PostMessageRunnable()
  {}

  nsRefPtr<MessagePort> mPort;
  nsRefPtr<SharedMessagePortMessage> mData;
};

NS_IMPL_ISUPPORTS(PostMessageRunnable, nsICancelableRunnable, nsIRunnable)

MessagePortBase::MessagePortBase(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
{
}

MessagePortBase::MessagePortBase()
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(MessagePort)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MessagePort,
                                                MessagePortBase)
  if (tmp->mDispatchRunnable) {
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mDispatchRunnable->mPort);
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessages);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessagesForTheOtherPort);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mUnshippedEntangledPort);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MessagePort,
                                                  MessagePortBase)
  if (tmp->mDispatchRunnable) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDispatchRunnable->mPort);
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mUnshippedEntangledPort);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MessagePort)
  NS_INTERFACE_MAP_ENTRY(nsIIPCBackgroundChildCreateCallback)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END_INHERITING(MessagePortBase)

NS_IMPL_ADDREF_INHERITED(MessagePort, MessagePortBase)
NS_IMPL_RELEASE_INHERITED(MessagePort, MessagePortBase)

namespace {

class MessagePortFeature final : public workers::WorkerFeature
{
  MessagePort* mPort;

public:
  explicit MessagePortFeature(MessagePort* aPort)
    : mPort(aPort)
  {
    MOZ_ASSERT(aPort);
    MOZ_COUNT_CTOR(MessagePortFeature);
  }

  virtual bool Notify(JSContext* aCx, workers::Status aStatus) override
  {
    if (mPort && aStatus > Running) {
      mPort->Close();
    }

    return true;
  }

private:
  ~MessagePortFeature()
  {
    MOZ_COUNT_DTOR(MessagePortFeature);
  }
};

class ForceCloseHelper final : public nsIIPCBackgroundChildCreateCallback
{
public:
  NS_DECL_ISUPPORTS

  static void ForceClose(const MessagePortIdentifier& aIdentifier)
  {
    PBackgroundChild* actor =
      mozilla::ipc::BackgroundChild::GetForCurrentThread();
    if (actor) {
      unused << actor->SendMessagePortForceClose(aIdentifier.uuid(),
                                                 aIdentifier.destinationUuid(),
                                                 aIdentifier.sequenceId());
      return;
    }

    nsRefPtr<ForceCloseHelper> helper = new ForceCloseHelper(aIdentifier);
    if (NS_WARN_IF(!mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread(helper))) {
      MOZ_CRASH();
    }
  }

private:
  explicit ForceCloseHelper(const MessagePortIdentifier& aIdentifier)
    : mIdentifier(aIdentifier)
  {}

  ~ForceCloseHelper() {}

  void ActorFailed() override
  {
    MOZ_CRASH("Failed to create a PBackgroundChild actor!");
  }

  void ActorCreated(mozilla::ipc::PBackgroundChild* aActor) override
  {
    ForceClose(mIdentifier);
  }

  const MessagePortIdentifier mIdentifier;
};

NS_IMPL_ISUPPORTS(ForceCloseHelper, nsIIPCBackgroundChildCreateCallback)

} // anonymous namespace

MessagePort::MessagePort(nsPIDOMWindow* aWindow)
  : MessagePortBase(aWindow)
  , mInnerID(0)
  , mMessageQueueEnabled(false)
  , mIsKeptAlive(false)
{
  mIdentifier = new MessagePortIdentifier();
  mIdentifier->neutered() = true;
  mIdentifier->sequenceId() = 0;
}

MessagePort::~MessagePort()
{
  Close();
  MOZ_ASSERT(!mWorkerFeature);
}

/* static */ already_AddRefed<MessagePort>
MessagePort::Create(nsPIDOMWindow* aWindow, const nsID& aUUID,
                    const nsID& aDestinationUUID, ErrorResult& aRv)
{
  nsRefPtr<MessagePort> mp = new MessagePort(aWindow);
  mp->Initialize(aUUID, aDestinationUUID, 1 /* 0 is an invalid sequence ID */,
                 false /* Neutered */, eStateUnshippedEntangled, aRv);
  return mp.forget();
}

/* static */ already_AddRefed<MessagePort>
MessagePort::Create(nsPIDOMWindow* aWindow,
                    const MessagePortIdentifier& aIdentifier,
                    ErrorResult& aRv)
{
  nsRefPtr<MessagePort> mp = new MessagePort(aWindow);
  mp->Initialize(aIdentifier.uuid(), aIdentifier.destinationUuid(),
                 aIdentifier.sequenceId(), aIdentifier.neutered(),
                 eStateEntangling, aRv);
  return mp.forget();
}

void
MessagePort::UnshippedEntangle(MessagePort* aEntangledPort)
{
  MOZ_ASSERT(aEntangledPort);
  MOZ_ASSERT(!mUnshippedEntangledPort);

  mUnshippedEntangledPort = aEntangledPort;
}

void
MessagePort::Initialize(const nsID& aUUID,
                        const nsID& aDestinationUUID,
                        uint32_t aSequenceID, bool mNeutered,
                        State aState, ErrorResult& aRv)
{
  MOZ_ASSERT(mIdentifier);
  mIdentifier->uuid() = aUUID;
  mIdentifier->destinationUuid() = aDestinationUUID;
  mIdentifier->sequenceId() = aSequenceID;

  mState = aState;
  mNextStep = eNextStepNone;

  if (mNeutered) {
    // If this port is neutered we don't want to keep it alive artificially nor
    // we want to add listeners or workerFeatures.
    mState = eStateDisentangled;
    return;
  }

  if (mState == eStateEntangling) {
    ConnectToPBackground();
  } else {
    MOZ_ASSERT(mState == eStateUnshippedEntangled);
  }

  // The port has to keep itself alive until it's entangled.
  UpdateMustKeepAlive();

  if (NS_IsMainThread()) {
    MOZ_ASSERT(GetOwner());
    MOZ_ASSERT(GetOwner()->IsInnerWindow());
    mInnerID = GetOwner()->WindowID();

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->AddObserver(this, "inner-window-destroyed", false);
    }
  } else {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    MOZ_ASSERT(!mWorkerFeature);

    nsAutoPtr<WorkerFeature> feature(new MessagePortFeature(this));
    JSContext* cx = workerPrivate->GetJSContext();
    if (NS_WARN_IF(!workerPrivate->AddFeature(cx, feature))) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    mWorkerFeature = Move(feature);
  }
}

JSObject*
MessagePort::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MessagePortBinding::Wrap(aCx, this, aGivenProto);
}

void
MessagePort::PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                         const Optional<Sequence<JS::Value>>& aTransferable,
                         ErrorResult& aRv)
{
  // We *must* clone the data here, or the JS::Value could be modified
  // by script

  JS::Rooted<JS::Value> transferable(aCx, JS::UndefinedValue());
  if (aTransferable.WasPassed()) {
    const Sequence<JS::Value>& realTransferable = aTransferable.Value();

    // Here we want to check if the transerable object list contains
    // this port. No other checks are done.
    for (const JS::Value& value : realTransferable) {
      if (!value.isObject()) {
        continue;
      }

      MessagePortBase* port = nullptr;
      nsresult rv = UNWRAP_OBJECT(MessagePort, &value.toObject(), port);
      if (NS_FAILED(rv)) {
        continue;
      }

      if (port == this) {
        aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
        return;
      }
    }

    // The input sequence only comes from the generated bindings code, which
    // ensures it is rooted.
    JS::HandleValueArray elements =
      JS::HandleValueArray::fromMarkedLocation(realTransferable.Length(),
                                               realTransferable.Elements());

    JSObject* array =
      JS_NewArrayObject(aCx, elements);
    if (!array) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }

    transferable.setObject(*array);
  }

  nsRefPtr<SharedMessagePortMessage> data = new SharedMessagePortMessage();

  if (!WriteStructuredCloneWithTransfer(aCx, aMessage, transferable,
                                        data->mData, data->mClosure)) {
    aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
    return;
  }

  // This message has to be ignored.
  if (mState > eStateEntangled) {
    return;
  }

  // If we are unshipped we are connected to the other port on the same thread.
  if (mState == eStateUnshippedEntangled) {
    MOZ_ASSERT(mUnshippedEntangledPort);
    mUnshippedEntangledPort->mMessages.AppendElement(data);
    mUnshippedEntangledPort->Dispatch();
    return;
  }

  // Not entangled yet, but already closed.
  if (mNextStep != eNextStepNone) {
    return;
  }

  RemoveDocFromBFCache();

  // Not entangled yet.
  if (mState == eStateEntangling) {
    mMessagesForTheOtherPort.AppendElement(data);
    return;
  }

  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mMessagesForTheOtherPort.IsEmpty());

  nsAutoTArray<nsRefPtr<SharedMessagePortMessage>, 1> array;
  array.AppendElement(data);

  nsAutoTArray<MessagePortMessage, 1> messages;
  SharedMessagePortMessage::FromSharedToMessagesChild(mActor, array, messages);
  mActor->SendPostMessages(messages);
}

void
MessagePort::Start()
{
  if (mMessageQueueEnabled) {
    return;
  }

  mMessageQueueEnabled = true;
  Dispatch();
}

void
MessagePort::Dispatch()
{
  if (!mMessageQueueEnabled || mMessages.IsEmpty() || mDispatchRunnable ||
      mState > eStateEntangled || mNextStep != eNextStepNone) {
    return;
  }

  nsRefPtr<SharedMessagePortMessage> data = mMessages.ElementAt(0);
  mMessages.RemoveElementAt(0);

  nsRefPtr<PostMessageRunnable> runnable = new PostMessageRunnable(this, data);

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToCurrentThread(runnable)));

  mDispatchRunnable = new DispatchEventRunnable(this);

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToCurrentThread(mDispatchRunnable)));
}

void
MessagePort::Close()
{
  // Not entangled yet, but already closed.
  if (mNextStep != eNextStepNone) {
    return;
  }

  if (mState == eStateUnshippedEntangled) {
    MOZ_ASSERT(mUnshippedEntangledPort);

    // This avoids loops.
    nsRefPtr<MessagePort> port = Move(mUnshippedEntangledPort);
    MOZ_ASSERT(mUnshippedEntangledPort == nullptr);

    mState = eStateDisentangled;
    port->Close();

    UpdateMustKeepAlive();
    return;
  }

  // Not entangled yet, we have to wait.
  if (mState < eStateEntangling) {
    mNextStep = eNextStepClose;
    return;
  }

  if (mState > eStateEntangled) {
    return;
  }

  // We don't care about stopping the sending of messages because from now all
  // the incoming messages will be ignored.
  mState = eStateDisentangled;

  MOZ_ASSERT(mActor);

  mActor->SendClose();
  mActor->SetPort(nullptr);
  mActor = nullptr;

  UpdateMustKeepAlive();
}

EventHandlerNonNull*
MessagePort::GetOnmessage()
{
  if (NS_IsMainThread()) {
    return GetEventHandler(nsGkAtoms::onmessage, EmptyString());
  }
  return GetEventHandler(nullptr, NS_LITERAL_STRING("message"));
}

void
MessagePort::SetOnmessage(EventHandlerNonNull* aCallback)
{
  if (NS_IsMainThread()) {
    SetEventHandler(nsGkAtoms::onmessage, EmptyString(), aCallback);
  } else {
    SetEventHandler(nullptr, NS_LITERAL_STRING("message"), aCallback);
  }

  // When using onmessage, the call to start() is implied.
  Start();
}

// This method is called when the PMessagePortChild actor is entangled to
// another actor. It receives a list of messages to be dispatch. It can be that
// we were waiting for this entangling step in order to disentangle the port or
// to close it.
void
MessagePort::Entangled(nsTArray<MessagePortMessage>& aMessages)
{
  MOZ_ASSERT(mState == eStateEntangling);

  mState = eStateEntangled;

  // If we have pending messages, these have to be sent.
  if (!mMessagesForTheOtherPort.IsEmpty()) {
    nsTArray<MessagePortMessage> messages;
    SharedMessagePortMessage::FromSharedToMessagesChild(mActor,
                                                        mMessagesForTheOtherPort,
                                                        messages);
    mMessagesForTheOtherPort.Clear();
    mActor->SendPostMessages(messages);
  }

  // We must convert the messages into SharedMessagePortMessages to avoid leaks.
  FallibleTArray<nsRefPtr<SharedMessagePortMessage>> data;
  if (NS_WARN_IF(!SharedMessagePortMessage::FromMessagesToSharedChild(aMessages,
                                                                      data))) {
    // OOM, we cannot continue.
    return;
  }

  if (mNextStep == eNextStepClose) {
    Close();
    return;
  }

  mMessages.AppendElements(data);

  // We were waiting for the entangling callback in order to disentangle this
  // port immediately after.
  if (mNextStep == eNextStepDisentangle) {
    StartDisentangling();
    return;
  }

  MOZ_ASSERT(mNextStep == eNextStepNone);
  Dispatch();
}

void
MessagePort::StartDisentangling()
{
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mState == eStateEntangled);

  mState = eStateDisentangling;
  mNextStep = eNextStepNone;

  // Sending this message we communicate to the parent actor that we don't want
  // to receive any new messages. It is possible that a message has been
  // already sent but not received yet. So we have to collect all of them and
  // we send them in the SendDispatch() request.
  mActor->SendStopSendingData();
}

void
MessagePort::MessagesReceived(nsTArray<MessagePortMessage>& aMessages)
{
  MOZ_ASSERT(mState == eStateEntangled || mState == eStateDisentangling);
  MOZ_ASSERT(mNextStep == eNextStepNone);
  MOZ_ASSERT(mMessagesForTheOtherPort.IsEmpty());

  RemoveDocFromBFCache();

  FallibleTArray<nsRefPtr<SharedMessagePortMessage>> data;
  if (!NS_WARN_IF(SharedMessagePortMessage::FromMessagesToSharedChild(aMessages,
                                                                      data))) {
    // OOM, We cannot continue.
    return;
  }

  mMessages.AppendElements(data);

  if (mState == eStateEntangled) {
    Dispatch();
  }
}

void
MessagePort::StopSendingDataConfirmed()
{
  MOZ_ASSERT(mState == eStateDisentangling);
  MOZ_ASSERT(mActor);

  Disentangle();
}

void
MessagePort::Disentangle()
{
  MOZ_ASSERT(mState == eStateDisentangling);
  MOZ_ASSERT(mActor);

  mState = eStateDisentangled;

  nsTArray<MessagePortMessage> messages;
  SharedMessagePortMessage::FromSharedToMessagesChild(mActor, mMessages,
                                                      messages);
  mMessages.Clear();
  mActor->SendDisentangle(messages);

  mActor->SetPort(nullptr);
  mActor = nullptr;

  UpdateMustKeepAlive();
}

bool
MessagePort::CloneAndDisentangle(MessagePortIdentifier& aIdentifier)
{
  MOZ_ASSERT(mIdentifier);

  // We can clone a port that has already been transfered. In this case, on the
  // otherside will have a neutered port. Here we set neutered to true so that
  // we are safe in case a early return.
  aIdentifier.neutered() = true;

  if (mState > eStateEntangled) {
    return true;
  }

  // We already have a 'next step'. We have to consider this port as already
  // cloned/closed/disentangled.
  if (mNextStep != eNextStepNone) {
    return true;
  }

  aIdentifier.uuid() = mIdentifier->uuid();
  aIdentifier.destinationUuid() = mIdentifier->destinationUuid();
  aIdentifier.sequenceId() = mIdentifier->sequenceId() + 1;
  aIdentifier.neutered() = false;

  // We have to entangle first.
  if (mState == eStateUnshippedEntangled) {
    MOZ_ASSERT(mUnshippedEntangledPort);
    MOZ_ASSERT(mMessagesForTheOtherPort.IsEmpty());

    // Disconnect the entangled port and connect it to PBackground.
    mUnshippedEntangledPort->ConnectToPBackground();
    mUnshippedEntangledPort = nullptr;

    // In this case, we don't need to be connected to the PBackground service.
    if (mMessages.IsEmpty()) {
      aIdentifier.sequenceId() = mIdentifier->sequenceId();

      mState = eStateDisentangled;
      UpdateMustKeepAlive();
      return true;
    }

    // Register this component to PBackground.
    ConnectToPBackground();

    mNextStep = eNextStepDisentangle;
    return true;
  }

  // Not entangled yet, we have to wait.
  if (mState < eStateEntangled) {
    mNextStep = eNextStepDisentangle;
    return true;
  }

  StartDisentangling();
  return true;
}

void
MessagePort::Closed()
{
  if (mState == eStateDisentangled) {
    return;
  }

  mState = eStateDisentangled;

  if (mActor) {
    mActor->SetPort(nullptr);
    mActor = nullptr;
  }

  UpdateMustKeepAlive();
}

void
MessagePort::ConnectToPBackground()
{
  mState = eStateEntangling;

  PBackgroundChild* actor =
    mozilla::ipc::BackgroundChild::GetForCurrentThread();
  if (actor) {
    ActorCreated(actor);
  } else {
    if (NS_WARN_IF(
        !mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread(this))) {
      MOZ_CRASH();
    }
  }
}

void
MessagePort::ActorFailed()
{
  MOZ_CRASH("Failed to create a PBackgroundChild actor!");
}

void
MessagePort::ActorCreated(mozilla::ipc::PBackgroundChild* aActor)
{
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(!mActor);
  MOZ_ASSERT(mIdentifier);
  MOZ_ASSERT(mState == eStateEntangling);

  PMessagePortChild* actor =
    aActor->SendPMessagePortConstructor(mIdentifier->uuid(),
                                        mIdentifier->destinationUuid(),
                                        mIdentifier->sequenceId());

  mActor = static_cast<MessagePortChild*>(actor);
  MOZ_ASSERT(mActor);

  mActor->SetPort(this);
}

void
MessagePort::UpdateMustKeepAlive()
{
  if (mState == eStateDisentangled && mIsKeptAlive) {
    mIsKeptAlive = false;

    if (mWorkerFeature) {
      WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
      MOZ_ASSERT(workerPrivate);

      workerPrivate->RemoveFeature(workerPrivate->GetJSContext(),
                                   mWorkerFeature);
      mWorkerFeature = nullptr;
    }

    if (NS_IsMainThread()) {
      nsCOMPtr<nsIObserverService> obs =
        do_GetService("@mozilla.org/observer-service;1");
      if (obs) {
        obs->RemoveObserver(this, "inner-window-destroyed");
      }
    }

    Release();
    return;
  }

  if (mState < eStateDisentangled && !mIsKeptAlive) {
    mIsKeptAlive = true;
    AddRef();
  }
}

NS_IMETHODIMP
MessagePort::Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (strcmp(aTopic, "inner-window-destroyed")) {
    return NS_OK;
  }

  // If the window id destroyed we have to release the reference that we are
  // keeping.
  if (!mIsKeptAlive) {
    return NS_OK;
  }

  nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(aSubject);
  NS_ENSURE_TRUE(wrapper, NS_ERROR_FAILURE);

  uint64_t innerID;
  nsresult rv = wrapper->GetData(&innerID);
  NS_ENSURE_SUCCESS(rv, rv);

  if (innerID == mInnerID) {
    Close();
  }

  return NS_OK;
}

void
MessagePort::RemoveDocFromBFCache()
{
  if (!NS_IsMainThread()) {
    return;
  }

  nsPIDOMWindow* window = GetOwner();
  if (!window) {
    return;
  }

  nsIDocument* doc = window->GetExtantDoc();
  if (!doc) {
    return;
  }

  nsCOMPtr<nsIBFCacheEntry> bfCacheEntry = doc->GetBFCacheEntry();
  if (!bfCacheEntry) {
    return;
  }

  bfCacheEntry->RemoveFromBFCacheSync();
}

/* static */ void
MessagePort::ForceClose(const MessagePortIdentifier& aIdentifier)
{
  ForceCloseHelper::ForceClose(aIdentifier);
}

} // namespace dom
} // namespace mozilla
