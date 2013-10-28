/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VolumeServiceIOThread.h"
#include "base/message_loop.h"
#include "nsVolumeService.h"
#include "nsXULAppAPI.h"
#include "Volume.h"
#include "VolumeManager.h"

namespace mozilla {
namespace system {

VolumeServiceIOThread::VolumeServiceIOThread(nsVolumeService* aVolumeService)
  : mVolumeService(aVolumeService)
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  VolumeManager::RegisterStateObserver(this);
  Volume::RegisterObserver(this);
  UpdateAllVolumes();
}

VolumeServiceIOThread::~VolumeServiceIOThread()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  Volume::UnregisterObserver(this);
  VolumeManager::UnregisterStateObserver(this);
}

void
VolumeServiceIOThread::Notify(Volume* const & aVolume)
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  if (VolumeManager::State() != VolumeManager::VOLUMES_READY) {
    return;
  }
  mVolumeService->UpdateVolumeIOThread(aVolume);
}

void
VolumeServiceIOThread::Notify(const VolumeManager::StateChangedEvent& aEvent)
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  UpdateAllVolumes();
}

void
VolumeServiceIOThread::UpdateAllVolumes()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  if (VolumeManager::State() != VolumeManager::VOLUMES_READY) {
    return;
  }
  VolumeManager::VolumeArray::size_type numVolumes = VolumeManager::NumVolumes();
  VolumeManager::VolumeArray::index_type volIndex;

  for (volIndex = 0; volIndex < numVolumes; volIndex++) {
    RefPtr<Volume>  vol = VolumeManager::GetVolume(volIndex);
    mVolumeService->UpdateVolumeIOThread(vol);
  }
}

static StaticRefPtr<VolumeServiceIOThread> sVolumeServiceIOThread;

void
InitVolumeServiceIOThread(nsVolumeService* const & aVolumeService)
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  sVolumeServiceIOThread = new VolumeServiceIOThread(aVolumeService);
}

void
ShutdownVolumeServiceIOThread()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  sVolumeServiceIOThread = nullptr;
}

} // system
} // mozilla
