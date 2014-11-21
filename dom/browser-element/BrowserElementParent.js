/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu, interfaces: Ci} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/BrowserElementParent.jsm");

/**
 * BrowserElementParent implements one half of <iframe mozbrowser>.  (The other
 * half is, unsurprisingly, BrowserElementChild.)
 */
this.NSGetFactory = XPCOMUtils.generateNSGetFactory([BrowserElementParent]);
