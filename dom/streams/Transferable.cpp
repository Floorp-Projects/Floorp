/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ErrorList.h"
#include "js/RootingAPI.h"
#include "js/String.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/dom/DOMExceptionBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDOMEventListener.h"
#include "nsIGlobalObject.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/MessageEvent.h"
#include "mozilla/dom/MessageChannel.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Promise-inl.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/ReadableStreamPipeTo.h"
#include "mozilla/dom/WritableStream.h"
#include "mozilla/dom/TransformStream.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {

static void PackAndPostMessage(JSContext* aCx, MessagePort* aPort,
                               const nsAString& aType,
                               JS::Handle<JS::Value> aValue, ErrorResult& aRv) {
  JS::Rooted<JSObject*> obj(aCx,
                            JS_NewObjectWithGivenProto(aCx, nullptr, nullptr));
  if (!obj) {
    // XXX: Should we crash here and there? See also bug 1762233.
    JS_ClearPendingException(aCx);
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  JS::Rooted<JS::Value> type(aCx);
  if (!xpc::NonVoidStringToJsval(aCx, aType, &type)) {
    JS_ClearPendingException(aCx);
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  if (!JS_DefineProperty(aCx, obj, "type", type, JSPROP_ENUMERATE)) {
    JS_ClearPendingException(aCx);
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  if (!JS_DefineProperty(aCx, obj, "value", aValue, JSPROP_ENUMERATE)) {
    JS_ClearPendingException(aCx);
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  Sequence<JSObject*> transferables;  // none in this case
  JS::Rooted<JS::Value> objValue(aCx, JS::ObjectValue(*obj));
  aPort->PostMessage(aCx, objValue, transferables, aRv);
}

// https://streams.spec.whatwg.org/#abstract-opdef-crossrealmtransformsenderror
static void CrossRealmTransformSendError(JSContext* aCx, MessagePort* aPort,
                                         JS::Handle<JS::Value> aError) {
  // Step 1: Perform PackAndPostMessage(port, "error", error), discarding the
  // result.
  PackAndPostMessage(aCx, aPort, u"error"_ns, aError, IgnoreErrors());
}

class SetUpTransformWritableMessageEventListener final
    : public nsIDOMEventListener {
 public:
  SetUpTransformWritableMessageEventListener(
      WritableStreamDefaultController* aController, Promise* aPromise)
      : mController(aController), mBackpressurePromise(aPromise) {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(SetUpTransformWritableMessageEventListener)

  // https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformwritable
  // The handler steps of Step 4.
  MOZ_CAN_RUN_SCRIPT NS_IMETHOD HandleEvent(Event* aEvent) override {
    AutoJSAPI jsapi;
    if (!jsapi.Init(mController->GetParentObject())) {
      return NS_OK;
    }
    JSContext* cx = jsapi.cx();
    MessageEvent* messageEvent = aEvent->AsMessageEvent();
    if (NS_WARN_IF(!messageEvent || !messageEvent->IsTrusted())) {
      return NS_OK;
    }

    // Step 1: Let data be the data of the message.
    JS::Rooted<JS::Value> dataValue(cx);
    IgnoredErrorResult rv;
    messageEvent->GetData(cx, &dataValue, rv);
    if (rv.Failed()) {
      return NS_OK;
    }

    // Step 2: Assert: Type(data) is Object.
    // (But we check in runtime instead to avoid potential malicious events from
    // a compromised process. Same below.)
    if (NS_WARN_IF(!dataValue.isObject())) {
      return NS_OK;
    }
    JS::Rooted<JSObject*> data(cx, &dataValue.toObject());

    // Step 3: Let type be ! Get(data, "type").
    JS::Rooted<JS::Value> type(cx);
    if (!JS_GetProperty(cx, data, "type", &type)) {
      // XXX: See bug 1762233
      JS_ClearPendingException(cx);
      return NS_OK;
    }

    // Step 4: Let value be ! Get(data, "value").
    JS::Rooted<JS::Value> value(cx);
    if (!JS_GetProperty(cx, data, "value", &value)) {
      JS_ClearPendingException(cx);
      return NS_OK;
    }

    // Step 5: Assert: Type(type) is String.
    if (NS_WARN_IF(!type.isString())) {
      return NS_OK;
    }

    // Step 6: If type is "pull",
    bool equals = false;
    if (!JS_StringEqualsLiteral(cx, type.toString(), "pull", &equals)) {
      JS_ClearPendingException(cx);
      return NS_OK;
    }
    if (equals) {
      // Step 6.1: If backpressurePromise is not undefined,
      MaybeResolveAndClearBackpressurePromise();
      return NS_OK;  // implicit
    }

    // Step 7: If type is "error",
    if (!JS_StringEqualsLiteral(cx, type.toString(), "error", &equals)) {
      JS_ClearPendingException(cx);
      return NS_OK;
    }
    if (equals) {
      // Step 7.1: Perform !
      // WritableStreamDefaultControllerErrorIfNeeded(controller, value).
      WritableStreamDefaultControllerErrorIfNeeded(cx, mController, value, rv);
      if (rv.Failed()) {
        return NS_OK;
      }

      // Step 7.2: If backpressurePromise is not undefined,
      MaybeResolveAndClearBackpressurePromise();
      return NS_OK;  // implicit
    }

    // Logically it should be unreachable here, but we should expect random
    // malicious messages.
    NS_WARNING("Got an unexpected type other than pull/error.");
    return NS_OK;
  }

  void MaybeResolveAndClearBackpressurePromise() {
    if (mBackpressurePromise) {
      mBackpressurePromise->MaybeResolveWithUndefined();
      mBackpressurePromise = nullptr;
    }
  }

  // Note: This promise field is shared with the sink algorithms.
  Promise* BackpressurePromise() { return mBackpressurePromise; }

  void CreateBackpressurePromise(ErrorResult& aRv) {
    mBackpressurePromise = Promise::Create(mController->GetParentObject(), aRv);
  }

 private:
  ~SetUpTransformWritableMessageEventListener() = default;

  // mController never changes before CC
  // TODO: MOZ_IMMUTABLE_OUTSIDE_CC
  MOZ_KNOWN_LIVE RefPtr<WritableStreamDefaultController> mController;
  RefPtr<Promise> mBackpressurePromise;
};

NS_IMPL_CYCLE_COLLECTION(SetUpTransformWritableMessageEventListener,
                         mController, mBackpressurePromise)
NS_IMPL_CYCLE_COLLECTING_ADDREF(SetUpTransformWritableMessageEventListener)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SetUpTransformWritableMessageEventListener)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(
    SetUpTransformWritableMessageEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
NS_INTERFACE_MAP_END

class SetUpTransformWritableMessageErrorEventListener final
    : public nsIDOMEventListener {
 public:
  SetUpTransformWritableMessageErrorEventListener(
      WritableStreamDefaultController* aController, MessagePort* aPort)
      : mController(aController), mPort(aPort) {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(
      SetUpTransformWritableMessageErrorEventListener)

  // https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformwritable
  // The handler steps of Step 5.
  MOZ_CAN_RUN_SCRIPT NS_IMETHOD HandleEvent(Event* aEvent) override {
    auto cleanupPort =
        MakeScopeExit([port = RefPtr<MessagePort>(mPort)]() { port->Close(); });

    if (NS_WARN_IF(!aEvent->AsMessageEvent() || !aEvent->IsTrusted())) {
      return NS_OK;
    }

    // Step 1: Let error be a new "DataCloneError" DOMException.
    RefPtr<DOMException> exception =
        DOMException::Create(NS_ERROR_DOM_DATA_CLONE_ERR);

    AutoJSAPI jsapi;
    if (!jsapi.Init(mPort->GetParentObject())) {
      return NS_OK;
    }
    JSContext* cx = jsapi.cx();
    JS::Rooted<JS::Value> error(cx);
    if (!ToJSValue(cx, *exception, &error)) {
      return NS_OK;
    }

    // Step 2: Perform ! CrossRealmTransformSendError(port, error).
    CrossRealmTransformSendError(cx, mPort, error);

    // Step 3: Perform
    // ! WritableStreamDefaultControllerErrorIfNeeded(controller, error).
    WritableStreamDefaultControllerErrorIfNeeded(cx, mController, error,
                                                 IgnoreErrors());

    // Step 4: Disentangle port.
    // (Close() does it)
    mPort->Close();
    cleanupPort.release();

    return NS_OK;
  }

 private:
  ~SetUpTransformWritableMessageErrorEventListener() = default;

  // mController never changes before CC
  // TODO: MOZ_IMMUTABLE_OUTSIDE_CC
  MOZ_KNOWN_LIVE RefPtr<WritableStreamDefaultController> mController;
  RefPtr<MessagePort> mPort;
};

NS_IMPL_CYCLE_COLLECTION(SetUpTransformWritableMessageErrorEventListener,
                         mController, mPort)
NS_IMPL_CYCLE_COLLECTING_ADDREF(SetUpTransformWritableMessageErrorEventListener)
NS_IMPL_CYCLE_COLLECTING_RELEASE(
    SetUpTransformWritableMessageErrorEventListener)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(
    SetUpTransformWritableMessageErrorEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
NS_INTERFACE_MAP_END

// https://streams.spec.whatwg.org/#abstract-opdef-packandpostmessagehandlingerror
static bool PackAndPostMessageHandlingError(
    JSContext* aCx, mozilla::dom::MessagePort* aPort, const nsAString& aType,
    JS::Handle<JS::Value> aValue, JS::MutableHandle<JS::Value> aError) {
  // Step 1: Let result be PackAndPostMessage(port, type, value).
  ErrorResult rv;
  PackAndPostMessage(aCx, aPort, aType, aValue, rv);

  // Step 2: If result is an abrupt completion,
  if (rv.Failed()) {
    // Step 2.2: Perform ! CrossRealmTransformSendError(port, result.[[Value]]).
    MOZ_ALWAYS_TRUE(ToJSValue(aCx, std::move(rv), aError));
    CrossRealmTransformSendError(aCx, aPort, aError);
    return false;
  }

  // Step 3: Return result as a completion record.
  return true;
}

// https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformwritable
class CrossRealmWritableUnderlyingSinkAlgorithms final
    : public UnderlyingSinkAlgorithmsBase {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(
      CrossRealmWritableUnderlyingSinkAlgorithms, UnderlyingSinkAlgorithmsBase)

  CrossRealmWritableUnderlyingSinkAlgorithms(
      SetUpTransformWritableMessageEventListener* aListener, MessagePort* aPort)
      : mListener(aListener), mPort(aPort) {}

  void StartCallback(JSContext* aCx,
                     WritableStreamDefaultController& aController,
                     JS::MutableHandle<JS::Value> aRetVal,
                     ErrorResult& aRv) override {
    // Step 7. Let startAlgorithm be an algorithm that returns undefined.
    aRetVal.setUndefined();
  }

  already_AddRefed<Promise> WriteCallback(
      JSContext* aCx, JS::Handle<JS::Value> aChunk,
      WritableStreamDefaultController& aController, ErrorResult& aRv) override {
    // Step 1: If backpressurePromise is undefined, set backpressurePromise to a
    // promise resolved with undefined.
    // Note: This promise field is shared with the message event listener.
    if (!mListener->BackpressurePromise()) {
      mListener->CreateBackpressurePromise(aRv);
      if (aRv.Failed()) {
        return nullptr;
      }
      mListener->BackpressurePromise()->MaybeResolveWithUndefined();
    }

    // Step 2: Return the result of reacting to backpressurePromise with the
    // following fulfillment steps:
    auto result =
        mListener->BackpressurePromise()->ThenWithCycleCollectedArgsJS(
            [](JSContext* aCx, JS::Handle<JS::Value>, ErrorResult& aRv,
               SetUpTransformWritableMessageEventListener* aListener,
               MessagePort* aPort,
               JS::Handle<JS::Value> aChunk) -> already_AddRefed<Promise> {
              // Step 2.1: Set backpressurePromise to a new promise.
              aListener->CreateBackpressurePromise(aRv);
              if (aRv.Failed()) {
                aPort->Close();
                return nullptr;
              }

              // Step 2.2: Let result be PackAndPostMessageHandlingError(port,
              // "chunk", chunk).
              JS::Rooted<JS::Value> error(aCx);
              bool result = PackAndPostMessageHandlingError(
                  aCx, aPort, u"chunk"_ns, aChunk, &error);

              // Step 2.3: If result is an abrupt completion,
              if (!result) {
                // Step 2.3.1: Disentangle port.
                // (Close() does it)
                aPort->Close();

                // Step 2.3.2: Return a promise rejected with result.[[Value]].
                return Promise::CreateRejected(aPort->GetParentObject(), error,
                                               aRv);
              }

              // Step 2.4: Otherwise, return a promise resolved with undefined.
              return Promise::CreateResolvedWithUndefined(
                  aPort->GetParentObject(), aRv);
            },
            std::make_tuple(mListener, mPort), std::make_tuple(aChunk));
    if (result.isErr()) {
      aRv.Throw(result.unwrapErr());
      return nullptr;
    }
    return result.unwrap().forget();
  }

  already_AddRefed<Promise> CloseCallback(JSContext* aCx,
                                          ErrorResult& aRv) override {
    // Step 1: Perform ! PackAndPostMessage(port, "close", undefined).
    PackAndPostMessage(aCx, mPort, u"close"_ns, JS::UndefinedHandleValue, aRv);
    // (We'll check the result after step 2)

    // Step 2: Disentangle port.
    // (Close() will do this)
    mPort->Close();

    if (aRv.Failed()) {
      return nullptr;
    }

    // Step 3: Return a promise resolved with undefined.
    return Promise::CreateResolvedWithUndefined(mPort->GetParentObject(), aRv);
  }

  already_AddRefed<Promise> AbortCallback(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv) override {
    // Step 1: Let result be PackAndPostMessageHandlingError(port, "error",
    // reason).
    JS::Rooted<JS::Value> error(aCx);
    bool result = PackAndPostMessageHandlingError(
        aCx, mPort, u"error"_ns,
        aReason.WasPassed() ? aReason.Value() : JS::UndefinedHandleValue,
        &error);

    // Step 2: Disentangle port.
    // (Close() will do this)
    mPort->Close();

    // Step 3: If result is an abrupt completion, return a promise rejected with
    // result.[[Value]].
    if (!result) {
      return Promise::CreateRejected(mPort->GetParentObject(), error, aRv);
    }

    // Step 4: Otherwise, return a promise resolved with undefined.
    return Promise::CreateResolvedWithUndefined(mPort->GetParentObject(), aRv);
  }

 protected:
  ~CrossRealmWritableUnderlyingSinkAlgorithms() override = default;

 private:
  RefPtr<SetUpTransformWritableMessageEventListener> mListener;
  RefPtr<MessagePort> mPort;
};

NS_IMPL_CYCLE_COLLECTION_INHERITED(CrossRealmWritableUnderlyingSinkAlgorithms,
                                   UnderlyingSinkAlgorithmsBase, mListener,
                                   mPort)
NS_IMPL_ADDREF_INHERITED(CrossRealmWritableUnderlyingSinkAlgorithms,
                         UnderlyingSinkAlgorithmsBase)
NS_IMPL_RELEASE_INHERITED(CrossRealmWritableUnderlyingSinkAlgorithms,
                          UnderlyingSinkAlgorithmsBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(
    CrossRealmWritableUnderlyingSinkAlgorithms)
NS_INTERFACE_MAP_END_INHERITING(CrossRealmWritableUnderlyingSinkAlgorithms)

// https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformwritable
MOZ_CAN_RUN_SCRIPT static void SetUpCrossRealmTransformWritable(
    WritableStream* aWritable, MessagePort* aPort, ErrorResult& aRv) {
  // (This is only needed for step 12, but let's do this early to fail early
  // enough)
  AutoJSAPI jsapi;
  if (!jsapi.Init(aWritable->GetParentObject())) {
    return;
  }
  JSContext* cx = jsapi.cx();

  // Step 1: Perform ! InitializeWritableStream(stream).
  // (Done by the constructor)

  // Step 2: Let controller be a new WritableStreamDefaultController.
  auto controller = MakeRefPtr<WritableStreamDefaultController>(
      aWritable->GetParentObject(), *aWritable);

  // Step 3: Let backpressurePromise be a new promise.
  RefPtr<Promise> backpressurePromise =
      Promise::Create(aWritable->GetParentObject(), aRv);
  if (aRv.Failed()) {
    return;
  }

  // Step 4: Add a handler for port’s message event with the following steps:
  auto listener = MakeRefPtr<SetUpTransformWritableMessageEventListener>(
      controller, backpressurePromise);
  aPort->AddEventListener(u"message"_ns, listener, false);

  // Step 5: Add a handler for port’s messageerror event with the following
  // steps:
  auto errorListener =
      MakeRefPtr<SetUpTransformWritableMessageErrorEventListener>(controller,
                                                                  aPort);
  aPort->AddEventListener(u"messageerror"_ns, errorListener, false);

  // Step 6: Enable port’s port message queue.
  // (Start() does it)
  aPort->Start();

  // Step 7 - 10:
  auto algorithms =
      MakeRefPtr<CrossRealmWritableUnderlyingSinkAlgorithms>(listener, aPort);

  // Step 11: Let sizeAlgorithm be an algorithm that returns 1.
  // (nullptr should serve this purpose. See also WritableStream::Constructor)

  // Step 12: Perform ! SetUpWritableStreamDefaultController(stream, controller,
  // startAlgorithm, writeAlgorithm, closeAlgorithm, abortAlgorithm, 1,
  // sizeAlgorithm).
  SetUpWritableStreamDefaultController(cx, aWritable, controller, algorithms, 1,
                                       /* aSizeAlgorithm */ nullptr, aRv);
}

class SetUpTransformReadableMessageEventListener final
    : public nsIDOMEventListener {
 public:
  SetUpTransformReadableMessageEventListener(
      ReadableStreamDefaultController* aController, MessagePort* aPort)
      : mController(aController), mPort(aPort) {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(SetUpTransformReadableMessageEventListener)

  // https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformreadable
  // The handler steps of Step 3.
  MOZ_CAN_RUN_SCRIPT NS_IMETHOD HandleEvent(Event* aEvent) override {
    auto cleanupPort =
        MakeScopeExit([port = RefPtr<MessagePort>(mPort)]() { port->Close(); });

    AutoJSAPI jsapi;
    if (!jsapi.Init(mPort->GetParentObject())) {
      return NS_OK;
    }
    JSContext* cx = jsapi.cx();
    MessageEvent* messageEvent = aEvent->AsMessageEvent();
    if (NS_WARN_IF(!messageEvent || !messageEvent->IsTrusted())) {
      return NS_OK;
    }

    // Step 1: Let data be the data of the message.
    JS::Rooted<JS::Value> dataValue(cx);
    IgnoredErrorResult rv;
    messageEvent->GetData(cx, &dataValue, rv);
    if (rv.Failed()) {
      return NS_OK;
    }

    // Step 2: Assert: Type(data) is Object.
    // (But we check in runtime instead to avoid potential malicious events from
    // a compromised process. Same below.)
    if (NS_WARN_IF(!dataValue.isObject())) {
      return NS_OK;
    }
    JS::Rooted<JSObject*> data(cx, JS::ToObject(cx, dataValue));

    // Step 3: Let type be ! Get(data, "type").
    JS::Rooted<JS::Value> type(cx);
    if (!JS_GetProperty(cx, data, "type", &type)) {
      // XXX: See bug 1762233
      JS_ClearPendingException(cx);
      return NS_OK;
    }

    // Step 4: Let value be ! Get(data, "value").
    JS::Rooted<JS::Value> value(cx);
    if (!JS_GetProperty(cx, data, "value", &value)) {
      JS_ClearPendingException(cx);
      return NS_OK;
    }

    // Step 5: Assert: Type(type) is String.
    if (NS_WARN_IF(!type.isString())) {
      return NS_OK;
    }

    // Step 6: If type is "chunk",
    bool equals = false;
    if (!JS_StringEqualsLiteral(cx, type.toString(), "chunk", &equals)) {
      JS_ClearPendingException(cx);
      return NS_OK;
    }
    if (equals) {
      // Step 6.1: Perform ! ReadableStreamDefaultControllerEnqueue(controller,
      // value).
      ReadableStreamDefaultControllerEnqueue(cx, mController, value,
                                             IgnoreErrors());
      cleanupPort.release();
      return NS_OK;  // implicit
    }

    // Step 7: Otherwise, if type is "close",
    if (!JS_StringEqualsLiteral(cx, type.toString(), "close", &equals)) {
      JS_ClearPendingException(cx);
      return NS_OK;
    }
    if (equals) {
      // Step 7.1: Perform ! ReadableStreamDefaultControllerClose(controller).
      ReadableStreamDefaultControllerClose(cx, mController, IgnoreErrors());
      // Step 7.2: Disentangle port.
      // (Close() does it)
      mPort->Close();
      cleanupPort.release();
      return NS_OK;  // implicit
    }

    // Step 8: Otherwise, if type is "error",
    if (!JS_StringEqualsLiteral(cx, type.toString(), "error", &equals)) {
      JS_ClearPendingException(cx);
      return NS_OK;
    }
    if (equals) {
      // Step 8.1: Perform ! ReadableStreamDefaultControllerError(controller,
      // value).
      ReadableStreamDefaultControllerError(cx, mController, value,
                                           IgnoreErrors());

      // Step 8.2: Disentangle port.
      // (Close() does it)
      mPort->Close();
      cleanupPort.release();
      return NS_OK;  // implicit
    }

    // Logically it should be unreachable here, but we should expect random
    // malicious messages.
    NS_WARNING("Got an unexpected type other than chunk/close/error.");
    return NS_OK;
  }

 private:
  ~SetUpTransformReadableMessageEventListener() = default;

  // mController never changes before CC
  // TODO: MOZ_IMMUTABLE_OUTSIDE_CC
  MOZ_KNOWN_LIVE RefPtr<ReadableStreamDefaultController> mController;
  RefPtr<MessagePort> mPort;
};

NS_IMPL_CYCLE_COLLECTION(SetUpTransformReadableMessageEventListener,
                         mController, mPort)
NS_IMPL_CYCLE_COLLECTING_ADDREF(SetUpTransformReadableMessageEventListener)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SetUpTransformReadableMessageEventListener)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(
    SetUpTransformReadableMessageEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
NS_INTERFACE_MAP_END

class SetUpTransformReadableMessageErrorEventListener final
    : public nsIDOMEventListener {
 public:
  SetUpTransformReadableMessageErrorEventListener(
      ReadableStreamDefaultController* aController, MessagePort* aPort)
      : mController(aController), mPort(aPort) {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(
      SetUpTransformReadableMessageErrorEventListener)

  // https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformreadable
  // The handler steps of Step 4.
  MOZ_CAN_RUN_SCRIPT NS_IMETHOD HandleEvent(Event* aEvent) override {
    auto cleanupPort =
        MakeScopeExit([port = RefPtr<MessagePort>(mPort)]() { port->Close(); });

    if (NS_WARN_IF(!aEvent->AsMessageEvent() || !aEvent->IsTrusted())) {
      return NS_OK;
    }

    // Step 1: Let error be a new "DataCloneError" DOMException.
    RefPtr<DOMException> exception =
        DOMException::Create(NS_ERROR_DOM_DATA_CLONE_ERR);

    AutoJSAPI jsapi;
    if (!jsapi.Init(mPort->GetParentObject())) {
      return NS_OK;
    }
    JSContext* cx = jsapi.cx();
    JS::Rooted<JS::Value> error(cx);
    if (!ToJSValue(cx, *exception, &error)) {
      return NS_OK;
    }

    // Step 2: Perform ! CrossRealmTransformSendError(port, error).
    CrossRealmTransformSendError(cx, mPort, error);

    // Step 3: Perform ! ReadableStreamDefaultControllerError(controller,
    // error).
    ReadableStreamDefaultControllerError(cx, mController, error,
                                         IgnoreErrors());

    // Step 4: Disentangle port.
    // (Close() does it)
    mPort->Close();
    cleanupPort.release();

    return NS_OK;
  }

 private:
  ~SetUpTransformReadableMessageErrorEventListener() = default;

  RefPtr<ReadableStreamDefaultController> mController;
  RefPtr<MessagePort> mPort;
};

NS_IMPL_CYCLE_COLLECTION(SetUpTransformReadableMessageErrorEventListener,
                         mController, mPort)
NS_IMPL_CYCLE_COLLECTING_ADDREF(SetUpTransformReadableMessageErrorEventListener)
NS_IMPL_CYCLE_COLLECTING_RELEASE(
    SetUpTransformReadableMessageErrorEventListener)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(
    SetUpTransformReadableMessageErrorEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
NS_INTERFACE_MAP_END

// https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformreadable
class CrossRealmReadableUnderlyingSourceAlgorithms final
    : public UnderlyingSourceAlgorithmsBase {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(
      CrossRealmReadableUnderlyingSourceAlgorithms,
      UnderlyingSourceAlgorithmsBase)

  explicit CrossRealmReadableUnderlyingSourceAlgorithms(MessagePort* aPort)
      : mPort(aPort) {}

  void StartCallback(JSContext* aCx, ReadableStreamController& aController,
                     JS::MutableHandle<JS::Value> aRetVal,
                     ErrorResult& aRv) override {
    // Step 6. Let startAlgorithm be an algorithm that returns undefined.
    aRetVal.setUndefined();
  }

  already_AddRefed<Promise> PullCallback(JSContext* aCx,
                                         ReadableStreamController& aController,
                                         ErrorResult& aRv) override {
    // Step 7: Let pullAlgorithm be the following steps:

    // Step 7.1: Perform ! PackAndPostMessage(port, "pull", undefined).
    PackAndPostMessage(aCx, mPort, u"pull"_ns, JS::UndefinedHandleValue, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }

    // Step 7.2: Return a promise resolved with undefined.
    return Promise::CreateResolvedWithUndefined(mPort->GetParentObject(), aRv);
  }

  already_AddRefed<Promise> CancelCallback(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv) override {
    // Step 8: Let cancelAlgorithm be the following steps, taking a reason
    // argument:

    // Step 8.1: Let result be PackAndPostMessageHandlingError(port, "error",
    // reason).
    JS::Rooted<JS::Value> error(aCx);
    bool result = PackAndPostMessageHandlingError(
        aCx, mPort, u"error"_ns,
        aReason.WasPassed() ? aReason.Value() : JS::UndefinedHandleValue,
        &error);

    // Step 8.2: Disentangle port.
    // (Close() does it)
    mPort->Close();

    // Step 8.3: If result is an abrupt completion, return a promise rejected
    // with result.[[Value]].
    if (!result) {
      return Promise::CreateRejected(mPort->GetParentObject(), error, aRv);
    }

    // Step 8.4: Otherwise, return a promise resolved with undefined.
    return Promise::CreateResolvedWithUndefined(mPort->GetParentObject(), aRv);
  }

  void ErrorCallback() override {}

 protected:
  ~CrossRealmReadableUnderlyingSourceAlgorithms() override = default;

 private:
  RefPtr<MessagePort> mPort;
};

NS_IMPL_CYCLE_COLLECTION_INHERITED(CrossRealmReadableUnderlyingSourceAlgorithms,
                                   UnderlyingSourceAlgorithmsBase, mPort)
NS_IMPL_ADDREF_INHERITED(CrossRealmReadableUnderlyingSourceAlgorithms,
                         UnderlyingSourceAlgorithmsBase)
NS_IMPL_RELEASE_INHERITED(CrossRealmReadableUnderlyingSourceAlgorithms,
                          UnderlyingSourceAlgorithmsBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(
    CrossRealmReadableUnderlyingSourceAlgorithms)
NS_INTERFACE_MAP_END_INHERITING(CrossRealmReadableUnderlyingSourceAlgorithms)

// https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformreadable
MOZ_CAN_RUN_SCRIPT static void SetUpCrossRealmTransformReadable(
    ReadableStream* aReadable, MessagePort* aPort, ErrorResult& aRv) {
  // (This is only needed for step 10, but let's do this early to fail early
  // enough)
  AutoJSAPI jsapi;
  if (!jsapi.Init(aReadable->GetParentObject())) {
    return;
  }
  JSContext* cx = jsapi.cx();

  // Step 1: Perform ! InitializeReadableStream(stream).
  // (This is implicitly done by the constructor)

  // Step 2: Let controller be a new ReadableStreamDefaultController.
  auto controller =
      MakeRefPtr<ReadableStreamDefaultController>(aReadable->GetParentObject());

  // Step 3: Add a handler for port’s message event with the following steps:
  auto listener =
      MakeRefPtr<SetUpTransformReadableMessageEventListener>(controller, aPort);
  aPort->AddEventListener(u"message"_ns, listener, false);

  // Step 4: Add a handler for port’s messageerror event with the following
  // steps:
  auto errorListener =
      MakeRefPtr<SetUpTransformReadableMessageErrorEventListener>(controller,
                                                                  aPort);
  aPort->AddEventListener(u"messageerror"_ns, errorListener, false);

  // Step 5: Enable port’s port message queue.
  // (Start() does it)
  aPort->Start();

  // Step 6-8:
  auto algorithms =
      MakeRefPtr<CrossRealmReadableUnderlyingSourceAlgorithms>(aPort);

  // Step 9: Let sizeAlgorithm be an algorithm that returns 1.
  // (nullptr should serve this purpose. See also ReadableStream::Constructor)

  // Step 10: Perform ! SetUpReadableStreamDefaultController(stream, controller,
  // startAlgorithm, pullAlgorithm, cancelAlgorithm, 0, sizeAlgorithm).
  SetUpReadableStreamDefaultController(cx, aReadable, controller, algorithms, 0,
                                       /* aSizeAlgorithm */ nullptr, aRv);
}

// https://streams.spec.whatwg.org/#ref-for-transfer-steps
bool ReadableStream::Transfer(JSContext* aCx, UniqueMessagePortId& aPortId) {
  // Step 1: If ! IsReadableStreamLocked(value) is true, throw a
  // "DataCloneError" DOMException.
  // (Implemented in StructuredCloneHolder::CustomCanTransferHandler)

  // Step 2: Let port1 be a new MessagePort in the current Realm.
  // Step 3: Let port2 be a new MessagePort in the current Realm.
  // Step 4: Entangle port1 and port2.
  // (The MessageChannel constructor does exactly that.)
  // https://html.spec.whatwg.org/multipage/web-messaging.html#dom-messagechannel
  ErrorResult rv;
  RefPtr<dom::MessageChannel> channel =
      dom::MessageChannel::Constructor(mGlobal, rv);
  if (rv.MaybeSetPendingException(aCx)) {
    return false;
  }

  // Step 5: Let writable be a new WritableStream in the current Realm.
  RefPtr<WritableStream> writable = new WritableStream(mGlobal);

  // Step 6: Perform ! SetUpCrossRealmTransformWritable(writable, port1).
  // MOZ_KnownLive because Port1 never changes before CC
  SetUpCrossRealmTransformWritable(writable, MOZ_KnownLive(channel->Port1()),
                                   rv);
  if (rv.MaybeSetPendingException(aCx)) {
    return false;
  }

  // Step 7: Let promise be ! ReadableStreamPipeTo(value, writable, false,
  // false, false).
  RefPtr<Promise> promise =
      ReadableStreamPipeTo(this, writable, false, false, false, nullptr, rv);
  if (rv.MaybeSetPendingException(aCx)) {
    return false;
  }

  // Step 8: Set promise.[[PromiseIsHandled]] to true.
  MOZ_ALWAYS_TRUE(promise->SetAnyPromiseIsHandled());

  // Step 9: Set dataHolder.[[port]] to ! StructuredSerializeWithTransfer(port2,
  // « port2 »).
  channel->Port2()->CloneAndDisentangle(aPortId);

  return true;
}

// https://streams.spec.whatwg.org/#ref-for-transfer-receiving-steps
MOZ_CAN_RUN_SCRIPT static already_AddRefed<ReadableStream>
ReadableStreamTransferReceivingStepsImpl(JSContext* aCx,
                                         nsIGlobalObject* aGlobal,
                                         MessagePort& aPort) {
  // Step 1: Let deserializedRecord be
  // ! StructuredDeserializeWithTransfer(dataHolder.[[port]], the current
  // Realm).
  // Step 2: Let port be deserializedRecord.[[Deserialized]].

  // Step 3: Perform ! SetUpCrossRealmTransformReadable(value, port).
  RefPtr<ReadableStream> readable = new ReadableStream(aGlobal);
  ErrorResult rv;
  SetUpCrossRealmTransformReadable(readable, &aPort, rv);
  if (rv.MaybeSetPendingException(aCx)) {
    return nullptr;
  }
  return readable.forget();
}

bool ReadableStream::ReceiveTransfer(
    JSContext* aCx, nsIGlobalObject* aGlobal, MessagePort& aPort,
    JS::MutableHandle<JSObject*> aReturnObject) {
  RefPtr<ReadableStream> readable =
      ReadableStreamTransferReceivingStepsImpl(aCx, aGlobal, aPort);
  if (!readable) {
    return false;
  }

  JS::Rooted<JS::Value> value(aCx);
  if (!GetOrCreateDOMReflector(aCx, readable, &value)) {
    JS_ClearPendingException(aCx);
    return false;
  }
  aReturnObject.set(&value.toObject());

  return true;
}

// https://streams.spec.whatwg.org/#ref-for-transfer-steps①
bool WritableStream::Transfer(JSContext* aCx, UniqueMessagePortId& aPortId) {
  // Step 1: If ! IsWritableStreamLocked(value) is true, throw a
  // "DataCloneError" DOMException.
  // (Implemented in StructuredCloneHolder::CustomCanTransferHandler)

  // Step 2: Let port1 be a new MessagePort in the current Realm.
  // Step 3: Let port2 be a new MessagePort in the current Realm.
  // Step 4: Entangle port1 and port2.
  // (The MessageChannel constructor does exactly that.)
  // https://html.spec.whatwg.org/multipage/web-messaging.html#dom-messagechannel
  ErrorResult rv;
  RefPtr<dom::MessageChannel> channel =
      dom::MessageChannel::Constructor(mGlobal, rv);
  if (rv.MaybeSetPendingException(aCx)) {
    return false;
  }

  // Step 5: Let readable be a new ReadableStream in the current Realm.
  auto readable = MakeRefPtr<ReadableStream>(mGlobal);

  // Step 6: Perform ! SetUpCrossRealmTransformReadable(readable, port1).
  // MOZ_KnownLive because Port1 never changes before CC
  SetUpCrossRealmTransformReadable(readable, MOZ_KnownLive(channel->Port1()),
                                   rv);
  if (rv.MaybeSetPendingException(aCx)) {
    return false;
  }

  // Step 7: Let promise be ! ReadableStreamPipeTo(readable, value, false,
  // false, false).
  RefPtr<Promise> promise =
      ReadableStreamPipeTo(readable, this, false, false, false, nullptr, rv);
  if (rv.Failed()) {
    return false;
  }

  // Step 8: Set promise.[[PromiseIsHandled]] to true.
  MOZ_ALWAYS_TRUE(promise->SetAnyPromiseIsHandled());

  // Step 9: Set dataHolder.[[port]] to ! StructuredSerializeWithTransfer(port2,
  // « port2 »).
  channel->Port2()->CloneAndDisentangle(aPortId);

  return true;
}

// https://streams.spec.whatwg.org/#ref-for-transfer-receiving-steps①
MOZ_CAN_RUN_SCRIPT static already_AddRefed<WritableStream>
WritableStreamTransferReceivingStepsImpl(JSContext* aCx,
                                         nsIGlobalObject* aGlobal,
                                         MessagePort& aPort) {
  // Step 1: Let deserializedRecord be !
  // StructuredDeserializeWithTransfer(dataHolder.[[port]], the current Realm).
  // Step 2: Let port be a deserializedRecord.[[Deserialized]].

  // Step 3: Perform ! SetUpCrossRealmTransformWritable(value, port).
  auto writable = MakeRefPtr<WritableStream>(aGlobal);
  ErrorResult rv;
  SetUpCrossRealmTransformWritable(writable, &aPort, rv);
  if (rv.MaybeSetPendingException(aCx)) {
    return nullptr;
  }
  return writable.forget();
}

// https://streams.spec.whatwg.org/#ref-for-transfer-receiving-steps①
bool WritableStream::ReceiveTransfer(
    JSContext* aCx, nsIGlobalObject* aGlobal, MessagePort& aPort,
    JS::MutableHandle<JSObject*> aReturnObject) {
  RefPtr<WritableStream> writable =
      WritableStreamTransferReceivingStepsImpl(aCx, aGlobal, aPort);
  if (!writable) {
    return false;
  }

  JS::Rooted<JS::Value> value(aCx);
  if (!GetOrCreateDOMReflector(aCx, writable, &value)) {
    JS_ClearPendingException(aCx);
    return false;
  }
  aReturnObject.set(&value.toObject());

  return true;
}

// https://streams.spec.whatwg.org/#ref-for-transfer-steps②
bool TransformStream::Transfer(JSContext* aCx, UniqueMessagePortId& aPortId1,
                               UniqueMessagePortId& aPortId2) {
  // Step 1: Let readable be value.[[readable]].
  // Step 2: Let writable be value.[[writable]].
  // Step 3: If ! IsReadableStreamLocked(readable) is true, throw a
  // "DataCloneError" DOMException.
  // Step 4: If ! IsWritableStreamLocked(writable) is true, throw a
  // "DataCloneError" DOMException.
  // (Implemented in StructuredCloneHolder::CustomCanTransferHandler)

  // Step 5: Set dataHolder.[[readable]] to !
  // StructuredSerializeWithTransfer(readable, « readable »).
  // TODO: Mark mReadable as MOZ_KNOWN_LIVE again (bug 1769854)
  if (!MOZ_KnownLive(mReadable)->Transfer(aCx, aPortId1)) {
    return false;
  }

  // Step 6: Set dataHolder.[[writable]] to !
  // StructuredSerializeWithTransfer(writable, « writable »).
  // TODO: Mark mReadable as MOZ_KNOWN_LIVE again (bug 1769854)
  return MOZ_KnownLive(mWritable)->Transfer(aCx, aPortId2);
}

// https://streams.spec.whatwg.org/#ref-for-transfer-receiving-steps②
bool TransformStream::ReceiveTransfer(
    JSContext* aCx, nsIGlobalObject* aGlobal, MessagePort& aPort1,
    MessagePort& aPort2, JS::MutableHandle<JSObject*> aReturnObject) {
  // Step 1: Let readableRecord be !
  // StructuredDeserializeWithTransfer(dataHolder.[[readable]], the current
  // Realm).
  RefPtr<ReadableStream> readable =
      ReadableStreamTransferReceivingStepsImpl(aCx, aGlobal, aPort1);
  if (!readable) {
    return false;
  }

  // Step 2: Let writableRecord be !
  // StructuredDeserializeWithTransfer(dataHolder.[[writable]], the current
  // Realm).
  RefPtr<WritableStream> writable =
      WritableStreamTransferReceivingStepsImpl(aCx, aGlobal, aPort2);
  if (!writable) {
    return false;
  }

  // Step 3: Set value.[[readable]] to readableRecord.[[Deserialized]].
  // Step 4: Set value.[[writable]] to writableRecord.[[Deserialized]].
  // Step 5: Set value.[[backpressure]], value.[[backpressureChangePromise]],
  // and value.[[controller]] to undefined.
  auto stream = MakeRefPtr<TransformStream>(aGlobal, readable, writable);
  JS::Rooted<JS::Value> value(aCx);
  if (!GetOrCreateDOMReflector(aCx, stream, &value)) {
    JS_ClearPendingException(aCx);
    return false;
  }
  aReturnObject.set(&value.toObject());

  return true;
}

}  // namespace mozilla::dom
