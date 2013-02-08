/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let { classes: Cc, interfaces: Ci, results: Cr, utils: Cu }  = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Geometry.jsm");

function debug(msg) {
  //dump("BrowserElementChild - " + msg + "\n");
}

// NB: this must happen before we process any messages from
// mozbrowser API clients.
docShell.isActive = true;

let infos = sendSyncMessage('browser-element-api:call',
                            { 'msg_name': 'hello' })[0];
docShell.QueryInterface(Ci.nsIDocShellTreeItem).name = infos.name;
docShell.setFullscreenAllowed(infos.fullscreenAllowed);


if (!('BrowserElementIsPreloaded' in this)) {
  // This is a produc-specific file that's sometimes unavailable.
  try {
    Services.scriptloader.loadSubScript("chrome://browser/content/forms.js");
  } catch (e) {
  }
  Services.scriptloader.loadSubScript("chrome://global/content/BrowserElementPanning.js");
  Services.scriptloader.loadSubScript("chrome://global/content/BrowserElementChildPreload.js");
}

var BrowserElementIsReady = true;
