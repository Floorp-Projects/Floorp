/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_volumeserviceiothread_h__
#define mozilla_system_volumeserviceiothread_h__

#include "Volume.h"
#include "VolumeManager.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace system {

class nsVolumeService;

/***************************************************************************
* The nsVolumeServiceIOThread is a companion class to the nsVolumeService
* class, but whose methods are called from IOThread.
*/
class VolumeServiceIOThread : public VolumeManager::StateObserver,
                              public Volume::EventObserver
{
  ~VolumeServiceIOThread();

public:
  NS_INLINE_DECL_REFCOUNTING(VolumeServiceIOThread)

  VolumeServiceIOThread(nsVolumeService* aVolumeService);

private:
  void  UpdateAllVolumes();

  virtual void Notify(const VolumeManager::StateChangedEvent& aEvent);
  virtual void Notify(Volume* const & aVolume);

  RefPtr<nsVolumeService>   mVolumeService;
};

void InitVolumeServiceIOThread(nsVolumeService* const & aVolumeService);
void ShutdownVolumeServiceIOThread();
void FormatVolume(const nsCString& aVolume);
void MountVolume(const nsCString& aVolume);
void UnmountVolume(const nsCString& aVolume);

} // system
} // mozilla

#endif  // mozilla_system_volumeserviceiothread_h__
