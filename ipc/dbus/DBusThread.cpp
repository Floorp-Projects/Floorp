/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
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

// TODO Add Wakeup to this list once we've moved to ics

enum {
  DBUS_EVENT_LOOP_EXIT = 1,
  DBUS_EVENT_LOOP_ADD = 2,
  DBUS_EVENT_LOOP_REMOVE = 3,
} DBusEventTypes;

// Signals that the DBus thread should listen for. Needs to include
// all signals any DBus observer object may need.

static const char* DBUS_SIGNALS[] =
{
  "type='signal',interface='org.bluez.Adapter'",
  "type='signal',interface='org.bluez.Manager'",
  "type='signal',interface='org.bluez.Device'",
  "type='signal',interface='org.bluez.Input'",
  "type='signal',interface='org.bluez.Network'",
  "type='signal',interface='org.bluez.NetworkServer'",
  "type='signal',interface='org.bluez.HealthDevice'",
  "type='signal',interface='org.bluez.AudioSink'"
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

  bool StartEventLoop();
  bool StopEventLoop();
  bool IsEventLoopRunning();
  void EventLoop();

  // Thread members
  nsCOMPtr<nsIThread> mThread;

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
  bool SetUpEventLoop();
  bool TearDownData();
  bool TearDownEventLoop();
};

static nsAutoPtr<DBusThread> sDBusThread;

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

    // TODO change this to dbus_watch_get_unix_fd once we move to ics
    int fd = dbus_watch_get_fd(aWatch);
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

  // TODO change this to dbus_watch_get_unix_fd once we move to ics
  int fd = dbus_watch_get_fd(aWatch);
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

static void HandleWatchRemove(DBusThread* aDbt)
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

// DBus Thread Implementation

DBusThread::DBusThread()
{
}

DBusThread::~DBusThread()
{

}

bool
DBusThread::SetUpEventLoop()
{
  // If we already have a connection, exit
  if (mConnection) {
    return false;
  }

  dbus_threads_init_default();
  DBusError err;
  dbus_error_init(&err);

  // If we can't establish a connection to dbus, nothing else will work
  nsresult rv = EstablishDBusConnection();
  if (NS_FAILED(rv)) {
    NS_WARNING("Cannot create DBus Connection for DBus Thread!");
    return false;
  }

  // Set which messages will be processed by this dbus connection.
  // Since we are maintaining a single thread for all the DBus bluez
  // signals we want, register all of them in this thread at startup.
  // The event handler will sort the destinations out as needed.
  for (uint32_t i = 0; i < ArrayLength(DBUS_SIGNALS); ++i) {
    dbus_bus_add_match(mConnection,
                       DBUS_SIGNALS[i],
                       &err);
    if (dbus_error_is_set(&err)) {
      LOG_AND_FREE_DBUS_ERROR(&err);
      return false;
    }
  }
  return true;
}

bool
DBusThread::TearDownData()
{
  LOG("Removing DBus Sockets\n");
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
  return true;
}

bool
DBusThread::TearDownEventLoop()
{
  MOZ_ASSERT(mConnection);

  DBusError err;
  dbus_error_init(&err);

  for (uint32_t i = 0; i < ArrayLength(DBUS_SIGNALS); ++i) {
    dbus_bus_remove_match(mConnection,
                          DBUS_SIGNALS[i],
                          &err);
    if (dbus_error_is_set(&err)) {
      LOG_AND_FREE_DBUS_ERROR(&err);
    }
  }

  return true;
}

void
DBusThread::EventLoop()
{
  dbus_connection_set_watch_functions(mConnection, AddWatch,
                                      RemoveWatch, ToggleWatch, this, NULL);

  LOG("DBus Event Loop Starting\n");
  while (1) {
    poll(mPollData.Elements(), mPollData.Length(), -1);

    for (uint32_t i = 0; i < mPollData.Length(); i++) {
      if (!mPollData[i].revents) {
        continue;
      }

      if (mPollData[i].fd == mControlFdR.get()) {
        char data;
        while (recv(mControlFdR.get(), &data, sizeof(char), MSG_DONTWAIT)
               != -1) {
          switch (data) {
            case DBUS_EVENT_LOOP_EXIT:
              LOG("DBus Event Loop Exiting\n");
              dbus_connection_set_watch_functions(mConnection,
                                                  NULL, NULL, NULL, NULL, NULL);
              TearDownEventLoop();
              return;
            case DBUS_EVENT_LOOP_ADD:
              HandleWatchAdd(this);
              break;
            case DBUS_EVENT_LOOP_REMOVE:
              HandleWatchRemove(this);
              break;
          }
        }
      } else {
        short events = mPollData[i].revents;
        unsigned int flags = UnixEventsToDBusFlags(events);
        dbus_watch_handle(mWatchData[i], flags);
        mPollData[i].revents = 0;
        // Break at this point since we don't know if the operation
        // was destructive
        break;
      }
    }
    while (dbus_connection_dispatch(mConnection) ==
           DBUS_DISPATCH_DATA_REMAINS)
    {}
  }
}

bool
DBusThread::StartEventLoop()
{
  // socketpair opens two sockets for the process to communicate on.
  // This is how android's implementation of the dbus event loop
  // communicates with itself in relation to IPC signals. These
  // sockets are contained sequentially in the same struct in the
  // android code, but we break them out into class members here.
  // Therefore we read into a local array and then copy.

  int sockets[2];
  if (socketpair(AF_LOCAL, SOCK_STREAM, 0, (int*)(&sockets)) < 0) {
    TearDownData();
    return false;
  }
  mControlFdR.rwget() = sockets[0];
  mControlFdW.rwget() = sockets[1];
  pollfd p;
  p.fd = mControlFdR.get();
  p.events = POLLIN;
  mPollData.AppendElement(p);

  // Due to the fact that mPollData and mWatchData have to match, we
  // push a null to the front of mWatchData since it has the control
  // fd in the first slot of mPollData.

  mWatchData.AppendElement((DBusWatch*)NULL);
  if (!SetUpEventLoop()) {
    TearDownData();
    return false;
  }
  if (NS_FAILED(NS_NewNamedThread("DBus Poll",
                                  getter_AddRefs(mThread),
                                  NS_NewNonOwningRunnableMethod(this,
                                                                &DBusThread::EventLoop)))) {
    NS_WARNING("Cannot create DBus Thread!");
    return false;    
  }
  LOG("DBus Thread Starting\n");
  return true;
}

bool
DBusThread::StopEventLoop()
{
  if (!mThread) {
    return true;
  }
  char data = DBUS_EVENT_LOOP_EXIT;
  ssize_t wret = write(mControlFdW.get(), &data, sizeof(char));
  if(wret < 0) {
    NS_ERROR("Cannot write exit flag to Dbus Thread!");
    return false;
  }
  LOG("DBus Thread Joining\n");
  nsCOMPtr<nsIThread> tmpThread;
  mThread.swap(tmpThread);
  if(NS_FAILED(tmpThread->Shutdown())) {
    NS_WARNING("DBus thread shutdown failed!");
  }
  LOG("DBus Thread Joined\n");
  TearDownData();
  return true;
}

// Startup/Shutdown utility functions

bool
StartDBus()
{
  MOZ_ASSERT(!NS_IsMainThread());
  if (sDBusThread) {
    NS_WARNING("Trying to start DBus Thread that is already currently running, skipping.");
    return true;
  }
  nsAutoPtr<DBusThread> thread(new DBusThread());
  if (!thread->StartEventLoop()) {
    NS_WARNING("Cannot start DBus event loop!");
    return false;
  }
  sDBusThread = thread;
  return true;
}

bool
StopDBus()
{
  MOZ_ASSERT(!NS_IsMainThread());
  if (!sDBusThread) {
    return true;
  }

  nsAutoPtr<DBusThread> thread(sDBusThread);
  sDBusThread = nullptr;  
  return thread->StopEventLoop();
}

}
}
