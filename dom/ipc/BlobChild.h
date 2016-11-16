/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ipc_BlobChild_h
#define mozilla_dom_ipc_BlobChild_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/PBlobChild.h"
#include "nsCOMPtr.h"
#include "nsID.h"

class nsIEventTarget;
class nsIRemoteBlob;
class nsString;

namespace mozilla {
namespace ipc {

class PBackgroundChild;

} // namespace ipc

namespace dom {

class Blob;
class BlobImpl;
class ContentChild;
class nsIContentChild;
class PBlobStreamChild;

class BlobChild final
  : public PBlobChild
{
  typedef mozilla::ipc::PBackgroundChild PBackgroundChild;

  class RemoteBlobImpl;
  friend class RemoteBlobImpl;

  class RemoteBlobSliceImpl;
  friend class RemoteBlobSliceImpl;

  BlobImpl* mBlobImpl;
  RemoteBlobImpl* mRemoteBlobImpl;

  // One of these will be null and the other non-null.
  PBackgroundChild* mBackgroundManager;
  nsCOMPtr<nsIContentChild> mContentManager;

  nsCOMPtr<nsIEventTarget> mEventTarget;

  nsID mParentID;

  bool mOwnsBlobImpl;

public:
  class FriendKey;

  static void
  Startup(const FriendKey& aKey);

  // These create functions are called on the sending side.
  static BlobChild*
  GetOrCreate(nsIContentChild* aManager, BlobImpl* aBlobImpl);

  static BlobChild*
  GetOrCreate(PBackgroundChild* aManager, BlobImpl* aBlobImpl);

  // These create functions are called on the receiving side.
  static BlobChild*
  Create(nsIContentChild* aManager, const ChildBlobConstructorParams& aParams);

  static BlobChild*
  Create(PBackgroundChild* aManager,
         const ChildBlobConstructorParams& aParams);

  static void
  Destroy(PBlobChild* aActor)
  {
    delete static_cast<BlobChild*>(aActor);
  }

  bool
  HasManager() const
  {
    return mBackgroundManager || mContentManager;
  }

  PBackgroundChild*
  GetBackgroundManager() const
  {
    return mBackgroundManager;
  }

  nsIContentChild*
  GetContentManager() const
  {
    return mContentManager;
  }

  const nsID&
  ParentID() const;

  // Get the BlobImpl associated with this actor. This may always be called
  // on the sending side. It may also be called on the receiving side unless
  // this is a "mystery" blob that has not yet received a SetMysteryBlobInfo()
  // call.
  already_AddRefed<BlobImpl>
  GetBlobImpl();

  // Use this for files.
  bool
  SetMysteryBlobInfo(const nsString& aName,
                     const nsString& aContentType,
                     uint64_t aLength,
                     int64_t aLastModifiedDate);

  // Use this for non-file blobs.
  bool
  SetMysteryBlobInfo(const nsString& aContentType, uint64_t aLength);

  void
  AssertIsOnOwningThread() const
#ifdef DEBUG
  ;
#else
  { }
#endif

private:
  // These constructors are called on the sending side.
  BlobChild(nsIContentChild* aManager, BlobImpl* aBlobImpl);

  BlobChild(PBackgroundChild* aManager, BlobImpl* aBlobImpl);

  BlobChild(nsIContentChild* aManager, BlobChild* aOther);

  BlobChild(PBackgroundChild* aManager, BlobChild* aOther, BlobImpl* aBlobImpl);

  // These constructors are called on the receiving side.
  BlobChild(nsIContentChild* aManager,
            const ChildBlobConstructorParams& aParams);

  BlobChild(PBackgroundChild* aManager,
            const ChildBlobConstructorParams& aParams);

  // These constructors are called for slices.
  BlobChild(nsIContentChild* aManager,
            const nsID& aParentID,
            RemoteBlobSliceImpl* aRemoteBlobSliceImpl);

  BlobChild(PBackgroundChild* aManager,
            const nsID& aParentID,
            RemoteBlobSliceImpl* aRemoteBlobSliceImpl);

  // Only called by Destroy().
  ~BlobChild();

  void
  CommonInit(BlobImpl* aBlobImpl);

  void
  CommonInit(BlobChild* aOther, BlobImpl* aBlobImpl);

  void
  CommonInit(const ChildBlobConstructorParams& aParams);

  void
  CommonInit(const nsID& aParentID, RemoteBlobImpl* aRemoteBlobImpl);

  template <class ChildManagerType>
  static BlobChild*
  GetOrCreateFromImpl(ChildManagerType* aManager, BlobImpl* aBlobImpl);

  template <class ChildManagerType>
  static BlobChild*
  CreateFromParams(ChildManagerType* aManager,
                   const ChildBlobConstructorParams& aParams);

  template <class ChildManagerType>
  static BlobChild*
  SendSliceConstructor(ChildManagerType* aManager,
                       RemoteBlobSliceImpl* aRemoteBlobSliceImpl,
                       const ParentBlobConstructorParams& aParams);

  static BlobChild*
  MaybeGetActorFromRemoteBlob(nsIRemoteBlob* aRemoteBlob,
                              nsIContentChild* aManager,
                              BlobImpl* aBlobImpl);

  static BlobChild*
  MaybeGetActorFromRemoteBlob(nsIRemoteBlob* aRemoteBlob,
                              PBackgroundChild* aManager,
                              BlobImpl* aBlobImpl);

  void
  NoteDyingRemoteBlobImpl();

  nsIEventTarget*
  EventTarget() const
  {
    return mEventTarget;
  }

  bool
  IsOnOwningThread() const;

  // These methods are only called by the IPDL message machinery.
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

  virtual PBlobStreamChild*
  AllocPBlobStreamChild(const uint64_t& aStart,
                        const uint64_t& aLength) override;

  virtual bool
  DeallocPBlobStreamChild(PBlobStreamChild* aActor) override;

  virtual mozilla::ipc::IPCResult
  RecvCreatedFromKnownBlob() override;
};

// Only let ContentChild call BlobChild::Startup() and ensure that
// ContentChild can't access any other BlobChild internals.
class BlobChild::FriendKey final
{
  friend class ContentChild;

private:
  FriendKey()
  { }

  FriendKey(const FriendKey& /* aOther */)
  { }

public:
  ~FriendKey()
  { }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ipc_BlobChild_h
