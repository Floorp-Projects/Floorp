/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MessagePort.h"
#include "mozilla/dom/MessageChannel.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "nsGlobalWindow.h"
#include "nsContentUtils.h"
#include "nsEventDispatcher.h"
#include "nsPresContext.h"
#include "nsDOMEvent.h"

#include "nsIDocument.h"
#include "nsIDOMFile.h"
#include "nsIDOMFileList.h"
#include "nsIDOMMessageEvent.h"
#include "nsIPresShell.h"

namespace mozilla {
namespace dom {

class PostMessageRunnable : public nsRunnable
{
  public:
    NS_DECL_NSIRUNNABLE

    PostMessageRunnable(MessagePort* aPort)
      : mPort(aPort)
      , mMessage(nullptr)
      , mMessageLen(0)
    {
    }

    ~PostMessageRunnable()
    {
      NS_ASSERTION(!mMessage, "Message should have been deserialized!");
    }

    void SetJSData(JSAutoStructuredCloneBuffer& aBuffer)
    {
      NS_ASSERTION(!mMessage && mMessageLen == 0, "Don't call twice!");
      aBuffer.steal(&mMessage, &mMessageLen);
    }

    bool StoreISupports(nsISupports* aSupports)
    {
      mSupportsArray.AppendElement(aSupports);
      return true;
    }

  private:
    nsRefPtr<MessagePort> mPort;
    uint64_t* mMessage;
    size_t mMessageLen;

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
  NS_ASSERTION(closure, "Must have closure!");

  if (tag == SCTAG_DOM_BLOB || tag == SCTAG_DOM_FILELIST) {
    NS_ASSERTION(!data, "Data should be empty");

    nsISupports* supports;
    if (JS_ReadBytes(reader, &supports, sizeof(supports))) {
      JS::Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));
      if (global) {
        JS::Rooted<JS::Value> val(cx);
        nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
        if (NS_SUCCEEDED(nsContentUtils::WrapNative(cx, global, supports,
                                                    val.address(),
                                                    getter_AddRefs(wrapper)))) {
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
        if (JS_WrapObject(cx, obj.address())) {
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

  MessagePort* port = nullptr;
  nsresult rv = UNWRAP_OBJECT(MessagePort, cx, obj, port);
  if (NS_SUCCEEDED(rv)) {
    nsRefPtr<MessagePort> newPort = port->Clone(scInfo->mPort->GetOwner());

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
  // Ensure that the buffer is freed even if we fail to post the message
  JSAutoStructuredCloneBuffer buffer;
  buffer.adopt(mMessage, mMessageLen);
  mMessage = nullptr;
  mMessageLen = 0;

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

    if (!buffer.read(cx, messageData.address(), &kPostMessageCallbacks,
                     &scInfo)) {
      return NS_ERROR_DOM_DATA_CLONE_ERR;
    }
  }

  // Create the event
  nsIDocument* doc = mPort->GetOwner()->GetExtantDoc();
  if (!doc) {
    return NS_OK;
  }

  ErrorResult error;
  nsRefPtr<nsDOMEvent> event =
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

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(MessagePort, nsDOMEventTargetHelper,
                                     mEntangledPort)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MessagePort)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MessagePort, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MessagePort, nsDOMEventTargetHelper)

MessagePort::MessagePort(nsPIDOMWindow* aWindow)
  : nsDOMEventTargetHelper(aWindow)
{
  MOZ_COUNT_CTOR(MessagePort);
  SetIsDOMBinding();
}

MessagePort::~MessagePort()
{
  MOZ_COUNT_DTOR(MessagePort);
  Close();
}

JSObject*
MessagePort::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return MessagePortBinding::Wrap(aCx, aScope, this);
}

void
MessagePort::PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                         const Optional<JS::Handle<JS::Value> >& aTransfer,
                         ErrorResult& aRv)
{
  if (!mEntangledPort) {
    return;
  }

  nsRefPtr<PostMessageRunnable> event = new PostMessageRunnable(mEntangledPort);

  // We *must* clone the data here, or the JS::Value could be modified
  // by script
  JSAutoStructuredCloneBuffer buffer;
  StructuredCloneInfo scInfo;
  scInfo.mEvent = event;
  scInfo.mPort = this;

  JS::Handle<JS::Value> transferable = aTransfer.WasPassed()
                                       ? aTransfer.Value()
                                       : JS::UndefinedHandleValue;

  if (!buffer.write(aCx, aMessage, transferable, &kPostMessageCallbacks,
                    &scInfo)) {
    aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
    return;
  }

  event->SetJSData(buffer);
  NS_DispatchToCurrentThread(event);
}

void
MessagePort::Start()
{
  // TODO
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

void
MessagePort::Entangle(MessagePort* aMessagePort)
{
  MOZ_ASSERT(aMessagePort);
  MOZ_ASSERT(aMessagePort != this);

  Close();

  mEntangledPort = aMessagePort;
}

already_AddRefed<MessagePort>
MessagePort::Clone(nsPIDOMWindow* aWindow)
{
  nsRefPtr<MessagePort> newPort = new MessagePort(aWindow->GetCurrentInnerWindow());

  // TODO Move all the events in the port message queue of original port to the
  // port message queue of new port, if any, leaving the new port's port
  // message queue in its initial disabled state.

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
