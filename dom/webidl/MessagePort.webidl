/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * For more information on this interface, please see
 * http://www.whatwg.org/specs/web-apps/current-work/#channel-messaging
 */

[Pref="dom.messageChannel.enabled"]
interface MessagePort : EventTarget {
  // TODO void postMessage(any message, optional sequence<Transferable> transfer);
  [Throws]
  void postMessage(any message, optional any transfer);

  void start();
  void close();

  // event handlers
  [SetterThrows]
           attribute EventHandler onmessage;
};
// MessagePort implements Transferable;
