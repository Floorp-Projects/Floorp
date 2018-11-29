/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_localstorage_LSObject_h
#define mozilla_dom_localstorage_LSObject_h

#include "mozilla/dom/Storage.h"

class nsIPrincipal;
class nsPIDOMWindowInner;

namespace mozilla {

class ErrorResult;

namespace dom {

class LSDatabase;
class LSRequestChild;
class LSRequestChildCallback;
class LSRequestParams;
class PrincipalOrQuotaInfo;

class LSObject final
  : public Storage
{
  nsAutoPtr<PrincipalOrQuotaInfo> mInfo;

  RefPtr<LSDatabase> mDatabase;

public:
  static nsresult
  Create(nsPIDOMWindowInner* aWindow,
         Storage** aStorage);

  static void
  CancelSyncLoop();

  void
  AssertIsOnOwningThread() const
  {
    NS_ASSERT_OWNINGTHREAD(LSObject);
  }

  LSRequestChild*
  StartRequest(nsIEventTarget* aMainEventTarget,
               const LSRequestParams& aParams,
               LSRequestChildCallback* aCallback);

  // Storage overrides.
  StorageType
  Type() const override;

  bool
  IsForkOf(const Storage* aStorage) const override;

  int64_t
  GetOriginQuotaUsage() const override;

  uint32_t
  GetLength(nsIPrincipal& aSubjectPrincipal,
            ErrorResult& aError) override;

  void
  Key(uint32_t aIndex,
      nsAString& aResult,
      nsIPrincipal& aSubjectPrincipal,
      ErrorResult& aError) override;

  void
  GetItem(const nsAString& aKey,
          nsAString& aResult,
          nsIPrincipal& aSubjectPrincipal,
          ErrorResult& aError) override;

  void
  GetSupportedNames(nsTArray<nsString>& aNames) override;

  void
  SetItem(const nsAString& aKey,
          const nsAString& aValue,
          nsIPrincipal& aSubjectPrincipal,
          ErrorResult& aError) override;

  void
  RemoveItem(const nsAString& aKey,
             nsIPrincipal& aSubjectPrincipal,
             ErrorResult& aError) override;

  void
  Clear(nsIPrincipal& aSubjectPrincipal,
        ErrorResult& aError) override;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(LSObject, Storage)

private:
  LSObject(nsPIDOMWindowInner* aWindow,
           nsIPrincipal* aPrincipal);

  ~LSObject();

  nsresult
  EnsureDatabase();

  void
  DropDatabase();

  // Storage overrides.
  void
  LastRelease() override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_localstorage_LSObject_h
