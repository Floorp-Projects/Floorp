/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_WorkerCommon_h
#define mozilla_dom_workers_WorkerCommon_h

#include "js/TypeDecls.h"

class nsPIDOMWindowInner;

namespace mozilla::dom {

class WorkerPrivate;

// All of these are implemented in RuntimeService.cpp

WorkerPrivate* GetWorkerPrivateFromContext(JSContext* aCx);

WorkerPrivate* GetCurrentThreadWorkerPrivate();

bool IsCurrentThreadRunningWorker();

bool IsCurrentThreadRunningChromeWorker();

JSContext* GetCurrentWorkerThreadJSContext();

JSObject* GetCurrentThreadWorkerGlobal();

JSObject* GetCurrentThreadWorkerDebuggerGlobal();

void CancelWorkersForWindow(const nsPIDOMWindowInner& aWindow);

void FreezeWorkersForWindow(const nsPIDOMWindowInner& aWindow);

void ThawWorkersForWindow(const nsPIDOMWindowInner& aWindow);

void SuspendWorkersForWindow(const nsPIDOMWindowInner& aWindow);

void ResumeWorkersForWindow(const nsPIDOMWindowInner& aWindow);

void PropagateStorageAccessPermissionGrantedToWorkers(
    const nsPIDOMWindowInner& aWindow);

// All of these are implemented in WorkerScope.cpp

bool IsWorkerGlobal(JSObject* global);

bool IsWorkerDebuggerGlobal(JSObject* global);

bool IsWorkerDebuggerSandbox(JSObject* object);

}  // namespace mozilla::dom

#endif  // mozilla_dom_workers_WorkerCommon_h
