/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsVolume.h"
#include "nsISupportsUtils.h"
#include "nsIVolume.h"
#include "nsVolumeStat.h"

namespace mozilla {
namespace system {

const char *
NS_VolumeStateStr(PRInt32 aState)
{
  switch (aState) {
    case nsIVolume::STATE_INIT:       return "Init";
    case nsIVolume::STATE_NOMEDIA:    return "NoMedia";
    case nsIVolume::STATE_IDLE:       return "Idle";
    case nsIVolume::STATE_PENDING:    return "Pending";
    case nsIVolume::STATE_CHECKING:   return "Checking";
    case nsIVolume::STATE_MOUNTED:    return "Mounted";
    case nsIVolume::STATE_UNMOUNTING: return "Unmounting";
    case nsIVolume::STATE_FORMATTING: return "Formatting";
    case nsIVolume::STATE_SHARED:     return "Shared";
    case nsIVolume::STATE_SHAREDMNT:  return "Shared-Mounted";
  }
  return "???";
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsVolume, nsIVolume)

NS_IMETHODIMP nsVolume::GetName(nsAString &aName)
{
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP nsVolume::GetMountPoint(nsAString &aMountPoint)
{
  aMountPoint = mMountPoint;
  return NS_OK;
}

NS_IMETHODIMP nsVolume::GetState(PRInt32 *aState)
{
  *aState = mState;
  return NS_OK;
}

NS_IMETHODIMP nsVolume::GetStats(nsIVolumeStat **aResult NS_OUTPARAM)
{
  if (mState != STATE_MOUNTED) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_IF_ADDREF(*aResult = new nsVolumeStat(mMountPoint));
  return NS_OK;
}

} // system
} // mozilla
