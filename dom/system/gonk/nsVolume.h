/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_nsvolume_h__
#define mozilla_system_nsvolume_h__

#include "nsIVolume.h"
#include "nsString.h"

namespace mozilla {
namespace system {

class Volume;

class nsVolume : public nsIVolume
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVOLUME

  nsVolume(const Volume *aVolume);

  nsVolume(const nsAString &aName, const nsAString &aMountPoint, const int32_t &aState)
    : mName(aName), mMountPoint(aMountPoint), mState(aState)
  {
  }

  nsVolume(const nsAString &aName)
    : mName(aName),
      mState(STATE_INIT)
  {
  }

  bool Equals(const nsVolume *aVolume)
  {
    return mName.Equals(aVolume->mName)
        && mMountPoint.Equals(aVolume->mMountPoint)
        && (mState == aVolume->mState);
  }

  void Set(const nsVolume *aVolume)
  {
    mName = aVolume->mName;
    mMountPoint = aVolume->mMountPoint;
    mState = aVolume->mState;
  }

  const nsString &Name() const        { return mName; }
  const char *NameStr() const         { return NS_LossyConvertUTF16toASCII(mName).get(); }

  const nsString &MountPoint() const  { return mMountPoint; }
  const char *MountPointStr() const   { return NS_LossyConvertUTF16toASCII(mMountPoint).get(); }

  int32_t State() const               { return mState; }
  const char *StateStr() const        { return NS_VolumeStateStr(mState); }

  typedef nsTArray<nsRefPtr<nsVolume> > Array;

private:
  ~nsVolume() {}

protected:
  nsString mName;
  nsString mMountPoint;
  int32_t  mState;
};

} // system
} // mozilla

#endif  // mozilla_system_nsvolume_h__
