/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkletGlobalScope.h"
#include "mozilla/dom/WorkletGlobalScopeBinding.h"
#include "mozilla/dom/WorkletImpl.h"
#include "mozilla/dom/WorkletThread.h"
#include "mozilla/dom/Console.h"
#include "nsContentUtils.h"
#include "nsJSUtils.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(WorkletGlobalScope)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(WorkletGlobalScope)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mConsole)
  tmp->UnlinkObjectsInGlobal();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(WorkletGlobalScope)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mConsole)
  tmp->TraverseObjectsInGlobal(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(WorkletGlobalScope)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WorkletGlobalScope)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WorkletGlobalScope)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
  NS_INTERFACE_MAP_ENTRY(WorkletGlobalScope)
NS_INTERFACE_MAP_END

WorkletGlobalScope::WorkletGlobalScope(WorkletImpl* aImpl)
    : mImpl(aImpl), mCreationTimeStamp(TimeStamp::Now()) {}

WorkletGlobalScope::~WorkletGlobalScope() = default;

JSObject* WorkletGlobalScope::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto) {
  MOZ_CRASH("We should never get here!");
  return nullptr;
}

already_AddRefed<Console> WorkletGlobalScope::GetConsole(JSContext* aCx,
                                                         ErrorResult& aRv) {
  if (!mConsole) {
    MOZ_ASSERT(Impl());
    const WorkletLoadInfo& loadInfo = Impl()->LoadInfo();
    mConsole = Console::CreateForWorklet(aCx, this, loadInfo.OuterWindowID(),
                                         loadInfo.InnerWindowID(), aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  }

  RefPtr<Console> console = mConsole;
  return console.forget();
}

OriginTrials WorkletGlobalScope::Trials() const { return mImpl->Trials(); }

Maybe<nsID> WorkletGlobalScope::GetAgentClusterId() const {
  return mImpl->GetAgentClusterId();
}

bool WorkletGlobalScope::IsSharedMemoryAllowed() const {
  return mImpl->IsSharedMemoryAllowed();
}

void WorkletGlobalScope::Dump(const Optional<nsAString>& aString) const {
  WorkletThread::AssertIsOnWorkletThread();

  if (!nsJSUtils::DumpEnabled()) {
    return;
  }

  if (!aString.WasPassed()) {
    return;
  }

  NS_ConvertUTF16toUTF8 str(aString.Value());

  MOZ_LOG(nsContentUtils::DOMDumpLog(), mozilla::LogLevel::Debug,
          ("[Worklet.Dump] %s", str.get()));
#ifdef ANDROID
  __android_log_print(ANDROID_LOG_INFO, "Gecko", "%s", str.get());
#endif
  fputs(str.get(), stdout);
  fflush(stdout);
}

}  // namespace mozilla::dom
