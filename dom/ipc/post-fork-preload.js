/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Preload some things, in an attempt to make app startup faster.
//
// This script is run when the preallocated process starts.  It is injected as
// a frame script.
// If Nuwa process is enabled, this script will run in preallocated process
// forked by Nuwa.

(function (global) {
  "use strict";

  Components.utils.import("resource://gre/modules/AppsServiceChild.jsm");
  Components.classes["@mozilla.org/network/protocol-proxy-service;1"].
    getService(Ci["nsIProtocolProxyService"]);

  DOMApplicationRegistry.resetList();
})(this);
