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
#include "mozilla/RefPtr.h"
#include "mozilla/Monitor.h"
#include "mozilla/Util.h"
#include "mozilla/FileUtils.h"
#include "nsAutoPtr.h"
#include "nsIThread.h"
#include "nsXULAppAPI.h"

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk", args);
#else
#define LOG(args...)  printf(args);
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
  "type='signal',interface='org.freedesktop.DBus'",
  "type='signal',interface='org.bluez.Adapter'",
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
  void StopEventLoop();
  bool IsEventLoopRunning();
  static void* EventLoop(void* aPtr);
  
  // Thread members
  pthread_t mThread;
  Mutex mMutex;
  bool mIsRunning;

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
    write(dbt->mControlFdW.mFd, &control, sizeof(char));

    // TODO change this to dbus_watch_get_unix_fd once we move to ics
    int fd = dbus_watch_get_fd(aWatch);
    write(dbt->mControlFdW.mFd, &fd, sizeof(int));

    unsigned int flags = dbus_watch_get_flags(aWatch);
    write(dbt->mControlFdW.mFd, &flags, sizeof(unsigned int));

    write(dbt->mControlFdW.mFd, &aWatch, sizeof(DBusWatch*));
  }
  return true;
}

static void
RemoveWatch(DBusWatch *aWatch, void *aData)
{
  DBusThread *dbt = (DBusThread *)aData;

  char control = DBUS_EVENT_LOOP_REMOVE;
  write(dbt->mControlFdW.mFd, &control, sizeof(char));

  // TODO change this to dbus_watch_get_unix_fd once we move to ics
  int fd = dbus_watch_get_fd(aWatch);
  write(dbt->mControlFdW.mFd, &fd, sizeof(int));

  unsigned int flags = dbus_watch_get_flags(aWatch);
  write(dbt->mControlFdW.mFd, &flags, sizeof(unsigned int));
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
  read(aDbt->mControlFdR.mFd, &newFD, sizeof(int));
  read(aDbt->mControlFdR.mFd, &flags, sizeof(unsigned int));
  read(aDbt->mControlFdR.mFd, &watch, sizeof(DBusWatch *));
  short events = DBusFlagsToUnixEvents(flags);

  pollfd p;
  p.fd = newFD;
  p.revents = 0;
  p.events = events;
  if(aDbt->mPollData.Contains(p, PollFdComparator())) return;
  aDbt->mPollData.AppendElement(p);
  aDbt->mWatchData.AppendElement(watch);
}

static void HandleWatchRemove(DBusThread* aDbt) {
  int removeFD;
  unsigned int flags;

  read(aDbt->mControlFdR.mFd, &removeFD, sizeof(int));
  read(aDbt->mControlFdR.mFd, &flags, sizeof(unsigned int));
  short events = DBusFlagsToUnixEvents(flags);
  pollfd p;
  p.fd = removeFD;
  p.events = events;
  int index = aDbt->mPollData.IndexOf(p, 0, PollFdComparator());
  aDbt->mPollData.RemoveElementAt(index);

  // DBusWatch pointers are maintained by DBus, so we won't leak by
  // removing.
  aDbt->mWatchData.RemoveElementAt(index);
}

// Called by dbus during WaitForAndDispatchEventNative()
static DBusHandlerResult
EventFilter(DBusConnection *aConn, DBusMessage *aMsg,
            void *aData)
{
  DBusError err;

  dbus_error_init(&err);

  if (dbus_message_get_type(aMsg) != DBUS_MESSAGE_TYPE_SIGNAL) {
    LOG("%s: not interested (not a signal).\n", __FUNCTION__);
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  LOG("%s: Received signal %s:%s from %s\n", __FUNCTION__,
      dbus_message_get_interface(aMsg), dbus_message_get_member(aMsg),
      dbus_message_get_path(aMsg));

  return DBUS_HANDLER_RESULT_HANDLED;
}

// DBus Thread Implementation

DBusThread::DBusThread() : mMutex("DBusGonk.mMutex")
                         , mIsRunning(false)
                         , mControlFdR(-1)
                         , mControlFdW(-1)
{
}

DBusThread::~DBusThread()
{

}

bool
DBusThread::SetUpEventLoop()
{
  // If we already have a connection, exit
  if(mConnection) {
    return false;
  }
  // If we can't establish a connection to dbus, nothing else will work
  if(!Create()) {
    return false;
  }

  dbus_threads_init_default();
  DBusError err;
  dbus_error_init(&err);

  // Add a filter for all incoming messages_base
  if (!dbus_connection_add_filter(mConnection, EventFilter, this, NULL)){
    return false;
  }

  // Set which messages will be processed by this dbus connection.
  // Since we are maintaining a single thread for all the DBus bluez
  // signals we want, register all of them in this thread at startup.
  // The event handler will sort the destinations out as needed.
  for(uint32_t i = 0; i < ArrayLength(DBUS_SIGNALS); ++i) {
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
  if (mControlFdW.mFd) {
    close(mControlFdW.mFd);
    mControlFdW.mFd = 0;
  }
  if (mControlFdR.mFd) {
    close(mControlFdR.mFd);
    mControlFdR.mFd = 0;
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

  for(uint32_t i = 0; i < ArrayLength(DBUS_SIGNALS); ++i) {
    dbus_bus_remove_match(mConnection,
                          DBUS_SIGNALS[i],
                          &err);
    if (dbus_error_is_set(&err)) {
      LOG_AND_FREE_DBUS_ERROR(&err);
    }
  }

  dbus_connection_remove_filter(mConnection, EventFilter, this);
  return true;
}

void*
DBusThread::EventLoop(void *aPtr)
{
  DBusThread* dbt = static_cast<DBusThread*>(aPtr);
  MOZ_ASSERT(dbt);

  dbus_connection_set_watch_functions(dbt->mConnection, AddWatch,
                                      RemoveWatch, ToggleWatch, aPtr, NULL);

  dbt->mIsRunning = true;

  while (1) {
    poll(dbt->mPollData.Elements(), dbt->mPollData.Length(), -1);
    
    for (uint32_t i = 0; i < dbt->mPollData.Length(); i++) {
      if (!dbt->mPollData[i].revents) {
        continue;
      }

      if (dbt->mPollData[i].fd == dbt->mControlFdR.mFd) {
        char data;
        while (recv(dbt->mControlFdR.mFd, &data, sizeof(char), MSG_DONTWAIT)
               != -1) {
          switch (data) {
          case DBUS_EVENT_LOOP_EXIT:
          {
            dbus_connection_set_watch_functions(dbt->mConnection,
                                                NULL, NULL, NULL, NULL, NULL);
            dbt->TearDownEventLoop();
            return NULL;
          }
          case DBUS_EVENT_LOOP_ADD:
          {
            HandleWatchAdd(dbt);
            break;
          }
          case DBUS_EVENT_LOOP_REMOVE:
          {
            HandleWatchRemove(dbt);
            break;
          }
          }
        }
      } else {
        short events = dbt->mPollData[i].revents;
        unsigned int flags = UnixEventsToDBusFlags(events);
        dbus_watch_handle(dbt->mWatchData[i], flags);
        dbt->mPollData[i].revents = 0;
        // Break at this point since we don't know if the operation
        // was destructive
        break;
      }
    }
    while (dbus_connection_dispatch(dbt->mConnection) ==
           DBUS_DISPATCH_DATA_REMAINS)
    {}
  }
}

bool
DBusThread::StartEventLoop()
{
  MutexAutoLock lock(mMutex);
  mIsRunning = false;
  if (socketpair(AF_LOCAL, SOCK_STREAM, 0, &(mControlFdR.mFd))) {
    TearDownData();
    return false;
  }
  pollfd p;
  p.fd = mControlFdR.mFd;
  p.events = POLLIN;
  mPollData.AppendElement(p);
  // Due to the fact that mPollData and mWatchData have to match, we
  // push a null to the front of mWatchData since it has the control
  // fd in the first slot of mPollData.
  mWatchData.AppendElement((DBusWatch*)NULL);
  if (SetUpEventLoop() != true) {
    TearDownData();
    return false;
  }
  pthread_create(&(mThread), NULL, DBusThread::EventLoop, this);
  return true;
}

void
DBusThread::StopEventLoop()
{
  MutexAutoLock lock(mMutex);
  if (mIsRunning) {
    char data = DBUS_EVENT_LOOP_EXIT;
    write(mControlFdW.mFd, &data, sizeof(char));
    void *ret;
    pthread_join(mThread, &ret);
    TearDownData();
  }
  mIsRunning = false;
}

bool
DBusThread::IsEventLoopRunning()
{
  MutexAutoLock lock(mMutex);
  return mIsRunning;
}

// Startup/Shutdown utility functions

static void
ConnectDBus(Monitor* aMonitor, bool* aSuccess)
{
  MOZ_ASSERT(!sDBusThread);

  sDBusThread = new DBusThread();
  *aSuccess = true;
  if(!sDBusThread->StartEventLoop())
  {
    *aSuccess = false;
  }
  {
    MonitorAutoLock lock(*aMonitor);
    lock.Notify();
  }
}

static void
DisconnectDBus(Monitor* aMonitor, bool* aSuccess)
{
  MOZ_ASSERT(sDBusThread);

  *aSuccess = true;
  sDBusThread->StopEventLoop();
  sDBusThread = NULL;
  {
    MonitorAutoLock lock(*aMonitor);
    lock.Notify();
  }
}

bool
StartDBus()
{
  Monitor monitor("StartDBus.monitor");
  bool success;
  {
    MonitorAutoLock lock(monitor);

    XRE_GetIOMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableFunction(ConnectDBus, &monitor, &success));
    lock.Wait();
  }
  return success;
}

bool
StopDBus()
{
  Monitor monitor("StopDBus.monitor");
  bool success;
  {
    MonitorAutoLock lock(monitor);

    XRE_GetIOMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableFunction(DisconnectDBus, &monitor, &success));
    lock.Wait();
  }
  return success;
}

}
}
