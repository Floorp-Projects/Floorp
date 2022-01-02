/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This IDL file is related to the WebExtensions API object.
 *
 * You are granted a license to use, reproduce and create derivative works of
 * this document.
 */

[Exposed=(ServiceWorker), LegacyNoInterfaceObject]
interface ExtensionEventManager {
  [Throws]
  void    addListener(Function callback, optional object listenerOptions);

  [Throws]
  void    removeListener(Function callback);

  [Throws]
  boolean hasListener(Function callback);

  [Throws]
  boolean hasListeners();
};
