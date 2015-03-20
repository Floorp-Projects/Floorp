/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheStreamControlChild.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/unused.h"
#include "mozilla/dom/cache/ActorUtils.h"
#include "mozilla/dom/cache/PCacheTypes.h"
#include "mozilla/dom/cache/ReadStream.h"
#include "mozilla/ipc/FileDescriptorSetChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PFileDescriptorSetChild.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::ipc::FileDescriptor;
using mozilla::ipc::FileDescriptorSetChild;
using mozilla::ipc::OptionalFileDescriptorSet;
using mozilla::ipc::PFileDescriptorSetChild;

// declared in ActorUtils.h
PCacheStreamControlChild*
AllocPCacheStreamControlChild()
{
  return new CacheStreamControlChild();
}

// declared in ActorUtils.h
void
DeallocPCacheStreamControlChild(PCacheStreamControlChild* aActor)
{
  delete aActor;
}

CacheStreamControlChild::CacheStreamControlChild()
  : mDestroyStarted(false)
{
  MOZ_COUNT_CTOR(cache::CacheStreamControlChild);
}

CacheStreamControlChild::~CacheStreamControlChild()
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  MOZ_COUNT_DTOR(cache::CacheStreamControlChild);
}

void
CacheStreamControlChild::StartDestroy()
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  // This can get called twice under some circumstances.  For example, if the
  // actor is added to a Feature that has already been notified and the Cache
  // actor has no mListener.
  if (mDestroyStarted) {
    return;
  }
  mDestroyStarted = true;

  // Begin shutting down all streams.  This is the same as if the parent had
  // asked us to shutdown.  So simulate the CloseAll IPC message.
  RecvCloseAll();
}

void
CacheStreamControlChild::SerializeControl(PCacheReadStream* aReadStreamOut)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  aReadStreamOut->controlParent() = nullptr;
  aReadStreamOut->controlChild() = this;
}

void
CacheStreamControlChild::SerializeFds(PCacheReadStream* aReadStreamOut,
                                      const nsTArray<FileDescriptor>& aFds)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  PFileDescriptorSetChild* fdSet = nullptr;
  if (!aFds.IsEmpty()) {
    fdSet = Manager()->SendPFileDescriptorSetConstructor(aFds[0]);
    for (uint32_t i = 1; i < aFds.Length(); ++i) {
      unused << fdSet->SendAddFileDescriptor(aFds[i]);
    }
  }

  if (fdSet) {
    aReadStreamOut->fds() = fdSet;
  } else {
    aReadStreamOut->fds() = void_t();
  }
}

void
CacheStreamControlChild::DeserializeFds(const PCacheReadStream& aReadStream,
                                        nsTArray<FileDescriptor>& aFdsOut)
{
  if (aReadStream.fds().type() !=
      OptionalFileDescriptorSet::TPFileDescriptorSetChild) {
    return;
  }

  auto fdSetActor = static_cast<FileDescriptorSetChild*>(
    aReadStream.fds().get_PFileDescriptorSetChild());
  MOZ_ASSERT(fdSetActor);

  fdSetActor->ForgetFileDescriptors(aFdsOut);
  MOZ_ASSERT(!aFdsOut.IsEmpty());

  unused << fdSetActor->Send__delete__(fdSetActor);
}

void
CacheStreamControlChild::NoteClosedAfterForget(const nsID& aId)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  unused << SendNoteClosed(aId);
}

#ifdef DEBUG
void
CacheStreamControlChild::AssertOwningThread()
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
}
#endif

void
CacheStreamControlChild::ActorDestroy(ActorDestroyReason aReason)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  CloseAllReadStreamsWithoutReporting();
  RemoveFeature();
}

bool
CacheStreamControlChild::RecvClose(const nsID& aId)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  CloseReadStreams(aId);
  return true;
}

bool
CacheStreamControlChild::RecvCloseAll()
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  CloseAllReadStreams();
  return true;
}

} // namespace cache
} // namespace dom
} // namespace mozilla
