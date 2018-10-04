/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {AddonManager} = require("resource://gre/modules/AddonManager.jsm");
const Services = require("Services");
const EventEmitter = require("devtools/shared/event-emitter");

const ADB_LINK = Services.prefs.getCharPref("devtools.remote.adb.extensionURL");
const ADB_ADDON_ID = Services.prefs.getCharPref("devtools.remote.adb.extensionID");

// Extension ID for adb helper extension that might be installed on Firefox 63 or older.
const OLD_ADB_ADDON_ID = "adbhelper@mozilla.org";

const platform = Services.appShell.hiddenDOMWindow.navigator.platform;
let OS = "";
if (platform.includes("Win")) {
  OS = "win32";
} else if (platform.includes("Mac")) {
  OS = "mac64";
} else if (platform.includes("Linux")) {
  if (platform.includes("x86_64")) {
    OS = "linux64";
  } else {
    OS = "linux32";
  }
}

function ADBAddon() {
  EventEmitter.decorate(this);

  this._status = "unknown";

  // This addon uses the string "linux" for "linux32"
  const fixedOS = OS == "linux32" ? "linux" : OS;
  this.xpiLink = ADB_LINK.replace(/#OS#/g, fixedOS);

  // Uninstall old version of the extension that might be installed on this profile.
  this.uninstallOldExtension();

  this.updateInstallStatus();

  const addonsListener = {};
  addonsListener.onEnabled =
  addonsListener.onDisabled =
  addonsListener.onInstalled =
  addonsListener.onUninstalled = () => this.updateInstallStatus();
  AddonManager.addAddonListener(addonsListener);
}

ADBAddon.prototype = {
  set status(value) {
    Devices.adbExtensionInstalled = (value == "installed");
    if (this._status != value) {
      this._status = value;
      this.emit("update");
    }
  },

  get status() {
    return this._status;
  },

  updateInstallStatus: async function() {
    const addon = await AddonManager.getAddonByID(ADB_ADDON_ID);
    if (addon && !addon.userDisabled) {
      this.status = "installed";
    } else {
      this.status = "uninstalled";
    }
  },

  /**
   * Install and enable the adb extension. Returns a promise that resolves when ADB is
   * enabled.
   *
   * @param {String} source
   *        String passed to the AddonManager for telemetry.
   */
  install: async function(source) {
    const addon = await AddonManager.getAddonByID(ADB_ADDON_ID);
    if (addon && !addon.userDisabled) {
      this.status = "installed";
      return;
    }
    this.status = "preparing";
    if (addon && addon.userDisabled) {
      await addon.enable();
    } else {
      const install = await AddonManager.getInstallForURL(
        this.xpiLink,
        "application/x-xpinstall",
        null, null, null, null, null,
        { source }
      );
      install.addListener(this);
      install.install();
    }
  },

  uninstall: async function() {
    const addon = await AddonManager.getAddonByID(ADB_ADDON_ID);
    addon.uninstall();
  },

  uninstallOldExtension: async function() {
    const oldAddon = await AddonManager.getAddonByID(OLD_ADB_ADDON_ID);
    if (oldAddon) {
      oldAddon.uninstall();
    }
  },

  installFailureHandler: function(install, message) {
    this.status = "uninstalled";
    this.emit("failure", message);
  },

  onDownloadStarted: function() {
    this.status = "downloading";
  },

  onInstallStarted: function() {
    this.status = "installing";
  },

  onDownloadProgress: function(install) {
    if (install.maxProgress == -1) {
      this.emit("progress", -1);
    } else {
      this.emit("progress", install.progress / install.maxProgress);
    }
  },

  onInstallEnded: function({addon}) {
    addon.enable();
  },

  onDownloadCancelled: function(install) {
    this.installFailureHandler(install, "Download cancelled");
  },
  onDownloadFailed: function(install) {
    this.installFailureHandler(install, "Download failed");
  },
  onInstallCancelled: function(install) {
    this.installFailureHandler(install, "Install cancelled");
  },
  onInstallFailed: function(install) {
    this.installFailureHandler(install, "Install failed");
  },
};

exports.adbAddon = new ADBAddon();
