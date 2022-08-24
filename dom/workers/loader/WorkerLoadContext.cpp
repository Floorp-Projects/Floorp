/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerLoadContext.h"
#include "CacheLoadHandler.h"  // CacheCreator refptr

namespace mozilla {
namespace dom {

WorkerLoadContext::WorkerLoadContext(Kind aKind,
                                     const Maybe<ClientInfo>& aClientInfo)
    : JS::loader::LoadContextBase(JS::loader::ContextKind::Worker),
      mKind(aKind),
      mClientInfo(aClientInfo){};

void WorkerLoadContext::SetCacheCreator(
    RefPtr<workerinternals::loader::CacheCreator> aCacheCreator) {
  AssertIsOnMainThread();
  mCacheCreator = aCacheCreator;
}

void WorkerLoadContext::ClearCacheCreator() {
  AssertIsOnMainThread();
  mCacheCreator = nullptr;
}

RefPtr<workerinternals::loader::CacheCreator>
WorkerLoadContext::GetCacheCreator() {
  return mCacheCreator;
}

}  // namespace dom
}  // namespace mozilla
