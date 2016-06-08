/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Pref="dom.flyweb.enabled"]
interface FlyWebPublishedServer : EventTarget {
  readonly attribute DOMString name;
  readonly attribute DOMString? uiUrl;

  void close();

  attribute EventHandler onclose;
  attribute EventHandler onfetch;
  attribute EventHandler onwebsocket;
};

dictionary FlyWebPublishOptions {
  DOMString? uiUrl = null; // URL to user interface. Can be different server. Makes
                           // endpoint show up in browser's "local services" UI.
                           // If relative, resolves against the root of the server.
};
