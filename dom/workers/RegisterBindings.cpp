/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerPrivate.h"
#include "ChromeWorkerScope.h"

#include "jsapi.h"
#include "mozilla/dom/DebuggerNotificationObserverBinding.h"
#include "mozilla/dom/RegisterWorkerBindings.h"
#include "mozilla/dom/RegisterWorkerDebuggerBindings.h"

using namespace mozilla::dom;

bool WorkerPrivate::RegisterBindings(JSContext* aCx,
                                     JS::Handle<JSObject*> aGlobal) {
  // Init Web IDL bindings
  if (!RegisterWorkerBindings(aCx, aGlobal)) {
    return false;
  }

  if (IsChromeWorker()) {
    if (!DefineChromeWorkerFunctions(aCx, aGlobal)) {
      return false;
    }
  }

  return true;
}

bool WorkerPrivate::RegisterDebuggerBindings(JSContext* aCx,
                                             JS::Handle<JSObject*> aGlobal) {
  // Init Web IDL bindings
  if (!RegisterWorkerDebuggerBindings(aCx, aGlobal)) {
    return false;
  }

  if (!ChromeUtils_Binding::GetConstructorObject(aCx)) {
    return false;
  }

  if (!DebuggerNotificationObserver_Binding::GetConstructorObject(aCx)) {
    return false;
  }

  if (!JS_DefineDebuggerObject(aCx, aGlobal)) {
    return false;
  }

  return true;
}
