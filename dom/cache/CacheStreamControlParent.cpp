/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheStreamControlParent.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/unused.h"
#include "mozilla/dom/cache/PCacheTypes.h"
#include "mozilla/dom/cache/ReadStream.h"
#include "mozilla/dom/cache/StreamList.h"
#include "mozilla/ipc/FileDescriptorSetParent.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/PFileDescriptorSetParent.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::ipc::FileDescriptor;
using mozilla::ipc::FileDescriptorSetParent;
using mozilla::ipc::OptionalFileDescriptorSet;
using mozilla::ipc::PFileDescriptorSetParent;

// declared in ActorUtils.h
void
DeallocPCacheStreamControlParent(PCacheStreamControlParent* aActor)
{
  delete aActor;
}

CacheStreamControlParent::CacheStreamControlParent()
{
  MOZ_COUNT_CTOR(cache::CacheStreamControlParent);
}

CacheStreamControlParent::~CacheStreamControlParent()
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  MOZ_ASSERT(!mStreamList);
  MOZ_COUNT_DTOR(cache::CacheStreamControlParent);
}

void
CacheStreamControlParent::SerializeControl(PCacheReadStream* aReadStreamOut)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  aReadStreamOut->controlChild() = nullptr;
  aReadStreamOut->controlParent() = this;
}

void
CacheStreamControlParent::SerializeFds(PCacheReadStream* aReadStreamOut,
                                       const nsTArray<FileDescriptor>& aFds)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  PFileDescriptorSetParent* fdSet = nullptr;
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
CacheStreamControlParent::DeserializeFds(const PCacheReadStream& aReadStream,
                                         nsTArray<FileDescriptor>& aFdsOut)
{
  if (aReadStream.fds().type() !=
      OptionalFileDescriptorSet::TPFileDescriptorSetParent) {
    return;
  }

  FileDescriptorSetParent* fdSetActor =
    static_cast<FileDescriptorSetParent*>(aReadStream.fds().get_PFileDescriptorSetParent());
  MOZ_ASSERT(fdSetActor);

  fdSetActor->ForgetFileDescriptors(aFdsOut);
  MOZ_ASSERT(!aFdsOut.IsEmpty());

  if (!fdSetActor->Send__delete__(fdSetActor)) {
    // child process is gone, warn and allow actor to clean up normally
    NS_WARNING("Cache failed to delete fd set actor.");
  }
}

void
CacheStreamControlParent::NoteClosedAfterForget(const nsID& aId)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  RecvNoteClosed(aId);
}

#ifdef DEBUG
void
CacheStreamControlParent::AssertOwningThread()
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
}
#endif

void
CacheStreamControlParent::ActorDestroy(ActorDestroyReason aReason)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  CloseAllReadStreamsWithoutReporting();
  mStreamList->RemoveStreamControl(this);
  mStreamList->NoteClosedAll();
  mStreamList = nullptr;
}

bool
CacheStreamControlParent::RecvNoteClosed(const nsID& aId)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  MOZ_ASSERT(mStreamList);
  mStreamList->NoteClosed(aId);
  return true;
}

void
CacheStreamControlParent::SetStreamList(StreamList* aStreamList)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  MOZ_ASSERT(!mStreamList);
  mStreamList = aStreamList;
}

void
CacheStreamControlParent::Close(const nsID& aId)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  NotifyClose(aId);
  unused << SendClose(aId);
}

void
CacheStreamControlParent::CloseAll()
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  NotifyCloseAll();
  unused << SendCloseAll();
}

void
CacheStreamControlParent::Shutdown()
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  if (!Send__delete__(this)) {
    // child process is gone, allow actor to be destroyed normally
    NS_WARNING("Cache failed to delete stream actor.");
    return;
  }
}

void
CacheStreamControlParent::NotifyClose(const nsID& aId)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  CloseReadStreams(aId);
}

void
CacheStreamControlParent::NotifyCloseAll()
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  CloseAllReadStreams();
}

} // namespace cache
} // namespace dom
} // namespace mozilla
