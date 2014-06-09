/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ipc_Blob_h
#define mozilla_dom_ipc_Blob_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/PBlobChild.h"
#include "mozilla/dom/PBlobParent.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"

class nsIDOMBlob;
class nsString;
template <class> class nsRevocableEventPtr;

namespace mozilla {
namespace dom {

class ContentChild;
class ContentParent;
class PBlobStreamChild;
class PBlobStreamParent;

class BlobChild MOZ_FINAL
  : public PBlobChild
{
  friend class ContentChild;

  class RemoteBlob;
  friend class RemoteBlob;

  nsIDOMBlob* mBlob;
  RemoteBlob* mRemoteBlob;
  nsRefPtr<ContentChild> mStrongManager;

  bool mOwnsBlob;
  bool mBlobIsFile;

public:
  // This create function is called on the sending side.
  static BlobChild*
  Create(ContentChild* aManager, nsIDOMBlob* aBlob)
  {
    return new BlobChild(aManager, aBlob);
  }

  // Get the blob associated with this actor. This may always be called on the
  // sending side. It may also be called on the receiving side unless this is a
  // "mystery" blob that has not yet received a SetMysteryBlobInfo() call.
  already_AddRefed<nsIDOMBlob>
  GetBlob();

  // Use this for files.
  bool
  SetMysteryBlobInfo(const nsString& aName,
                     const nsString& aContentType,
                     uint64_t aLength,
                     uint64_t aLastModifiedDate);

  // Use this for non-file blobs.
  bool
  SetMysteryBlobInfo(const nsString& aContentType, uint64_t aLength);

private:
  // This constructor is called on the sending side.
  BlobChild(ContentChild* aManager, nsIDOMBlob* aBlob);

  // This constructor is called on the receiving side.
  BlobChild(ContentChild* aManager, const ChildBlobConstructorParams& aParams);

  // Only destroyed by ContentChild.
  ~BlobChild();

  // This create function is called on the receiving side by ContentChild.
  static BlobChild*
  Create(ContentChild* aManager, const ChildBlobConstructorParams& aParams);

  static already_AddRefed<RemoteBlob>
  CreateRemoteBlob(const ChildBlobConstructorParams& aParams);

  void
  NoteDyingRemoteBlob();

  // These methods are only called by the IPDL message machinery.
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvResolveMystery(const ResolveMysteryParams& aParams) MOZ_OVERRIDE;

  virtual PBlobStreamChild*
  AllocPBlobStreamChild() MOZ_OVERRIDE;

  virtual bool
  RecvPBlobStreamConstructor(PBlobStreamChild* aActor) MOZ_OVERRIDE;

  virtual bool
  DeallocPBlobStreamChild(PBlobStreamChild* aActor) MOZ_OVERRIDE;
};

class BlobParent MOZ_FINAL
  : public PBlobParent
{
  friend class ContentParent;

  class OpenStreamRunnable;
  friend class OpenStreamRunnable;

  class RemoteBlob;
  friend class RemoteBlob;

  nsIDOMBlob* mBlob;
  RemoteBlob* mRemoteBlob;
  nsRefPtr<ContentParent> mStrongManager;

  // nsIInputStreams backed by files must ensure that the files are actually
  // opened and closed on a background thread before we can send their file
  // handles across to the child. The child process could crash during this
  // process so we need to make sure we cancel the intended response in such a
  // case. We do that by holding an array of nsRevocableEventPtr. If the child
  // crashes then this actor will be destroyed and the nsRevocableEventPtr
  // destructor will cancel any stream events that are currently in flight.
  nsTArray<nsRevocableEventPtr<OpenStreamRunnable>> mOpenStreamRunnables;

  bool mOwnsBlob;
  bool mBlobIsFile;

public:
  // This create function is called on the sending side.
  static BlobParent*
  Create(ContentParent* aManager, nsIDOMBlob* aBlob)
  {
    return new BlobParent(aManager, aBlob);
  }

  // Get the blob associated with this actor. This may always be called on the
  // sending side. It may also be called on the receiving side unless this is a
  // "mystery" blob that has not yet received a SetMysteryBlobInfo() call.
  already_AddRefed<nsIDOMBlob>
  GetBlob();

  // Use this for files.
  bool
  SetMysteryBlobInfo(const nsString& aName, const nsString& aContentType,
                     uint64_t aLength, uint64_t aLastModifiedDate);

  // Use this for non-file blobs.
  bool
  SetMysteryBlobInfo(const nsString& aContentType, uint64_t aLength);

private:
  // This constructor is called on the sending side.
  BlobParent(ContentParent* aManager, nsIDOMBlob* aBlob);

  // This constructor is called on the receiving side.
  BlobParent(ContentParent* aManager,
             const ParentBlobConstructorParams& aParams);

  ~BlobParent();

  // This create function is called on the receiving side by ContentParent.
  static BlobParent*
  Create(ContentParent* aManager, const ParentBlobConstructorParams& aParams);

  static already_AddRefed<RemoteBlob>
  CreateRemoteBlob(const ParentBlobConstructorParams& aParams);

  void
  NoteDyingRemoteBlob();

  void
  NoteRunnableCompleted(OpenStreamRunnable* aRunnable);

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
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ipc_Blob_h
