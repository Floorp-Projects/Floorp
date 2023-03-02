/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SerializedStackHolder.h"

#include "js/SavedFrameAPI.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/Services.h"
#include "nsJSPrincipals.h"
#include "nsIObserverService.h"
#include "xpcpublic.h"

namespace mozilla::dom {

SerializedStackHolder::SerializedStackHolder()
    : mHolder(StructuredCloneHolder::CloningSupported,
              StructuredCloneHolder::TransferringNotSupported,
              StructuredCloneHolder::StructuredCloneScope::SameProcess) {}

void SerializedStackHolder::WriteStack(JSContext* aCx,
                                       JS::Handle<JSObject*> aStack) {
  JS::Rooted<JS::Value> stackValue(aCx, JS::ObjectValue(*aStack));
  mHolder.Write(aCx, stackValue, IgnoreErrors());

  // StructuredCloneHolder::Write can leave a pending exception on the context.
  JS_ClearPendingException(aCx);
}

void SerializedStackHolder::SerializeMainThreadOrWorkletStack(
    JSContext* aCx, JS::Handle<JSObject*> aStack) {
  MOZ_ASSERT(!IsCurrentThreadRunningWorker());
  WriteStack(aCx, aStack);
}

void SerializedStackHolder::SerializeWorkerStack(JSContext* aCx,
                                                 WorkerPrivate* aWorkerPrivate,
                                                 JS::Handle<JSObject*> aStack) {
  MOZ_ASSERT(aWorkerPrivate->IsOnCurrentThread());

  RefPtr<StrongWorkerRef> workerRef =
      StrongWorkerRef::Create(aWorkerPrivate, "WorkerErrorReport");
  if (workerRef) {
    mWorkerRef = new ThreadSafeWorkerRef(workerRef);
  } else {
    // Don't write the stack if we can't create a ref to the worker.
    return;
  }

  WriteStack(aCx, aStack);
}

void SerializedStackHolder::SerializeCurrentStack(JSContext* aCx) {
  JS::Rooted<JSObject*> stack(aCx);
  if (JS::CurrentGlobalOrNull(aCx) && !JS::CaptureCurrentStack(aCx, &stack)) {
    JS_ClearPendingException(aCx);
    return;
  }

  if (stack) {
    if (NS_IsMainThread()) {
      SerializeMainThreadOrWorkletStack(aCx, stack);
    } else {
      WorkerPrivate* currentWorker = GetCurrentThreadWorkerPrivate();
      SerializeWorkerStack(aCx, currentWorker, stack);
    }
  }
}

JSObject* SerializedStackHolder::ReadStack(JSContext* aCx) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mHolder.HasData()) {
    return nullptr;
  }

  JS::Rooted<JS::Value> stackValue(aCx);

  mHolder.Read(xpc::CurrentNativeGlobal(aCx), aCx, &stackValue, IgnoreErrors());

  return stackValue.isObject() ? &stackValue.toObject() : nullptr;
}

UniquePtr<SerializedStackHolder> GetCurrentStackForNetMonitor(JSContext* aCx) {
  MOZ_ASSERT_IF(!NS_IsMainThread(),
                GetCurrentThreadWorkerPrivate()->IsWatchedByDevTools());

  return GetCurrentStack(aCx);
}

UniquePtr<SerializedStackHolder> GetCurrentStack(JSContext* aCx) {
  UniquePtr<SerializedStackHolder> stack = MakeUnique<SerializedStackHolder>();
  stack->SerializeCurrentStack(aCx);
  return stack;
}

void NotifyNetworkMonitorAlternateStack(
    nsISupports* aChannel, UniquePtr<SerializedStackHolder> aStackHolder) {
  if (!aStackHolder) {
    return;
  }

  nsString stackString;
  ConvertSerializedStackToJSON(std::move(aStackHolder), stackString);

  if (!stackString.IsEmpty()) {
    NotifyNetworkMonitorAlternateStack(aChannel, stackString);
  }
}

void ConvertSerializedStackToJSON(UniquePtr<SerializedStackHolder> aStackHolder,
                                  nsAString& aStackString) {
  // We need a JSContext to be able to stringify the SavedFrame stack.
  // This will not run any scripts. A privileged scope is needed to fully
  // inspect all stack frames we find.
  AutoJSAPI jsapi;
  DebugOnly<bool> ok = jsapi.Init(xpc::PrivilegedJunkScope());
  JSContext* cx = jsapi.cx();

  JS::Rooted<JSObject*> savedFrame(cx, aStackHolder->ReadStack(cx));
  if (!savedFrame) {
    return;
  }

  JS::Rooted<JSObject*> converted(cx);
  converted = JS::ConvertSavedFrameToPlainObject(
      cx, savedFrame, JS::SavedFrameSelfHosted::Exclude);
  if (!converted) {
    JS_ClearPendingException(cx);
    return;
  }

  JS::Rooted<JS::Value> convertedValue(cx, JS::ObjectValue(*converted));
  if (!nsContentUtils::StringifyJSON(cx, convertedValue, aStackString,
                                     UndefinedIsNullStringLiteral)) {
    JS_ClearPendingException(cx);
    return;
  }
}

void NotifyNetworkMonitorAlternateStack(nsISupports* aChannel,
                                        const nsAString& aStackJSON) {
  nsCOMPtr<nsIObserverService> obsService = services::GetObserverService();
  if (!obsService) {
    return;
  }

  obsService->NotifyObservers(aChannel, "network-monitor-alternate-stack",
                              PromiseFlatString(aStackJSON).get());
}

}  // namespace mozilla::dom
