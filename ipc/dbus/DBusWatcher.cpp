/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DBusWatcher.h"
#include "mozilla/Unused.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace ipc {

DBusWatcher::DBusWatcher(DBusConnection* aConnection, DBusWatch* aWatch)
  : mConnection(aConnection)
  , mWatch(aWatch)
{
  MOZ_ASSERT(mConnection);
  MOZ_ASSERT(mWatch);
}

DBusWatcher::~DBusWatcher()
{ }

DBusConnection*
DBusWatcher::GetConnection()
{
  return mConnection;
}

void
DBusWatcher::StartWatching()
{
  MOZ_ASSERT(!NS_IsMainThread());

  auto flags = dbus_watch_get_flags(mWatch);

  if (!(flags & (DBUS_WATCH_READABLE|DBUS_WATCH_WRITABLE))) {
    return;
  }

  auto ioLoop = MessageLoopForIO::current();

  auto fd = dbus_watch_get_unix_fd(mWatch);

  if (flags & DBUS_WATCH_READABLE) {
    ioLoop->WatchFileDescriptor(fd, true, MessageLoopForIO::WATCH_READ,
                                &mReadWatcher, this);
  }
  if (flags & DBUS_WATCH_WRITABLE) {
    ioLoop->WatchFileDescriptor(fd, true, MessageLoopForIO::WATCH_WRITE,
                                &mWriteWatcher, this);
  }
}

void
DBusWatcher::StopWatching()
{
  MOZ_ASSERT(!NS_IsMainThread());

  auto flags = dbus_watch_get_flags(mWatch);

  if (flags & DBUS_WATCH_READABLE) {
    mReadWatcher.StopWatchingFileDescriptor();
  }
  if (flags & DBUS_WATCH_WRITABLE) {
    mWriteWatcher.StopWatchingFileDescriptor();
  }
}

// DBus utility functions, used as function pointers in DBus setup

void
DBusWatcher::FreeFunction(void* aData)
{
  UniquePtr<DBusWatcher> watcher(static_cast<DBusWatcher*>(aData));
}

dbus_bool_t
DBusWatcher::AddWatchFunction(DBusWatch* aWatch, void* aData)
{
  MOZ_ASSERT(!NS_IsMainThread());

  auto connection = static_cast<DBusConnection*>(aData);

  UniquePtr<DBusWatcher> dbusWatcher =
    MakeUnique<DBusWatcher>(connection, aWatch);

  dbus_watch_set_data(aWatch, dbusWatcher.get(), DBusWatcher::FreeFunction);

  if (dbus_watch_get_enabled(aWatch)) {
    dbusWatcher->StartWatching();
  }

  Unused << dbusWatcher.release(); // picked up in |FreeFunction|

  return TRUE;
}

void
DBusWatcher::RemoveWatchFunction(DBusWatch* aWatch, void* aData)
{
  MOZ_ASSERT(!NS_IsMainThread());

  auto dbusWatcher = static_cast<DBusWatcher*>(dbus_watch_get_data(aWatch));

  dbusWatcher->StopWatching();
}

void
DBusWatcher::ToggleWatchFunction(DBusWatch* aWatch, void* aData)
{
  MOZ_ASSERT(!NS_IsMainThread());

  auto dbusWatcher = static_cast<DBusWatcher*>(dbus_watch_get_data(aWatch));

  if (dbus_watch_get_enabled(aWatch)) {
    dbusWatcher->StartWatching();
  } else {
    dbusWatcher->StopWatching();
  }
}

// I/O-loop callbacks

void
DBusWatcher::OnFileCanReadWithoutBlocking(int aFd)
{
  MOZ_ASSERT(!NS_IsMainThread());

  dbus_watch_handle(mWatch, DBUS_WATCH_READABLE);

  DBusDispatchStatus dbusDispatchStatus;

  do {
    dbusDispatchStatus = dbus_connection_dispatch(mConnection);
  } while (dbusDispatchStatus == DBUS_DISPATCH_DATA_REMAINS);
}

void
DBusWatcher::OnFileCanWriteWithoutBlocking(int aFd)
{
  MOZ_ASSERT(!NS_IsMainThread());

  dbus_watch_handle(mWatch, DBUS_WATCH_WRITABLE);
}

} // namespace ipc
} // namespace mozilla
