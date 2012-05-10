/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_dbus_gonk_dbusthread_h__
#define mozilla_ipc_dbus_gonk_dbusthread_h__

namespace mozilla {
namespace ipc {


// Starts the DBus thread, which handles returning signals to objects
// that call asynchronous functions. This should be called from the
// main thread at startup.
bool StartDBus();

// Stop the DBus thread, assuming it's currently running. Should be
// called from main thread.
bool StopDBus();
}
}
#endif

