/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ScriptCache_h
#define ScriptCache_h

#include "mozilla/ScriptPreloader.h"
#include "mozilla/loader/PScriptCacheChild.h"
#include "mozilla/loader/PScriptCacheParent.h"

namespace mozilla {
namespace ipc {
class FileDescriptor;
}

namespace loader {

using mozilla::ipc::FileDescriptor;
using mozilla::ipc::IPCResult;

class ScriptCacheParent final : public PScriptCacheParent {
  friend class PScriptCacheParent;

 public:
  explicit ScriptCacheParent(bool wantCacheData)
      : mWantCacheData(wantCacheData) {}

 protected:
  IPCResult Recv__delete__(nsTArray<ScriptData>&& scripts);

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  bool mWantCacheData;
};

class ScriptCacheChild final : public PScriptCacheChild {
  friend class mozilla::ScriptPreloader;

 public:
  ScriptCacheChild() = default;

  void Init(const Maybe<FileDescriptor>& cacheFile, bool wantCacheData);

 protected:
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  void SendScriptsAndFinalize(ScriptPreloader::ScriptHash& scripts);

 private:
  bool mWantCacheData = false;
};

}  // namespace loader
}  // namespace mozilla

#endif  // ScriptCache_h
