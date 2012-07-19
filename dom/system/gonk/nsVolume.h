/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_nsvolume_h__
#define mozilla_system_nsvolume_h__

#include "Volume.h"
#include "nsIVolume.h"

namespace mozilla {
namespace system {

class nsVolume : public nsIVolume
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVOLUME

  nsVolume(const Volume *aVolume)
    : mName(NS_ConvertUTF8toUTF16(aVolume->Name())),
      mMountPoint(NS_ConvertUTF8toUTF16(aVolume->MountPoint())),
      mState(aVolume->State())
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

  long State() const                  { return mState; }
  const char *StateStr() const        { return Volume::StateStr((Volume::STATE)mState); }

  typedef nsTArray<nsRefPtr<nsVolume> > Array;

private:
  ~nsVolume() {}

protected:
  nsString mName;
  nsString mMountPoint;
  long mState;
};

} // system
} // mozilla

#endif  // mozilla_system_nsvolume_h__
