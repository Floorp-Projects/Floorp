/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FuzzingInterface.h"

#include "jsapi.h"
#include "js/StructuredClone.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "mozilla/dom/SimpleGlobalObject.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/UniquePtr.h"

#include "nsCycleCollector.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;

JS::PersistentRooted<JSObject*> global;

static int FuzzingInitDomSC(int* argc, char*** argv) {
  JSObject* simpleGlobal =
      SimpleGlobalObject::Create(SimpleGlobalObject::GlobalType::BindingDetail);
  global.init(mozilla::dom::RootingCx());
  global.set(simpleGlobal);
  return 0;
}

static int FuzzingRunDomSC(const uint8_t* data, size_t size) {
  if (size < 8) {
    return 0;
  }

  AutoJSAPI jsapi;
  MOZ_RELEASE_ASSERT(jsapi.Init(global));

  JSContext* cx = jsapi.cx();
  auto gcGuard = mozilla::MakeScopeExit([&] {
    JS::PrepareForFullGC(cx);
    JS::NonIncrementalGC(cx, JS::GCOptions::Normal, JS::GCReason::API);
    nsCycleCollector_collect(CCReason::API, nullptr);
  });

  // The internals of SCInput have a release assert about the padding
  // of the data, so we fix it here to avoid performance problems
  // during fuzzing.
  size -= size % 8;

  StructuredCloneData scdata;
  if (!scdata.CopyExternalData(reinterpret_cast<const char*>(data), size)) {
    return 0;
  }

  JS::Rooted<JS::Value> result(cx);
  ErrorResult rv;
  scdata.Read(cx, &result, rv);

  rv.SuppressException();

  return 0;
}

MOZ_FUZZING_INTERFACE_RAW(FuzzingInitDomSC, FuzzingRunDomSC,
                          StructuredCloneReaderDOM);
