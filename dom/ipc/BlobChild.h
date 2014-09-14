/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ipc_BlobChild_h
#define mozilla_dom_ipc_BlobChild_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/PBlobChild.h"
#include "nsCOMPtr.h"

class nsIDOMBlob;
class nsIEventTarget;
class nsString;

namespace mozilla {
namespace ipc {

class PBackgroundChild;

} // namespace ipc

namespace dom {

class DOMFileImpl;
class nsIContentChild;
class PBlobStreamChild;

class BlobChild MOZ_FINAL
  : public PBlobChild
{
  typedef mozilla::ipc::PBackgroundChild PBackgroundChild;

  class RemoteBlob;
  friend class RemoteBlob;

  nsIDOMBlob* mBlob;
  RemoteBlob* mRemoteBlob;

  // One of these will be null and the other non-null.
  PBackgroundChild* mBackgroundManager;
  nsCOMPtr<nsIContentChild> mContentManager;

  nsCOMPtr<nsIEventTarget> mEventTarget;

  bool mOwnsBlob;

public:
  // These create functions are called on the sending side.
  static BlobChild*
  Create(nsIContentChild* aManager, nsIDOMBlob* aBlob)
  {
    return new BlobChild(aManager, aBlob);
  }

  static BlobChild*
  Create(PBackgroundChild* aManager, nsIDOMBlob* aBlob)
  {
    return new BlobChild(aManager, aBlob);
  }

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

  // Get the blob associated with this actor. This may always be called on the
  // sending side. It may also be called on the receiving side unless this is a
  // "mystery" blob that has not yet received a SetMysteryBlobInfo() call.
  already_AddRefed<nsIDOMBlob>
  GetBlob();

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
  BlobChild(nsIContentChild* aManager, nsIDOMBlob* aBlob);

  BlobChild(PBackgroundChild* aManager, nsIDOMBlob* aBlob);

  // These constructors are called on the receiving side.
  BlobChild(nsIContentChild* aManager,
            const ChildBlobConstructorParams& aParams);

  BlobChild(PBackgroundChild* aManager,
            const ChildBlobConstructorParams& aParams);

  // Only called by Destroy().
  ~BlobChild();

  void
  CommonInit(nsIDOMBlob* aBlob);

  void
  CommonInit(const ChildBlobConstructorParams& aParams);

  template <class ChildManagerType>
  static BlobChild*
  CreateFromParams(ChildManagerType* aManager,
                   const ChildBlobConstructorParams& aParams);

  template <class ChildManagerType>
  static BlobChild*
  SendSliceConstructor(ChildManagerType* aManager,
                       const NormalBlobConstructorParams& aParams,
                       const ParentBlobConstructorParams& aOtherSideParams);

  void
  NoteDyingRemoteBlob();

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
  RecvPBlobStreamConstructor(PBlobStreamChild* aActor) MOZ_OVERRIDE;

  virtual bool
  DeallocPBlobStreamChild(PBlobStreamChild* aActor) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ipc_BlobChild_h
