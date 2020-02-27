/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

enum DebuggerNotificationType {
  // DebuggerNotification
  "setTimeout",
  "clearTimeout",
  "setInterval",
  "clearInterval",
  "requestAnimationFrame",
  "cancelAnimationFrame",

  // CallbackDebuggerNotification
  "setTimeoutCallback",
  "setIntervalCallback",
  "requestAnimationFrameCallback",

  // EventCallbackDebuggerNotification
  "domEvent",
};

[ChromeOnly, Exposed=Window]
interface DebuggerNotification {
  readonly attribute DebuggerNotificationType type;

  // The global object that has triggered the notification.
  readonly attribute object global;
};

// For DOM events, we send notifications just before, and just after the
// event handler has been dispatched so that listeners
enum CallbackDebuggerNotificationPhase {
  "pre",
  "post",
};

// A base notification type for notifications that are dispatched as pairs with
// a before and after notification.
[ChromeOnly, Exposed=Window]
interface CallbackDebuggerNotification : DebuggerNotification {
  readonly attribute CallbackDebuggerNotificationPhase phase;
};

enum EventCallbackDebuggerNotificationType {
  "global",
  "node",
  "xhr",
  "worker",
  "websocket",
};

// A notification that about the engine calling a DOM event handler.
[ChromeOnly, Exposed=Window]
interface EventCallbackDebuggerNotification : CallbackDebuggerNotification {
  readonly attribute Event event;
  readonly attribute EventCallbackDebuggerNotificationType targetType;
};
