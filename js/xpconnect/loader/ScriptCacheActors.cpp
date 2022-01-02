/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ScriptPreloader.h"
#include "ScriptPreloader-inl.h"
#include "mozilla/loader/ScriptCacheActors.h"

#include "mozilla/dom/ContentParent.h"

namespace mozilla {
namespace loader {

void ScriptCacheChild::Init(const Maybe<FileDescriptor>& cacheFile,
                            bool wantCacheData) {
  mWantCacheData = wantCacheData;

  auto& cache = ScriptPreloader::GetChildSingleton();
  Unused << cache.InitCache(cacheFile, this);

  if (!wantCacheData) {
    // If the parent process isn't expecting any cache data from us, we're
    // done.
    Send__delete__(this, AutoTArray<ScriptData, 0>());
  }
}

// Finalize the script cache for the content process, and send back data about
// any scripts executed up to this point.
void ScriptCacheChild::SendScriptsAndFinalize(
    ScriptPreloader::ScriptHash& scripts) {
  MOZ_ASSERT(mWantCacheData);

  AutoSafeJSAPI jsapi;

  auto matcher = ScriptPreloader::Match<ScriptPreloader::ScriptStatus::Saved>();

  nsTArray<ScriptData> dataArray;
  for (auto& script : IterHash(scripts, matcher)) {
    if (!script->mSize && !script->XDREncode(jsapi.cx())) {
      continue;
    }

    auto data = dataArray.AppendElement();

    data->url() = script->mURL;
    data->cachePath() = script->mCachePath;
    data->loadTime() = script->mLoadTime;

    if (script->HasBuffer()) {
      auto& xdrData = script->Buffer();
      data->xdrData().AppendElements(xdrData.begin(), xdrData.length());
      script->FreeData();
    }
  }

  Send__delete__(this, dataArray);
}

void ScriptCacheChild::ActorDestroy(ActorDestroyReason aWhy) {
  auto& cache = ScriptPreloader::GetChildSingleton();
  cache.mChildActor = nullptr;
}

IPCResult ScriptCacheParent::Recv__delete__(nsTArray<ScriptData>&& scripts) {
  if (!mWantCacheData && scripts.Length()) {
    return IPC_FAIL(this, "UnexpectedScriptData");
  }

  // We don't want any more data from the process at this point.
  mWantCacheData = false;

  // Merge the child's script data with the parent's.
  auto parent = static_cast<dom::ContentParent*>(Manager());
  auto processType =
      ScriptPreloader::GetChildProcessType(parent->GetRemoteType());

  auto& cache = ScriptPreloader::GetChildSingleton();
  for (auto& script : scripts) {
    cache.NoteStencil(script.url(), script.cachePath(), processType,
                      std::move(script.xdrData()), script.loadTime());
  }

  return IPC_OK();
}

void ScriptCacheParent::ActorDestroy(ActorDestroyReason aWhy) {}

}  // namespace loader
}  // namespace mozilla
