/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MessageEventRunnable.h"

#include "mozilla/dom/MessageEvent.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/RootedDictionary.h"
#include "nsQueryObject.h"
#include "WorkerScope.h"

namespace mozilla::dom {

MessageEventRunnable::MessageEventRunnable(WorkerPrivate* aWorkerPrivate,
                                           Target aTarget)
    : WorkerDebuggeeRunnable(aWorkerPrivate, aTarget),
      StructuredCloneHolder(CloningSupported, TransferringSupported,
                            StructuredCloneScope::SameProcess) {}

bool MessageEventRunnable::DispatchDOMEvent(JSContext* aCx,
                                            WorkerPrivate* aWorkerPrivate,
                                            DOMEventTargetHelper* aTarget,
                                            bool aIsMainThread) {
  nsCOMPtr<nsIGlobalObject> parent = aTarget->GetParentObject();

  // For some workers without window, parent is null and we try to find it
  // from the JS Context.
  if (!parent) {
    JS::Rooted<JSObject*> globalObject(aCx, JS::CurrentGlobalOrNull(aCx));
    if (NS_WARN_IF(!globalObject)) {
      return false;
    }

    parent = xpc::NativeGlobal(globalObject);
    if (NS_WARN_IF(!parent)) {
      return false;
    }
  }

  MOZ_ASSERT(parent);

  JS::Rooted<JS::Value> messageData(aCx);
  IgnoredErrorResult rv;

  JS::CloneDataPolicy cloneDataPolicy;
  if (parent->GetClientInfo().isSome() &&
      parent->GetClientInfo()->AgentClusterId().isSome() &&
      parent->GetClientInfo()->AgentClusterId()->Equals(
          aWorkerPrivate->AgentClusterId())) {
    cloneDataPolicy.allowIntraClusterClonableSharedObjects();
  }

  if (aWorkerPrivate->IsSharedMemoryAllowed()) {
    cloneDataPolicy.allowSharedMemoryObjects();
  }

  Read(parent, aCx, &messageData, cloneDataPolicy, rv);

  if (NS_WARN_IF(rv.Failed())) {
    DispatchError(aCx, aTarget);
    return false;
  }

  Sequence<OwningNonNull<MessagePort>> ports;
  if (!TakeTransferredPortsAsSequence(ports)) {
    DispatchError(aCx, aTarget);
    return false;
  }

  RefPtr<MessageEvent> event = new MessageEvent(aTarget, nullptr, nullptr);
  event->InitMessageEvent(nullptr, u"message"_ns, CanBubble::eNo,
                          Cancelable::eNo, messageData, u""_ns, u""_ns, nullptr,
                          ports);

  event->SetTrusted(true);

  aTarget->DispatchEvent(*event);

  return true;
}

bool MessageEventRunnable::WorkerRun(JSContext* aCx,
                                     WorkerPrivate* aWorkerPrivate) {
  if (mTarget == ParentThread) {
    // Don't fire this event if the JS object has been disconnected from the
    // private object.
    if (!aWorkerPrivate->IsAcceptingEvents()) {
      return true;
    }

    aWorkerPrivate->AssertInnerWindowIsCorrect();

    return DispatchDOMEvent(aCx, aWorkerPrivate,
                            aWorkerPrivate->ParentEventTargetRef(),
                            !aWorkerPrivate->GetParent());
  }

  MOZ_ASSERT(aWorkerPrivate == GetWorkerPrivateFromContext(aCx));
  MOZ_ASSERT(aWorkerPrivate->GlobalScope());

  // If the worker start shutting down, don't dispatch the message event.
  if (NS_FAILED(
          aWorkerPrivate->GlobalScope()->CheckCurrentGlobalCorrectness())) {
    return true;
  }

  return DispatchDOMEvent(aCx, aWorkerPrivate, aWorkerPrivate->GlobalScope(),
                          false);
}

void MessageEventRunnable::DispatchError(JSContext* aCx,
                                         DOMEventTargetHelper* aTarget) {
  RootedDictionary<MessageEventInit> init(aCx);
  init.mBubbles = false;
  init.mCancelable = false;

  RefPtr<Event> event =
      MessageEvent::Constructor(aTarget, u"messageerror"_ns, init);
  event->SetTrusted(true);

  aTarget->DispatchEvent(*event);
}

}  // namespace mozilla::dom
