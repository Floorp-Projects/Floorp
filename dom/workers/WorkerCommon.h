/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_WorkerCommon_h
#define mozilla_dom_workers_WorkerCommon_h

#include "jsapi.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class WorkerPrivate;

// All of these are implemented in RuntimeService.cpp

WorkerPrivate*
GetWorkerPrivateFromContext(JSContext* aCx);

WorkerPrivate*
GetCurrentThreadWorkerPrivate();

bool
IsCurrentThreadRunningWorker();

bool
IsCurrentThreadRunningChromeWorker();

JSContext*
GetCurrentWorkerThreadJSContext();

JSObject*
GetCurrentThreadWorkerGlobal();

void
CancelWorkersForWindow(nsPIDOMWindowInner* aWindow);

void
FreezeWorkersForWindow(nsPIDOMWindowInner* aWindow);

void
ThawWorkersForWindow(nsPIDOMWindowInner* aWindow);

void
SuspendWorkersForWindow(nsPIDOMWindowInner* aWindow);

void
ResumeWorkersForWindow(nsPIDOMWindowInner* aWindow);

// All of these are implemented in WorkerScope.cpp

bool
IsWorkerGlobal(JSObject* global);

bool
IsWorkerDebuggerGlobal(JSObject* global);

bool
IsWorkerDebuggerSandbox(JSObject* object);

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_workers_WorkerCommon_h
