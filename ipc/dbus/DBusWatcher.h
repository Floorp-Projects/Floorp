/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_DBusWatcher_h
#define mozilla_ipc_DBusWatcher_h

#include <dbus/dbus.h>
#include "base/message_loop.h"

namespace mozilla {
namespace ipc {

class DBusWatcher : public MessageLoopForIO::Watcher
{
public:
  DBusWatcher(DBusConnection* aConnection, DBusWatch* aWatch);
  ~DBusWatcher();

  void StartWatching();
  void StopWatching();

  static void        FreeFunction(void* aData);
  static dbus_bool_t AddWatchFunction(DBusWatch* aWatch, void* aData);
  static void        RemoveWatchFunction(DBusWatch* aWatch, void* aData);
  static void        ToggleWatchFunction(DBusWatch* aWatch, void* aData);

  DBusConnection* GetConnection();

private:
  void OnFileCanReadWithoutBlocking(int aFd);
  void OnFileCanWriteWithoutBlocking(int aFd);

  // Read watcher for libevent. Only to be accessed on IO Thread.
  MessageLoopForIO::FileDescriptorWatcher mReadWatcher;

  // Write watcher for libevent. Only to be accessed on IO Thread.
  MessageLoopForIO::FileDescriptorWatcher mWriteWatcher;

  // DBus structures
  DBusConnection* mConnection;
  DBusWatch* mWatch;
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_DBusWatcher_h
