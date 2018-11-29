/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_localstorage_LSSnapshot_h
#define mozilla_dom_localstorage_LSSnapshot_h

namespace mozilla {
namespace dom {

class LSSnapshotChild;

class LSSnapshot final
  : public nsIRunnable
{
  RefPtr<LSDatabase> mDatabase;

  LSSnapshotChild* mActor;

  nsDataHashtable<nsStringHashKey, nsString> mValues;
  nsTArray<LSWriteInfo> mWriteInfos;

  int64_t mExactUsage;
  int64_t mPeakUsage;

#ifdef DEBUG
  bool mInitialized;
  bool mSentFinish;
#endif

public:
  explicit LSSnapshot(LSDatabase* aDatabase);

  void
  AssertIsOnOwningThread() const
  {
    NS_ASSERT_OWNINGTHREAD(LSSnapshot);
  }

  void
  SetActor(LSSnapshotChild* aActor);

  void
  ClearActor()
  {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mActor);

    mActor = nullptr;
  }

  nsresult
  Init(const LSSnapshotInitInfo& aInitInfo);

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
          LSNotifyInfo& aNotifyInfo);

  nsresult
  RemoveItem(const nsAString& aKey,
             LSNotifyInfo& aNotifyInfo);

  nsresult
  Clear(LSNotifyInfo& aNotifyInfo);

private:
  ~LSSnapshot();

  nsresult
  UpdateUsage(int64_t aDelta);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_localstorage_LSSnapshot_h
