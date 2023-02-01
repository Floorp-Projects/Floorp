/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This IDL file is related to the WebExtensions API object.
 *
 * The ExtensionSetting interface is used by the API namespace that expose
 * settings API sub-namespaces (in particular browserSettings, proxy,
 * captivePortal and privacy WebExtensions APIs).
 *
 * You are granted a license to use, reproduce and create derivative works of
 * this document.
 */

[Exposed=(ServiceWorker), LegacyNoInterfaceObject]
interface ExtensionSetting {
  // API methods.

  [Throws, WebExtensionStub="Async"]
  any get(object details, optional Function callback);

  [Throws, WebExtensionStub="Async"]
  any set(object details, optional Function callback);

  [Throws, WebExtensionStub="Async"]
  any clear(object details, optional Function callback);

  // API events.

  [Replaceable, SameObject]
  readonly attribute ExtensionEventManager onChange;
};
