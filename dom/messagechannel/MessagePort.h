/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MessagePort_h
#define mozilla_dom_MessagePort_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsIIPCBackgroundChildCreateCallback.h"
#include "nsTArray.h"

#ifdef XP_WIN
#undef PostMessage
#endif

class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class DispatchEventRunnable;
class MessagePortChild;
class MessagePortIdentifier;
class MessagePortMessage;
class SharedMessagePortMessage;

namespace workers {
class WorkerFeature;
} // namespace workers

class MessagePortBase : public DOMEventTargetHelper
{
protected:
  explicit MessagePortBase(nsPIDOMWindow* aWindow);
  MessagePortBase();

public:

  virtual void
  PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
              const Optional<Sequence<JS::Value>>& aTransferable,
              ErrorResult& aRv) = 0;

  virtual void
  Start() = 0;

  virtual void
  Close() = 0;

  // The 'message' event handler has to call |Start()| method, so we
  // cannot use IMPL_EVENT_HANDLER macro here.
  virtual EventHandlerNonNull*
  GetOnmessage() = 0;

  virtual void
  SetOnmessage(EventHandlerNonNull* aCallback) = 0;

  // Duplicate this message port. This method is used by the Structured Clone
  // Algorithm and populates a MessagePortIdentifier object with the information
  // useful to create new MessagePort.
  virtual bool
  CloneAndDisentangle(MessagePortIdentifier& aIdentifier) = 0;
};

class MessagePort final : public MessagePortBase
                        , public nsIIPCBackgroundChildCreateCallback
                        , public nsIObserver
{
  friend class DispatchEventRunnable;

public:
  NS_DECL_NSIIPCBACKGROUNDCHILDCREATECALLBACK
  NS_DECL_NSIOBSERVER
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MessagePort,
                                           MessagePortBase)

  static already_AddRefed<MessagePort>
  Create(nsPIDOMWindow* aWindow, const nsID& aUUID,
         const nsID& aDestinationUUID, ErrorResult& aRv);

  static already_AddRefed<MessagePort>
  Create(nsPIDOMWindow* aWindow, const MessagePortIdentifier& aIdentifier,
         ErrorResult& aRv);

  static void
  ForceClose(const MessagePortIdentifier& aIdentifier);

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  virtual void
  PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
              const Optional<Sequence<JS::Value>>& aTransferable,
              ErrorResult& aRv) override;

  virtual void Start() override;

  virtual void Close() override;

  virtual EventHandlerNonNull* GetOnmessage() override;

  virtual void SetOnmessage(EventHandlerNonNull* aCallback) override;

  // Non WebIDL methods

  void UnshippedEntangle(MessagePort* aEntangledPort);

  virtual bool CloneAndDisentangle(MessagePortIdentifier& aIdentifier) override;

  // These methods are useful for MessagePortChild

  void Entangled(nsTArray<MessagePortMessage>& aMessages);
  void MessagesReceived(nsTArray<MessagePortMessage>& aMessages);
  void StopSendingDataConfirmed();
  void Closed();

private:
  explicit MessagePort(nsPIDOMWindow* aWindow);
  ~MessagePort();

  enum State {
    // When a port is created by a MessageChannel it is entangled with the
    // other. They both run on the same thread, same event loop and the
    // messages are added to the queues without using PBackground actors.
    // When one of the port is shipped, the state is changed to
    // StateEntangling.
    eStateUnshippedEntangled,

    // If the port is closed or cloned when we are in this state, we set the
    // mNextStep. This 'next' operation will be done when entangled() message
    // is received.
    eStateEntangling,

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
    eStateDisentangled
  };

  void Initialize(const nsID& aUUID, const nsID& aDestinationUUID,
                  uint32_t aSequenceID, bool mNeutered, State aState,
                  ErrorResult& aRv);

  void ConnectToPBackground();

  // Dispatch events from the Message Queue using a nsRunnable.
  void Dispatch();

  void StartDisentangling();
  void Disentangle();

  void RemoveDocFromBFCache();

  // This method is meant to keep alive the MessagePort when this object is
  // creating the actor and until the actor is entangled.
  // We release the object when the port is closed or disentangled.
  void UpdateMustKeepAlive();

  bool IsCertainlyAliveForCC() const override
  {
    return mIsKeptAlive;
  }

  nsAutoPtr<workers::WorkerFeature> mWorkerFeature;

  nsRefPtr<DispatchEventRunnable> mDispatchRunnable;

  nsRefPtr<MessagePortChild> mActor;

  nsRefPtr<MessagePort> mUnshippedEntangledPort;

  nsTArray<nsRefPtr<SharedMessagePortMessage>> mMessages;
  nsTArray<nsRefPtr<SharedMessagePortMessage>> mMessagesForTheOtherPort;

  nsAutoPtr<MessagePortIdentifier> mIdentifier;

  uint64_t mInnerID;

  State mState;

  // This 'nextStep' is used when we are waiting to be entangled but the
  // content has called Clone() or Close().
  enum {
    eNextStepNone,
    eNextStepDisentangle,
    eNextStepClose
  } mNextStep;

  bool mMessageQueueEnabled;

  bool mIsKeptAlive;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MessagePort_h
