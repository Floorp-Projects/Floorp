/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_actorsparent_h__
#define mozilla_dom_indexeddb_actorsparent_h__

#include "nscore.h"

#include <stdint.h>

#include "mozilla/dom/indexedDB/PermissionRequestBase.h"
#include "mozilla/dom/PBrowserParent.h"

template <class>
struct already_AddRefed;
class nsIPrincipal;

namespace mozilla {
namespace dom {

class Element;
class FileHandleThreadPool;

namespace quota {

class Client;

}  // namespace quota

namespace indexedDB {

class FileManager;
class LoggingInfo;
class PBackgroundIDBFactoryParent;
class PBackgroundIndexedDBUtilsParent;

PBackgroundIDBFactoryParent* AllocPBackgroundIDBFactoryParent(
    const LoggingInfo& aLoggingInfo);

bool RecvPBackgroundIDBFactoryConstructor(PBackgroundIDBFactoryParent* aActor,
                                          const LoggingInfo& aLoggingInfo);

bool DeallocPBackgroundIDBFactoryParent(PBackgroundIDBFactoryParent* aActor);

PBackgroundIndexedDBUtilsParent* AllocPBackgroundIndexedDBUtilsParent();

bool DeallocPBackgroundIndexedDBUtilsParent(
    PBackgroundIndexedDBUtilsParent* aActor);

bool RecvFlushPendingFileDeletions();

already_AddRefed<mozilla::dom::quota::Client> CreateQuotaClient();

FileHandleThreadPool* GetFileHandleThreadPool();

class PermissionRequestHelper final : public PermissionRequestBase {
 public:
  PermissionRequestHelper(
      Element* aOwnerElement, nsIPrincipal* aPrincipal,
      PBrowserParent::IndexedDBPermissionRequestResolver& aResolver)
      : PermissionRequestBase(aOwnerElement, aPrincipal),
        mResolver(aResolver) {}

 protected:
  ~PermissionRequestHelper() override = default;

 private:
  PBrowserParent::IndexedDBPermissionRequestResolver mResolver;

  void OnPromptComplete(PermissionValue aPermissionValue) override;
};

nsresult AsyncDeleteFile(FileManager* aFileManager, int64_t aFileId);

}  // namespace indexedDB
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_indexeddb_actorsparent_h__
