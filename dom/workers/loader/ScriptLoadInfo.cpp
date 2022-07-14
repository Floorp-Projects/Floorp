/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptLoadInfo.h"
#include "CacheLoadHandler.h"  // CacheCreator refptr

namespace mozilla {
namespace dom {

ScriptLoadInfo::ScriptLoadInfo() {
  MOZ_ASSERT(mScriptIsUTF8 == false, "set by member initializer");
  MOZ_ASSERT(mScriptLength == 0, "set by member initializer");
  mScript.mUTF16 = nullptr;
}

ScriptLoadInfo::~ScriptLoadInfo() {
  if (void* data = mScriptIsUTF8 ? static_cast<void*>(mScript.mUTF8)
                                 : static_cast<void*>(mScript.mUTF16)) {
    js_free(data);
  }
}

void ScriptLoadInfo::SetCacheCreator(
    RefPtr<workerinternals::loader::CacheCreator> aCacheCreator) {
  AssertIsOnMainThread();
  mCacheCreator = aCacheCreator;
}

void ScriptLoadInfo::ClearCacheCreator() {
  AssertIsOnMainThread();
  mCacheCreator = nullptr;
}

RefPtr<workerinternals::loader::CacheCreator>
ScriptLoadInfo::GetCacheCreator() {
  return mCacheCreator;
}

}  // namespace dom
}  // namespace mozilla
