/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TaskQueue.h"

#include "gfxPrefs.h"
#include "MediaSystemResourceManagerChild.h"
#include "mozilla/layers/ImageBridgeChild.h"

#include "MediaSystemResourceManager.h"

namespace mozilla {

using namespace mozilla::ipc;
using namespace mozilla::layers;

/* static */ StaticRefPtr<MediaSystemResourceManager> MediaSystemResourceManager::sSingleton;

/* static */ MediaSystemResourceManager*
MediaSystemResourceManager::Get()
{
  if (sSingleton) {
    return sSingleton;
  }
  MediaSystemResourceManager::Init();
  return sSingleton;
}

/* static */ void
MediaSystemResourceManager::Shutdown()
{
  MOZ_ASSERT(InImageBridgeChildThread());
  if (sSingleton) {
    sSingleton->CloseIPC();
    sSingleton = nullptr;
  }
}

/* static */ void
MediaSystemResourceManager::Init()
{
  RefPtr<ImageBridgeChild> imageBridge = ImageBridgeChild::GetSingleton();
  if (!imageBridge) {
    NS_WARNING("ImageBridge does not exist");
    return;
  }

  if (InImageBridgeChildThread()) {
    if (!sSingleton) {
#ifdef DEBUG
      static int timesCreated = 0;
      timesCreated++;
      MOZ_ASSERT(timesCreated == 1);
#endif
      sSingleton = new MediaSystemResourceManager();
    }
    return;
  }

  ReentrantMonitor barrier("MediaSystemResourceManager::Init");
  ReentrantMonitorAutoEnter mainThreadAutoMon(barrier);
  bool done = false;

  RefPtr<Runnable> runnable =
    NS_NewRunnableFunction([&]() {
      if (!sSingleton) {
        sSingleton = new MediaSystemResourceManager();
      }
      ReentrantMonitorAutoEnter childThreadAutoMon(barrier);
      done = true;
      barrier.NotifyAll();
    });

  imageBridge->GetMessageLoop()->PostTask(runnable.forget());

  // should stop the thread until done.
  while (!done) {
    barrier.Wait();
  }
}

MediaSystemResourceManager::MediaSystemResourceManager()
  : mReentrantMonitor("MediaSystemResourceManager.mReentrantMonitor")
  , mShutDown(false)
  , mChild(nullptr)
{
  MOZ_ASSERT(InImageBridgeChildThread());
  OpenIPC();
}

MediaSystemResourceManager::~MediaSystemResourceManager()
{
  MOZ_ASSERT(IsIpcClosed());
}

void
MediaSystemResourceManager::OpenIPC()
{
  MOZ_ASSERT(InImageBridgeChildThread());
  MOZ_ASSERT(!mChild);

  media::PMediaSystemResourceManagerChild* child =
    ImageBridgeChild::GetSingleton()->SendPMediaSystemResourceManagerConstructor();
  mChild = static_cast<media::MediaSystemResourceManagerChild*>(child);
  mChild->SetManager(this);
}

void
MediaSystemResourceManager::CloseIPC()
{
  MOZ_ASSERT(InImageBridgeChildThread());

  if (!mChild) {
    return;
  }
  mChild->Destroy();
  mChild = nullptr;
  mShutDown = true;
}

void
MediaSystemResourceManager::OnIpcClosed()
{
  mChild = nullptr;
}

bool
MediaSystemResourceManager::IsIpcClosed()
{
  return mChild ? true : false;
}

void
MediaSystemResourceManager::Register(MediaSystemResourceClient* aClient)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  MOZ_ASSERT(aClient);
  MOZ_ASSERT(!mResourceClients.Get(aClient->mId));

  mResourceClients.Put(aClient->mId, aClient);
}

void
MediaSystemResourceManager::Unregister(MediaSystemResourceClient* aClient)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  MOZ_ASSERT(aClient);
  MOZ_ASSERT(mResourceClients.Get(aClient->mId));
  MOZ_ASSERT(mResourceClients.Get(aClient->mId) == aClient);

  mResourceClients.Remove(aClient->mId);
}

bool
MediaSystemResourceManager::SetListener(MediaSystemResourceClient* aClient,
                                  MediaSystemResourceReservationListener* aListener)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  MOZ_ASSERT(aClient);

  MediaSystemResourceClient* client = mResourceClients.Get(aClient->mId);
  MOZ_ASSERT(client);

  if (!client) {
    return false;
  }
  // State Check
  if (aClient->mResourceState != MediaSystemResourceClient::RESOURCE_STATE_START) {
    return false;
  }
  aClient->mListener = aListener;
  return true;
}

void
MediaSystemResourceManager::Acquire(MediaSystemResourceClient* aClient)
{
  MOZ_ASSERT(aClient);
  MOZ_ASSERT(!InImageBridgeChildThread());

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  MediaSystemResourceClient* client = mResourceClients.Get(aClient->mId);
  MOZ_ASSERT(client);
  MOZ_ASSERT(client == aClient);

  aClient->mIsSync = false; // async request

  if (!client) {
    HandleAcquireResult(aClient->mId, false);
    return;
  }
  // State Check
  if (aClient->mResourceState != MediaSystemResourceClient::RESOURCE_STATE_START) {
    HandleAcquireResult(aClient->mId, false);
    return;
  }
  aClient->mResourceState = MediaSystemResourceClient::RESOURCE_STATE_WAITING;
  ImageBridgeChild::GetSingleton()->GetMessageLoop()->PostTask(
    NewRunnableMethod<uint32_t>(
      this,
      &MediaSystemResourceManager::DoAcquire,
      aClient->mId));
}

bool
MediaSystemResourceManager::AcquireSyncNoWait(MediaSystemResourceClient* aClient)
{
  MOZ_ASSERT(aClient);
  MOZ_ASSERT(!InImageBridgeChildThread());

  ReentrantMonitor barrier("MediaSystemResourceManager::AcquireSyncNoWait");
  ReentrantMonitorAutoEnter autoMon(barrier);
  bool done = false;
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    MediaSystemResourceClient* client = mResourceClients.Get(aClient->mId);
    MOZ_ASSERT(client);
    MOZ_ASSERT(client == aClient);

    aClient->mIsSync = true; // sync request

    if (InImageBridgeChildThread()) {
      HandleAcquireResult(aClient->mId, false);
      return false;
    }
    if (!client || client != aClient) {
      HandleAcquireResult(aClient->mId, false);
      return false;
    }
    // State Check
    if (aClient->mResourceState != MediaSystemResourceClient::RESOURCE_STATE_START) {
      HandleAcquireResult(aClient->mId, false);
      return false;
    }
    // Hold barrier Monitor until acquire task end.
    aClient->mAcquireSyncWaitMonitor = &barrier;
    aClient->mAcquireSyncWaitDone = &done;
    aClient->mResourceState = MediaSystemResourceClient::RESOURCE_STATE_WAITING;
  }

  ImageBridgeChild::GetSingleton()->GetMessageLoop()->PostTask(
    NewRunnableMethod<uint32_t>(
      this,
      &MediaSystemResourceManager::DoAcquire,
      aClient->mId));

  // should stop the thread until done.
  while (!done) {
    barrier.Wait();
  }

  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    if (aClient->mResourceState != MediaSystemResourceClient::RESOURCE_STATE_ACQUIRED) {
      return false;
    }
    return true;
  }
}

void
MediaSystemResourceManager::DoAcquire(uint32_t aId)
{
  MOZ_ASSERT(InImageBridgeChildThread());
  if (mShutDown || !mChild) {
    HandleAcquireResult(aId, false);
    return;
  }
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    MediaSystemResourceClient* client = mResourceClients.Get(aId);
    MOZ_ASSERT(client);

    if (!client ||
        client->mResourceState != MediaSystemResourceClient::RESOURCE_STATE_WAITING) {
      HandleAcquireResult(aId, false);
      return;
    }
    MOZ_ASSERT(aId == client->mId);
    bool willWait = !client->mAcquireSyncWaitMonitor ? true : false;
    mChild->SendAcquire(client->mId,
                        client->mResourceType,
                        willWait);
  }
}

void
MediaSystemResourceManager::ReleaseResource(MediaSystemResourceClient* aClient)
{
  MOZ_ASSERT(aClient);
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    MediaSystemResourceClient* client = mResourceClients.Get(aClient->mId);
    MOZ_ASSERT(client);
    MOZ_ASSERT(client == aClient);

    if (!client ||
        client != aClient ||
        aClient->mResourceState == MediaSystemResourceClient::RESOURCE_STATE_START ||
        aClient->mResourceState == MediaSystemResourceClient::RESOURCE_STATE_END) {

      aClient->mResourceState = MediaSystemResourceClient::RESOURCE_STATE_END;
      return;
    }

    aClient->mResourceState = MediaSystemResourceClient::RESOURCE_STATE_END;

    ImageBridgeChild::GetSingleton()->GetMessageLoop()->PostTask(
      NewRunnableMethod<uint32_t>(
        this,
        &MediaSystemResourceManager::DoRelease,
        aClient->mId));
  }
}

void
MediaSystemResourceManager::DoRelease(uint32_t aId)
{
  MOZ_ASSERT(InImageBridgeChildThread());
  if (mShutDown || !mChild) {
    return;
  }
  mChild->SendRelease(aId);
}

void
MediaSystemResourceManager::RecvResponse(uint32_t aId, bool aSuccess)
{
  HandleAcquireResult(aId, aSuccess);
}

void
MediaSystemResourceManager::HandleAcquireResult(uint32_t aId, bool aSuccess)
{
  if (!InImageBridgeChildThread()) {
    ImageBridgeChild::GetSingleton()->GetMessageLoop()->PostTask(
      NewRunnableMethod<uint32_t, bool>(
        this,
        &MediaSystemResourceManager::HandleAcquireResult,
        aId,
        aSuccess));
    return;
  }

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  MediaSystemResourceClient* client = mResourceClients.Get(aId);
  if (!client) {
    // Client was already unregistered.
    return;
  }
  if (client->mResourceState != MediaSystemResourceClient::RESOURCE_STATE_WAITING) {
    return;
  }

  // Update state
  if (aSuccess) {
    client->mResourceState = MediaSystemResourceClient::RESOURCE_STATE_ACQUIRED;
  } else {
    client->mResourceState = MediaSystemResourceClient::RESOURCE_STATE_NOT_ACQUIRED;
  }

  if (client->mIsSync) {
    if (client->mAcquireSyncWaitMonitor) {
      // Notify AcquireSync() complete
      MOZ_ASSERT(client->mAcquireSyncWaitDone);
      ReentrantMonitorAutoEnter autoMon(*client->mAcquireSyncWaitMonitor);
      *client->mAcquireSyncWaitDone = true;
      client->mAcquireSyncWaitMonitor->NotifyAll();
      client->mAcquireSyncWaitMonitor = nullptr;
      client->mAcquireSyncWaitDone = nullptr;
    }
  } else {
    // Notify Acquire() result
    if (client->mListener) {
      if (aSuccess) {
        client->mListener->ResourceReserved();
      } else {
       client->mListener->ResourceReserveFailed();
      }
    }
  }
}

} // namespace mozilla
