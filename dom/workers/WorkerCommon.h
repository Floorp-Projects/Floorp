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

namespace workers {

// All of these are implemented in RuntimeService.cpp

#ifdef DEBUG
void
AssertIsOnMainThread();
#else
inline void
AssertIsOnMainThread()
{ }
#endif

WorkerPrivate*
GetWorkerPrivateFromContext(JSContext* aCx);

WorkerPrivate*
GetCurrentThreadWorkerPrivate();

bool
IsCurrentThreadRunningChromeWorker();

JSContext*
GetCurrentThreadJSContext();

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
IsDebuggerGlobal(JSObject* global);

bool
IsDebuggerSandbox(JSObject* object);

} // workers namespace
} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_workers_WorkerCommon_h
