/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Volume.h"
#include "VolumeCommand.h"
#include "VolumeManager.h"
#include "VolumeManagerLog.h"

namespace mozilla {
namespace system {

Volume::Volume(const nsCSubstring &aName)
  : mState(STATE_INIT),
    mName(aName)
{
  DBG("Volume %s: created", NameStr());
}

void
Volume::SetState(Volume::STATE aNewState)
{
  if (aNewState != mState) {
    LOG("Volume %s: changing state from %s to %s",
         NameStr(), StateStr(mState), StateStr(aNewState));

    mState = aNewState;
  }
}

void
Volume::SetMountPoint(const nsCSubstring &aMountPoint)
{
  if (!mMountPoint.Equals(aMountPoint)) {
    mMountPoint = aMountPoint;
    DBG("Volume %s: Setting mountpoint to '%s'", NameStr(), mMountPoint.get());
  }
}

void
Volume::StartMount(VolumeResponseCallback *aCallback)
{
  StartCommand(new VolumeActionCommand(this, "mount", "", aCallback));
}

void
Volume::StartUnmount(VolumeResponseCallback *aCallback)
{
  StartCommand(new VolumeActionCommand(this, "unmount", "force", aCallback));
}

void
Volume::StartShare(VolumeResponseCallback *aCallback)
{
  StartCommand(new VolumeActionCommand(this, "share", "ums", aCallback));
}

void
Volume::StartUnshare(VolumeResponseCallback *aCallback)
{
  StartCommand(new VolumeActionCommand(this, "unshare", "ums", aCallback));
}

void
Volume::StartCommand(VolumeCommand *aCommand)
{
  VolumeManager::PostCommand(aCommand);
}

} // namespace system
} // namespace mozilla
