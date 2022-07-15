/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_actorsparent_h__
#define mozilla_dom_indexeddb_actorsparent_h__

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/dom/PBrowserParent.h"
#include "mozilla/RefPtr.h"
#include "nsIPermissionManager.h"

class nsIPrincipal;

namespace mozilla::dom {

class Element;
class FileHandleThreadPool;

namespace quota {

class Client;

}  // namespace quota

namespace indexedDB {

enum class PermissionValue {
  kPermissionAllowed = nsIPermissionManager::ALLOW_ACTION,
  kPermissionDenied = nsIPermissionManager::DENY_ACTION,
  kPermissionPrompt = nsIPermissionManager::PROMPT_ACTION
};

class LoggingInfo;
class PBackgroundIDBFactoryParent;
class PBackgroundIndexedDBUtilsParent;

already_AddRefed<PBackgroundIDBFactoryParent> AllocPBackgroundIDBFactoryParent(
    const LoggingInfo& aLoggingInfo);

bool RecvPBackgroundIDBFactoryConstructor(PBackgroundIDBFactoryParent* aActor,
                                          const LoggingInfo& aLoggingInfo);

bool DeallocPBackgroundIDBFactoryParent(PBackgroundIDBFactoryParent* aActor);

PBackgroundIndexedDBUtilsParent* AllocPBackgroundIndexedDBUtilsParent();

bool DeallocPBackgroundIndexedDBUtilsParent(
    PBackgroundIndexedDBUtilsParent* aActor);

bool RecvFlushPendingFileDeletions();

RefPtr<mozilla::dom::quota::Client> CreateQuotaClient();

FileHandleThreadPool* GetFileHandleThreadPool();

}  // namespace indexedDB
}  // namespace mozilla::dom

#endif  // mozilla_dom_indexeddb_actorsparent_h__
