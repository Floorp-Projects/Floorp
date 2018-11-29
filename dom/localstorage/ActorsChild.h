/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_localstorage_ActorsChild_h
#define mozilla_dom_localstorage_ActorsChild_h

#include "mozilla/dom/PBackgroundLSDatabaseChild.h"
#include "mozilla/dom/PBackgroundLSRequestChild.h"

namespace mozilla {

namespace ipc {

class BackgroundChildImpl;

} // namespace ipc

namespace dom {

class LSDatabase;
class LSObject;
class LSRequestChildCallback;

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

class LSRequestChild final
  : public PBackgroundLSRequestChild
{
  friend class LSObject;
  friend class LSObjectChild;

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

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_localstorage_ActorsChild_h
