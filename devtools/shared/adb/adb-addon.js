/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AddonManager } = require("resource://gre/modules/AddonManager.jsm");
const Services = require("Services");
const EventEmitter = require("devtools/shared/event-emitter");

const PREF_ADB_EXTENSION_URL = "devtools.remote.adb.extensionURL";
const PREF_ADB_EXTENSION_ID = "devtools.remote.adb.extensionID";

// Extension ID for adb helper extension that might be installed on Firefox 63 or older.
const ADB_HELPER_ADDON_ID = "adbhelper@mozilla.org";
// Extension ID for Valence extension that is no longer supported.
const VALENCE_ADDON_ID = "fxdevtools-adapters@mozilla.org";

const ADB_ADDON_STATES = {
  DOWNLOADING: "downloading",
  INSTALLED: "installed",
  INSTALLING: "installing",
  PREPARING: "preparing",
  UNINSTALLED: "uninstalled",
  UNKNOWN: "unknown",
};
exports.ADB_ADDON_STATES = ADB_ADDON_STATES;

/**
 * Wrapper around the ADB Extension providing ADB binaries for devtools remote debugging.
 * Fires the following events:
 * - "update": the status of the addon was updated
 * - "failure": addon installation failed
 * - "progress": addon download in progress
 *
 * AdbAddon::state can take any of the values from ADB_ADDON_STATES.
 */
class ADBAddon extends EventEmitter {
  constructor() {
    super();

    this._status = ADB_ADDON_STATES.UNKNOWN;

    const addonsListener = {};
    addonsListener.onEnabled = addonsListener.onDisabled = addonsListener.onInstalled = addonsListener.onUninstalled = () =>
      this.updateInstallStatus();
    AddonManager.addAddonListener(addonsListener);

    this.updateInstallStatus();
  }

  set status(value) {
    if (this._status != value) {
      this._status = value;
      this.emit("update");
    }
  }

  get status() {
    return this._status;
  }

  async _getAddon() {
    const addonId = Services.prefs.getCharPref(PREF_ADB_EXTENSION_ID);
    return AddonManager.getAddonByID(addonId);
  }

  async updateInstallStatus() {
    const addon = await this._getAddon();
    if (addon && !addon.userDisabled) {
      this.status = ADB_ADDON_STATES.INSTALLED;
    } else {
      this.status = ADB_ADDON_STATES.UNINSTALLED;
    }
  }

  /**
   * Returns the platform specific download link for the ADB extension.
   */
  _getXpiLink() {
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
        OS = "linux";
      }
    }

    const xpiLink = Services.prefs.getCharPref(PREF_ADB_EXTENSION_URL);
    return xpiLink.replace(/#OS#/g, OS);
  }

  /**
   * Install and enable the adb extension. Returns a promise that resolves when ADB is
   * enabled.
   *
   * @param {String} source
   *        String passed to the AddonManager for telemetry.
   */
  async install(source) {
    if (!source) {
      throw new Error(
        "Missing mandatory `source` parameter for adb-addon.install"
      );
    }

    const addon = await this._getAddon();
    if (addon && !addon.userDisabled) {
      this.status = ADB_ADDON_STATES.INSTALLED;
      return;
    }
    this.status = ADB_ADDON_STATES.PREPARING;
    if (addon?.userDisabled) {
      await addon.enable();
    } else {
      const install = await AddonManager.getInstallForURL(this._getXpiLink(), {
        telemetryInfo: { source },
      });
      install.addListener(this);
      install.install();
    }
  }

  async uninstall() {
    const addon = await this._getAddon();
    addon.uninstall();
  }

  /**
   * Cleanup old remote debugging extensions from profiles that might still have them
   * installed. Should be called from remote debugging entry points.
   */
  async uninstallUnsupportedExtensions() {
    const [adbHelperAddon, valenceAddon] = await Promise.all([
      AddonManager.getAddonByID(ADB_HELPER_ADDON_ID),
      AddonManager.getAddonByID(VALENCE_ADDON_ID),
    ]);

    if (adbHelperAddon) {
      adbHelperAddon.uninstall();
    }

    if (valenceAddon) {
      valenceAddon.uninstall();
    }
  }

  installFailureHandler(install, message) {
    this.status = ADB_ADDON_STATES.UNINSTALLED;
    this.emit("failure", message);
  }

  // Expected AddonManager install listener.
  onDownloadStarted() {
    this.status = ADB_ADDON_STATES.DOWNLOADING;
  }

  // Expected AddonManager install listener.
  onDownloadProgress(install) {
    if (install.maxProgress == -1) {
      this.emit("progress", -1);
    } else {
      this.emit("progress", install.progress / install.maxProgress);
    }
  }

  // Expected AddonManager install listener.
  onDownloadCancelled(install) {
    this.installFailureHandler(install, "Download cancelled");
  }

  // Expected AddonManager install listener.
  onDownloadFailed(install) {
    this.installFailureHandler(install, "Download failed");
  }

  // Expected AddonManager install listener.
  onInstallStarted() {
    this.status = ADB_ADDON_STATES.INSTALLING;
  }

  // Expected AddonManager install listener.
  onInstallCancelled(install) {
    this.installFailureHandler(install, "Install cancelled");
  }

  // Expected AddonManager install listener.
  onInstallFailed(install) {
    this.installFailureHandler(install, "Install failed");
  }

  // Expected AddonManager install listener.
  onInstallEnded({ addon }) {
    addon.enable();
  }
}

exports.adbAddon = new ADBAddon();
