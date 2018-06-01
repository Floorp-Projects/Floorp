/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {AddonManager} = require("resource://gre/modules/AddonManager.jsm");
const Services = require("Services");
const EventEmitter = require("devtools/shared/event-emitter");

var ADB_LINK = Services.prefs.getCharPref("devtools.webide.adbAddonURL");
var ADB_ADDON_ID = Services.prefs.getCharPref("devtools.webide.adbAddonID");

var platform = Services.appShell.hiddenDOMWindow.navigator.platform;
var OS = "";
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

var addonsListener = {};
addonsListener.onEnabled =
addonsListener.onDisabled =
addonsListener.onInstalled =
addonsListener.onUninstalled = (updatedAddon) => {
  const addons = GetAvailableAddons();
  addons.adb.updateInstallStatus();
};
AddonManager.addAddonListener(addonsListener);

var AvailableAddons = null;
var GetAvailableAddons = exports.GetAvailableAddons = function() {
  if (!AvailableAddons) {
    AvailableAddons = {
      adb: new ADBAddon()
    };
  }
  return AvailableAddons;
};

exports.ForgetAddonsList = function() {
  AvailableAddons = null;
};

function Addon() {}
Addon.prototype = {
  _status: "unknown",
  set status(value) {
    if (this._status != value) {
      this._status = value;
      this.emit("update");
    }
  },
  get status() {
    return this._status;
  },

  updateInstallStatus: async function() {
    const addon = await AddonManager.getAddonByID(this.addonID);
    if (addon && !addon.userDisabled) {
      this.status = "installed";
    } else {
      this.status = "uninstalled";
    }
  },

  install: async function() {
    const addon = await AddonManager.getAddonByID(this.addonID);
    if (addon && !addon.userDisabled) {
      this.status = "installed";
      return;
    }
    this.status = "preparing";
    if (addon && addon.userDisabled) {
      await addon.enable();
    } else {
      const install = await AddonManager.getInstallForURL(this.xpiLink, "application/x-xpinstall");
      install.addListener(this);
      install.install();
    }
  },

  uninstall: async function() {
    const addon = await AddonManager.getAddonByID(this.addonID);
    addon.uninstall();
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

function ADBAddon() {
  EventEmitter.decorate(this);
  // This addon uses the string "linux" for "linux32"
  const fixedOS = OS == "linux32" ? "linux" : OS;
  this.xpiLink = ADB_LINK.replace(/#OS#/g, fixedOS);
  this.addonID = ADB_ADDON_ID;
  this.updateInstallStatus();
}
ADBAddon.prototype = Object.create(Addon.prototype);
