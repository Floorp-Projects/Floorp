/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cu} = require("chrome");
const {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm");
const {AddonManager} = Cu.import("resource://gre/modules/AddonManager.jsm");
const {EventEmitter} = Cu.import("resource://gre/modules/devtools/event-emitter.js");
const {Simulator} = Cu.import("resource://gre/modules/devtools/Simulator.jsm");
const {Devices} = Cu.import("resource://gre/modules/devtools/Devices.jsm");
const {Services} = Cu.import("resource://gre/modules/Services.jsm");
const {GetAddonsJSON} = require("devtools/webide/remote-resources");

let SIMULATOR_LINK = Services.prefs.getCharPref("devtools.webide.simulatorAddonsURL");
let ADB_LINK = Services.prefs.getCharPref("devtools.webide.adbAddonURL");
let SIMULATOR_ADDON_ID = Services.prefs.getCharPref("devtools.webide.simulatorAddonID");
let ADB_ADDON_ID = Services.prefs.getCharPref("devtools.webide.adbAddonID");

let platform = Services.appShell.hiddenDOMWindow.navigator.platform;
let OS = "";
if (platform.indexOf("Win") != -1) {
  OS = "win32";
} else if (platform.indexOf("Mac") != -1) {
  OS = "mac64";
} else if (platform.indexOf("Linux") != -1) {
  if (platform.indexOf("x86_64") != -1) {
    OS = "linux64";
  } else {
    OS = "linux";
  }
}

Simulator.on("unregister", updateSimulatorAddons);
Simulator.on("register", updateSimulatorAddons);
Devices.on("addon-status-updated", updateAdbAddon);

function updateSimulatorAddons(event, version) {
  GetAvailableAddons().then(addons => {
    let foundAddon = null;
    for (let addon of addons.simulators) {
      if (addon.version == version) {
        foundAddon = addon;
        break;
      }
    }
    if (!foundAddon) {
      console.warn("An unknown simulator (un)registered", version);
      return;
    }
    foundAddon.updateInstallStatus();
  });
}

function updateAdbAddon() {
  GetAvailableAddons().then(addons => {
    addons.adb.updateInstallStatus();
  });
}

let GetAvailableAddons_promise = null;
let GetAvailableAddons = exports.GetAvailableAddons = function() {
  if (!GetAvailableAddons_promise) {
    let deferred = promise.defer();
    GetAvailableAddons_promise = deferred.promise;
    let addons = {
      simulators: [],
      adb: null
    }
    GetAddonsJSON().then(json => {
      for (let stability in json) {
        for (let version of json[stability]) {
          addons.simulators.push(new SimulatorAddon(stability, version));
        }
      }
      addons.adb = new ADBAddon();
      deferred.resolve(addons);
    }, e => {
      GetAvailableAddons_promise = null;
      deferred.reject(e);
    });
  }
  return GetAvailableAddons_promise;
}

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

  install: function() {
    if (this.status != "uninstalled") {
      throw new Error("Not uninstalled");
    }
    this.status = "preparing";

    AddonManager.getAddonByID(this.addonID, (addon) => {
      if (addon && addon.userDisabled) {
        addon.userDisabled = false;
      } else {
        AddonManager.getInstallForURL(this.xpiLink, (install) => {
          install.addListener(this);
          install.install();
        }, "application/x-xpinstall");
      }
    });

  },

  uninstall: function() {
    AddonManager.getAddonByID(this.addonID, (addon) => {
      addon.uninstall();
    });
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
    addon.userDisabled = false;
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
}

function SimulatorAddon(stability, version) {
  EventEmitter.decorate(this);
  this.stability = stability;
  this.version = version;
  this.xpiLink = SIMULATOR_LINK.replace(/#OS#/g, OS)
                               .replace(/#VERSION#/g, version)
                               .replace(/#SLASHED_VERSION#/g, version.replace(/\./g, "_"));
  this.addonID = SIMULATOR_ADDON_ID.replace(/#SLASHED_VERSION#/g, version.replace(/\./g, "_"));
  this.updateInstallStatus();
}

SimulatorAddon.prototype = Object.create(Addon.prototype, {
  updateInstallStatus: {
    enumerable: true,
    value: function() {
      let sim = Simulator.getByVersion(this.version);
      if (sim) {
        this.status = "installed";
      } else {
        this.status = "uninstalled";
      }
    }
  },
});

function ADBAddon() {
  EventEmitter.decorate(this);
  this.xpiLink = ADB_LINK.replace(/#OS#/g, OS);
  this.addonID = ADB_ADDON_ID;
  this.updateInstallStatus();
}

ADBAddon.prototype = Object.create(Addon.prototype, {
  updateInstallStatus: {
    enumerable: true,
    value: function() {
      if (Devices.helperAddonInstalled) {
        this.status = "installed";
      } else {
        this.status = "uninstalled";
      }
    }
  },
});

