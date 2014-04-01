/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MessagePort.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/MessageChannel.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "nsGlobalWindow.h"
#include "nsContentUtils.h"
#include "nsPresContext.h"

#include "nsIDocument.h"
#include "nsIDOMFile.h"
#include "nsIDOMFileList.h"
#include "nsIDOMMessageEvent.h"
#include "nsIPresShell.h"

namespace mozilla {
namespace dom {

class DispatchEventRunnable : public nsRunnable
{
  friend class MessagePort;

  public:
    DispatchEventRunnable(MessagePort* aPort)
      : mPort(aPort)
    {
    }

    NS_IMETHOD
    Run()
    {
      nsRefPtr<DispatchEventRunnable> mKungFuDeathGrip(this);

      mPort->mDispatchRunnable = nullptr;
      mPort->Dispatch();

      return NS_OK;
    }

  private:
    nsRefPtr<MessagePort> mPort;
};

class PostMessageRunnable : public nsRunnable
{
  friend class MessagePort;

  public:
    NS_DECL_NSIRUNNABLE

    PostMessageRunnable()
    {
    }

    ~PostMessageRunnable()
    {
    }

    JSAutoStructuredCloneBuffer& Buffer()
    {
      return mBuffer;
    }

    bool StoreISupports(nsISupports* aSupports)
    {
      mSupportsArray.AppendElement(aSupports);
      return true;
    }

    void Dispatch(MessagePort* aPort)
    {
      mPort = aPort;
      NS_DispatchToCurrentThread(this);
    }

  private:
    nsRefPtr<MessagePort> mPort;
    JSAutoStructuredCloneBuffer mBuffer;

    nsTArray<nsCOMPtr<nsISupports> > mSupportsArray;
};

namespace {

struct StructuredCloneInfo
{
  PostMessageRunnable* mEvent;
  MessagePort* mPort;
};

static JSObject*
PostMessageReadStructuredClone(JSContext* cx,
                               JSStructuredCloneReader* reader,
                               uint32_t tag,
                               uint32_t data,
                               void* closure)
{
  StructuredCloneInfo* scInfo = static_cast<StructuredCloneInfo*>(closure);
  NS_ASSERTION(scInfo, "Must have scInfo!");

  if (tag == SCTAG_DOM_BLOB || tag == SCTAG_DOM_FILELIST) {
    NS_ASSERTION(!data, "Data should be empty");

    nsISupports* supports;
    if (JS_ReadBytes(reader, &supports, sizeof(supports))) {
      JS::Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));
      if (global) {
        JS::Rooted<JS::Value> val(cx);
        if (NS_SUCCEEDED(nsContentUtils::WrapNative(cx, global, supports,
                                                    &val))) {
          return JSVAL_TO_OBJECT(val);
        }
      }
    }
  }

  if (tag == SCTAG_DOM_MESSAGEPORT) {
    NS_ASSERTION(!data, "Data should be empty");

    MessagePort* port;
    if (JS_ReadBytes(reader, &port, sizeof(port))) {
      JS::Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));
      if (global) {
        JS::Rooted<JSObject*> obj(cx, port->WrapObject(cx, global));
        if (JS_WrapObject(cx, &obj)) {
          port->BindToOwner(scInfo->mPort->GetOwner());
          return obj;
        }
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

static bool
PostMessageWriteStructuredClone(JSContext* cx,
                                JSStructuredCloneWriter* writer,
                                JS::Handle<JSObject*> obj,
                                void *closure)
{
  StructuredCloneInfo* scInfo = static_cast<StructuredCloneInfo*>(closure);
  NS_ASSERTION(scInfo, "Must have scInfo!");

  nsCOMPtr<nsIXPConnectWrappedNative> wrappedNative;
  nsContentUtils::XPConnect()->
    GetWrappedNativeOfJSObject(cx, obj, getter_AddRefs(wrappedNative));
  if (wrappedNative) {
    uint32_t scTag = 0;
    nsISupports* supports = wrappedNative->Native();

    nsCOMPtr<nsIDOMBlob> blob = do_QueryInterface(supports);
    if (blob) {
      scTag = SCTAG_DOM_BLOB;
    }

    nsCOMPtr<nsIDOMFileList> list = do_QueryInterface(supports);
    if (list) {
      scTag = SCTAG_DOM_FILELIST;
    }

    if (scTag) {
      return JS_WriteUint32Pair(writer, scTag, 0) &&
             JS_WriteBytes(writer, &supports, sizeof(supports)) &&
             scInfo->mEvent->StoreISupports(supports);
    }
  }

  MessagePortBase* port = nullptr;
  nsresult rv = UNWRAP_OBJECT(MessagePort, obj, port);
  if (NS_SUCCEEDED(rv)) {
    nsRefPtr<MessagePortBase> newPort = port->Clone();

    if (!newPort) {
      return false;
    }

    return JS_WriteUint32Pair(writer, SCTAG_DOM_MESSAGEPORT, 0) &&
           JS_WriteBytes(writer, &newPort, sizeof(newPort)) &&
           scInfo->mEvent->StoreISupports(newPort);
  }

  const JSStructuredCloneCallbacks* runtimeCallbacks =
    js::GetContextStructuredCloneCallbacks(cx);

  if (runtimeCallbacks) {
    return runtimeCallbacks->write(cx, writer, obj, nullptr);
  }

  return false;
}

JSStructuredCloneCallbacks kPostMessageCallbacks = {
  PostMessageReadStructuredClone,
  PostMessageWriteStructuredClone,
  nullptr
};

} // anonymous namespace

NS_IMETHODIMP
PostMessageRunnable::Run()
{
  MOZ_ASSERT(mPort);

  // Get the JSContext for the target window
  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(mPort->GetOwner());
  NS_ENSURE_STATE(sgo);
  nsCOMPtr<nsIScriptContext> scriptContext = sgo->GetContext();
  AutoPushJSContext cx(scriptContext ? scriptContext->GetNativeContext()
                                     : nsContentUtils::GetSafeJSContext());

  MOZ_ASSERT(cx);

  // Deserialize the structured clone data
  JS::Rooted<JS::Value> messageData(cx);
  {
    StructuredCloneInfo scInfo;
    scInfo.mEvent = this;
    scInfo.mPort = mPort;

    if (!mBuffer.read(cx, &messageData, &kPostMessageCallbacks, &scInfo)) {
      return NS_ERROR_DOM_DATA_CLONE_ERR;
    }
  }

  // Create the event
  nsIDocument* doc = mPort->GetOwner()->GetExtantDoc();
  if (!doc) {
    return NS_OK;
  }

  ErrorResult error;
  nsRefPtr<Event> event =
    doc->CreateEvent(NS_LITERAL_STRING("MessageEvent"), error);
  if (error.Failed()) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMMessageEvent> message = do_QueryInterface(event);
  nsresult rv = message->InitMessageEvent(NS_LITERAL_STRING("message"),
                                          false /* non-bubbling */,
                                          true /* cancelable */,
                                          messageData,
                                          EmptyString(),
                                          EmptyString(),
                                          mPort->GetOwner());
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  message->SetTrusted(true);

  bool status;
  mPort->DispatchEvent(event, &status);
  return status ? NS_OK : NS_ERROR_FAILURE;
}

MessagePortBase::MessagePortBase(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
{
  // SetIsDOMBinding() is called by DOMEventTargetHelper's ctor.
}

MessagePortBase::MessagePortBase()
{
  SetIsDOMBinding();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(MessagePort)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MessagePort,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mEntangledPort)

  // Custom unlink loop because this array contains nsRunnable objects
  // which are not cycle colleactable.
  while (!tmp->mMessageQueue.IsEmpty()) {
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessageQueue[0]->mPort);
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessageQueue[0]->mSupportsArray);
    tmp->mMessageQueue.RemoveElementAt(0);
  }

  if (tmp->mDispatchRunnable) {
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mDispatchRunnable->mPort);
  }

NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MessagePort,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEntangledPort)

  // Custom unlink loop because this array contains nsRunnable objects
  // which are not cycle colleactable.
  for (uint32_t i = 0, len = tmp->mMessageQueue.Length(); i < len; ++i) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessageQueue[i]->mPort);
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessageQueue[i]->mSupportsArray);
  }

  if (tmp->mDispatchRunnable) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDispatchRunnable->mPort);
  }

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MessagePort)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MessagePort, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MessagePort, DOMEventTargetHelper)

MessagePort::MessagePort(nsPIDOMWindow* aWindow)
  : MessagePortBase(aWindow)
  , mMessageQueueEnabled(false)
{
}

MessagePort::~MessagePort()
{
  Close();
}

JSObject*
MessagePort::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return MessagePortBinding::Wrap(aCx, aScope, this);
}

void
MessagePort::PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                            const Optional<Sequence<JS::Value>>& aTransferable,
                            ErrorResult& aRv)
{
  nsRefPtr<PostMessageRunnable> event = new PostMessageRunnable();

  // We *must* clone the data here, or the JS::Value could be modified
  // by script
  StructuredCloneInfo scInfo;
  scInfo.mEvent = event;
  scInfo.mPort = this;

  JS::Rooted<JS::Value> transferable(aCx, JS::UndefinedValue());
  if (aTransferable.WasPassed()) {
    const Sequence<JS::Value>& realTransferable = aTransferable.Value();

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

  if (!event->Buffer().write(aCx, aMessage, transferable,
                             &kPostMessageCallbacks, &scInfo)) {
    aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
    return;
  }

  if (!mEntangledPort) {
    return;
  }

  mEntangledPort->mMessageQueue.AppendElement(event);
  mEntangledPort->Dispatch();
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
  if (!mMessageQueueEnabled || mMessageQueue.IsEmpty() || mDispatchRunnable) {
    return;
  }

  nsRefPtr<PostMessageRunnable> event = mMessageQueue.ElementAt(0);
  mMessageQueue.RemoveElementAt(0);

  event->Dispatch(this);

  mDispatchRunnable = new DispatchEventRunnable(this);
  NS_DispatchToCurrentThread(mDispatchRunnable);
}

void
MessagePort::Close()
{
  if (!mEntangledPort) {
    return;
  }

  // This avoids loops.
  nsRefPtr<MessagePort> port = mEntangledPort;
  mEntangledPort = nullptr;

  // Let's disentangle the 2 ports symmetrically.
  port->Close();
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

void
MessagePort::Entangle(MessagePort* aMessagePort)
{
  MOZ_ASSERT(aMessagePort);
  MOZ_ASSERT(aMessagePort != this);

  Close();

  mEntangledPort = aMessagePort;
}

already_AddRefed<MessagePortBase>
MessagePort::Clone()
{
  nsRefPtr<MessagePort> newPort = new MessagePort(nullptr);

  // Move all the events in the port message queue of original port.
  newPort->mMessageQueue.SwapElements(mMessageQueue);

  if (mEntangledPort) {
    nsRefPtr<MessagePort> port = mEntangledPort;
    mEntangledPort = nullptr;

    newPort->Entangle(port);
    port->Entangle(newPort);
  }

  return newPort.forget();
}

} // namespace dom
} // namespace mozilla
