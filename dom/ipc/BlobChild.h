/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ipc_BlobChild_h
#define mozilla_dom_ipc_BlobChild_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/PBlobChild.h"
#include "nsCOMPtr.h"
#include "nsID.h"

class nsIDOMBlob;
class nsIEventTarget;
class nsIRemoteBlob;
class nsString;

namespace mozilla {
namespace ipc {

class PBackgroundChild;

} // namespace ipc

namespace dom {

class ContentChild;
class DOMFileImpl;
class nsIContentChild;
class PBlobStreamChild;

class BlobChild MOZ_FINAL
  : public PBlobChild
{
  typedef mozilla::ipc::PBackgroundChild PBackgroundChild;

  class RemoteBlobImpl;
  friend class RemoteBlobImpl;

  DOMFileImpl* mBlobImpl;
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
  GetOrCreate(nsIContentChild* aManager, DOMFileImpl* aBlobImpl);

  static BlobChild*
  GetOrCreate(PBackgroundChild* aManager, DOMFileImpl* aBlobImpl);

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

  // Get the DOMFileImpl associated with this actor. This may always be called
  // on the sending side. It may also be called on the receiving side unless
  // this is a "mystery" blob that has not yet received a SetMysteryBlobInfo()
  // call.
  already_AddRefed<DOMFileImpl>
  GetBlobImpl();

  // Use this for files.
  bool
  SetMysteryBlobInfo(const nsString& aName,
                     const nsString& aContentType,
                     uint64_t aLength,
                     uint64_t aLastModifiedDate);

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
  BlobChild(nsIContentChild* aManager, DOMFileImpl* aBlobImpl);

  BlobChild(PBackgroundChild* aManager, DOMFileImpl* aBlobImpl);

  BlobChild(nsIContentChild* aManager, BlobChild* aOther);

  BlobChild(PBackgroundChild* aManager, BlobChild* aOther);

  // These constructors are called on the receiving side.
  BlobChild(nsIContentChild* aManager,
            const ChildBlobConstructorParams& aParams);

  BlobChild(PBackgroundChild* aManager,
            const ChildBlobConstructorParams& aParams);

  // Only called by Destroy().
  ~BlobChild();

  void
  CommonInit(DOMFileImpl* aBlobImpl);

  void
  CommonInit(BlobChild* aOther);

  void
  CommonInit(const ChildBlobConstructorParams& aParams);

  template <class ChildManagerType>
  static BlobChild*
  GetOrCreateFromImpl(ChildManagerType* aManager, DOMFileImpl* aBlobImpl);

  template <class ChildManagerType>
  static BlobChild*
  CreateFromParams(ChildManagerType* aManager,
                   const ChildBlobConstructorParams& aParams);

  template <class ChildManagerType>
  static BlobChild*
  SendSliceConstructor(ChildManagerType* aManager,
                       const ChildBlobConstructorParams& aParams,
                       const ParentBlobConstructorParams& aOtherSideParams);

  static BlobChild*
  MaybeGetActorFromRemoteBlob(nsIRemoteBlob* aRemoteBlob,
                              nsIContentChild* aManager);

  static BlobChild*
  MaybeGetActorFromRemoteBlob(nsIRemoteBlob* aRemoteBlob,
                              PBackgroundChild* aManager);

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
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual PBlobStreamChild*
  AllocPBlobStreamChild() MOZ_OVERRIDE;

  virtual bool
  DeallocPBlobStreamChild(PBlobStreamChild* aActor) MOZ_OVERRIDE;
};

// Only let ContentChild call BlobChild::Startup() and ensure that
// ContentChild can't access any other BlobChild internals.
class BlobChild::FriendKey MOZ_FINAL
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
