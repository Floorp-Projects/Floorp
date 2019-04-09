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
class LSSnapshot;

class LSDatabase final {
  class Observer;

  LSDatabaseChild* mActor;

  LSSnapshot* mSnapshot;

  const nsCString mOrigin;

  bool mAllowedToClose;
  bool mRequestedAllowToClose;

  static StaticRefPtr<Observer> sObserver;

 public:
  explicit LSDatabase(const nsACString& aOrigin);

  static LSDatabase* Get(const nsACString& aOrigin);

  NS_INLINE_DECL_REFCOUNTING(LSDatabase)

  void AssertIsOnOwningThread() const { NS_ASSERT_OWNINGTHREAD(LSDatabase); }

  void SetActor(LSDatabaseChild* aActor);

  void ClearActor() {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mActor);

    mActor = nullptr;
  }

  bool IsAllowedToClose() const {
    AssertIsOnOwningThread();

    return mAllowedToClose;
  }

  void RequestAllowToClose();

  void NoteFinishedSnapshot(LSSnapshot* aSnapshot);

  nsresult GetLength(LSObject* aObject, uint32_t* aResult);

  nsresult GetKey(LSObject* aObject, uint32_t aIndex, nsAString& aResult);

  nsresult GetItem(LSObject* aObject, const nsAString& aKey,
                   nsAString& aResult);

  nsresult GetKeys(LSObject* aObject, nsTArray<nsString>& aKeys);

  nsresult SetItem(LSObject* aObject, const nsAString& aKey,
                   const nsAString& aValue, LSNotifyInfo& aNotifyInfo);

  nsresult RemoveItem(LSObject* aObject, const nsAString& aKey,
                      LSNotifyInfo& aNotifyInfo);

  nsresult Clear(LSObject* aObject, LSNotifyInfo& aNotifyInfo);

  nsresult BeginExplicitSnapshot(LSObject* aObject);

  nsresult EndExplicitSnapshot(LSObject* aObject);

 private:
  ~LSDatabase();

  nsresult EnsureSnapshot(LSObject* aObject, const nsAString& aKey,
                          bool aExplicit = false);

  void AllowToClose();
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_localstorage_LSDatabase_h
