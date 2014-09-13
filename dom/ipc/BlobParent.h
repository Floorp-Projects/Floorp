/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ipc_BlobParent_h
#define mozilla_dom_ipc_BlobParent_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/PBlobParent.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"

class nsIDOMBlob;
class nsIEventTarget;
class nsString;
template <class> class nsRevocableEventPtr;

namespace mozilla {
namespace ipc {

class PBackgroundParent;

} // namespace ipc

namespace dom {

class DOMFileImpl;
class nsIContentParent;
class PBlobStreamParent;

class BlobParent MOZ_FINAL
  : public PBlobParent
{
  typedef mozilla::ipc::PBackgroundParent PBackgroundParent;

  class OpenStreamRunnable;
  friend class OpenStreamRunnable;

  class RemoteBlob;
  friend class RemoteBlob;

  nsIDOMBlob* mBlob;
  RemoteBlob* mRemoteBlob;

  // One of these will be null and the other non-null.
  PBackgroundParent* mBackgroundManager;
  nsCOMPtr<nsIContentParent> mContentManager;

  // This is only set when the blob is created with an existing DOMFileImpl.
  nsRefPtr<DOMFileImpl> mBlobImpl;

  nsCOMPtr<nsIEventTarget> mEventTarget;

  // nsIInputStreams backed by files must ensure that the files are actually
  // opened and closed on a background thread before we can send their file
  // handles across to the child. The child process could crash during this
  // process so we need to make sure we cancel the intended response in such a
  // case. We do that by holding an array of nsRevocableEventPtr. If the child
  // crashes then this actor will be destroyed and the nsRevocableEventPtr
  // destructor will cancel any stream events that are currently in flight.
  nsTArray<nsRevocableEventPtr<OpenStreamRunnable>> mOpenStreamRunnables;

  bool mOwnsBlob;
  bool mOwnsRemoteBlob;

public:
  // These create functions are called on the sending side.
  static BlobParent*
  Create(nsIContentParent* aManager, nsIDOMBlob* aBlob)
  {
    return new BlobParent(aManager, aBlob);
  }

  static BlobParent*
  Create(PBackgroundParent* aManager, nsIDOMBlob* aBlob)
  {
    return new BlobParent(aManager, aBlob);
  }

  static BlobParent*
  Create(PBackgroundParent* aManager, DOMFileImpl* aBlobImpl)
  {
    return new BlobParent(aManager, aBlobImpl);
  }

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

  // Get the blob associated with this actor. This may always be called on the
  // sending side. It may also be called on the receiving side unless this is a
  // "mystery" blob that has not yet received a SetMysteryBlobInfo() call. Must
  // only be called on a DOM thread since blobs are cycle-collected objects.
  already_AddRefed<nsIDOMBlob>
  GetBlob();

  already_AddRefed<DOMFileImpl>
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
  BlobParent(nsIContentParent* aManager, nsIDOMBlob* aBlob);

  BlobParent(PBackgroundParent* aManager, nsIDOMBlob* aBlob);

  BlobParent(PBackgroundParent* aManager, DOMFileImpl* aBlobImpl);

  // These constructors are called on the receiving side.
  BlobParent(nsIContentParent* aManager,
             const ParentBlobConstructorParams& aParams);

  BlobParent(PBackgroundParent* aManager,
             const ParentBlobConstructorParams& aParams);

  // Only destroyed by BackgroundParentImpl and ContentParent.
  ~BlobParent();

  void
  CommonInit(nsIDOMBlob* aBlob);

  void
  CommonInit(const ParentBlobConstructorParams& aParams);

  static BlobParent*
  CreateFromParams(nsIContentParent* aManager,
                   const ParentBlobConstructorParams& aParams);

  static BlobParent*
  CreateFromParams(PBackgroundParent* aManager,
                   const ParentBlobConstructorParams& aParams);

  template <class ParentManagerType>
  static BlobParent*
  SendSliceConstructor(ParentManagerType* aManager,
                       const ParentBlobConstructorParams& aParams,
                       const ChildBlobConstructorParams& aOtherSideParams);

  void
  NoteDyingRemoteBlob();

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
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual PBlobStreamParent*
  AllocPBlobStreamParent() MOZ_OVERRIDE;

  virtual bool
  RecvPBlobStreamConstructor(PBlobStreamParent* aActor) MOZ_OVERRIDE;

  virtual bool
  DeallocPBlobStreamParent(PBlobStreamParent* aActor) MOZ_OVERRIDE;

  virtual bool
  RecvResolveMystery(const ResolveMysteryParams& aParams) MOZ_OVERRIDE;

  virtual bool
  RecvGetFileId(int64_t* aFileId) MOZ_OVERRIDE;

  virtual bool
  RecvGetFilePath(nsString* aFilePath) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ipc_BlobParent_h
