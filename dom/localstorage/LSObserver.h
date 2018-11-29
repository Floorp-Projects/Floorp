/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_localstorage_LSObserver_h
#define mozilla_dom_localstorage_LSObserver_h

namespace mozilla {
namespace dom {

class LSObserverChild;

class LSObserver final
{
  friend class LSObject;

  LSObserverChild* mActor;

  const nsCString mOrigin;

public:
  explicit LSObserver(const nsACString& aOrigin);

  static LSObserver*
  Get(const nsACString& aOrigin);

  NS_INLINE_DECL_REFCOUNTING(LSObserver)

  void
  AssertIsOnOwningThread() const
  {
    NS_ASSERT_OWNINGTHREAD(LSDatabase);
  }

  void
  SetActor(LSObserverChild* aActor);

  void
  ClearActor()
  {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mActor);

    mActor = nullptr;
  }

private:
  ~LSObserver();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_localstorage_LSObserver_h
