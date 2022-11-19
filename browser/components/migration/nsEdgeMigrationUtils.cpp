/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsEdgeMigrationUtils.h"

#include "mozilla/dom/Promise.h"
#include "nsCOMPtr.h"
#include "nsIEventTarget.h"

#include <windows.h>

namespace mozilla {

NS_IMPL_ISUPPORTS(nsEdgeMigrationUtils, nsIEdgeMigrationUtils)

NS_IMETHODIMP
nsEdgeMigrationUtils::IsDbLocked(nsIFile* aFile, JSContext* aCx,
                                 dom::Promise** aPromise) {
  NS_ENSURE_ARG_POINTER(aFile);

  nsString path;
  nsresult rv = aFile->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  ErrorResult err;
  RefPtr<dom::Promise> promise =
      dom::Promise::Create(xpc::CurrentNativeGlobal(aCx), err);

  if (MOZ_UNLIKELY(err.Failed())) {
    return err.StealNSResult();
  }

  nsMainThreadPtrHandle<dom::Promise> promiseHolder(
      new nsMainThreadPtrHolder<dom::Promise>("nsEdgeMigrationUtils Promise",
                                              promise));

  NS_DispatchBackgroundTask(NS_NewRunnableFunction(
      __func__,
      [promiseHolder = std::move(promiseHolder), path = std::move(path)]() {
        HANDLE file = ::CreateFileW(path.get(), GENERIC_READ, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING, 0, nullptr);

        bool locked = true;
        if (file != INVALID_HANDLE_VALUE) {
          locked = false;
          ::CloseHandle(file);
        }

        NS_DispatchToMainThread(NS_NewRunnableFunction(
            __func__, [promiseHolder = std::move(promiseHolder), locked]() {
              promiseHolder.get()->MaybeResolve(locked);
            }));
      }));

  promise.forget(aPromise);

  return NS_OK;
}

}  // namespace mozilla
