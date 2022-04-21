/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebTaskController_h
#define mozilla_dom_WebTaskController_h

#include "nsWrapperCache.h"

#include "mozilla/dom/WebTaskSchedulingBinding.h"
#include "mozilla/dom/AbortController.h"

namespace mozilla::dom {
class WebTaskController : public AbortController {
 public:
  explicit WebTaskController(nsIGlobalObject* aGlobal, TaskPriority aPriority);

  static already_AddRefed<WebTaskController> Constructor(
      const GlobalObject& aGlobal, const TaskControllerInit& aInit,
      ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  void SetPriority(TaskPriority aPriority, ErrorResult& aRv);

 private:
  ~WebTaskController() = default;
};
}  // namespace mozilla::dom

#endif
