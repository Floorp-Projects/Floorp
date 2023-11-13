/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "EventWithOptionsRunnable.h"
#include "WorkerScope.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "js/StructuredClone.h"
#include "js/RootingAPI.h"
#include "js/Value.h"
#include "nsJSPrincipals.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "nsGlobalWindowInner.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/ErrorResult.h"
#include "nsIGlobalObject.h"
#include "nsCOMPtr.h"
#include "js/GlobalObject.h"
#include "xpcpublic.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/WorkerCommon.h"

namespace mozilla::dom {
EventWithOptionsRunnable::EventWithOptionsRunnable(Worker& aWorker)
    : WorkerDebuggeeRunnable(aWorker.mWorkerPrivate,
                             WorkerRunnable::WorkerThread),
      StructuredCloneHolder(CloningSupported, TransferringSupported,
                            StructuredCloneScope::SameProcess) {}

EventWithOptionsRunnable::~EventWithOptionsRunnable() = default;

void EventWithOptionsRunnable::InitOptions(
    JSContext* aCx, JS::Handle<JS::Value> aOptions,
    const Sequence<JSObject*>& aTransferable, ErrorResult& aRv) {
  JS::Rooted<JS::Value> transferable(aCx, JS::UndefinedValue());

  aRv = nsContentUtils::CreateJSValueFromSequenceOfObject(aCx, aTransferable,
                                                          &transferable);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  JS::CloneDataPolicy clonePolicy;
  // DedicatedWorkers are always part of the same agent cluster.
  clonePolicy.allowIntraClusterClonableSharedObjects();

  MOZ_ASSERT(NS_IsMainThread());
  nsGlobalWindowInner* win = nsContentUtils::IncumbentInnerWindow();
  if (win && win->IsSharedMemoryAllowed()) {
    clonePolicy.allowSharedMemoryObjects();
  }

  Write(aCx, aOptions, transferable, clonePolicy, aRv);
}

// Cargo-culted from MesssageEventRunnable.
bool EventWithOptionsRunnable::BuildAndFireEvent(
    JSContext* aCx, WorkerPrivate* aWorkerPrivate,
    DOMEventTargetHelper* aTarget) {
  IgnoredErrorResult rv;
  nsCOMPtr<nsIGlobalObject> parent = aTarget->GetParentObject();

  // For some workers without window, parent is null and we try to find it
  // from the JS Context.
  if (!parent) {
    JS::Rooted<JSObject*> globalObject(aCx, JS::CurrentGlobalOrNull(aCx));
    if (NS_WARN_IF(!globalObject)) {
      rv.ThrowDataCloneError("failed to get global object");
      OptionsDeserializeFailed(rv);
      return false;
    }

    parent = xpc::NativeGlobal(globalObject);
    if (NS_WARN_IF(!parent)) {
      rv.ThrowDataCloneError("failed to get parent");
      OptionsDeserializeFailed(rv);
      return false;
    }
  }

  MOZ_ASSERT(parent);

  JS::Rooted<JS::Value> options(aCx);

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

  Read(parent, aCx, &options, cloneDataPolicy, rv);

  if (NS_WARN_IF(rv.Failed())) {
    OptionsDeserializeFailed(rv);
    return false;
  }

  Sequence<OwningNonNull<MessagePort>> ports;
  if (NS_WARN_IF(!TakeTransferredPortsAsSequence(ports))) {
    // TODO: Is this an appropriate type? What does this actually do?
    rv.ThrowDataCloneError("TakeTransferredPortsAsSequence failed");
    OptionsDeserializeFailed(rv);
    return false;
  }

  RefPtr<dom::Event> event = BuildEvent(aCx, parent, aTarget, options);

  if (NS_WARN_IF(!event)) {
    return false;
  }

  aTarget->DispatchEvent(*event);
  return true;
}

bool EventWithOptionsRunnable::WorkerRun(JSContext* aCx,
                                         WorkerPrivate* aWorkerPrivate) {
  if (mTarget == ParentThread) {
    // Don't fire this event if the JS object has been disconnected from the
    // private object.
    if (!aWorkerPrivate->IsAcceptingEvents()) {
      return true;
    }

    // Once a window has frozen its workers, their
    // mMainThreadDebuggeeEventTargets should be paused, and their
    // WorkerDebuggeeRunnables should not be being executed. The same goes for
    // WorkerDebuggeeRunnables sent from child to parent workers, but since a
    // frozen parent worker runs only control runnables anyway, that is taken
    // care of naturally.
    MOZ_ASSERT(!aWorkerPrivate->IsFrozen());

    // Similarly for paused windows; all its workers should have been informed.
    // (Subworkers are unaffected by paused windows.)
    MOZ_ASSERT(!aWorkerPrivate->IsParentWindowPaused());

    aWorkerPrivate->AssertInnerWindowIsCorrect();

    return BuildAndFireEvent(aCx, aWorkerPrivate,
                             aWorkerPrivate->ParentEventTargetRef());
  }

  MOZ_ASSERT(aWorkerPrivate == GetWorkerPrivateFromContext(aCx));
  MOZ_ASSERT(aWorkerPrivate->GlobalScope());

  // If the worker start shutting down, don't dispatch the event.
  if (NS_FAILED(
          aWorkerPrivate->GlobalScope()->CheckCurrentGlobalCorrectness())) {
    return true;
  }

  return BuildAndFireEvent(aCx, aWorkerPrivate, aWorkerPrivate->GlobalScope());
}

}  // namespace mozilla::dom
