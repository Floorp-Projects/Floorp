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
#include "mozilla/Util.h"
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

// Functions for converting between unix events in the poll struct,
// and their dbus definitions

enum DBusEventTypes {
  DBUS_EVENT_LOOP_EXIT = 1,
  DBUS_EVENT_LOOP_ADD = 2,
  DBUS_EVENT_LOOP_REMOVE = 3,
  DBUS_EVENT_LOOP_WAKEUP = 4
};

static unsigned int UnixEventsToDBusFlags(short events)
{
  return (events & DBUS_WATCH_READABLE ? POLLIN : 0) |
    (events & DBUS_WATCH_WRITABLE ? POLLOUT : 0) |
    (events & DBUS_WATCH_ERROR ? POLLERR : 0) |
    (events & DBUS_WATCH_HANGUP ? POLLHUP : 0);
}

static short DBusFlagsToUnixEvents(unsigned int flags)
{
  return (flags & POLLIN ? DBUS_WATCH_READABLE : 0) |
    (flags & POLLOUT ? DBUS_WATCH_WRITABLE : 0) |
    (flags & POLLERR ? DBUS_WATCH_ERROR : 0) |
    (flags & POLLHUP ? DBUS_WATCH_HANGUP : 0);
}

namespace mozilla {
namespace ipc {

struct PollFdComparator {
  bool Equals(const pollfd& a, const pollfd& b) const {
    return ((a.fd == b.fd) &&
            (a.events == b.events));
  }
  bool LessThan(const pollfd& a, const pollfd&b) const {
    return false;
  }
};

// DBus Thread Class prototype

struct DBusThread : public RawDBusConnection
{
  DBusThread();
  ~DBusThread();

  bool Initialize();
  void CleanUp();

  void WakeUp();

  // Information about the sockets we're polling. Socket counts
  // increase/decrease depending on how many add/remove watch signals
  // we're received via the control sockets.
  nsTArray<pollfd> mPollData;
  nsTArray<DBusWatch*> mWatchData;

  // Sockets for receiving dbus control information (watch
  // add/removes, loop shutdown, etc...)
  ScopedClose mControlFdR;
  ScopedClose mControlFdW;

protected:
  bool SetUp();
};

// DBus utility functions
// Free statics, as they're used as function pointers in dbus setup

static dbus_bool_t
AddWatch(DBusWatch *aWatch, void *aData)
{
  DBusThread *dbt = (DBusThread *)aData;

  if (dbus_watch_get_enabled(aWatch)) {
    // note that we can't just send the watch and inspect it later
    // because we may get a removeWatch call before this data is reacted
    // to by our eventloop and remove this watch..  reading the add first
    // and then inspecting the recently deceased watch would be bad.
    char control = DBUS_EVENT_LOOP_ADD;
    if (write(dbt->mControlFdW.get(), &control, sizeof(char)) < 0) {
      LOG("Cannot write DBus add watch control data to socket!\n");
      return false;
    }

    int fd = dbus_watch_get_unix_fd(aWatch);
    if (write(dbt->mControlFdW.get(), &fd, sizeof(int)) < 0) {
      LOG("Cannot write DBus add watch descriptor data to socket!\n");
      return false;
    }

    unsigned int flags = dbus_watch_get_flags(aWatch);
    if (write(dbt->mControlFdW.get(), &flags, sizeof(unsigned int)) < 0) {
      LOG("Cannot write DBus add watch flag data to socket!\n");
      return false;
    }

    if (write(dbt->mControlFdW.get(), &aWatch, sizeof(DBusWatch*)) < 0) {
      LOG("Cannot write DBus add watch struct data to socket!\n");
      return false;
    }
  }
  return true;
}

static void
RemoveWatch(DBusWatch *aWatch, void *aData)
{
  DBusThread *dbt = (DBusThread *)aData;

  char control = DBUS_EVENT_LOOP_REMOVE;
  if (write(dbt->mControlFdW.get(), &control, sizeof(char)) < 0) {
    LOG("Cannot write DBus remove watch control data to socket!\n");
    return;
  }

  int fd = dbus_watch_get_unix_fd(aWatch);
  if (write(dbt->mControlFdW.get(), &fd, sizeof(int)) < 0) {
    LOG("Cannot write DBus remove watch descriptor data to socket!\n");
    return;
  }

  unsigned int flags = dbus_watch_get_flags(aWatch);
  if (write(dbt->mControlFdW.get(), &flags, sizeof(unsigned int)) < 0) {
    LOG("Cannot write DBus remove watch flag data to socket!\n");
    return;
  }
}

static void
ToggleWatch(DBusWatch *aWatch, void *aData)
{
  if (dbus_watch_get_enabled(aWatch)) {
    AddWatch(aWatch, aData);
  } else {
    RemoveWatch(aWatch, aData);
  }
}

static void
HandleWatchAdd(DBusThread* aDbt)
{
  DBusWatch *watch;
  int newFD;
  unsigned int flags;
  if (read(aDbt->mControlFdR.get(), &newFD, sizeof(int)) < 0) {
    LOG("Cannot read DBus watch add descriptor data from socket!\n");
    return;
  }
  if (read(aDbt->mControlFdR.get(), &flags, sizeof(unsigned int)) < 0) {
    LOG("Cannot read DBus watch add flag data from socket!\n");
    return;
  }
  if (read(aDbt->mControlFdR.get(), &watch, sizeof(DBusWatch *)) < 0) {
    LOG("Cannot read DBus watch add watch data from socket!\n");
    return;
  }
  short events = DBusFlagsToUnixEvents(flags);

  pollfd p;
  p.fd = newFD;
  p.revents = 0;
  p.events = events;
  if (aDbt->mPollData.Contains(p, PollFdComparator())) return;
  aDbt->mPollData.AppendElement(p);
  aDbt->mWatchData.AppendElement(watch);
}

static void
HandleWatchRemove(DBusThread* aDbt)
{
  int removeFD;
  unsigned int flags;

  if (read(aDbt->mControlFdR.get(), &removeFD, sizeof(int)) < 0) {
    LOG("Cannot read DBus watch remove descriptor data from socket!\n");
    return;
  }
  if (read(aDbt->mControlFdR.get(), &flags, sizeof(unsigned int)) < 0) {
    LOG("Cannot read DBus watch remove flag data from socket!\n");
    return;
  }
  short events = DBusFlagsToUnixEvents(flags);
  pollfd p;
  p.fd = removeFD;
  p.events = events;
  int index = aDbt->mPollData.IndexOf(p, 0, PollFdComparator());
  // There are times where removes can be requested for watches that
  // haven't been added (for example, whenever gecko comes up after
  // adapters have already been enabled), so check to make sure we're
  // using the watch in the first place
  if (index < 0) {
    LOG("DBus requested watch removal of non-existant socket, ignoring...");
    return;
  }
  aDbt->mPollData.RemoveElementAt(index);

  // DBusWatch pointers are maintained by DBus, so we won't leak by
  // removing.
  aDbt->mWatchData.RemoveElementAt(index);
}

static
void DBusWakeup(void* aData)
{
  MOZ_ASSERT(aData);
  DBusThread* dbusThread = static_cast<DBusThread*>(aData);
  dbusThread->WakeUp();
}

// DBus Thread Implementation

DBusThread::DBusThread()
{
}

DBusThread::~DBusThread()
{

}

bool
DBusThread::SetUp()
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

  pollfd *p = mPollData.AppendElement();

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
    dbus_connection_set_watch_functions(mConnection, AddWatch, RemoveWatch,
                                        ToggleWatch, this, nullptr);
  NS_ENSURE_TRUE(success == TRUE, false);

  dbus_connection_set_wakeup_main_function(mConnection, DBusWakeup, this, nullptr);

  return true;
}

bool
DBusThread::Initialize()
{
  if (!SetUp()) {
    CleanUp();
    return false;
  }

  return true;
}

void
DBusThread::CleanUp()
{
  MOZ_ASSERT(!NS_IsMainThread());

  dbus_connection_set_wakeup_main_function(mConnection, nullptr, nullptr, nullptr);

  dbus_bool_t success = dbus_connection_set_watch_functions(mConnection, nullptr,
                                                            nullptr, nullptr,
                                                            nullptr, nullptr);
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
DBusThread::WakeUp()
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

  ssize_t rv = TEMP_FAILURE_RETRY(write(mControlFdW.get(), &control, sizeof(control)));

  if (rv < 0) {
    NS_WARNING("Cannot write wakeup bit to DBus controller!");
  }
}

// Main task for polling the DBus system

class DBusPollTask : public nsRunnable
{
public:
  DBusPollTask(DBusThread* aConnection)
  : mConnection(aConnection)
  { }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    bool exitThread = false;

    while (!exitThread) {

      int res = TEMP_FAILURE_RETRY(poll(mConnection->mPollData.Elements(),
                                        mConnection->mPollData.Length(),
                                        -1));
      NS_ENSURE_TRUE(res > 0, NS_OK);

      nsTArray<pollfd>::size_type i = 0;

      while (i < mConnection->mPollData.Length()) {
        if (mConnection->mPollData[i].revents == POLLIN) {

          if (mConnection->mPollData[i].fd == mConnection->mControlFdR.get()) {
            char data;
            res = TEMP_FAILURE_RETRY(read(mConnection->mControlFdR.get(), &data, sizeof(data)));
            NS_ENSURE_TRUE(res > 0, NS_OK);

            switch (data) {
            case DBUS_EVENT_LOOP_EXIT:
              exitThread = true;
              break;
            case DBUS_EVENT_LOOP_ADD:
              HandleWatchAdd(mConnection);
              break;
            case DBUS_EVENT_LOOP_REMOVE:
              HandleWatchRemove(mConnection);
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
            short events = mConnection->mPollData[i].revents;
            unsigned int flags = UnixEventsToDBusFlags(events);
            dbus_watch_handle(mConnection->mWatchData[i], flags);
            mConnection->mPollData[i].revents = 0;
            // Break at this point since we don't know if the operation
            // was destructive
            break;
          }
          while (dbus_connection_dispatch(mConnection->GetConnection()) ==
                 DBUS_DISPATCH_DATA_REMAINS)
          {}
        }
        ++i;
      }
    }

    return NS_OK;
  }

private:
  DBusThread* mConnection;
};

static StaticAutoPtr<DBusThread> gDBusThread;
static StaticRefPtr<nsIThread>   gDBusServiceThread;

// Startup/Shutdown utility functions

bool
StartDBus()
{
  MOZ_ASSERT(!NS_IsMainThread());
  NS_ENSURE_TRUE(!gDBusThread, true);

  nsAutoPtr<DBusThread> dbusThread(new DBusThread());

  bool eventLoopStarted = dbusThread->Initialize();
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

  nsRefPtr<nsIRunnable> pollTask(new DBusPollTask(dbusThread));
  NS_ENSURE_TRUE(pollTask, false);

  rv = gDBusServiceThread->Dispatch(pollTask, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, false);

  gDBusThread = dbusThread.forget();

  return true;
}

bool
StopDBus()
{
  MOZ_ASSERT(!NS_IsMainThread());
  NS_ENSURE_TRUE(gDBusServiceThread, true);

  if (gDBusThread) {
    static const char data = DBUS_EVENT_LOOP_EXIT;
    ssize_t wret = TEMP_FAILURE_RETRY(write(gDBusThread->mControlFdW.get(),
                                            &data, sizeof(data)));
    NS_ENSURE_TRUE(wret == 1, false);
  }

#ifdef DEBUG
  LOG("DBus Thread Joining\n");
#endif

  if (NS_FAILED(gDBusServiceThread->Shutdown())) {
    NS_WARNING("DBus thread shutdown failed!");
  }
  gDBusServiceThread = nullptr;

#ifdef DEBUG
  LOG("DBus Thread Joined\n");
#endif

  if (gDBusThread) {
    gDBusThread->CleanUp();
    gDBusThread = nullptr;
  }

  return true;
}

nsresult
DispatchToDBusThread(nsIRunnable* event)
{
  MOZ_ASSERT(gDBusServiceThread);
  MOZ_ASSERT(gDBusThread);

  nsresult rv = gDBusServiceThread->Dispatch(event, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  gDBusThread->WakeUp();

  return NS_OK;
}

}
}
