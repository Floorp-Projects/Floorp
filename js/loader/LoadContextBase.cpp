/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ScriptLoadContext.h"
#include "mozilla/loader/ComponentModuleLoader.h"
#include "mozilla/dom/WorkerLoadContext.h"
#include "mozilla/dom/worklet/WorkletModuleLoader.h"  // WorkletLoadContext
#include "js/loader/LoadContextBase.h"
#include "js/loader/ScriptLoadRequest.h"

namespace JS::loader {

////////////////////////////////////////////////////////////////
// LoadContextBase
////////////////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(LoadContextBase)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(LoadContextBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(LoadContextBase)

NS_IMPL_CYCLE_COLLECTION(LoadContextBase, mRequest)

LoadContextBase::LoadContextBase(ContextKind kind) : mKind(kind) {}

void LoadContextBase::SetRequest(ScriptLoadRequest* aRequest) {
  MOZ_ASSERT(!mRequest);
  mRequest = aRequest;
}

void LoadContextBase::GetProfilerLabel(nsACString& aOutString) {
  aOutString.Append("Unknown Script Element");
}

mozilla::dom::ScriptLoadContext* LoadContextBase::AsWindowContext() {
  MOZ_ASSERT(IsWindowContext());
  return static_cast<mozilla::dom::ScriptLoadContext*>(this);
}

mozilla::loader::ComponentLoadContext* LoadContextBase::AsComponentContext() {
  MOZ_ASSERT(IsComponentContext());
  return static_cast<mozilla::loader::ComponentLoadContext*>(this);
}

mozilla::dom::WorkerLoadContext* LoadContextBase::AsWorkerContext() {
  MOZ_ASSERT(IsWorkerContext());
  return static_cast<mozilla::dom::WorkerLoadContext*>(this);
}

mozilla::dom::WorkletLoadContext* LoadContextBase::AsWorkletContext() {
  MOZ_ASSERT(IsWorkletContext());
  return static_cast<mozilla::dom::WorkletLoadContext*>(this);
}

}  // namespace JS::loader
