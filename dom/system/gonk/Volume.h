/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_volume_h__
#define mozilla_system_volume_h__

#include "mozilla/RefPtr.h"
#include "nsString.h"
#include "VolumeCommand.h"

namespace mozilla {
namespace system {

/***************************************************************************
*
*   There is an instance of the Volume class for each volume reported
*   from vold.
*
*   Each volume originates from the /system/etv/vold.fstab file.
*
***************************************************************************/

class Volume : public RefCounted<Volume>
{
public:
  // These MUST match the states from android's system/vold/Volume.h header
  enum STATE
  {
    STATE_INIT        = -1,
    STATE_NOMEDIA     = 0,
    STATE_IDLE        = 1,
    STATE_PENDING     = 2,
    STATE_CHECKING    = 3,
    STATE_MOUNTED     = 4,
    STATE_UNMOUNTING  = 5,
    STATE_FORMATTING  = 6,
    STATE_SHARED      = 7,
    STATE_SHAREDMNT   = 8
  };

  Volume(const nsCSubstring &aVolumeName);

  const char *StateStr(STATE aState) const
  {
    switch (aState) {
      case STATE_INIT:        return "Init";
      case STATE_NOMEDIA:     return "NoMedia";
      case STATE_IDLE:        return "Idle";
      case STATE_PENDING:     return "Pending";
      case STATE_CHECKING:    return "Checking";
      case STATE_MOUNTED:     return "Mounted";
      case STATE_UNMOUNTING:  return "Unmounting";
      case STATE_FORMATTING:  return "Formatting";
      case STATE_SHARED:      return "Shared";
      case STATE_SHAREDMNT:   return "Shared-Mounted";
    }
    return "???";
  }
  const char *StateStr() const  { return StateStr(mState); }
  STATE State() const           { return mState; }

  const nsCString &Name() const { return mName; }
  const char *NameStr() const   { return mName.get(); }

  // The mount point is the name of the directory where the volume is mounted.
  // (i.e. path that leads to the files stored on the volume).
  const nsCString &MountPoint() const { return mMountPoint; }

  // The StartXxx functions will queue up a command to the VolumeManager.
  // You can queue up as many commands as you like, and aCallback will
  // be called as each one completes.
  void StartMount(VolumeResponseCallback *aCallback);
  void StartUnmount(VolumeResponseCallback *aCallback);
  void StartShare(VolumeResponseCallback *aCallback);
  void StartUnshare(VolumeResponseCallback *aCallback);

private:
  friend class VolumeManager;       // Calls SetState
  friend class VolumeListCallback;  // Calls SetMountPoint, SetState

  void SetState(STATE aNewState);
  void SetMountPoint(const nsCSubstring &aMountPoint);
  void StartCommand(VolumeCommand *aCommand);

  STATE             mState;
  const nsCString   mName;

  nsCString         mMountPoint;
};

} // system
} // mozilla

#endif  // mozilla_system_volumemanager_h__
