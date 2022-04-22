/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebTaskController.h"
#include "TaskSignal.h"

#include "mozilla/dom/TaskPriorityChangeEvent.h"

namespace mozilla::dom {

WebTaskController::WebTaskController(nsIGlobalObject* aGlobal,
                                     TaskPriority aPriority)
    : AbortController(aGlobal) {
  MOZ_ASSERT(!mSignal);
  mSignal = new TaskSignal(aGlobal, aPriority);
}

void WebTaskController::SetPriority(TaskPriority aPriority, ErrorResult& aRv) {
  // https://wicg.github.io/scheduling-apis/#tasksignal-signal-priority-change
  RefPtr<TaskSignal> taskSignal = static_cast<TaskSignal*>(mSignal.get());
  MOZ_ASSERT(taskSignal);
  if (taskSignal->PriorityChanging()) {
    aRv.ThrowNotAllowedError("Signal's priority changing is true");
    return;
  }

  if (taskSignal->Priority() == aPriority) {
    return;
  }

  taskSignal->SetPriorityChanging(true);

  TaskPriority previousPriority = taskSignal->Priority();
  taskSignal->SetPriority(aPriority);

  taskSignal->RunPriorityChangeAlgorithms();

  TaskPriorityChangeEventInit init;
  init.mPreviousPriority = previousPriority;
  RefPtr<TaskPriorityChangeEvent> event = TaskPriorityChangeEvent::Constructor(
      taskSignal, u"prioritychange"_ns, init);

  event->SetTrusted(true);

  taskSignal->DispatchEvent(*event);
  taskSignal->SetPriorityChanging(false);
}

already_AddRefed<WebTaskController> WebTaskController::Constructor(
    const GlobalObject& aGlobal, const TaskControllerInit& aInit,
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  RefPtr<WebTaskController> webTaskController =
      new WebTaskController(global, aInit.mPriority);
  return webTaskController.forget();
}

JSObject* WebTaskController::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return TaskController_Binding::Wrap(aCx, this, aGivenProto);
}
}  // namespace mozilla::dom
