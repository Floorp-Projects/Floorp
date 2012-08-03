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
#include "mozilla/dom/PContentChild.h"
#include "mozilla/dom/PContentParent.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"

class nsIDOMBlob;

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
  typedef mozilla::dom::PBlobParent BaseType;
  typedef mozilla::dom::PBlobStreamParent StreamType;
  typedef mozilla::dom::PContentParent ManagerType;
};

template <>
struct BlobTraits<Child>
{
  typedef mozilla::dom::PBlobChild BaseType;
  typedef mozilla::dom::PBlobStreamChild StreamType;
  typedef mozilla::dom::PContentChild ManagerType;
};

template <ActorFlavorEnum>
class RemoteBlob;

template <ActorFlavorEnum ActorFlavor>
class Blob : public BlobTraits<ActorFlavor>::BaseType
{
public:
  typedef typename BlobTraits<ActorFlavor>::BaseType BaseType;
  typedef typename BlobTraits<ActorFlavor>::StreamType StreamType;
  typedef typename BlobTraits<ActorFlavor>::ManagerType ManagerType;
  typedef RemoteBlob<ActorFlavor> RemoteBlobType;
  typedef mozilla::ipc::IProtocolManager<
                      mozilla::ipc::RPCChannel::RPCListener>::ActorDestroyReason
          ActorDestroyReason;
  typedef mozilla::dom::BlobConstructorParams BlobConstructorParams;

protected:
  nsIDOMBlob* mBlob;
  RemoteBlobType* mRemoteBlob;
  bool mOwnsBlob;

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

  already_AddRefed<nsIDOMBlob>
  GetBlob();

  void
  NoteDyingRemoteBlob();

private:
  // This constructor is called on the sending side.
  Blob(nsIDOMBlob* aBlob);

  // This constructor is called on the receiving side.
  Blob(const BlobConstructorParams& aParams);

  // These methods are only called by the IPDL message machinery.
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvPBlobStreamConstructor(StreamType* aActor) MOZ_OVERRIDE;

  virtual StreamType*
  AllocPBlobStream() MOZ_OVERRIDE;

  virtual bool
  DeallocPBlobStream(StreamType* aActor) MOZ_OVERRIDE;
};

} // namespace ipc

typedef mozilla::dom::ipc::Blob<mozilla::dom::ipc::Child> BlobChild;
typedef mozilla::dom::ipc::Blob<mozilla::dom::ipc::Parent> BlobParent;

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ipc_Blob_h
