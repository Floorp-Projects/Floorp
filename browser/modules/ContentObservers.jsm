/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module is for small observers that we want to register once per content
 * process, usually in order to forward content-based observer service notifications
 * to the chrome process through message passing. Using a JSM avoids having them
 * in content.js and thereby registering N observers for N open tabs, which is bad
 * for perf.
 */

"use strict";

this.EXPORTED_SYMBOLS = [];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");

let gEMEUIObserver = function(subject, topic, data) {
  let win = subject.top;
  let mm = getMessageManagerForWindow(win);
  if (mm) {
    mm.sendAsyncMessage("EMEVideo:ContentMediaKeysRequest", data);
  }
};

function getMessageManagerForWindow(aContentWindow) {
  let ir = aContentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDocShell)
                         .sameTypeRootTreeItem
                         .QueryInterface(Ci.nsIInterfaceRequestor);
  try {
    // If e10s is disabled, this throws NS_NOINTERFACE for closed tabs.
    return ir.getInterface(Ci.nsIContentFrameMessageManager);
  } catch(e if e.result == Cr.NS_NOINTERFACE) {
    return null;
  }
}

Services.obs.addObserver(gEMEUIObserver, "mediakeys-request", false);
