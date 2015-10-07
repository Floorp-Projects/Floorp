/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_volumemanager_h__
#define mozilla_system_volumemanager_h__

#include <vector>
#include <queue>

#include "base/message_loop.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Observer.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nsTArray.h"

#include "Volume.h"
#include "VolumeCommand.h"

namespace mozilla {
namespace system {

/***************************************************************************
*
*   All of the public API mentioned in this file (unless otherwise
*   mentioned) must run from the IOThread.
*
***************************************************************************/

/***************************************************************************
*
*   The VolumeManager class is a front-end for android's vold service.
*
*   Vold uses a unix socket interface and accepts null-terminated string
*   commands. The following commands were determined by examining the vold
*   source code:
*
*       volume list
*       volume mount <volname>
*       volume unmount <volname> [force]
*       volume debug [on|off]
*       volume format <volname>
*       volume share <volname> <method>
*       volume unshare <volname> <method>
*       volume shared <volname> <method>
*
*           <volname> is the name of the volume as used in /system/etc/vold.fstab
*           <method> is ums
*
*       dump
*
*       share status <method>	(Determines if a particular sharing method is available)
*                             (GB only - not available in ICS)
*
*       storage users		(??? always crashes vold ???)
*
*       asec list
*       asec ...lots more...
*
*       obb list
*       obb ...lots more...
*
*       xwarp enable
*       xwarp disable
*       xwarp status
*
*   There is also a command line tool called vdc, which can be used to send
*   the above commands to vold.
*
*   Currently, only the volume list, share/unshare, and mount/unmount
*   commands are being used.
*
***************************************************************************/

class VolumeManager final : public MessageLoopForIO::LineWatcher
{
  virtual ~VolumeManager();

public:
  NS_INLINE_DECL_REFCOUNTING(VolumeManager)

  typedef nsTArray<RefPtr<Volume>> VolumeArray;

  VolumeManager();

  //-----------------------------------------------------------------------
  //
  // State related methods.
  //
  // The VolumeManager starts off in the STARTING state. Once a connection
  // is established with vold, it asks for a list of volumes, and once the
  // volume list has been received, then the VolumeManager enters the
  // VOLUMES_READY state.
  //
  // If vold crashes, then the VolumeManager will once again enter the
  // STARTING state and try to reestablish a connection with vold.

  enum STATE
  {
    UNINITIALIZED,
    STARTING,
    VOLUMES_READY
  };

  static STATE State();
  static const char* StateStr(STATE aState);
  static const char* StateStr() { return StateStr(State()); }

  class StateChangedEvent
  {
  public:
    StateChangedEvent() {}
  };

  typedef mozilla::Observer<StateChangedEvent>      StateObserver;
  typedef mozilla::ObserverList<StateChangedEvent>  StateObserverList;

  static void RegisterStateObserver(StateObserver* aObserver);
  static void UnregisterStateObserver(StateObserver* aObserver);

  //-----------------------------------------------------------------------

  static void Start();
  static void Dump(const char* aLabel);

  static VolumeArray::size_type NumVolumes();
  static already_AddRefed<Volume> GetVolume(VolumeArray::index_type aIndex);
  static already_AddRefed<Volume> FindVolumeByName(const nsCSubstring& aName);
  static already_AddRefed<Volume> FindAddVolumeByName(const nsCSubstring& aName);
  static bool RemoveVolumeByName(const nsCSubstring& aName);
  static void InitConfig();

  static void       PostCommand(VolumeCommand* aCommand);

protected:

  virtual void OnLineRead(int aFd, nsDependentCSubstring& aMessage);
  virtual void OnFileCanWriteWithoutBlocking(int aFd);
  virtual void OnError();

  static void DefaultConfig();

private:
  bool OpenSocket();

  friend class VolumeListCallback; // Calls SetState

  static void SetState(STATE aNewState);

  void Restart();
  void WriteCommandData();
  void HandleBroadcast(int aResponseCode, nsCString& aResponseLine);

  typedef std::queue<RefPtr<VolumeCommand> > CommandQueue;

  static STATE              mState;
  static StateObserverList  mStateObserverList;

  static const int    kRcvBufSize = 1024;
  ScopedClose         mSocket;
  VolumeArray         mVolumeArray;
  CommandQueue        mCommands;
  bool                mCommandPending;
  MessageLoopForIO::FileDescriptorWatcher mReadWatcher;
  MessageLoopForIO::FileDescriptorWatcher mWriteWatcher;
  RefPtr<VolumeResponseCallback>          mBroadcastCallback;
};

/***************************************************************************
*
*   The initialization/shutdown functions do not need to be called from
*   the IOThread context.
*
***************************************************************************/

/**
 * Initialize the Volume Manager. On initialization, the VolumeManager will
 * attempt to connect with vold and collect the list of volumes that vold
 * knows about.
 */
void InitVolumeManager();

/**
 * Shuts down the Volume Manager.
 */
void ShutdownVolumeManager();

} // system
} // mozilla

#endif  // mozilla_system_volumemanager_h__
