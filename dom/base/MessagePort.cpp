/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MessagePort.h"
#include "MessageEvent.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/MessageChannel.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/MessagePortList.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsPresContext.h"
#include "ScriptSettings.h"

#include "nsIDocument.h"
#include "nsIDOMFileList.h"
#include "nsIPresShell.h"

namespace mozilla {
namespace dom {

class DispatchEventRunnable : public nsRunnable
{
  friend class MessagePort;

  public:
    explicit DispatchEventRunnable(MessagePort* aPort)
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
  nsRefPtrHashtable<nsRefPtrHashKey<MessagePortBase>, MessagePortBase> mPorts;
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

  if (tag == SCTAG_DOM_BLOB) {
    NS_ASSERTION(!data, "Data should be empty");

    // What we get back from the reader is a FileImpl.
    // From that we create a new File.
    FileImpl* blobImpl;
    if (JS_ReadBytes(reader, &blobImpl, sizeof(blobImpl))) {
      MOZ_ASSERT(blobImpl);

      // nsRefPtr<File> needs to go out of scope before toObjectOrNull() is
      // called because the static analysis thinks dereferencing XPCOM objects
      // can GC (because in some cases it can!), and a return statement with a
      // JSObject* type means that JSObject* is on the stack as a raw pointer
      // while destructors are running.
      JS::Rooted<JS::Value> val(cx);
      {
        nsRefPtr<File> blob = new File(scInfo->mPort->GetParentObject(),
                                             blobImpl);
        if (!WrapNewBindingObject(cx, blob, &val)) {
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

static bool
PostMessageWriteStructuredClone(JSContext* cx,
                                JSStructuredCloneWriter* writer,
                                JS::Handle<JSObject*> obj,
                                void *closure)
{
  StructuredCloneInfo* scInfo = static_cast<StructuredCloneInfo*>(closure);
  NS_ASSERTION(scInfo, "Must have scInfo!");

  // See if this is a File/Blob object.
  {
    File* blob = nullptr;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(Blob, obj, blob))) {
      FileImpl* blobImpl = blob->Impl();
      if (JS_WriteUint32Pair(writer, SCTAG_DOM_BLOB, 0) &&
          JS_WriteBytes(writer, &blobImpl, sizeof(blobImpl))) {
        scInfo->mEvent->StoreISupports(blobImpl);
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
    if (list) {
      scTag = SCTAG_DOM_FILELIST;
    }

    if (scTag) {
      return JS_WriteUint32Pair(writer, scTag, 0) &&
             JS_WriteBytes(writer, &supports, sizeof(supports)) &&
             scInfo->mEvent->StoreISupports(supports);
    }
  }

  const JSStructuredCloneCallbacks* runtimeCallbacks =
    js::GetContextStructuredCloneCallbacks(cx);

  if (runtimeCallbacks) {
    return runtimeCallbacks->write(cx, writer, obj, nullptr);
  }

  return false;
}

static bool
PostMessageReadTransferStructuredClone(JSContext* aCx,
                                       JSStructuredCloneReader* reader,
                                       uint32_t tag, void* data,
                                       uint64_t unused,
                                       void* aClosure,
                                       JS::MutableHandle<JSObject*> returnObject)
{
  StructuredCloneInfo* scInfo = static_cast<StructuredCloneInfo*>(aClosure);
  NS_ASSERTION(scInfo, "Must have scInfo!");

  if (tag == SCTAG_DOM_MAP_MESSAGEPORT) {
    MessagePort* port = static_cast<MessagePort*>(data);
    port->BindToOwner(scInfo->mPort->GetOwner());
    scInfo->mPorts.Put(port, nullptr);

    JS::Rooted<JSObject*> obj(aCx, port->WrapObject(aCx));
    if (!obj || !JS_WrapObject(aCx, &obj)) {
      return false;
    }

    MOZ_ASSERT(port->GetOwner() == scInfo->mPort->GetOwner());
    returnObject.set(obj);
    return true;
  }

  return false;
}

static bool
PostMessageTransferStructuredClone(JSContext* aCx,
                                   JS::Handle<JSObject*> aObj,
                                   void* aClosure,
                                   uint32_t* aTag,
                                   JS::TransferableOwnership* aOwnership,
                                   void** aContent,
                                   uint64_t *aExtraData)
{
  StructuredCloneInfo* scInfo = static_cast<StructuredCloneInfo*>(aClosure);
  NS_ASSERTION(scInfo, "Must have scInfo!");

  MessagePortBase *port = nullptr;
  nsresult rv = UNWRAP_OBJECT(MessagePort, aObj, port);
  if (NS_SUCCEEDED(rv)) {
    nsRefPtr<MessagePortBase> newPort;
    if (scInfo->mPorts.Get(port, getter_AddRefs(newPort))) {
      // No duplicate.
      return false;
    }

    newPort = port->Clone();
    scInfo->mPorts.Put(port, newPort);

    *aTag = SCTAG_DOM_MAP_MESSAGEPORT;
    *aOwnership = JS::SCTAG_TMO_CUSTOM;
    *aContent = newPort;
    *aExtraData = 0;

    return true;
  }

  return false;
}

static void
PostMessageFreeTransferStructuredClone(uint32_t aTag, JS::TransferableOwnership aOwnership,
                                       void* aData,
                                       uint64_t aExtraData,
                                       void* aClosure)
{
  StructuredCloneInfo* scInfo = static_cast<StructuredCloneInfo*>(aClosure);
  NS_ASSERTION(scInfo, "Must have scInfo!");

  if (aTag == SCTAG_DOM_MAP_MESSAGEPORT) {
    MOZ_ASSERT(aOwnership == JS::SCTAG_TMO_CUSTOM);
    nsRefPtr<MessagePort> port(static_cast<MessagePort*>(aData));
    scInfo->mPorts.Remove(port);
  }
}

JSStructuredCloneCallbacks kPostMessageCallbacks = {
  PostMessageReadStructuredClone,
  PostMessageWriteStructuredClone,
  nullptr,
  PostMessageReadTransferStructuredClone,
  PostMessageTransferStructuredClone,
  PostMessageFreeTransferStructuredClone
};

} // anonymous namespace

static PLDHashOperator
PopulateMessagePortList(MessagePortBase* aKey, MessagePortBase* aValue, void* aClosure)
{
  nsTArray<nsRefPtr<MessagePortBase> > *array =
    static_cast<nsTArray<nsRefPtr<MessagePortBase> > *>(aClosure);

  array->AppendElement(aKey);
  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
PostMessageRunnable::Run()
{
  MOZ_ASSERT(mPort);

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mPort->GetParentObject()))) {
    return NS_ERROR_UNEXPECTED;
  }
  JSContext* cx = jsapi.cx();

  // Deserialize the structured clone data
  JS::Rooted<JS::Value> messageData(cx);
  StructuredCloneInfo scInfo;
  scInfo.mEvent = this;
  scInfo.mPort = mPort;

  if (!mBuffer.read(cx, &messageData, &kPostMessageCallbacks, &scInfo)) {
    return NS_ERROR_DOM_DATA_CLONE_ERR;
  }

  // Create the event
  nsCOMPtr<mozilla::dom::EventTarget> eventTarget =
    do_QueryInterface(mPort->GetOwner());
  nsRefPtr<MessageEvent> event =
    new MessageEvent(eventTarget, nullptr, nullptr);

  event->InitMessageEvent(NS_LITERAL_STRING("message"), false /* non-bubbling */,
                          false /* cancelable */, messageData, EmptyString(),
                          EmptyString(), nullptr);
  event->SetTrusted(true);
  event->SetSource(mPort);

  nsTArray<nsRefPtr<MessagePortBase> > ports;
  scInfo.mPorts.EnumerateRead(PopulateMessagePortList, &ports);
  event->SetPorts(new MessagePortList(static_cast<dom::Event*>(event.get()), ports));

  bool status;
  mPort->DispatchEvent(static_cast<dom::Event*>(event.get()), &status);
  return status ? NS_OK : NS_ERROR_FAILURE;
}

MessagePortBase::MessagePortBase(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
{
}

MessagePortBase::MessagePortBase()
{
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
MessagePort::WrapObject(JSContext* aCx)
{
  return MessagePortBinding::Wrap(aCx, this);
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
