/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * You are granted a license to use, reproduce and create derivative works of
 * this document.
 *
 * This IDL file is related to the WebExtensions API object.
 *
 * More info about generating webidl API bindings for WebExtensions API at:
 *
 * https://firefox-source-docs.mozilla.org/toolkit/components/extensions/webextensions/webidl_bindings.html
 *
 * A short summary of the special setup used by these WebIDL files (meant to aid
 * webidl peers reviews and sign-offs) is available in the following section:
 *
 * https://firefox-source-docs.mozilla.org/toolkit/components/extensions/webextensions/webidl_bindings.html#review-process-on-changes-to-webidl-definitions
 */

[Exposed=(ServiceWorker), LegacyNoInterfaceObject]
interface ExtensionEventManager {
  [Throws]
  undefined addListener(Function callback, optional object listenerOptions);

  [Throws]
  undefined removeListener(Function callback);

  [Throws]
  boolean hasListener(Function callback);

  [Throws]
  boolean hasListeners();
};
