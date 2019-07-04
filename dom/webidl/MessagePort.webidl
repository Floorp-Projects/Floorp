/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * For more information on this interface, please see
 * http://www.whatwg.org/specs/web-apps/current-work/#channel-messaging
 */

[Exposed=(Window,Worker,AudioWorklet)]
interface MessagePort : EventTarget {
  [Throws]
  void postMessage(any message, sequence<object> transferable);
  [Throws]
  void postMessage(any message, optional PostMessageOptions options = {});

  void start();
  void close();

  // event handlers
  attribute EventHandler onmessage;
  attribute EventHandler onmessageerror;
};
// MessagePort implements Transferable;

dictionary PostMessageOptions {
  sequence<object> transfer = [];
};
