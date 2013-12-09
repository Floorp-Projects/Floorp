/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/*
 * Copyright 2009, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * NOTE: Due to being based on the dbus compatibility layer for
 * android's bluetooth implementation, this file is licensed under the
 * apache license instead of MPL.
 *
 */

#include "DBusThread.h"
#include "RawDBusConnection.h"
#include "DBusUtils.h"

#include <dbus/dbus.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/types.h>

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>

#include <list>

#include "base/eintr_wrapper.h"
#include "base/message_loop.h"
#include "nsTArray.h"
#include "nsDataHashtable.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/NullPtr.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Monitor.h"
#include "mozilla/FileUtils.h"
#include "nsThreadUtils.h"
#include "nsIThread.h"
#include "nsXULAppAPI.h"
#include "nsServiceManagerUtils.h"
#include "nsCOMPtr.h"

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GonkDBus", args);
#else
#define BTDEBUG true
#define LOG(args...) if (BTDEBUG) printf(args);
#endif

#define DEFAULT_INITIAL_POLLFD_COUNT 8

namespace mozilla {
namespace ipc {

class DBusWatcher : public RawDBusConnection
{
public:
  DBusWatcher()
  { }

  ~DBusWatcher()
  { }

  bool Initialize();
  void CleanUp();

  void WakeUp();
  bool Stop();

  bool Poll();

  bool AddWatch(DBusWatch* aWatch);
  void RemoveWatch(DBusWatch* aWatch);

  void HandleWatchAdd();
  void HandleWatchRemove();

  // Information about the sockets we're polling. Socket counts
  // increase/decrease depending on how many add/remove watch signals
  // we're received via the control sockets.
  nsTArray<pollfd> mPollData;
  nsTArray<DBusWatch*> mWatchData;

  // Sockets for receiving dbus control information (watch
  // add/removes, loop shutdown, etc...)
  ScopedClose mControlFdR;
  ScopedClose mControlFdW;

private:
  struct PollFdComparator {
    bool Equals(const pollfd& a, const pollfd& b) const {
      return ((a.fd == b.fd) && (a.events == b.events));
    }
    bool LessThan(const pollfd& a, const pollfd&b) const {
      return false;
    }
  };

  enum DBusEventTypes {
    DBUS_EVENT_LOOP_EXIT = 1,
    DBUS_EVENT_LOOP_ADD = 2,
    DBUS_EVENT_LOOP_REMOVE = 3,
    DBUS_EVENT_LOOP_WAKEUP = 4
  };

  static unsigned int UnixEventsToDBusFlags(short events);
  static short        DBusFlagsToUnixEvents(unsigned int flags);

  static dbus_bool_t AddWatchFunction(DBusWatch* aWatch, void* aData);
  static void        RemoveWatchFunction(DBusWatch* aWatch, void* aData);
  static void        ToggleWatchFunction(DBusWatch* aWatch, void* aData);
  static void        DBusWakeupFunction(void* aData);

  bool SetUp();
};

bool
DBusWatcher::Initialize()
{
  if (!SetUp()) {
    CleanUp();
    return false;
  }

  return true;
}

void
DBusWatcher::CleanUp()
{
  MOZ_ASSERT(!NS_IsMainThread());

  dbus_connection_set_wakeup_main_function(mConnection, nullptr,
                                           nullptr, nullptr);
  dbus_bool_t success = dbus_connection_set_watch_functions(mConnection,
                                                            nullptr, nullptr,
                                                            nullptr, nullptr,
                                                            nullptr);
  if (success != TRUE) {
    NS_WARNING("dbus_connection_set_watch_functions failed");
  }

#ifdef DEBUG
  LOG("Removing DBus Sockets\n");
#endif
  if (mControlFdW.get()) {
    mControlFdW.dispose();
  }
  if (mControlFdR.get()) {
    mControlFdR.dispose();
  }
  mPollData.Clear();

  // DBusWatch pointers are maintained by DBus, so we won't leak by
  // clearing.
  mWatchData.Clear();
}

void
DBusWatcher::WakeUp()
{
  static const char control = DBUS_EVENT_LOOP_WAKEUP;

  struct pollfd fds = {
    mControlFdW.get(),
    POLLOUT,
    0
  };

  int nfds = TEMP_FAILURE_RETRY(poll(&fds, 1, 0));
  NS_ENSURE_TRUE_VOID(nfds == 1);
  NS_ENSURE_TRUE_VOID(fds.revents == POLLOUT);

  ssize_t res = TEMP_FAILURE_RETRY(
    write(mControlFdW.get(), &control, sizeof(control)));
  if (res < 0) {
    NS_WARNING("Cannot write wakeup bit to DBus controller!");
  }
}

bool
DBusWatcher::Stop()
{
  static const char data = DBUS_EVENT_LOOP_EXIT;

  ssize_t res =
    TEMP_FAILURE_RETRY(write(mControlFdW.get(), &data, sizeof(data)));
  NS_ENSURE_TRUE(res == 1, false);

  return true;
}

bool
DBusWatcher::Poll()
{
  int res = TEMP_FAILURE_RETRY(poll(mPollData.Elements(),
                                    mPollData.Length(), -1));
  NS_ENSURE_TRUE(res > 0, false);

  bool continueThread = true;

  nsTArray<pollfd>::size_type i = 0;

  while (i < mPollData.Length()) {
    if (mPollData[i].revents == POLLIN) {
      if (mPollData[i].fd == mControlFdR.get()) {
        char data;
        res = TEMP_FAILURE_RETRY(read(mControlFdR.get(), &data, sizeof(data)));
        NS_ENSURE_TRUE(res > 0, false);

        switch (data) {
          case DBUS_EVENT_LOOP_EXIT:
            continueThread = false;
            break;
          case DBUS_EVENT_LOOP_ADD:
            HandleWatchAdd();
            break;
          case DBUS_EVENT_LOOP_REMOVE:
            HandleWatchRemove();
            // don't increment i, or we'll skip one element
            continue;
          case DBUS_EVENT_LOOP_WAKEUP:
            NS_ProcessPendingEvents(NS_GetCurrentThread(),
                                    PR_INTERVAL_NO_TIMEOUT);
            break;
          default:
#if DEBUG
            nsCString warning("unknown command ");
            warning.AppendInt(data);
            NS_WARNING(warning.get());
#endif
            break;
        }
      } else {
        short events = mPollData[i].revents;
        mPollData[i].revents = 0;

        dbus_watch_handle(mWatchData[i], UnixEventsToDBusFlags(events));

        DBusDispatchStatus dbusDispatchStatus;
        do {
          dbusDispatchStatus = dbus_connection_dispatch(GetConnection());
        } while (dbusDispatchStatus == DBUS_DISPATCH_DATA_REMAINS);

        // Break at this point since we don't know if the operation
        // was destructive
        break;
      }
    }

    ++i;
  }

  return continueThread;
}

bool
DBusWatcher::AddWatch(DBusWatch* aWatch)
{
  static const char control = DBUS_EVENT_LOOP_ADD;

  if (dbus_watch_get_enabled(aWatch) == FALSE) {
    return true;
  }

  // note that we can't just send the watch and inspect it later
  // because we may get a removeWatch call before this data is reacted
  // to by our eventloop and remove this watch..  reading the add first
  // and then inspecting the recently deceased watch would be bad.
  ssize_t res =
    TEMP_FAILURE_RETRY(write(mControlFdW.get(),&control, sizeof(control)));
  if (res < 0) {
    LOG("Cannot write DBus add watch control data to socket!\n");
    return false;
  }

  int fd = dbus_watch_get_unix_fd(aWatch);
  res = TEMP_FAILURE_RETRY(write(mControlFdW.get(), &fd, sizeof(fd)));
  if (res < 0) {
    LOG("Cannot write DBus add watch descriptor data to socket!\n");
    return false;
  }

  unsigned int flags = dbus_watch_get_flags(aWatch);
  res = TEMP_FAILURE_RETRY(write(mControlFdW.get(), &flags, sizeof(flags)));
  if (res < 0) {
    LOG("Cannot write DBus add watch flag data to socket!\n");
    return false;
  }

  res = TEMP_FAILURE_RETRY(write(mControlFdW.get(), &aWatch, sizeof(aWatch)));
  if (res < 0) {
    LOG("Cannot write DBus add watch struct data to socket!\n");
    return false;
  }

  return true;
}

void
DBusWatcher::RemoveWatch(DBusWatch* aWatch)
{
  static const char control = DBUS_EVENT_LOOP_REMOVE;

  ssize_t res =
    TEMP_FAILURE_RETRY(write(mControlFdW.get(), &control, sizeof(control)));
  if (res < 0) {
    LOG("Cannot write DBus remove watch control data to socket!\n");
    return;
  }

  int fd = dbus_watch_get_unix_fd(aWatch);
  res = TEMP_FAILURE_RETRY(write(mControlFdW.get(), &fd, sizeof(fd)));
  if (res < 0) {
    LOG("Cannot write DBus remove watch descriptor data to socket!\n");
    return;
  }

  unsigned int flags = dbus_watch_get_flags(aWatch);
  res = TEMP_FAILURE_RETRY(write(mControlFdW.get(), &flags, sizeof(flags)));
  if (res < 0) {
    LOG("Cannot write DBus remove watch flag data to socket!\n");
    return;
  }
}

void
DBusWatcher::HandleWatchAdd()
{
  int fd;
  ssize_t res = TEMP_FAILURE_RETRY(read(mControlFdR.get(), &fd, sizeof(fd)));
  if (res < 0) {
    LOG("Cannot read DBus watch add descriptor data from socket!\n");
    return;
  }

  unsigned int flags;
  res = TEMP_FAILURE_RETRY(read(mControlFdR.get(), &flags, sizeof(flags)));
  if (res < 0) {
    LOG("Cannot read DBus watch add flag data from socket!\n");
    return;
  }

  DBusWatch* watch;
  res = TEMP_FAILURE_RETRY(read(mControlFdR.get(), &watch, sizeof(watch)));
  if (res < 0) {
    LOG("Cannot read DBus watch add watch data from socket!\n");
    return;
  }

  struct pollfd p = {
    fd, // .fd
    DBusFlagsToUnixEvents(flags), // .events
    0 // .revents
  };
  if (mPollData.Contains(p, PollFdComparator())) {
    return;
  }
  mPollData.AppendElement(p);
  mWatchData.AppendElement(watch);
}

void
DBusWatcher::HandleWatchRemove()
{
  int fd;
  ssize_t res = TEMP_FAILURE_RETRY(read(mControlFdR.get(), &fd, sizeof(fd)));
  if (res < 0) {
    LOG("Cannot read DBus watch remove descriptor data from socket!\n");
    return;
  }

  unsigned int flags;
  res = TEMP_FAILURE_RETRY(read(mControlFdR.get(), &flags, sizeof(flags)));
  if (res < 0) {
    LOG("Cannot read DBus watch remove flag data from socket!\n");
    return;
  }

  struct pollfd p = {
    fd, // .fd
    DBusFlagsToUnixEvents(flags), // .events
    0 // .revents
  };
  int index = mPollData.IndexOf(p, 0, PollFdComparator());
  // There are times where removes can be requested for watches that
  // haven't been added (for example, whenever gecko comes up after
  // adapters have already been enabled), so check to make sure we're
  // using the watch in the first place
  if (index < 0) {
    LOG("DBus requested watch removal of non-existant socket, ignoring...");
    return;
  }
  mPollData.RemoveElementAt(index);

  // DBusWatch pointers are maintained by DBus, so we won't leak by
  // removing.
  mWatchData.RemoveElementAt(index);
}

// Flag conversion

unsigned int
DBusWatcher::UnixEventsToDBusFlags(short events)
{
  return (events & DBUS_WATCH_READABLE ? POLLIN : 0) |
         (events & DBUS_WATCH_WRITABLE ? POLLOUT : 0) |
         (events & DBUS_WATCH_ERROR ? POLLERR : 0) |
         (events & DBUS_WATCH_HANGUP ? POLLHUP : 0);
}

short
DBusWatcher::DBusFlagsToUnixEvents(unsigned int flags)
{
  return (flags & POLLIN ? DBUS_WATCH_READABLE : 0) |
         (flags & POLLOUT ? DBUS_WATCH_WRITABLE : 0) |
         (flags & POLLERR ? DBUS_WATCH_ERROR : 0) |
         (flags & POLLHUP ? DBUS_WATCH_HANGUP : 0);
}

// DBus utility functions, used as function pointers in DBus setup

dbus_bool_t
DBusWatcher::AddWatchFunction(DBusWatch* aWatch, void* aData)
{
  MOZ_ASSERT(aData);
  DBusWatcher* dbusWatcher = static_cast<DBusWatcher*>(aData);
  return dbusWatcher->AddWatch(aWatch);
}

void
DBusWatcher::RemoveWatchFunction(DBusWatch* aWatch, void* aData)
{
  MOZ_ASSERT(aData);
  DBusWatcher* dbusWatcher = static_cast<DBusWatcher*>(aData);
  dbusWatcher->RemoveWatch(aWatch);
}

void
DBusWatcher::ToggleWatchFunction(DBusWatch* aWatch, void* aData)
{
  MOZ_ASSERT(aData);
  DBusWatcher* dbusWatcher = static_cast<DBusWatcher*>(aData);

  if (dbus_watch_get_enabled(aWatch)) {
    dbusWatcher->AddWatch(aWatch);
  } else {
    dbusWatcher->RemoveWatch(aWatch);
  }
}

void
DBusWatcher::DBusWakeupFunction(void* aData)
{
  MOZ_ASSERT(aData);
  DBusWatcher* dbusWatcher = static_cast<DBusWatcher*>(aData);
  dbusWatcher->WakeUp();
}

bool
DBusWatcher::SetUp()
{
  MOZ_ASSERT(!NS_IsMainThread());

  // If we already have a connection, exit
  if (mConnection) {
    return false;
  }

  // socketpair opens two sockets for the process to communicate on.
  // This is how android's implementation of the dbus event loop
  // communicates with itself in relation to IPC signals. These
  // sockets are contained sequentially in the same struct in the
  // android code, but we break them out into class members here.
  // Therefore we read into a local array and then copy.

  int sockets[2];
  if (socketpair(AF_LOCAL, SOCK_STREAM, 0, sockets) < 0) {
    return false;
  }

  mControlFdR.rwget() = sockets[0];
  mControlFdW.rwget() = sockets[1];

  pollfd* p = mPollData.AppendElement();

  p->fd = mControlFdR.get();
  p->events = POLLIN;
  p->revents = 0;

  // Due to the fact that mPollData and mWatchData have to match, we
  // push a null to the front of mWatchData since it has the control
  // fd in the first slot of mPollData.

  mWatchData.AppendElement(static_cast<DBusWatch*>(nullptr));

  // If we can't establish a connection to dbus, nothing else will work
  nsresult rv = EstablishDBusConnection();
  if (NS_FAILED(rv)) {
    NS_WARNING("Cannot create DBus Connection for DBus Thread!");
    return false;
  }

  dbus_bool_t success =
    dbus_connection_set_watch_functions(mConnection, AddWatchFunction,
                                        RemoveWatchFunction,
                                        ToggleWatchFunction, this, nullptr);
  NS_ENSURE_TRUE(success == TRUE, false);

  dbus_connection_set_wakeup_main_function(mConnection, DBusWakeupFunction,
                                           this, nullptr);
  return true;
}

// Main task for polling the DBus system

class DBusPollTask : public nsRunnable
{
public:
  DBusPollTask(DBusWatcher* aDBusWatcher)
  : mDBusWatcher(aDBusWatcher)
  { }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    bool continueThread;

    do {
      continueThread = mDBusWatcher->Poll();
    } while (continueThread);

    mDBusWatcher->CleanUp();

    nsIThread* thread;
    nsresult rv = NS_GetCurrentThread(&thread);
    NS_ENSURE_SUCCESS(rv, rv);

    nsRefPtr<nsIRunnable> runnable =
      NS_NewRunnableMethod(thread, &nsIThread::Shutdown);
    rv = NS_DispatchToMainThread(runnable);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

private:
  nsRefPtr<DBusWatcher> mDBusWatcher;
};

static StaticRefPtr<DBusWatcher> gDBusWatcher;
static StaticRefPtr<nsIThread>   gDBusServiceThread;

// Startup/Shutdown utility functions

bool
StartDBus()
{
  MOZ_ASSERT(!NS_IsMainThread());
  NS_ENSURE_TRUE(!gDBusWatcher, true);

  nsRefPtr<DBusWatcher> dbusWatcher(new DBusWatcher());

  bool eventLoopStarted = dbusWatcher->Initialize();
  NS_ENSURE_TRUE(eventLoopStarted, false);

  nsresult rv;

  if (!gDBusServiceThread) {
    nsIThread* dbusServiceThread;
    rv = NS_NewNamedThread("DBus Thread", &dbusServiceThread);
    NS_ENSURE_SUCCESS(rv, false);
    gDBusServiceThread = dbusServiceThread;
  }

#ifdef DEBUG
  LOG("DBus Thread Starting\n");
#endif

  nsRefPtr<nsIRunnable> pollTask(new DBusPollTask(dbusWatcher));
  NS_ENSURE_TRUE(pollTask, false);

  rv = gDBusServiceThread->Dispatch(pollTask, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, false);

  gDBusWatcher = dbusWatcher;

  return true;
}

bool
StopDBus()
{
  MOZ_ASSERT(!NS_IsMainThread());
  NS_ENSURE_TRUE(gDBusServiceThread, true);

  nsRefPtr<DBusWatcher> dbusWatcher(gDBusWatcher);
  gDBusWatcher = nullptr;

  if (dbusWatcher && !dbusWatcher->Stop()) {
    return false;
  }

  gDBusServiceThread = nullptr;

  return true;
}

nsresult
DispatchToDBusThread(nsIRunnable* event)
{
  nsRefPtr<nsIThread> dbusServiceThread(gDBusServiceThread);
  nsRefPtr<DBusWatcher> dbusWatcher(gDBusWatcher);

  NS_ENSURE_TRUE(dbusServiceThread.get() && dbusWatcher.get(),
                 NS_ERROR_NOT_INITIALIZED);

  nsresult rv = dbusServiceThread->Dispatch(event, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  dbusWatcher->WakeUp();

  return NS_OK;
}

}
}
