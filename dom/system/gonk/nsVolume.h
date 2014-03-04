/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_nsvolume_h__
#define mozilla_system_nsvolume_h__

#include "nsCOMPtr.h"
#include "nsIVolume.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {
namespace system {

class Volume;
class VolumeMountLock;

class nsVolume : public nsIVolume
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIVOLUME

  // This constructor is used by the UpdateVolumeRunnable constructor
  nsVolume(const Volume* aVolume);

  // This constructor is used by ContentChild::RecvFileSystemUpdate which is
  // used to update the volume cache maintained in the child process.
  nsVolume(const nsAString& aName, const nsAString& aMountPoint,
           const int32_t& aState, const int32_t& aMountGeneration,
           const bool& aIsMediaPresent, const bool& aIsSharing,
           const bool& aIsFormatting)
    : mName(aName),
      mMountPoint(aMountPoint),
      mState(aState),
      mMountGeneration(aMountGeneration),
      mMountLocked(false),
      mIsFake(false),
      mIsMediaPresent(aIsMediaPresent),
      mIsSharing(aIsSharing),
      mIsFormatting(aIsFormatting)
  {
  }

  // This constructor is used by nsVolumeService::FindAddVolumeByName, and
  // will be followed shortly by a Set call.
  nsVolume(const nsAString& aName)
    : mName(aName),
      mState(STATE_INIT),
      mMountGeneration(-1),
      mMountLocked(true),  // Needs to agree with Volume::Volume
      mIsFake(false),
      mIsMediaPresent(false),
      mIsSharing(false),
      mIsFormatting(false)
  {
  }

  bool Equals(nsIVolume* aVolume);
  void Set(nsIVolume* aVolume);

  void LogState() const;

  const nsString& Name() const        { return mName; }
  nsCString NameStr() const           { return NS_LossyConvertUTF16toASCII(mName); }

  int32_t MountGeneration() const     { return mMountGeneration; }
  bool IsMountLocked() const          { return mMountLocked; }

  const nsString& MountPoint() const  { return mMountPoint; }
  nsCString MountPointStr() const     { return NS_LossyConvertUTF16toASCII(mMountPoint); }

  int32_t State() const               { return mState; }
  const char* StateStr() const        { return NS_VolumeStateStr(mState); }

  bool IsFake() const                 { return mIsFake; }
  bool IsMediaPresent() const         { return mIsMediaPresent; }
  bool IsSharing() const              { return mIsSharing; }
  bool IsFormatting() const           { return mIsFormatting; }

  typedef nsTArray<nsRefPtr<nsVolume> > Array;

private:
  ~nsVolume() {}

  friend class nsVolumeService; // Calls the following XxxMountLock functions
  void UpdateMountLock(const nsAString& aMountLockState);
  void UpdateMountLock(bool aMountLocked);

  void SetIsFake(bool aIsFake);
  void SetState(int32_t aState);
  static void FormatVolumeIOThread(const nsCString& aVolume);
  static void MountVolumeIOThread(const nsCString& aVolume);
  static void UnmountVolumeIOThread(const nsCString& aVolume);

  nsString mName;
  nsString mMountPoint;
  int32_t  mState;
  int32_t  mMountGeneration;
  bool     mMountLocked;
  bool     mIsFake;
  bool     mIsMediaPresent;
  bool     mIsSharing;
  bool     mIsFormatting;
};

} // system
} // mozilla

#endif  // mozilla_system_nsvolume_h__
