/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "KeyValueStorage.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "KeyValueStorage.h"
#include "nsServiceManagerUtils.h"
#include "nsVariant.h"

namespace mozilla {

class DatabaseCallback final : public nsIKeyValueDatabaseCallback {
 public:
  NS_DECL_ISUPPORTS

  explicit DatabaseCallback(RefPtr<nsIKeyValueDatabase>& aDatabase)
      : mDatabase(aDatabase) {}
  NS_IMETHOD Resolve(nsIKeyValueDatabase* aDatabase) override {
    if (!aDatabase) {
      mResultPromise.Reject(NS_ERROR_FAILURE, __func__);
    }
    mDatabase = aDatabase;
    mResultPromise.Resolve(true, __func__);
    return NS_OK;
  }
  NS_IMETHOD Reject(const nsACString&) override {
    mResultPromise.Reject(NS_ERROR_FAILURE, __func__);
    return NS_OK;
  }
  RefPtr<GenericPromise> Ensure() { return mResultPromise.Ensure(__func__); }

 protected:
  ~DatabaseCallback() = default;
  RefPtr<nsIKeyValueDatabase>& mDatabase;
  MozPromiseHolder<GenericPromise> mResultPromise;
};
NS_IMPL_ISUPPORTS(DatabaseCallback, nsIKeyValueDatabaseCallback);

RefPtr<GenericPromise> KeyValueStorage::Init() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mDatabaseName.IsEmpty());
  nsresult rv;

  nsCOMPtr<nsIFile> profileDir;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                              getter_AddRefs(profileDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return GenericPromise::CreateAndReject(rv, __func__);
  }
  MOZ_ASSERT(profileDir);

  rv = profileDir->AppendNative(NS_LITERAL_CSTRING("mediacapabilities"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return GenericPromise::CreateAndReject(rv, __func__);
  }

  rv = profileDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
  if (rv != NS_ERROR_FILE_ALREADY_EXISTS && NS_WARN_IF(NS_FAILED(rv))) {
    return GenericPromise::CreateAndReject(rv, __func__);
  }

  nsCOMPtr<nsIKeyValueService> keyValueService =
      do_GetService("@mozilla.org/key-value-service;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return GenericPromise::CreateAndReject(rv, __func__);
  }
  MOZ_ASSERT(keyValueService);

  auto callback = MakeRefPtr<DatabaseCallback>(mDatabase);

  nsString path;
  profileDir->GetPath(path);
  keyValueService->GetOrCreate(callback, NS_ConvertUTF16toUTF8(path),
                               mDatabaseName);
  return callback->Ensure();
}

class VoidCallback final : public nsIKeyValueVoidCallback {
 public:
  NS_DECL_ISUPPORTS

  explicit VoidCallback(const RefPtr<KeyValueStorage>& aOwner)
      : nsIKeyValueVoidCallback(), mOwner(aOwner) {}

  NS_IMETHOD Resolve() override {
    mResultPromise.Resolve(true, __func__);
    return NS_OK;
  }
  NS_IMETHOD Reject(const nsACString&) override {
    mResultPromise.Reject(NS_ERROR_FAILURE, __func__);
    return NS_OK;
  }
  RefPtr<GenericPromise> Ensure(const char* aMethodName) {
    return mResultPromise.Ensure(aMethodName);
  }

 protected:
  ~VoidCallback() = default;
  MozPromiseHolder<GenericPromise> mResultPromise;
  RefPtr<KeyValueStorage> mOwner;
};
NS_IMPL_ISUPPORTS(VoidCallback, nsIKeyValueVoidCallback);

RefPtr<GenericPromise> KeyValueStorage::Put(const nsACString& aKey,
                                            int32_t aValue) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDatabase);
  MOZ_ASSERT(!mDatabaseName.IsEmpty());

  auto value = MakeRefPtr<nsVariant>();
  nsresult rv = value->SetAsInt32(aValue);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return GenericPromise::CreateAndReject(rv, __func__);
  }

  auto callback = MakeRefPtr<VoidCallback>(this);
  rv = mDatabase->Put(callback, aKey, value);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return GenericPromise::CreateAndReject(rv, __func__);
  }
  return callback->Ensure(__func__);
}

RefPtr<GenericPromise> KeyValueStorage::Put(const nsACString& aName,
                                            const nsACString& aKey,
                                            int32_t aValue) {
  if (!mDatabase || !mDatabaseName.Equals(aName)) {
    mDatabaseName = aName;
    RefPtr<KeyValueStorage> self = this;
    const nsCString key(aKey);
    return Init()->Then(
        GetCurrentThreadSerialEventTarget(), __func__,
        [self, key, aValue](bool) { return self->Put(key, aValue); },
        [](nsresult rv) {
          return GenericPromise::CreateAndReject(rv, __func__);
        });
  }
  return Put(aKey, aValue);
}

class GetValueCallback final : public nsIKeyValueVariantCallback {
 public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD Resolve(nsIVariant* aResult) override {
    int32_t value = 0;
    Unused << aResult->GetAsInt32(&value);
    mResultPromise.Resolve(value, __func__);
    return NS_OK;
  }
  NS_IMETHOD Reject(const nsACString&) override {
    mResultPromise.Reject(NS_ERROR_FAILURE, __func__);
    return NS_OK;
  }
  RefPtr<KeyValueStorage::GetPromise> Ensure() {
    return mResultPromise.Ensure(__func__);
  }

 protected:
  ~GetValueCallback() = default;

 private:
  MozPromiseHolder<KeyValueStorage::GetPromise> mResultPromise;
};
NS_IMPL_ISUPPORTS(GetValueCallback, nsIKeyValueVariantCallback);

RefPtr<KeyValueStorage::GetPromise> KeyValueStorage::Get(
    const nsACString& aKey) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDatabase);
  MOZ_ASSERT(!mDatabaseName.IsEmpty());

  RefPtr<nsVariant> defaultValue = new nsVariant;
  nsresult rv = defaultValue->SetAsInt32(-1);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return KeyValueStorage::GetPromise::CreateAndReject(rv, __func__);
  }

  auto callback = MakeRefPtr<GetValueCallback>();
  rv = mDatabase->Get(callback, aKey, defaultValue);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return KeyValueStorage::GetPromise::CreateAndReject(rv, __func__);
  }
  return callback->Ensure();
}

RefPtr<KeyValueStorage::GetPromise> KeyValueStorage::Get(
    const nsACString& aName, const nsACString& aKey) {
  if (!mDatabase || !mDatabaseName.Equals(aName)) {
    mDatabaseName = aName;
    RefPtr<KeyValueStorage> self = this;
    const nsCString key(aKey);
    return Init()->Then(
        GetCurrentThreadSerialEventTarget(), __func__,
        [self, key](bool) { return self->Get(key); },
        [](nsresult rv) {
          return KeyValueStorage::GetPromise::CreateAndReject(rv, __func__);
        });
  }
  return Get(aKey);
}

RefPtr<GenericPromise> KeyValueStorage::Clear() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDatabase);
  MOZ_ASSERT(!mDatabaseName.IsEmpty());

  auto callback = MakeRefPtr<VoidCallback>(this);
  nsresult rv = mDatabase->Clear(callback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return GenericPromise::CreateAndReject(rv, __func__);
  }
  return callback->Ensure(__func__);
}

RefPtr<GenericPromise> KeyValueStorage::Clear(const nsACString& aName) {
  if (!mDatabase || !mDatabaseName.Equals(aName)) {
    mDatabaseName = aName;
    RefPtr<KeyValueStorage> self = this;
    return Init()->Then(
        GetCurrentThreadSerialEventTarget(), __func__,
        [self](bool) { return self->Clear(); },
        [](nsresult rv) {
          return GenericPromise::CreateAndReject(rv, __func__);
        });
  }
  return Clear();
}

}  // namespace mozilla
