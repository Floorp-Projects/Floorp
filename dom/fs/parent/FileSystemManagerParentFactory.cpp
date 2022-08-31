/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemManagerParentFactory.h"

#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/dom/FileSystemDataManager.h"
#include "mozilla/dom/FileSystemManagerParent.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/ipc/Endpoint.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsString.h"

namespace mozilla {
LazyLogModule gOPFSLog("OPFS");
}

#define LOG(args) MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Verbose, args)

#define LOG_DEBUG(args) \
  MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Debug, args)

namespace mozilla::dom {

namespace fs::data {

EntryId GetRootHandle(const Origin& aOrigin) { return "not implemented"_ns; }

}  // namespace fs::data

mozilla::ipc::IPCResult CreateFileSystemManagerParent(
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
    mozilla::ipc::Endpoint<PFileSystemManagerParent>&& aParentEndpoint,
    std::function<void(const nsresult&)>&& aResolver) {
  QM_TRY(OkIf(StaticPrefs::dom_fs_enabled()), IPC_OK(),
         [aResolver](const auto&) { aResolver(NS_ERROR_DOM_NOT_ALLOWED_ERR); });

  QM_TRY(OkIf(aParentEndpoint.IsValid()), IPC_OK(),
         [aResolver](const auto&) { aResolver(NS_ERROR_INVALID_ARG); });

  nsAutoCString origin =
      quota::QuotaManager::GetOriginFromValidatedPrincipalInfo(aPrincipalInfo);

  // This creates the file system data manager, which has to be done on
  // PBackground
  QM_TRY_UNWRAP(
      RefPtr<fs::data::FileSystemDataManager> dataManager,
      fs::data::FileSystemDataManager::GetOrCreateFileSystemDataManager(origin),
      IPC_OK(), [aResolver](const auto& aRv) { aResolver(aRv); });

  fs::EntryId rootId = fs::data::GetRootHandle(origin);

  InvokeAsync(dataManager->MutableIOTargetPtr(), __func__,
              [dataManager = dataManager, rootId,
               parentEndpoint = std::move(aParentEndpoint)]() mutable {
                RefPtr<FileSystemManagerParent> parent =
                    new FileSystemManagerParent(std::move(dataManager), rootId);
                if (!parentEndpoint.Bind(parent)) {
                  return BoolPromise::CreateAndReject(NS_ERROR_FAILURE,
                                                      __func__);
                }
                return BoolPromise::CreateAndResolve(true, __func__);
              })
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [aResolver](const BoolPromise::ResolveOrRejectValue& aValue) {
               if (aValue.IsReject()) {
                 aResolver(aValue.RejectValue());
               } else {
                 aResolver(NS_OK);
               }
             });

  return IPC_OK();
}

}  // namespace mozilla::dom
