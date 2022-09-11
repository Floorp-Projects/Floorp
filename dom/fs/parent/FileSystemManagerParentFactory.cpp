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
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/ipc/Endpoint.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsString.h"

namespace mozilla {
extern LazyLogModule gOPFSLog;
}

#define LOG(args) MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Verbose, args)

#define LOG_DEBUG(args) \
  MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Debug, args)

namespace mozilla::dom {
mozilla::ipc::IPCResult CreateFileSystemManagerParent(
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
    mozilla::ipc::Endpoint<PFileSystemManagerParent>&& aParentEndpoint,
    std::function<void(const nsresult&)>&& aResolver) {
  using CreateActorPromise =
      MozPromise<RefPtr<FileSystemManagerParent>, nsresult, true>;

  QM_TRY(OkIf(StaticPrefs::dom_fs_enabled()), IPC_OK(),
         [aResolver](const auto&) { aResolver(NS_ERROR_DOM_NOT_ALLOWED_ERR); });

  QM_TRY(OkIf(aParentEndpoint.IsValid()), IPC_OK(),
         [aResolver](const auto&) { aResolver(NS_ERROR_INVALID_ARG); });

  quota::OriginMetadata originMetadata(
      quota::QuotaManager::GetInfoFromValidatedPrincipalInfo(aPrincipalInfo),
      quota::PERSISTENCE_TYPE_DEFAULT);

  LOG(("CreateFileSystemManagerParent, origin: %s",
       originMetadata.mOrigin.get()));

  // This creates the file system data manager, which has to be done on
  // PBackground
  fs::data::FileSystemDataManager::GetOrCreateFileSystemDataManager(
      originMetadata)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [origin = originMetadata.mOrigin,
           parentEndpoint = std::move(aParentEndpoint),
           aResolver](const fs::Registered<fs::data::FileSystemDataManager>&
                          dataManager) mutable {
            QM_TRY_UNWRAP(
                fs::EntryId rootId, fs::data::GetRootHandle(origin), QM_VOID,
                [aResolver](const auto& aRv) { aResolver(ToNSResult(aRv)); });

            InvokeAsync(
                dataManager->MutableIOTargetPtr(), __func__,
                [dataManager =
                     RefPtr<fs::data::FileSystemDataManager>(dataManager),
                 rootId, parentEndpoint = std::move(parentEndpoint)]() mutable {
                  RefPtr<FileSystemManagerParent> parent =
                      new FileSystemManagerParent(std::move(dataManager),
                                                  rootId);

                  if (!parentEndpoint.Bind(parent)) {
                    return CreateActorPromise::CreateAndReject(NS_ERROR_FAILURE,
                                                               __func__);
                  }

                  return CreateActorPromise::CreateAndResolve(std::move(parent),
                                                              __func__);
                })
                ->Then(GetCurrentSerialEventTarget(), __func__,
                       [dataManager = dataManager, aResolver](
                           CreateActorPromise::ResolveOrRejectValue&& aValue) {
                         if (aValue.IsReject()) {
                           aResolver(aValue.RejectValue());
                         } else {
                           RefPtr<FileSystemManagerParent> parent =
                               std::move(aValue.ResolveValue());

                           dataManager->RegisterActor(WrapNotNull(parent));

                           aResolver(NS_OK);
                         }
                       });
          },
          [aResolver](nsresult aRejectValue) { aResolver(aRejectValue); });

  return IPC_OK();
}

}  // namespace mozilla::dom
