/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "fs/FileSystemShutdownBlocker.h"

#include "MainThreadUtils.h"
#include "mozilla/Services.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "nsComponentManagerUtils.h"
#include "nsIAsyncShutdown.h"
#include "nsISupportsImpl.h"
#include "nsIWritablePropertyBag2.h"
#include "nsStringFwd.h"

namespace mozilla::dom::fs {

namespace {

nsString CreateBlockerName() {
  const int32_t blockerIdLength = 32;
  nsAutoCString blockerId;
  blockerId.SetLength(blockerIdLength);
  NS_MakeRandomString(blockerId.BeginWriting(), blockerIdLength);

  nsString blockerName = u"OPFS_"_ns;
  blockerName.Append(NS_ConvertUTF8toUTF16(blockerId));

  return blockerName;
}

class FileSystemWritableBlocker : public FileSystemShutdownBlocker {
 public:
  FileSystemWritableBlocker() : mName(CreateBlockerName()) {}

  void SetCallback(std::function<void()>&& aCallback) override;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIASYNCSHUTDOWNBLOCKER

  NS_IMETHODIMP Block() override;

  NS_IMETHODIMP Unblock() override;

 protected:
  virtual ~FileSystemWritableBlocker() = default;

  Result<already_AddRefed<nsIAsyncShutdownClient>, nsresult> GetBarrier() const;

 private:
  const nsString mName;
  std::function<void()> mCallback;
};

void FileSystemWritableBlocker::SetCallback(std::function<void()>&& aCallback) {
  mCallback = std::move(aCallback);
}

NS_IMPL_ISUPPORTS_INHERITED(FileSystemWritableBlocker,
                            FileSystemShutdownBlocker, nsIAsyncShutdownBlocker)

NS_IMETHODIMP FileSystemWritableBlocker::Block() {
  MOZ_ASSERT(NS_IsMainThread());
  QM_TRY_UNWRAP(nsCOMPtr<nsIAsyncShutdownClient> barrier, GetBarrier());

  QM_TRY(MOZ_TO_RESULT(barrier->AddBlocker(
      this, NS_ConvertUTF8toUTF16(nsCString(__FILE__)), __LINE__,
      NS_ConvertUTF8toUTF16(nsCString(__func__)))));

  return NS_OK;
}

NS_IMETHODIMP FileSystemWritableBlocker::Unblock() {
  MOZ_ASSERT(NS_IsMainThread());
  QM_TRY_UNWRAP(nsCOMPtr<nsIAsyncShutdownClient> barrier, GetBarrier());

  MOZ_ASSERT(NS_SUCCEEDED(barrier->RemoveBlocker(this)));

  return NS_OK;
}

Result<already_AddRefed<nsIAsyncShutdownClient>, nsresult>
FileSystemWritableBlocker::GetBarrier() const {
  nsCOMPtr<nsIAsyncShutdownService> svc = services::GetAsyncShutdownService();
  QM_TRY(OkIf(svc), Err(NS_ERROR_FAILURE));

  nsCOMPtr<nsIAsyncShutdownClient> barrier;
  QM_TRY(MOZ_TO_RESULT(svc->GetXpcomWillShutdown(getter_AddRefs(barrier))));

  return barrier.forget();
}

NS_IMETHODIMP
FileSystemWritableBlocker::GetName(nsAString& aName) {
  aName = mName;

  return NS_OK;
}

NS_IMETHODIMP
FileSystemWritableBlocker::GetState(nsIPropertyBag** aBagOut) {
  MOZ_ASSERT(aBagOut);

  nsCOMPtr<nsIWritablePropertyBag2> propertyBag =
      do_CreateInstance("@mozilla.org/hash-property-bag;1");

  QM_TRY(OkIf(propertyBag), NS_ERROR_OUT_OF_MEMORY)

  propertyBag.forget(aBagOut);

  return NS_OK;
}

NS_IMETHODIMP
FileSystemWritableBlocker::BlockShutdown(
    nsIAsyncShutdownClient* /* aBarrier */) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mCallback) {
    auto callback = std::move(mCallback);
    callback();
  }

  return NS_OK;
}

class FileSystemNullBlocker : public FileSystemShutdownBlocker {
 public:
  void SetCallback(std::function<void()>&& aCallback) override {}

  NS_IMETHODIMP Block() override { return NS_OK; }

  NS_IMETHODIMP Unblock() override { return NS_OK; }

 protected:
  virtual ~FileSystemNullBlocker() = default;
};

}  // namespace

/* static */
already_AddRefed<FileSystemShutdownBlocker>
FileSystemShutdownBlocker::CreateForWritable() {
// The shutdown blocker watches for xpcom-will-shutdown which is not fired
// during content process shutdown in release builds.
#ifdef DEBUG
  if (NS_IsMainThread()) {
    RefPtr<FileSystemShutdownBlocker> shutdownBlocker =
        new FileSystemWritableBlocker();

    return shutdownBlocker.forget();
  }
#endif

  RefPtr<FileSystemShutdownBlocker> shutdownBlocker =
      new FileSystemNullBlocker();

  return shutdownBlocker.forget();
}

NS_IMPL_ISUPPORTS(FileSystemShutdownBlocker, nsIAsyncShutdownBlocker)

/* nsIAsyncShutdownBlocker methods */
NS_IMETHODIMP
FileSystemShutdownBlocker::GetName(nsAString& /* aName */) { return NS_OK; }

NS_IMETHODIMP
FileSystemShutdownBlocker::GetState(nsIPropertyBag** /* aBagOut */) {
  return NS_OK;
}

NS_IMETHODIMP
FileSystemShutdownBlocker::BlockShutdown(
    nsIAsyncShutdownClient* /* aBarrier */) {
  return NS_OK;
}

}  // namespace mozilla::dom::fs
