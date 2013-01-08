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

/***************************************************************************
* The nsVolumeServiceIOThread is a companion class to the nsVolumeService
* class, but whose methods are called from IOThread.
*/
class VolumeServiceIOThread : public VolumeManager::StateObserver,
                              public Volume::EventObserver,
                              public RefCounted<VolumeServiceIOThread>
{
public:
  VolumeServiceIOThread();
  ~VolumeServiceIOThread();

private:
  void  UpdateAllVolumes();

  virtual void Notify(const VolumeManager::StateChangedEvent &aEvent);
  virtual void Notify(Volume * const &aVolume);

};

void InitVolumeServiceIOThread();
void ShutdownVolumeServiceIOThread();

} // system
} // mozilla

#endif  // mozilla_system_volumeserviceiothread_h__
