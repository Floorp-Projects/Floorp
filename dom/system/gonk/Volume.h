/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_volume_h__
#define mozilla_system_volume_h__

#include "VolumeCommand.h"
#include "nsIVolume.h"
#include "nsString.h"
#include "mozilla/Observer.h"
#include "mozilla/RefPtr.h"
#include "nsWhitespaceTokenizer.h"

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
  Volume(const nsCSubstring &aVolumeName);

  typedef long STATE; // States are now defined in nsIVolume.idl

  static const char *StateStr(STATE aState) { return NS_VolumeStateStr(aState); }
  const char *StateStr() const  { return StateStr(mState); }
  STATE State() const           { return mState; }

  const nsCString &Name() const { return mName; }
  const char *NameStr() const   { return mName.get(); }

  // The mount point is the name of the directory where the volume is mounted.
  // (i.e. path that leads to the files stored on the volume).
  const nsCString &MountPoint() const { return mMountPoint; }

  bool MediaPresent() const     { return mMediaPresent; }

  typedef mozilla::Observer<Volume *>     EventObserver;
  typedef mozilla::ObserverList<Volume *> EventObserverList;

  // NOTE: that observers must live in the IOThread.
  static void RegisterObserver(EventObserver *aObserver);
  static void UnregisterObserver(EventObserver *aObserver);

private:
  friend class AutoMounter;         // Calls StartXxx
  friend class VolumeManager;       // Calls HandleVoldResponse
  friend class VolumeListCallback;  // Calls SetMountPoint, SetState

  // The StartXxx functions will queue up a command to the VolumeManager.
  // You can queue up as many commands as you like, and aCallback will
  // be called as each one completes.
  void StartMount(VolumeResponseCallback *aCallback);
  void StartUnmount(VolumeResponseCallback *aCallback);
  void StartShare(VolumeResponseCallback *aCallback);
  void StartUnshare(VolumeResponseCallback *aCallback);

  void SetState(STATE aNewState);
  void SetMediaPresent(bool aMediaPresent);
  void SetMountPoint(const nsCSubstring &aMountPoint);
  void StartCommand(VolumeCommand *aCommand);

  void HandleVoldResponse(int aResponseCode, nsCWhitespaceTokenizer &aTokenizer);

  bool              mMediaPresent;
  STATE             mState;
  const nsCString   mName;
  nsCString         mMountPoint;

  static EventObserverList mEventObserverList;
};

} // system
} // mozilla

#endif  // mozilla_system_volumemanager_h__
