/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MessagePort_h
#define mozilla_dom_MessagePort_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"

#ifdef XP_WIN
#  undef PostMessage
#endif

class nsIGlobalObject;

namespace mozilla {
namespace dom {

class MessageData;
class MessagePortChild;
class PostMessageRunnable;
class RefMessageBodyService;
class SharedMessageBody;
class StrongWorkerRef;
struct StructuredSerializeOptions;

// A class to hold a MessagePortIdentifier from
// MessagePort::CloneAndDistentangle() and close if neither passed to
// MessagePort::Create() nor release()ed to send via IPC.
// When the `neutered` field of the MessagePortIdentifier is false, a close is
// required.
// This does not derive from MessagePortIdentifier because
// MessagePortIdentifier is final and because use of UniqueMessagePortId as a
// MessagePortIdentifier is intentionally prevented without release of
// ownership.
class UniqueMessagePortId final {
 public:
  UniqueMessagePortId() { mIdentifier.neutered() = true; }
  explicit UniqueMessagePortId(const MessagePortIdentifier& aIdentifier)
      : mIdentifier(aIdentifier) {}
  UniqueMessagePortId(UniqueMessagePortId&& aOther) noexcept
      : mIdentifier(aOther.mIdentifier) {
    aOther.mIdentifier.neutered() = true;
  }
  ~UniqueMessagePortId() { ForceClose(); };
  void ForceClose();

  [[nodiscard]] MessagePortIdentifier release() {
    MessagePortIdentifier id = mIdentifier;
    mIdentifier.neutered() = true;
    return id;
  }
  // const member accessors are not required because a const
  // UniqueMessagePortId is not useful.
  nsID& uuid() { return mIdentifier.uuid(); }
  nsID& destinationUuid() { return mIdentifier.destinationUuid(); }
  uint32_t& sequenceId() { return mIdentifier.sequenceId(); }
  bool& neutered() { return mIdentifier.neutered(); }

  UniqueMessagePortId(const UniqueMessagePortId& aOther) = delete;
  void operator=(const UniqueMessagePortId& aOther) = delete;

 private:
  MessagePortIdentifier mIdentifier;
};

class MessagePort final : public DOMEventTargetHelper {
  friend class PostMessageRunnable;

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MessagePort, DOMEventTargetHelper)

  static already_AddRefed<MessagePort> Create(nsIGlobalObject* aGlobal,
                                              const nsID& aUUID,
                                              const nsID& aDestinationUUID,
                                              ErrorResult& aRv);

  static already_AddRefed<MessagePort> Create(nsIGlobalObject* aGlobal,
                                              UniqueMessagePortId& aIdentifier,
                                              ErrorResult& aRv);

  // For IPC.
  static void ForceClose(const MessagePortIdentifier& aIdentifier);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  void PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                   const Sequence<JSObject*>& aTransferable, ErrorResult& aRv);

  void PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                   const StructuredSerializeOptions& aOptions,
                   ErrorResult& aRv);

  void Start();

  void Close();

  EventHandlerNonNull* GetOnmessage();

  void SetOnmessage(EventHandlerNonNull* aCallback);

  IMPL_EVENT_HANDLER(messageerror)

  // Non WebIDL methods

  void UnshippedEntangle(MessagePort* aEntangledPort);

  bool CanBeCloned() const { return !mHasBeenTransferredOrClosed; }

  void CloneAndDisentangle(UniqueMessagePortId& aIdentifier);

  void CloseForced();

  // These methods are useful for MessagePortChild

  void Entangled(nsTArray<MessageData>& aMessages);
  void MessagesReceived(nsTArray<MessageData>& aMessages);
  void StopSendingDataConfirmed();
  void Closed();

 private:
  enum State {
    // When a port is created by a MessageChannel it is entangled with the
    // other. They both run on the same thread, same event loop and the
    // messages are added to the queues without using PBackground actors.
    // When one of the port is shipped, the state is changed to
    // StateEntangling.
    eStateUnshippedEntangled,

    // If the port is closed or cloned when we are in this state, we go in one
    // of the following 2 steps. EntanglingForClose or ForDisentangle.
    eStateEntangling,

    // We are not fully entangled yet but are already disentangled.
    eStateEntanglingForDisentangle,

    // We are not fully entangled yet but are already closed.
    eStateEntanglingForClose,

    // When entangled() is received we send all the messages in the
    // mMessagesForTheOtherPort to the actor and we change the state to
    // StateEntangled. At this point the port is entangled with the other. We
    // send and receive messages.
    // If the port queue is not enabled, the received messages are stored in
    // the mMessages.
    eStateEntangled,

    // When the port is cloned or disentangled we want to stop receiving
    // messages. We call 'SendStopSendingData' to the actor and we wait for an
    // answer. All the messages received between now and the
    // 'StopSendingDataComfirmed are queued in the mMessages but not
    // dispatched.
    eStateDisentangling,

    // When 'StopSendingDataConfirmed' is received, we can disentangle the port
    // calling SendDisentangle in the actor because we are 100% sure that we
    // don't receive any other message, so nothing will be lost.
    // Disentangling the port we send all the messages from the mMessages
    // though the actor.
    eStateDisentangled,

    // We are here if Close() has been called. We are disentangled but we can
    // still send pending messages.
    eStateDisentangledForClose
  };

  explicit MessagePort(nsIGlobalObject* aGlobal, State aState);
  ~MessagePort();

  void DisconnectFromOwner() override;

  void Initialize(const nsID& aUUID, const nsID& aDestinationUUID,
                  uint32_t aSequenceID, bool aNeutered, ErrorResult& aRv);

  bool ConnectToPBackground();

  // Dispatch events from the Message Queue using a nsRunnable.
  void Dispatch();

  void DispatchError();

  void StartDisentangling();
  void Disentangle();

  void RemoveDocFromBFCache();

  void CloseInternal(bool aSoftly);

  // This method is meant to keep alive the MessagePort when this object is
  // creating the actor and until the actor is entangled.
  // We release the object when the port is closed or disentangled.
  void UpdateMustKeepAlive();

  bool IsCertainlyAliveForCC() const override { return mIsKeptAlive; }

  RefPtr<StrongWorkerRef> mWorkerRef;

  RefPtr<PostMessageRunnable> mPostMessageRunnable;

  RefPtr<MessagePortChild> mActor;

  RefPtr<MessagePort> mUnshippedEntangledPort;

  RefPtr<RefMessageBodyService> mRefMessageBodyService;

  nsTArray<RefPtr<SharedMessageBody>> mMessages;
  nsTArray<RefPtr<SharedMessageBody>> mMessagesForTheOtherPort;

  UniquePtr<MessagePortIdentifier> mIdentifier;

  State mState;

  bool mMessageQueueEnabled;

  bool mIsKeptAlive;

  // mHasBeenTransferredOrClosed is used to know if this port has been manually
  // closed or transferred via postMessage. Note that if the entangled port is
  // closed, this port is closed as well (see mState) but, just because close()
  // has not been called directly, by spec, this port can still be transferred.
  bool mHasBeenTransferredOrClosed;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MessagePort_h
