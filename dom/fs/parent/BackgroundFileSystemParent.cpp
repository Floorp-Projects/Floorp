/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BackgroundFileSystemParent.h"
#include "OriginPrivateFileSystemParent.h"

#include "nsNetCID.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/FileDescriptorUtils.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "mozilla/StaticPrefs_dom.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
LazyLogModule gOPFSLog("OPFS");
}

#define LOG(args) MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Verbose, args)

#define LOG_DEBUG(args) \
  MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Debug, args)

namespace mozilla::dom {

namespace fs::data {

EntryId GetRootHandle(const Origin& aOrigin) { return "not implemented"_ns; }

// TODO: Replace with real FileSystemDataManager
class FileSystemDataManager : public FileSystemDataManagerBase {
 public:
  using result_t = Result<FileSystemDataManager*, nsresult>;
  static FileSystemDataManager::result_t CreateFileSystemDataManager(
      const fs::Origin& aOrigin);
};

FileSystemDataManager::result_t
FileSystemDataManager::CreateFileSystemDataManager(
    const fs::Origin& /*aOrigin*/) {
  return nullptr;
}

}  // namespace fs::data

using RootPromise = MozPromise<fs::FileSystemGetRootResponse, nsresult, false>;

mozilla::ipc::IPCResult BackgroundFileSystemParent::RecvGetRoot(
    Endpoint<POriginPrivateFileSystemParent>&& aParentEp,
    GetRootResolver&& aResolver) {
  QM_TRY(
      OkIf(StaticPrefs::dom_fs_enabled()), IPC_OK(), [aResolver](const auto&) {
        aResolver(fs::FileSystemGetRootResponse(NS_ERROR_DOM_NOT_ALLOWED_ERR));
      });

  QM_TRY(OkIf(aParentEp.IsValid()), IPC_OK(), [aResolver](const auto&) {
    aResolver(fs::FileSystemGetRootResponse(NS_ERROR_INVALID_ARG));
  });

  nsAutoCString origin =
      quota::QuotaManager::GetOriginFromValidatedPrincipalInfo(mPrincipalInfo);

  // This opens the quota manager, which has to be done on PBackground
  auto res =
      fs::data::FileSystemDataManager::CreateFileSystemDataManager(origin);
  if (NS_WARN_IF(res.isErr())) {
    MOZ_ASSERT(false, "Can't create FileSystemDataManager");
    aResolver(fs::FileSystemGetRootResponse(NS_ERROR_FAILURE));

    return IPC_OK();
  }
  nsCOMPtr<nsIThread> pbackground = NS_GetCurrentThread();

  nsCOMPtr<nsIEventTarget> target =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(target);

  nsCString name("OPFS ");
  name += origin;
  RefPtr<TaskQueue> taskqueue =
      TaskQueue::Create(target.forget(), PromiseFlatCString(name).get());
  // We'll have to thread-hop back to this thread to respond.  We could
  // just have the create be one-way, then send the actual request on the
  // new channel, but that's an extra IPC instead.
  InvokeAsync(
      taskqueue, __func__,
      [origin, parentEp = std::move(aParentEp), aResolver, data = res.unwrap(),
       taskqueue, pbackground]() mutable {
        RefPtr<OriginPrivateFileSystemParent> parent =
            new OriginPrivateFileSystemParent(taskqueue);
        if (!parentEp.Bind(parent)) {
          auto response = fs::FileSystemGetRootResponse(NS_ERROR_FAILURE);
          return RootPromise::CreateAndReject(response, __func__);
        }

        // Send response back to pbackground to send to child
        auto response =
            fs::FileSystemGetRootResponse(fs::data::GetRootHandle(origin));
        return RootPromise::CreateAndResolve(response, __func__);
      })
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [aResolver](const RootPromise::ResolveOrRejectValue& aValue) {
               if (aValue.IsReject()) {
                 aResolver(aValue.RejectValue());
               } else {
                 aResolver(aValue.ResolveValue());
               }
             });

  return IPC_OK();
}

}  // namespace mozilla::dom
