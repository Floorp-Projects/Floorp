/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_localstorage_LSDatabase_h
#define mozilla_dom_localstorage_LSDatabase_h

namespace mozilla {
namespace dom {

class LSDatabaseChild;
class LSWriteOpResponse;

class LSDatabase final
{
  LSDatabaseChild* mActor;

  bool mAllowedToClose;

public:
  LSDatabase();

  NS_INLINE_DECL_REFCOUNTING(LSDatabase)

  void
  AssertIsOnOwningThread() const
  {
    NS_ASSERT_OWNINGTHREAD(LSDatabase);
  }

  void
  SetActor(LSDatabaseChild* aActor);

  void
  ClearActor()
  {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mActor);

    mActor = nullptr;
  }

  void
  AllowToClose();

  bool
  IsAllowedToClose() const
  {
    AssertIsOnOwningThread();

    return mAllowedToClose;
  }

  nsresult
  GetLength(uint32_t* aResult);

  nsresult
  GetKey(uint32_t aIndex,
         nsAString& aResult);

  nsresult
  GetItem(const nsAString& aKey,
          nsAString& aResult);

  nsresult
  GetKeys(nsTArray<nsString>& aKeys);

  nsresult
  SetItem(const nsAString& aKey,
          const nsAString& aValue,
          LSWriteOpResponse& aResponse);

  nsresult
  RemoveItem(const nsAString& aKey,
             LSWriteOpResponse& aResponse);

  nsresult
  Clear(LSWriteOpResponse& aResponse);

private:
  ~LSDatabase();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_localstorage_LSDatabase_h
