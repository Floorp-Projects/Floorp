/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported install, uninstall, startup, shutdown */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/AddonManager.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Timer.jsm");

ChromeUtils.defineModuleGetter(this, "TestRunner",
                               "chrome://mozscreenshots/content/TestRunner.jsm");

async function install(data, reason) {
  if (!isAppSupported()) {
    uninstallExtension(data);
    return;
  }

  let addon = await AddonManager.getAddonByID(data.id);
  if (addon) {
    addon.userDisabled = false;
  }
}

async function startup(data, reason) {
  if (!isAppSupported()) {
    uninstallExtension(data);
    return;
  }

  let addon = await AddonManager.getAddonByID(data.id);
  let extensionPath = addon.getResourceURI();
  TestRunner.init(extensionPath);
}

function shutdown(data, reason) { }

function uninstall(data, reason) { }

/**
 * @return boolean whether the test suite applies to the application.
 */
function isAppSupported() {
  return true;
}

async function uninstallExtension(data) {
  let addon = await AddonManager.getAddonByID(data.id);
  addon.uninstall();
}
