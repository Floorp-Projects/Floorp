/* -*- Mode: Java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const VERBOSE = 1;
let log =
  VERBOSE ?
  function log_dump(msg) { dump("UpdatePrompt: "+ msg +"\n"); } :
  function log_noop(msg) { };

function UpdatePrompt() { }

UpdatePrompt.prototype = {
  classID: Components.ID("{88b3eb21-d072-4e3b-886d-f89d8c49fe59}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIUpdatePrompt]),

  // nsIUpdatePrompt

  // FIXME/bug 737601: we should have users opt-in to downloading
  // updates when on a billed pipe.  Initially, opt-in for 3g, but
  // that doesn't cover all cases.
  checkForUpdates: function UP_checkForUpdates() { },
  showUpdateAvailable: function UP_showUpdateAvailable(aUpdate) { },

  showUpdateDownloaded: function UP_showUpdateDownloaded(aUpdate, aBackground) {
    // FIXME/bug 737598: we should let the user request that the
    // update be applied later, e.g. if they're in the middle of a
    // phone call ;).

    log("Update downloaded, restarting to apply it");

    let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup);
    // NB: on Gonk, we rely on the system process manager to restart
    // us.  Trying to restart here would conflict with the process
    // manager.  We should be using a runtime check to detect Gonk
    // instead of this gross ifdef, but the ifdef works for now.
    appStartup.quit(appStartup.eForceQuit
#ifndef ANDROID
                    | appStartup.eRestart
#endif
      );
  },

  showUpdateInstalled: function UP_showUpdateInstalled() { },

  showUpdateError: function UP_showUpdateError(aUpdate) {
    if (aUpdate.state == "failed") {
      log("Failed to download update");
    }
  },

  showUpdateHistory: function UP_showUpdateHistory(aParent) { },
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([UpdatePrompt]);
