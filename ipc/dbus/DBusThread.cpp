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
#include "base/message_loop.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

namespace mozilla {
namespace ipc {

class WatchDBusConnectionTask : public Task
{
public:
  WatchDBusConnectionTask(RawDBusConnection* aConnection)
  : mConnection(aConnection)
  {
    MOZ_ASSERT(mConnection);
  }

  void Run()
  {
    mConnection->Watch();
  }

private:
  RawDBusConnection* mConnection;
};

class DeleteDBusConnectionTask : public Task
{
public:
  DeleteDBusConnectionTask(RawDBusConnection* aConnection)
  : mConnection(aConnection)
  {
    MOZ_ASSERT(mConnection);
  }

  void Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    // This command closes the DBus connection and all instances of
    // DBusWatch will be removed and free'd.
    delete mConnection;
  }

private:
  RawDBusConnection* mConnection;
};

 // Startup/Shutdown utility functions

static RawDBusConnection* gDBusConnection;

bool
StartDBus()
{
  MOZ_ASSERT(!NS_IsMainThread());
  NS_ENSURE_TRUE(!gDBusConnection, true);

  RawDBusConnection* connection = new RawDBusConnection();
  nsresult rv = connection->EstablishDBusConnection();
  NS_ENSURE_SUCCESS(rv, false);

  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
    new WatchDBusConnectionTask(connection));

  gDBusConnection = connection;

  return true;
}

bool
StopDBus()
{
  MOZ_ASSERT(!NS_IsMainThread());
  NS_ENSURE_TRUE(gDBusConnection, true);

  RawDBusConnection* connection = gDBusConnection;
  gDBusConnection = nullptr;

  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
    new DeleteDBusConnectionTask(connection));

  return true;
}

nsresult
DispatchToDBusThread(Task* task)
{
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE, task);

  return NS_OK;
}

RawDBusConnection*
GetDBusConnection()
{
  NS_ENSURE_TRUE(gDBusConnection, nullptr);

  return gDBusConnection;
}

}
}
