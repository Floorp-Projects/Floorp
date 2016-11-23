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

class nsVolume : public nsIVolume
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIVOLUME

  // This constructor is used by the UpdateVolumeRunnable constructor
  nsVolume(const Volume* aVolume);

  nsVolume(const nsVolume* aVolume);

  bool Equals(nsIVolume* aVolume);
  void UpdateMountLock(nsVolume* aOldVolume);

  void LogState() const;

  const nsString& Name() const        { return mName; }
  nsCString NameStr() const           { return NS_LossyConvertUTF16toASCII(mName); }

  void Dump(const char* aLabel) const;

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
  bool IsUnmounting() const           { return mIsUnmounting; }
  bool IsRemovable() const            { return mIsRemovable; }
  bool IsHotSwappable() const         { return mIsHotSwappable; }

  typedef nsTArray<RefPtr<nsVolume> > Array;

private:
  virtual ~nsVolume() {}  // MozExternalRefCountType complains if this is non-virtual

  friend class nsVolumeService; // Calls the following XxxMountLock functions
  void UpdateMountLock(const nsAString& aMountLockState);
  void UpdateMountLock(bool aMountLocked);

  void SetIsFake(bool aIsFake);
  void SetIsRemovable(bool aIsRemovable);
  void SetIsHotSwappable(bool aIsHotSwappble);
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
  bool     mIsUnmounting;
  bool     mIsRemovable;
  bool     mIsHotSwappable;
};

} // system
} // mozilla

#endif  // mozilla_system_nsvolume_h__
