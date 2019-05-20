/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SerializedStackHolder.h"

#include "js/SavedFrameAPI.h"
#include "mozilla/dom/WorkerPrivate.h"

namespace mozilla {
namespace dom {

SerializedStackHolder::SerializedStackHolder()
  : mHolder(StructuredCloneHolder::CloningSupported,
            StructuredCloneHolder::TransferringNotSupported,
            StructuredCloneHolder::StructuredCloneScope::SameProcessDifferentThread) {}

void SerializedStackHolder::WriteStack(JSContext* aCx,
                                       JS::HandleObject aStack) {
  JS::RootedValue stackValue(aCx, JS::ObjectValue(*aStack));
  mHolder.Write(aCx, stackValue, IgnoreErrors());

  // StructuredCloneHolder::Write can leave a pending exception on the context.
  JS_ClearPendingException(aCx);
}

void SerializedStackHolder::SerializeMainThreadStack(JSContext* aCx,
                                                     JS::HandleObject aStack) {
  MOZ_ASSERT(NS_IsMainThread());
  WriteStack(aCx, aStack);
}

void SerializedStackHolder::SerializeWorkerStack(JSContext* aCx,
                                                 WorkerPrivate* aWorkerPrivate,
                                                 JS::HandleObject aStack) {
  MOZ_ASSERT(aWorkerPrivate->IsOnCurrentThread());

  RefPtr<StrongWorkerRef> workerRef = StrongWorkerRef::Create(
      aWorkerPrivate, "WorkerErrorReport");
  if (workerRef) {
    mWorkerRef = new ThreadSafeWorkerRef(workerRef);
  } else {
    // Don't write the stack if we can't create a ref to the worker.
    return;
  }

  WriteStack(aCx, aStack);
}

void SerializedStackHolder::SerializeCurrentStack(JSContext* aCx) {
  JS::RootedObject stack(aCx);
  if (JS::CurrentGlobalOrNull(aCx) && !JS::CaptureCurrentStack(aCx, &stack)) {
    JS_ClearPendingException(aCx);
    return;
  }

  if (stack) {
    if (NS_IsMainThread()) {
      SerializeMainThreadStack(aCx, stack);
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

  Maybe<nsJSPrincipals::AutoSetActiveWorkerPrincipal> set;
  if (mWorkerRef) {
    set.emplace(mWorkerRef->Private()->GetPrincipal());
  }

  JS::RootedValue stackValue(aCx);
  mHolder.Read(xpc::CurrentNativeGlobal(aCx), aCx, &stackValue,
               IgnoreErrors());
  return stackValue.isObject() ? &stackValue.toObject() : nullptr;
}

UniquePtr<SerializedStackHolder> GetCurrentStackForNetMonitor(JSContext* aCx) {
  MOZ_ASSERT_IF(!NS_IsMainThread(),
                GetCurrentThreadWorkerPrivate()->IsWatchedByDevtools());

  UniquePtr<SerializedStackHolder> stack = MakeUnique<SerializedStackHolder>();
  stack->SerializeCurrentStack(aCx);
  return stack;
}

void NotifyNetworkMonitorAlternateStack(nsISupports* aChannel,
                                        UniquePtr<SerializedStackHolder> aStackHolder) {
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

  JS::RootedObject savedFrame(cx, aStackHolder->ReadStack(cx));
  if (!savedFrame) {
    return;
  }

  JS::RootedObject converted(cx);
  converted = JS::ConvertSavedFrameToPlainObject(cx, savedFrame,
                                                 JS::SavedFrameSelfHosted::Exclude);
  if (!converted) {
    JS_ClearPendingException(cx);
    return;
  }

  JS::RootedValue convertedValue(cx, JS::ObjectValue(*converted));
  if (!nsContentUtils::StringifyJSON(cx, &convertedValue, aStackString)) {
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

}  // namespace dom
}  // namespace mozilla
