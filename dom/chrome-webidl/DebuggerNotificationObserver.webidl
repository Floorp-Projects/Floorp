/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

callback DebuggerNotificationCallback = void (DebuggerNotification n);

[ChromeOnly, Exposed=(Window, Worker)]
interface DebuggerNotificationObserver {
  [Throws]
  constructor();

  // Throws if the object is not a browser global or does not support
  // debugger notifications.
  // Returns false if already connected to this global.
  [Throws]
  boolean connect(object global);

  // Throws if the object is not a browser global or does not support
  // debugger notifications.
  // Returns false if not connected to this global.
  [Throws]
  boolean disconnect(object global);

  // Returns false if listener already added.
  boolean addListener(DebuggerNotificationCallback handler);

  // Returns false if listener was not found.
  boolean removeListener(DebuggerNotificationCallback handler);
};
