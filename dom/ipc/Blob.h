/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ipc_Blob_h
#define mozilla_dom_ipc_Blob_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/PBlobChild.h"
#include "mozilla/dom/PBlobParent.h"
#include "mozilla/dom/PBlobStreamChild.h"
#include "mozilla/dom/PBlobStreamParent.h"
#include "mozilla/dom/PContent.h"

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

class nsIDOMBlob;
class nsIIPCSerializableInputStream;
class nsIInputStream;

namespace mozilla {
namespace dom {
namespace ipc {

enum ActorFlavorEnum
{
  Parent = 0,
  Child
};

template <ActorFlavorEnum>
struct BlobTraits
{ };

template <>
struct BlobTraits<Parent>
{
  typedef mozilla::dom::PBlobParent ProtocolType;
  typedef mozilla::dom::PBlobStreamParent StreamType;
  typedef mozilla::dom::PContentParent ContentManagerType;
  typedef ProtocolType BlobManagerType;

  // BaseType on the parent side is a bit more complicated than for the child
  // side. In the case of nsIInputStreams backed by files we need to ensure that
  // the files are actually opened and closed on a background thread before we
  // can send their file handles across to the child. The child process could
  // crash during this process so we need to make sure we cancel the intended
  // response in such a case. We do that by holding an array of
  // nsRevocableEventPtr. If the child crashes then this actor will be destroyed
  // and the nsRevocableEventPtr destructor will cancel any stream events that
  // are currently in flight.
  class BaseType : public ProtocolType
  {
  protected:
    BaseType();
    virtual ~BaseType();

    class OpenStreamRunnable;
    friend class OpenStreamRunnable;

    void
    NoteRunnableCompleted(OpenStreamRunnable* aRunnable);

    nsTArray<nsRevocableEventPtr<OpenStreamRunnable> > mOpenStreamRunnables;
  };

  static void*
  Allocate(size_t aSize)
  {
    // We want fallible allocation in the parent.
    return moz_malloc(aSize);
  }
};

template <>
struct BlobTraits<Child>
{
  typedef mozilla::dom::PBlobChild ProtocolType;
  typedef mozilla::dom::PBlobStreamChild StreamType;
  typedef mozilla::dom::PContentChild ContentManagerType;
  typedef ProtocolType BlobManagerType;

  class BaseType : public ProtocolType
  {
  protected:
    BaseType()
    { }

    virtual ~BaseType()
    { }
  };

  static void*
  Allocate(size_t aSize)
  {
    // We want infallible allocation in the child.
    return moz_xmalloc(aSize);
  }
};

template <ActorFlavorEnum>
class RemoteBlobBase;
template <ActorFlavorEnum>
class RemoteBlob;
template <ActorFlavorEnum>
class RemoteMemoryBlob;
template <ActorFlavorEnum>
class RemoteMultipartBlob;

template <ActorFlavorEnum ActorFlavor>
class Blob : public BlobTraits<ActorFlavor>::BaseType
{
  friend class RemoteBlobBase<ActorFlavor>;

public:
  typedef typename BlobTraits<ActorFlavor>::ProtocolType ProtocolType;
  typedef typename BlobTraits<ActorFlavor>::StreamType StreamType;
  typedef typename BlobTraits<ActorFlavor>::BaseType BaseType;
  typedef typename BlobTraits<ActorFlavor>::ContentManagerType ContentManagerType;
  typedef typename BlobTraits<ActorFlavor>::BlobManagerType BlobManagerType;
  typedef RemoteBlob<ActorFlavor> RemoteBlobType;
  typedef RemoteMemoryBlob<ActorFlavor> RemoteMemoryBlobType;
  typedef RemoteMultipartBlob<ActorFlavor> RemoteMultipartBlobType;
  typedef mozilla::ipc::IProtocolManager<
                      mozilla::ipc::RPCChannel::RPCListener>::ActorDestroyReason
          ActorDestroyReason;
  typedef mozilla::dom::BlobConstructorParams BlobConstructorParams;

protected:
  nsIDOMBlob* mBlob;
  RemoteBlobType* mRemoteBlob;
  RemoteMemoryBlobType* mRemoteMemoryBlob;
  RemoteMultipartBlobType* mRemoteMultipartBlob;

  ContentManagerType* mContentManager;
  BlobManagerType* mBlobManager;

  bool mOwnsBlob;
  bool mBlobIsFile;

public:
  // This create function is called on the sending side.
  static Blob*
  Create(nsIDOMBlob* aBlob)
  {
    return new Blob(aBlob);
  }

  // This create function is called on the receiving side.
  static Blob*
  Create(const BlobConstructorParams& aParams);

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

  ProtocolType*
  ConstructPBlobOnManager(ProtocolType* aActor,
                          const BlobConstructorParams& aParams);

  bool
  ManagerIs(const ContentManagerType* aManager) const
  {
    return aManager == mContentManager;
  }

  bool
  ManagerIs(const BlobManagerType* aManager) const
  {
    return aManager == mBlobManager;
  }

#ifdef DEBUG
  bool
  HasManager() const
  {
    return !!mContentManager || !!mBlobManager;
  }
#endif

  void
  SetManager(ContentManagerType* aManager);
  void
  SetManager(BlobManagerType* aManager);

  void
  PropagateManager(Blob<ActorFlavor>* aActor) const;

private:
  // This constructor is called on the sending side.
  Blob(nsIDOMBlob* aBlob);

  // These constructors are called on the receiving side.
  Blob(nsRefPtr<RemoteBlobType>& aBlob,
       bool aIsFile);
  Blob(nsRefPtr<RemoteMemoryBlobType>& aBlob,
       bool aIsFile);
  Blob(nsRefPtr<RemoteMultipartBlobType>& aBlob,
       bool aIsFile);

  void
  NoteDyingRemoteBlob();

  // These methods are only called by the IPDL message machinery.
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvResolveMystery(const ResolveMysteryParams& aParams) MOZ_OVERRIDE;

  virtual bool
  RecvPBlobStreamConstructor(StreamType* aActor) MOZ_OVERRIDE;

  virtual bool
  RecvPBlobConstructor(ProtocolType* aActor,
                       const BlobConstructorParams& aParams) MOZ_OVERRIDE;

  virtual StreamType*
  AllocPBlobStream() MOZ_OVERRIDE;

  virtual bool
  DeallocPBlobStream(StreamType* aActor) MOZ_OVERRIDE;

  virtual ProtocolType*
  AllocPBlob(const BlobConstructorParams& params) MOZ_OVERRIDE;

  virtual bool
  DeallocPBlob(ProtocolType* actor) MOZ_OVERRIDE;
};

} // namespace ipc

typedef mozilla::dom::ipc::Blob<mozilla::dom::ipc::Child> BlobChild;
typedef mozilla::dom::ipc::Blob<mozilla::dom::ipc::Parent> BlobParent;

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ipc_Blob_h
