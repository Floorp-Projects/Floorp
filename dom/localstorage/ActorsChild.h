/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_localstorage_ActorsChild_h
#define mozilla_dom_localstorage_ActorsChild_h

#include "mozilla/dom/PBackgroundLSDatabaseChild.h"
#include "mozilla/dom/PBackgroundLSObjectChild.h"
#include "mozilla/dom/PBackgroundLSObserverChild.h"
#include "mozilla/dom/PBackgroundLSRequestChild.h"
#include "mozilla/dom/PBackgroundLSSimpleRequestChild.h"

namespace mozilla {

namespace ipc {

class BackgroundChildImpl;

} // namespace ipc

namespace dom {

class LocalStorageManager2;
class LSDatabase;
class LSObject;
class LSObserver;
class LSRequestChildCallback;
class LSSimpleRequestChildCallback;

class LSObjectChild final
  : public PBackgroundLSObjectChild
{
  friend class mozilla::ipc::BackgroundChildImpl;
  friend class LSObject;

  LSObject* mObject;

  NS_DECL_OWNINGTHREAD

public:
  void
  AssertIsOnOwningThread() const
  {
    NS_ASSERT_OWNINGTHREAD(LSObjectChild);
  }

private:
  // Only created by LSObject.
  explicit LSObjectChild(LSObject* aObject);

  // Only destroyed by mozilla::ipc::BackgroundChildImpl.
  ~LSObjectChild();

  void
  SendDeleteMeInternal();

  // IPDL methods are only called by IPDL.
  void
  ActorDestroy(ActorDestroyReason aWhy) override;

  PBackgroundLSDatabaseChild*
  AllocPBackgroundLSDatabaseChild(const uint64_t& aDatastoreId) override;

  bool
  DeallocPBackgroundLSDatabaseChild(PBackgroundLSDatabaseChild* aActor)
                                    override;
};

class LSDatabaseChild final
  : public PBackgroundLSDatabaseChild
{
  friend class mozilla::ipc::BackgroundChildImpl;
  friend class LSDatabase;
  friend class LSObject;

  LSDatabase* mDatabase;

  NS_DECL_OWNINGTHREAD

public:
  void
  AssertIsOnOwningThread() const
  {
    NS_ASSERT_OWNINGTHREAD(LSDatabaseChild);
  }

private:
  // Only created by LSObject.
  explicit LSDatabaseChild(LSDatabase* aDatabase);

  // Only destroyed by mozilla::ipc::BackgroundChildImpl.
  ~LSDatabaseChild();

  void
  SendDeleteMeInternal();

  // IPDL methods are only called by IPDL.
  void
  ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult
  RecvRequestAllowToClose() override;
};

class LSObserverChild final
  : public PBackgroundLSObserverChild
{
  friend class mozilla::ipc::BackgroundChildImpl;
  friend class LSObserver;
  friend class LSObject;

  LSObserver* mObserver;

  NS_DECL_OWNINGTHREAD

public:
  void
  AssertIsOnOwningThread() const
  {
    NS_ASSERT_OWNINGTHREAD(LSObserverChild);
  }

private:
  // Only created by LSObject.
  explicit LSObserverChild(LSObserver* aObserver);

  // Only destroyed by mozilla::ipc::BackgroundChildImpl.
  ~LSObserverChild();

  void
  SendDeleteMeInternal();

  // IPDL methods are only called by IPDL.
  void
  ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult
  RecvObserve(const PrincipalInfo& aPrinciplaInfo,
              const uint32_t& aPrivateBrowsingId,
              const nsString& aDocumentURI,
              const nsString& aKey,
              const nsString& aOldValue,
              const nsString& aNewValue) override;
};

class LSRequestChild final
  : public PBackgroundLSRequestChild
{
  friend class LSObject;
  friend class LocalStorageManager2;

  RefPtr<LSRequestChildCallback> mCallback;

  bool mFinishing;

  NS_DECL_OWNINGTHREAD

public:
  void
  AssertIsOnOwningThread() const
  {
    NS_ASSERT_OWNINGTHREAD(LSReqeustChild);
  }

  bool
  Finishing() const;

private:
  // Only created by LSObject.
  explicit LSRequestChild(LSRequestChildCallback* aCallback);

  // Only destroyed by mozilla::ipc::BackgroundChildImpl.
  ~LSRequestChild();

  // IPDL methods are only called by IPDL.
  void
  ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult
  Recv__delete__(const LSRequestResponse& aResponse) override;

  mozilla::ipc::IPCResult
  RecvReady() override;
};

class NS_NO_VTABLE LSRequestChildCallback
{
public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual void
  OnResponse(const LSRequestResponse& aResponse) = 0;

protected:
  virtual ~LSRequestChildCallback()
  { }
};

class LSSimpleRequestChild final
  : public PBackgroundLSSimpleRequestChild
{
  friend class LocalStorageManager2;

  RefPtr<LSSimpleRequestChildCallback> mCallback;

  NS_DECL_OWNINGTHREAD

public:
  void
  AssertIsOnOwningThread() const
  {
    NS_ASSERT_OWNINGTHREAD(LSSimpleReqeustChild);
  }

private:
  // Only created by LocalStorageManager2.
  explicit LSSimpleRequestChild(LSSimpleRequestChildCallback* aCallback);

  // Only destroyed by mozilla::ipc::BackgroundChildImpl.
  ~LSSimpleRequestChild();

  // IPDL methods are only called by IPDL.
  void
  ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult
  Recv__delete__(const LSSimpleRequestResponse& aResponse) override;
};

class NS_NO_VTABLE LSSimpleRequestChildCallback
{
public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual void
  OnResponse(const LSSimpleRequestResponse& aResponse) = 0;

protected:
  virtual ~LSSimpleRequestChildCallback()
  { }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_localstorage_ActorsChild_h
