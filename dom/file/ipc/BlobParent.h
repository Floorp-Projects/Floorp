/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ipc_BlobParent_h
#define mozilla_dom_ipc_BlobParent_h

#include "mozilla/Attributes.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/PBlobParent.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"

template <class, class> class nsDataHashtable;
class nsIDHashKey;
class nsIEventTarget;
class nsIRemoteBlob;
template <class> class nsRevocableEventPtr;
class nsString;

namespace mozilla {

class Mutex;

namespace ipc {

class PBackgroundParent;

} // namespace ipc

namespace dom {

class ContentParent;
class BlobImpl;
class nsIContentParent;
class PBlobStreamParent;

class BlobParent final
  : public PBlobParent
{
  typedef mozilla::ipc::PBackgroundParent PBackgroundParent;

  class IDTableEntry;
  typedef nsDataHashtable<nsIDHashKey, IDTableEntry*> IDTable;

  class OpenStreamRunnable;
  friend class OpenStreamRunnable;

  class RemoteBlobImpl;

  struct CreateBlobImplMetadata;

  static StaticAutoPtr<IDTable> sIDTable;
  static StaticAutoPtr<Mutex> sIDTableMutex;

  BlobImpl* mBlobImpl;
  RemoteBlobImpl* mRemoteBlobImpl;

  // One of these will be null and the other non-null.
  PBackgroundParent* mBackgroundManager;
  nsCOMPtr<nsIContentParent> mContentManager;

  nsCOMPtr<nsIEventTarget> mEventTarget;

  // nsIInputStreams backed by files must ensure that the files are actually
  // opened and closed on a background thread before we can send their file
  // handles across to the child. The child process could crash during this
  // process so we need to make sure we cancel the intended response in such a
  // case. We do that by holding an array of nsRevocableEventPtr. If the child
  // crashes then this actor will be destroyed and the nsRevocableEventPtr
  // destructor will cancel any stream events that are currently in flight.
  nsTArray<nsRevocableEventPtr<OpenStreamRunnable>> mOpenStreamRunnables;

  RefPtr<IDTableEntry> mIDTableEntry;

  bool mOwnsBlobImpl;

public:
  class FriendKey;

  static void
  Startup(const FriendKey& aKey);

  // These create functions are called on the sending side.
  static BlobParent*
  GetOrCreate(nsIContentParent* aManager, BlobImpl* aBlobImpl);

  static BlobParent*
  GetOrCreate(PBackgroundParent* aManager, BlobImpl* aBlobImpl);

  // These create functions are called on the receiving side.
  static BlobParent*
  Create(nsIContentParent* aManager,
         const ParentBlobConstructorParams& aParams);

  static BlobParent*
  Create(PBackgroundParent* aManager,
         const ParentBlobConstructorParams& aParams);

  static void
  Destroy(PBlobParent* aActor)
  {
    delete static_cast<BlobParent*>(aActor);
  }

  static already_AddRefed<BlobImpl>
  GetBlobImplForID(const nsID& aID);

  bool
  HasManager() const
  {
    return mBackgroundManager || mContentManager;
  }

  PBackgroundParent*
  GetBackgroundManager() const
  {
    return mBackgroundManager;
  }

  nsIContentParent*
  GetContentManager() const
  {
    return mContentManager;
  }

  // Get the BlobImpl associated with this actor.
  already_AddRefed<BlobImpl>
  GetBlobImpl();

  void
  AssertIsOnOwningThread() const
#ifdef DEBUG
  ;
#else
  { }
#endif

private:
  // These constructors are called on the sending side.
  BlobParent(nsIContentParent* aManager, IDTableEntry* aIDTableEntry);

  BlobParent(PBackgroundParent* aManager, IDTableEntry* aIDTableEntry);

  // These constructors are called on the receiving side.
  BlobParent(nsIContentParent* aManager,
             BlobImpl* aBlobImpl,
             IDTableEntry* aIDTableEntry);

  BlobParent(PBackgroundParent* aManager,
             BlobImpl* aBlobImpl,
             IDTableEntry* aIDTableEntry);

  // Only destroyed by BackgroundParentImpl and ContentParent.
  ~BlobParent();

  void
  CommonInit(IDTableEntry* aIDTableEntry);

  void
  CommonInit(BlobImpl* aBlobImpl, IDTableEntry* aIDTableEntry);

  template <class ParentManagerType>
  static BlobParent*
  GetOrCreateFromImpl(ParentManagerType* aManager,
                      BlobImpl* aBlobImpl);

  template <class ParentManagerType>
  static BlobParent*
  CreateFromParams(ParentManagerType* aManager,
                   const ParentBlobConstructorParams& aParams);

  template <class ParentManagerType>
  static BlobParent*
  SendSliceConstructor(ParentManagerType* aManager,
                       const ParentBlobConstructorParams& aParams,
                       const ChildBlobConstructorParams& aOtherSideParams);

  static BlobParent*
  MaybeGetActorFromRemoteBlob(nsIRemoteBlob* aRemoteBlob,
                              nsIContentParent* aManager);

  static BlobParent*
  MaybeGetActorFromRemoteBlob(nsIRemoteBlob* aRemoteBlob,
                              PBackgroundParent* aManager);

  void
  NoteDyingRemoteBlobImpl();

  void
  NoteRunnableCompleted(OpenStreamRunnable* aRunnable);

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

  virtual PBlobStreamParent*
  AllocPBlobStreamParent(const uint64_t& aStart,
                         const uint64_t& aLength) override;

  virtual mozilla::ipc::IPCResult
  RecvPBlobStreamConstructor(PBlobStreamParent* aActor,
                             const uint64_t& aStart,
                             const uint64_t& aLength) override;

  virtual bool
  DeallocPBlobStreamParent(PBlobStreamParent* aActor) override;

  virtual mozilla::ipc::IPCResult
  RecvResolveMystery(const ResolveMysteryParams& aParams) override;

  virtual mozilla::ipc::IPCResult
  RecvBlobStreamSync(const uint64_t& aStart,
                     const uint64_t& aLength,
                     InputStreamParams* aParams,
                     OptionalFileDescriptorSet* aFDs) override;

  virtual mozilla::ipc::IPCResult
  RecvWaitForSliceCreation() override;

  virtual mozilla::ipc::IPCResult
  RecvGetFileId(int64_t* aFileId) override;

  virtual mozilla::ipc::IPCResult
  RecvGetFilePath(nsString* aFilePath) override;
};

// Only let ContentParent call BlobParent::Startup() and ensure that
// ContentParent can't access any other BlobParent internals.
class BlobParent::FriendKey final
{
  friend class ContentParent;

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

#endif // mozilla_dom_ipc_BlobParent_h
