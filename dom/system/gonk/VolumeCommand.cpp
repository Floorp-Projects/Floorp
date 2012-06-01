/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"
#include "nsWhitespaceTokenizer.h"

#include "Volume.h"
#include "VolumeCommand.h"
#include "VolumeManager.h"
#include "VolumeManagerLog.h"

namespace mozilla {
namespace system {

/***************************************************************************
*
* The VolumeActionCommand class is used to send commands which apply
* to a particular volume.
*
* The following commands would fit into this category:
*
*   volume mount <volname>
*   volume unmount <volname> [force]
*   volume format <volname>
*   volume share <volname> <method>
*   volume unshare <volname> <method>
*   volume shared <volname> <method>
*
* A typical response looks like:
*
*   # vdc volume unshare sdcard ums
*   605 Volume sdcard /mnt/sdcard state changed from 7 (Shared-Unmounted) to 1 (Idle-Unmounted)
*   200 volume operation succeeded
*
*   Note that the 600 series of responses are considered unsolicited and
*   are dealt with directly by the VolumeManager. This command will only
*   see the terminating response code (200 in the example above).
*
***************************************************************************/

VolumeActionCommand::VolumeActionCommand(Volume *aVolume,
                                         const char *aAction,
                                         const char *aExtraArgs,
                                         VolumeResponseCallback *aCallback)
  : VolumeCommand(aCallback),
    mVolume(aVolume)
{
  nsCAutoString cmd;

  cmd = "volume ";
  cmd += aAction;
  cmd += " ";
  cmd += aVolume->Name().get();

  // vold doesn't like trailing white space, so only add it if we really need to.
  if (aExtraArgs && (*aExtraArgs != '\0')) {
    cmd += " ";
    cmd += aExtraArgs;
  }
  SetCmd(cmd);
}

/***************************************************************************
*
* The VolumeListCommand class is used to send the "volume list" command to
* vold.
*
* A typical response looks like:
*
*   # vdc volume list
*   110 sdcard /mnt/sdcard 4
*   110 sdcard1 /mnt/sdcard/external_sd 4
*   200 Volumes listed.
*
***************************************************************************/

VolumeListCommand::VolumeListCommand(VolumeResponseCallback *aCallback)
  : VolumeCommand(NS_LITERAL_CSTRING("volume list"), aCallback)
{
}

} // system
} // mozilla

