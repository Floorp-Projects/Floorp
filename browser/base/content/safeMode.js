/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

const appStartup = Services.startup;

Cu.import("resource://gre/modules/ResetProfile.jsm");

var defaultToReset = false;

function restartApp() {
  appStartup.quit(appStartup.eForceQuit | appStartup.eRestart);
}

function resetProfile() {
  // Set the reset profile environment variable.
  let env = Cc["@mozilla.org/process/environment;1"]
              .getService(Ci.nsIEnvironment);
  env.set("MOZ_RESET_PROFILE_RESTART", "1");
}

function showResetDialog() {
  // Prompt the user to confirm the reset.
  let retVals = {
    reset: false,
  };
  window.openDialog("chrome://global/content/resetProfile.xul", null,
                    "chrome,modal,centerscreen,titlebar,dialog=yes", retVals);
  if (!retVals.reset)
    return;
  resetProfile();
  restartApp();
}

function onDefaultButton() {
  if (defaultToReset) {
    // Restart to reset the profile.
    resetProfile();
    restartApp();
    // Return false to prevent starting into safe mode while restarting.
    return false;
  }
  // Continue in safe mode. No restart needed.
  return true;
}

function onCancel() {
  appStartup.quit(appStartup.eForceQuit);
}

function onExtra1() {
  if (defaultToReset) {
    // Continue in safe mode
    window.close();
    return true;
  }
  // The reset dialog will handle starting the reset process if the user confirms.
  showResetDialog();
  return false;
}

function onLoad() {
  let dialog = document.documentElement;
  if (appStartup.automaticSafeModeNecessary) {
    document.getElementById("autoSafeMode").hidden = false;
    document.getElementById("safeMode").hidden = true;
    if (ResetProfile.resetSupported()) {
      document.getElementById("resetProfile").hidden = false;
    } else {
      // Hide the reset button is it's not supported.
      document.documentElement.getButton("extra1").hidden = true;
    }
  } else if (!ResetProfile.resetSupported()) {
    // Hide the reset button and text if it's not supported.
    document.documentElement.getButton("extra1").hidden = true;
    document.getElementById("resetProfileInstead").hidden = true;
  }
}
