/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This IDL file is related to the WebExtensions browser.runtime's Port.
 *
 * You are granted a license to use, reproduce and create derivative works of
 * this document.
 */

[Exposed=(ServiceWorker), LegacyNoInterfaceObject]
interface ExtensionPort {
  [Replaceable]
  readonly attribute DOMString name;
  [Replaceable]
  readonly attribute object sender;
  [Replaceable]
  readonly attribute object? error;

  [Throws, WebExtensionStub="NoReturn"]
  void disconnect();
  [Throws, WebExtensionStub="NoReturn"]
  void postMessage(any message);

  [Replaceable, SameObject]
  readonly attribute ExtensionEventManager onDisconnect;
  [Replaceable, SameObject]
  readonly attribute ExtensionEventManager onMessage;
};

// Dictionary used by ExtensionAPIRequestForwarder and ExtensionCallabck to receive from the
// mozIExtensionAPIRequestHandler an internal description of a runtime.Port (and then used in
// the webidl implementation to create an ExtensionPort instance).
[GenerateInit]
dictionary ExtensionPortDescriptor {
  required DOMString portId;
  DOMString name = "";
};
