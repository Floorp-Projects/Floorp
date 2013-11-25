/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerPrivate.h"
#include "ChromeWorkerScope.h"
#include "File.h"
#include "RuntimeService.h"

#include "jsapi.h"
#include "js/OldDebugAPI.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DOMExceptionBinding.h"
#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/EventHandlerBinding.h"
#include "mozilla/dom/EventTargetBinding.h"
#include "mozilla/dom/FileReaderSyncBinding.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/dom/ImageDataBinding.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/PromiseBinding.h"
#include "mozilla/dom/TextDecoderBinding.h"
#include "mozilla/dom/TextEncoderBinding.h"
#include "mozilla/dom/XMLHttpRequestBinding.h"
#include "mozilla/dom/XMLHttpRequestUploadBinding.h"
#include "mozilla/dom/URLBinding.h"
#include "mozilla/dom/WorkerBinding.h"
#include "mozilla/dom/WorkerLocationBinding.h"
#include "mozilla/dom/WorkerNavigatorBinding.h"
#include "mozilla/OSFileConstants.h"

USING_WORKERS_NAMESPACE
using namespace mozilla::dom;

bool
WorkerPrivate::RegisterBindings(JSContext* aCx, JS::Handle<JSObject*> aGlobal)
{
  JS::Rooted<JSObject*> eventTargetProto(aCx,
    EventTargetBinding::GetProtoObject(aCx, aGlobal));
  if (!eventTargetProto) {
    return false;
  }

  if (IsChromeWorker()) {
    if (!ChromeWorkerBinding::GetConstructorObject(aCx, aGlobal) ||
        !DefineChromeWorkerFunctions(aCx, aGlobal) ||
        !DefineOSFileConstants(aCx, aGlobal)) {
      return false;
    }
  }

  // Init other classes we care about.
  if (!file::InitClasses(aCx, aGlobal)) {
    return false;
  }

  // Init other paris-bindings.
  if (!DOMExceptionBinding::GetConstructorObject(aCx, aGlobal) ||
      !EventBinding::GetConstructorObject(aCx, aGlobal) ||
      !FileReaderSyncBinding_workers::GetConstructorObject(aCx, aGlobal) ||
      !ImageDataBinding::GetConstructorObject(aCx, aGlobal) ||
      !MessageEventBinding::GetConstructorObject(aCx, aGlobal) ||
      !MessagePortBinding::GetConstructorObject(aCx, aGlobal) ||
      (PromiseEnabled() &&
        !PromiseBinding::GetConstructorObject(aCx, aGlobal)) ||
      !TextDecoderBinding::GetConstructorObject(aCx, aGlobal) ||
      !TextEncoderBinding::GetConstructorObject(aCx, aGlobal) ||
      !XMLHttpRequestBinding_workers::GetConstructorObject(aCx, aGlobal) ||
      !XMLHttpRequestUploadBinding_workers::GetConstructorObject(aCx, aGlobal) ||
      !URLBinding_workers::GetConstructorObject(aCx, aGlobal) ||
      !WorkerBinding::GetConstructorObject(aCx, aGlobal) ||
      !WorkerLocationBinding_workers::GetConstructorObject(aCx, aGlobal) ||
      !WorkerNavigatorBinding_workers::GetConstructorObject(aCx, aGlobal)) {
    return false;
  }

  if (!JS_DefineProfilingFunctions(aCx, aGlobal)) {
    return false;
  }

  return true;
}
