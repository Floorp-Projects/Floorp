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
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/MessagePortChild.h"
#include "mozilla/dom/PMessagePort.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/dom/RefMessageBodyService.h"
#include "mozilla/dom/SharedMessageBody.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/MessagePortTimelineMarker.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/TimelineConsumers.h"
#include "mozilla/TimelineMarker.h"
#include "mozilla/Unused.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsPresContext.h"

#ifdef XP_WIN
#  undef PostMessage
#endif

namespace mozilla::dom {

void UniqueMessagePortId::ForceClose() {
  if (!mIdentifier.neutered()) {
    MessagePort::ForceClose(mIdentifier);
    mIdentifier.neutered() = true;
  }
}

class PostMessageRunnable final : public CancelableRunnable {
  friend class MessagePort;

 public:
  PostMessageRunnable(MessagePort* aPort, SharedMessageBody* aData)
      : CancelableRunnable("dom::PostMessageRunnable"),
        mPort(aPort),
        mData(aData) {
    MOZ_ASSERT(aPort);
    MOZ_ASSERT(aData);
  }

  NS_IMETHOD
  Run() override {
    NS_ASSERT_OWNINGTHREAD(Runnable);

    // The port can be cycle collected while this runnable is pending in
    // the event queue.
    if (!mPort) {
      return NS_OK;
    }

    MOZ_ASSERT(mPort->mPostMessageRunnable == this);

    DispatchMessage();

    // We must check if we were waiting for this message in order to shutdown
    // the port.
    mPort->UpdateMustKeepAlive();

    mPort->mPostMessageRunnable = nullptr;
    mPort->Dispatch();

    return NS_OK;
  }

  nsresult Cancel() override {
    NS_ASSERT_OWNINGTHREAD(Runnable);

    mPort = nullptr;
    mData = nullptr;
    return NS_OK;
  }

 private:
  void DispatchMessage() const {
    NS_ASSERT_OWNINGTHREAD(Runnable);

    if (NS_FAILED(mPort->CheckCurrentGlobalCorrectness())) {
      return;
    }

    nsCOMPtr<nsIGlobalObject> globalObject = mPort->GetParentObject();

    AutoJSAPI jsapi;
    if (!globalObject || !jsapi.Init(globalObject)) {
      NS_WARNING("Failed to initialize AutoJSAPI object.");
      return;
    }

    JSContext* cx = jsapi.cx();

    IgnoredErrorResult rv;
    JS::Rooted<JS::Value> value(cx);

    UniquePtr<AbstractTimelineMarker> start;
    UniquePtr<AbstractTimelineMarker> end;
    RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
    bool isTimelineRecording = timelines && !timelines->IsEmpty();

    if (isTimelineRecording) {
      start = MakeUnique<MessagePortTimelineMarker>(
          ProfileTimelineMessagePortOperationType::DeserializeData,
          MarkerTracingType::START);
    }

    mData->Read(cx, &value, mPort->mRefMessageBodyService,
                SharedMessageBody::ReadMethod::StealRefMessageBody, rv);

    if (isTimelineRecording) {
      end = MakeUnique<MessagePortTimelineMarker>(
          ProfileTimelineMessagePortOperationType::DeserializeData,
          MarkerTracingType::END);
      timelines->AddMarkerForAllObservedDocShells(start);
      timelines->AddMarkerForAllObservedDocShells(end);
    }

    if (NS_WARN_IF(rv.Failed())) {
      JS_ClearPendingException(cx);
      mPort->DispatchError();
      return;
    }

    // Create the event
    nsCOMPtr<mozilla::dom::EventTarget> eventTarget =
        do_QueryInterface(mPort->GetOwner());
    RefPtr<MessageEvent> event =
        new MessageEvent(eventTarget, nullptr, nullptr);

    Sequence<OwningNonNull<MessagePort>> ports;
    if (!mData->TakeTransferredPortsAsSequence(ports)) {
      mPort->DispatchError();
      return;
    }

    event->InitMessageEvent(nullptr, u"message"_ns, CanBubble::eNo,
                            Cancelable::eNo, value, u""_ns, u""_ns, nullptr,
                            ports);
    event->SetTrusted(true);

    mPort->DispatchEvent(*event);
  }

 private:
  ~PostMessageRunnable() = default;

  RefPtr<MessagePort> mPort;
  RefPtr<SharedMessageBody> mData;
};

NS_IMPL_CYCLE_COLLECTION_CLASS(MessagePort)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MessagePort,
                                                DOMEventTargetHelper)
  if (tmp->mPostMessageRunnable) {
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mPostMessageRunnable->mPort);
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessagesForTheOtherPort);

  tmp->CloseForced();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MessagePort,
                                                  DOMEventTargetHelper)
  if (tmp->mPostMessageRunnable) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPostMessageRunnable->mPort);
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mUnshippedEntangledPort);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MessagePort)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MessagePort, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MessagePort, DOMEventTargetHelper)

MessagePort::MessagePort(nsIGlobalObject* aGlobal, State aState)
    : DOMEventTargetHelper(aGlobal),
      mRefMessageBodyService(RefMessageBodyService::GetOrCreate()),
      mState(aState),
      mMessageQueueEnabled(false),
      mIsKeptAlive(false),
      mHasBeenTransferredOrClosed(false) {
  MOZ_ASSERT(aGlobal);

  mIdentifier = MakeUnique<MessagePortIdentifier>();
  mIdentifier->neutered() = true;
  mIdentifier->sequenceId() = 0;
}

MessagePort::~MessagePort() {
  CloseForced();
  MOZ_ASSERT(!mWorkerRef);
}

/* static */
already_AddRefed<MessagePort> MessagePort::Create(nsIGlobalObject* aGlobal,
                                                  const nsID& aUUID,
                                                  const nsID& aDestinationUUID,
                                                  ErrorResult& aRv) {
  MOZ_ASSERT(aGlobal);

  RefPtr<MessagePort> mp = new MessagePort(aGlobal, eStateUnshippedEntangled);
  mp->Initialize(aUUID, aDestinationUUID, 1 /* 0 is an invalid sequence ID */,
                 false /* Neutered */, aRv);
  return mp.forget();
}

/* static */
already_AddRefed<MessagePort> MessagePort::Create(
    nsIGlobalObject* aGlobal, UniqueMessagePortId& aIdentifier,
    ErrorResult& aRv) {
  MOZ_ASSERT(aGlobal);

  RefPtr<MessagePort> mp = new MessagePort(aGlobal, eStateEntangling);
  mp->Initialize(aIdentifier.uuid(), aIdentifier.destinationUuid(),
                 aIdentifier.sequenceId(), aIdentifier.neutered(), aRv);
  aIdentifier.neutered() = true;
  return mp.forget();
}

void MessagePort::UnshippedEntangle(MessagePort* aEntangledPort) {
  MOZ_DIAGNOSTIC_ASSERT(aEntangledPort);
  MOZ_DIAGNOSTIC_ASSERT(!mUnshippedEntangledPort);

  mUnshippedEntangledPort = aEntangledPort;
}

void MessagePort::Initialize(const nsID& aUUID, const nsID& aDestinationUUID,
                             uint32_t aSequenceID, bool aNeutered,
                             ErrorResult& aRv) {
  MOZ_ASSERT(mIdentifier);
  mIdentifier->uuid() = aUUID;
  mIdentifier->destinationUuid() = aDestinationUUID;
  mIdentifier->sequenceId() = aSequenceID;

  if (aNeutered) {
    // If this port is neutered we don't want to keep it alive artificially nor
    // we want to add listeners or WorkerRefs.
    mState = eStateDisentangled;
    return;
  }

  if (mState == eStateEntangling) {
    if (!ConnectToPBackground()) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
  } else {
    MOZ_ASSERT(mState == eStateUnshippedEntangled);
  }

  // The port has to keep itself alive until it's entangled.
  UpdateMustKeepAlive();

  if (WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate()) {
    RefPtr<MessagePort> self = this;

    // When the callback is executed, we cannot process messages anymore because
    // we cannot dispatch new runnables. Let's force a Close().
    RefPtr<StrongWorkerRef> strongWorkerRef = StrongWorkerRef::Create(
        workerPrivate, "MessagePort", [self]() { self->CloseForced(); });
    if (NS_WARN_IF(!strongWorkerRef)) {
      // The worker is shutting down.
      mState = eStateDisentangled;
      UpdateMustKeepAlive();
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    MOZ_ASSERT(!mWorkerRef);
    mWorkerRef = std::move(strongWorkerRef);
  }
}

JSObject* MessagePort::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return MessagePort_Binding::Wrap(aCx, this, aGivenProto);
}

void MessagePort::PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                              const Sequence<JSObject*>& aTransferable,
                              ErrorResult& aRv) {
  // We *must* clone the data here, or the JS::Value could be modified
  // by script

  // Here we want to check if the transerable object list contains
  // this port.
  for (uint32_t i = 0; i < aTransferable.Length(); ++i) {
    JS::Rooted<JSObject*> object(aCx, aTransferable[i]);
    if (!object) {
      continue;
    }

    MessagePort* port = nullptr;
    nsresult rv = UNWRAP_OBJECT(MessagePort, &object, port);
    if (NS_SUCCEEDED(rv) && port == this) {
      aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
      return;
    }
  }

  JS::Rooted<JS::Value> transferable(aCx, JS::UndefinedValue());

  aRv = nsContentUtils::CreateJSValueFromSequenceOfObject(aCx, aTransferable,
                                                          &transferable);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  Maybe<nsID> agentClusterId;
  nsCOMPtr<nsIGlobalObject> global = GetOwnerGlobal();
  if (global) {
    agentClusterId = global->GetAgentClusterId();
  }

  RefPtr<SharedMessageBody> data = new SharedMessageBody(
      StructuredCloneHolder::TransferringSupported, agentClusterId);

  UniquePtr<AbstractTimelineMarker> start;
  UniquePtr<AbstractTimelineMarker> end;
  RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
  bool isTimelineRecording = timelines && !timelines->IsEmpty();

  if (isTimelineRecording) {
    start = MakeUnique<MessagePortTimelineMarker>(
        ProfileTimelineMessagePortOperationType::SerializeData,
        MarkerTracingType::START);
  }

  data->Write(aCx, aMessage, transferable, mIdentifier->uuid(),
              mRefMessageBodyService, aRv);

  if (isTimelineRecording) {
    end = MakeUnique<MessagePortTimelineMarker>(
        ProfileTimelineMessagePortOperationType::SerializeData,
        MarkerTracingType::END);
    timelines->AddMarkerForAllObservedDocShells(start);
    timelines->AddMarkerForAllObservedDocShells(end);
  }

  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  // This message has to be ignored.
  if (mState > eStateEntangled) {
    return;
  }

  // If we are unshipped we are connected to the other port on the same thread.
  if (mState == eStateUnshippedEntangled) {
    MOZ_DIAGNOSTIC_ASSERT(mUnshippedEntangledPort);
    mUnshippedEntangledPort->mMessages.AppendElement(data);
    mUnshippedEntangledPort->Dispatch();
    return;
  }

  // Not entangled yet, but already closed/disentangled.
  if (mState == eStateEntanglingForDisentangle ||
      mState == eStateEntanglingForClose) {
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

  AutoTArray<RefPtr<SharedMessageBody>, 1> array;
  array.AppendElement(data);

  AutoTArray<MessageData, 1> messages;
  // note: `messages` will borrow the underlying buffer, but this is okay
  // because reverse destruction order means `messages` will be destroyed prior
  // to `array`/`data`.
  SharedMessageBody::FromSharedToMessagesChild(mActor->Manager(), array,
                                               messages);
  mActor->SendPostMessages(messages);
}

void MessagePort::PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                              const StructuredSerializeOptions& aOptions,
                              ErrorResult& aRv) {
  PostMessage(aCx, aMessage, aOptions.mTransfer, aRv);
}

void MessagePort::Start() {
  if (mMessageQueueEnabled) {
    return;
  }

  mMessageQueueEnabled = true;
  Dispatch();
}

void MessagePort::Dispatch() {
  if (!mMessageQueueEnabled || mMessages.IsEmpty() || mPostMessageRunnable) {
    return;
  }

  switch (mState) {
    case eStateUnshippedEntangled:
      // Everything is fine here. We have messages because the other
      // port populates our queue directly.
      break;

    case eStateEntangling:
      // Everything is fine here as well. We have messages because the other
      // port populated our queue directly when we were in the
      // eStateUnshippedEntangled state.
      break;

    case eStateEntanglingForDisentangle:
      // Here we don't want to ship messages because these messages must be
      // delivered by the cloned version of this one. They will be sent in the
      // SendDisentangle().
      return;

    case eStateEntanglingForClose:
      // We still want to deliver messages if we are closing. These messages
      // are here from the previous eStateUnshippedEntangled state.
      break;

    case eStateEntangled:
      // This port is up and running.
      break;

    case eStateDisentangling:
      // If we are in the process to disentangle the port, we cannot dispatch
      // messages. They will be sent to the cloned version of this port via
      // SendDisentangle();
      return;

    case eStateDisentangled:
      MOZ_CRASH("This cannot happen.");
      // It cannot happen because Disentangle should take off all the pending
      // messages.
      break;

    case eStateDisentangledForClose:
      // If we are here is because the port has been closed. We can still
      // process the pending messages.
      break;
  }

  RefPtr<SharedMessageBody> data = mMessages.ElementAt(0);
  mMessages.RemoveElementAt(0);

  mPostMessageRunnable = new PostMessageRunnable(this, data);

  nsCOMPtr<nsIGlobalObject> global = GetOwnerGlobal();
  if (NS_IsMainThread() && global) {
    MOZ_ALWAYS_SUCCEEDS(
        global->Dispatch(TaskCategory::Other, do_AddRef(mPostMessageRunnable)));
    return;
  }

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(mPostMessageRunnable));
}

void MessagePort::Close() {
  mHasBeenTransferredOrClosed = true;
  CloseInternal(true /* aSoftly */);
}

void MessagePort::CloseForced() { CloseInternal(false /* aSoftly */); }

void MessagePort::CloseInternal(bool aSoftly) {
  // If we have some messages to send but we don't want a 'soft' close, we have
  // to flush them now.
  if (!aSoftly) {
    mMessages.Clear();
  }

  // Let's inform the RefMessageBodyService that any our shared messages are
  // now invalid.
  mRefMessageBodyService->ForgetPort(mIdentifier->uuid());

  if (mState == eStateUnshippedEntangled) {
    MOZ_DIAGNOSTIC_ASSERT(mUnshippedEntangledPort);

    // This avoids loops.
    RefPtr<MessagePort> port = std::move(mUnshippedEntangledPort);

    mState = eStateDisentangledForClose;
    port->CloseInternal(aSoftly);

    UpdateMustKeepAlive();
    return;
  }

  // Not entangled yet, we have to wait.
  if (mState == eStateEntangling) {
    mState = eStateEntanglingForClose;
    return;
  }

  // Not entangled but already cloned or closed
  if (mState == eStateEntanglingForDisentangle ||
      mState == eStateEntanglingForClose) {
    return;
  }

  // Maybe we were already closing the port but softly. In this case we call
  // UpdateMustKeepAlive() to consider the empty pending message queue.
  if (mState == eStateDisentangledForClose && !aSoftly) {
    UpdateMustKeepAlive();
    return;
  }

  if (mState > eStateEntangled) {
    return;
  }

  // We don't care about stopping the sending of messages because from now all
  // the incoming messages will be ignored.
  mState = eStateDisentangledForClose;

  MOZ_ASSERT(mActor);

  mActor->SendClose();
  mActor->SetPort(nullptr);
  mActor = nullptr;

  UpdateMustKeepAlive();
}

EventHandlerNonNull* MessagePort::GetOnmessage() {
  return GetEventHandler(nsGkAtoms::onmessage);
}

void MessagePort::SetOnmessage(EventHandlerNonNull* aCallback) {
  SetEventHandler(nsGkAtoms::onmessage, aCallback);

  // When using onmessage, the call to start() is implied.
  Start();
}

// This method is called when the PMessagePortChild actor is entangled to
// another actor. It receives a list of messages to be dispatch. It can be that
// we were waiting for this entangling step in order to disentangle the port or
// to close it.
void MessagePort::Entangled(nsTArray<MessageData>& aMessages) {
  MOZ_ASSERT(mState == eStateEntangling ||
             mState == eStateEntanglingForDisentangle ||
             mState == eStateEntanglingForClose);

  State oldState = mState;
  mState = eStateEntangled;

  // If we have pending messages, these have to be sent.
  if (!mMessagesForTheOtherPort.IsEmpty()) {
    {
      nsTArray<MessageData> messages;
      SharedMessageBody::FromSharedToMessagesChild(
          mActor->Manager(), mMessagesForTheOtherPort, messages);
      mActor->SendPostMessages(messages);
    }
    // Because `messages` borrow the underlying JSStructuredCloneData buffers,
    // only clear after `messages` have gone out of scope.
    mMessagesForTheOtherPort.Clear();
  }

  // We must convert the messages into SharedMessageBodys to avoid leaks.
  FallibleTArray<RefPtr<SharedMessageBody>> data;
  if (NS_WARN_IF(
          !SharedMessageBody::FromMessagesToSharedChild(aMessages, data))) {
    DispatchError();
    return;
  }

  // If the next step is to close the port, we do it ignoring the received
  // messages.
  if (oldState == eStateEntanglingForClose) {
    CloseForced();
    return;
  }

  mMessages.AppendElements(data);

  // We were waiting for the entangling callback in order to disentangle this
  // port immediately after.
  if (oldState == eStateEntanglingForDisentangle) {
    StartDisentangling();
    return;
  }

  Dispatch();
}

void MessagePort::StartDisentangling() {
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mState == eStateEntangled);

  mState = eStateDisentangling;

  // Sending this message we communicate to the parent actor that we don't want
  // to receive any new messages. It is possible that a message has been
  // already sent but not received yet. So we have to collect all of them and
  // we send them in the SendDispatch() request.
  mActor->SendStopSendingData();
}

void MessagePort::MessagesReceived(nsTArray<MessageData>& aMessages) {
  MOZ_ASSERT(mState == eStateEntangled || mState == eStateDisentangling ||
             // This last step can happen only if Close() has been called
             // manually. At this point SendClose() is sent but we can still
             // receive something until the Closing request is processed.
             mState == eStateDisentangledForClose);
  MOZ_ASSERT(mMessagesForTheOtherPort.IsEmpty());

  RemoveDocFromBFCache();

  FallibleTArray<RefPtr<SharedMessageBody>> data;
  if (NS_WARN_IF(
          !SharedMessageBody::FromMessagesToSharedChild(aMessages, data))) {
    DispatchError();
    return;
  }

  mMessages.AppendElements(data);

  if (mState == eStateEntangled) {
    Dispatch();
  }
}

void MessagePort::StopSendingDataConfirmed() {
  MOZ_ASSERT(mState == eStateDisentangling);
  MOZ_ASSERT(mActor);

  Disentangle();
}

void MessagePort::Disentangle() {
  MOZ_ASSERT(mState == eStateDisentangling);
  MOZ_ASSERT(mActor);

  mState = eStateDisentangled;

  {
    nsTArray<MessageData> messages;
    SharedMessageBody::FromSharedToMessagesChild(mActor->Manager(), mMessages,
                                                 messages);
    mActor->SendDisentangle(messages);
  }

  // Let's inform the RefMessageBodyService that any our shared messages are
  // now invalid.
  mRefMessageBodyService->ForgetPort(mIdentifier->uuid());

  // Only clear mMessages after the MessageData instances have gone out of scope
  // because they borrow mMessages' underlying JSStructuredCloneDatas.
  mMessages.Clear();

  mActor->SetPort(nullptr);
  mActor = nullptr;

  UpdateMustKeepAlive();
}

void MessagePort::CloneAndDisentangle(UniqueMessagePortId& aIdentifier) {
  MOZ_ASSERT(mIdentifier);
  MOZ_ASSERT(!mHasBeenTransferredOrClosed);

  mHasBeenTransferredOrClosed = true;

  // We can clone a port that has already been transfered. In this case, on the
  // otherside will have a neutered port. Here we set neutered to true so that
  // we are safe in case a early return.
  aIdentifier.neutered() = true;

  if (mState > eStateEntangled) {
    return;
  }

  // We already have a 'next step'. We have to consider this port as already
  // cloned/closed/disentangled.
  if (mState == eStateEntanglingForDisentangle ||
      mState == eStateEntanglingForClose) {
    return;
  }

  aIdentifier.uuid() = mIdentifier->uuid();
  aIdentifier.destinationUuid() = mIdentifier->destinationUuid();
  aIdentifier.sequenceId() = mIdentifier->sequenceId() + 1;
  aIdentifier.neutered() = false;

  // We have to entangle first.
  if (mState == eStateUnshippedEntangled) {
    MOZ_ASSERT(mUnshippedEntangledPort);
    MOZ_ASSERT(mMessagesForTheOtherPort.IsEmpty());

    RefPtr<MessagePort> port = std::move(mUnshippedEntangledPort);

    // Disconnect the entangled port and connect it to PBackground.
    if (!port->ConnectToPBackground()) {
      // We are probably shutting down. We cannot proceed.
      mState = eStateDisentangled;
      UpdateMustKeepAlive();
      return;
    }

    // In this case, we don't need to be connected to the PBackground service.
    if (mMessages.IsEmpty()) {
      aIdentifier.sequenceId() = mIdentifier->sequenceId();

      mState = eStateDisentangled;
      UpdateMustKeepAlive();
      return;
    }

    // Register this component to PBackground.
    if (!ConnectToPBackground()) {
      // We are probably shutting down. We cannot proceed.
      return;
    }

    mState = eStateEntanglingForDisentangle;
    return;
  }

  // Not entangled yet, we have to wait.
  if (mState == eStateEntangling) {
    mState = eStateEntanglingForDisentangle;
    return;
  }

  MOZ_ASSERT(mState == eStateEntangled);
  StartDisentangling();
}

void MessagePort::Closed() {
  if (mState >= eStateDisentangled) {
    return;
  }

  mState = eStateDisentangledForClose;

  if (mActor) {
    mActor->SetPort(nullptr);
    mActor = nullptr;
  }

  UpdateMustKeepAlive();
}

bool MessagePort::ConnectToPBackground() {
  RefPtr<MessagePort> self = this;
  auto raii = MakeScopeExit([self] {
    self->mState = eStateDisentangled;
    self->UpdateMustKeepAlive();
  });

  mozilla::ipc::PBackgroundChild* actorChild =
      mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!actorChild)) {
    return false;
  }

  PMessagePortChild* actor = actorChild->SendPMessagePortConstructor(
      mIdentifier->uuid(), mIdentifier->destinationUuid(),
      mIdentifier->sequenceId());
  if (NS_WARN_IF(!actor)) {
    return false;
  }

  mActor = static_cast<MessagePortChild*>(actor);
  MOZ_ASSERT(mActor);

  mActor->SetPort(this);
  mState = eStateEntangling;

  raii.release();
  return true;
}

void MessagePort::UpdateMustKeepAlive() {
  if (mState >= eStateDisentangled && mMessages.IsEmpty() && mIsKeptAlive) {
    mIsKeptAlive = false;

    // The DTOR of this WorkerRef will release the worker for us.
    mWorkerRef = nullptr;

    Release();
    return;
  }

  if (mState < eStateDisentangled && !mIsKeptAlive) {
    mIsKeptAlive = true;
    AddRef();
  }
}

void MessagePort::DisconnectFromOwner() {
  CloseForced();
  DOMEventTargetHelper::DisconnectFromOwner();
}

void MessagePort::RemoveDocFromBFCache() {
  if (!NS_IsMainThread()) {
    return;
  }

  if (nsPIDOMWindowInner* window = GetOwner()) {
    window->RemoveFromBFCacheSync();
  }
}

/* static */
void MessagePort::ForceClose(const MessagePortIdentifier& aIdentifier) {
  mozilla::ipc::PBackgroundChild* actorChild =
      mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!actorChild)) {
    MOZ_CRASH("Failed to create a PBackgroundChild actor!");
  }

  Unused << actorChild->SendMessagePortForceClose(aIdentifier.uuid(),
                                                  aIdentifier.destinationUuid(),
                                                  aIdentifier.sequenceId());
}

void MessagePort::DispatchError() {
  nsCOMPtr<nsIGlobalObject> globalObject = GetParentObject();

  AutoJSAPI jsapi;
  if (!globalObject || !jsapi.Init(globalObject)) {
    NS_WARNING("Failed to initialize AutoJSAPI object.");
    return;
  }

  RootedDictionary<MessageEventInit> init(jsapi.cx());
  init.mBubbles = false;
  init.mCancelable = false;

  RefPtr<Event> event =
      MessageEvent::Constructor(this, u"messageerror"_ns, init);
  event->SetTrusted(true);

  DispatchEvent(*event);
}

}  // namespace mozilla::dom
