/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");

loader.lazyRequireGetter(this, "adbAddon", "devtools/shared/adb/adb-addon", true);
loader.lazyRequireGetter(this, "ADBScanner", "devtools/shared/adb/adb-scanner", true);

/**
 * The AddonAwareADBScanner decorates an instance of ADBScanner. It will wait until the
 * ADB addon is installed to enable() the real scanner, and will automatically disable
 * it if the addon is uninstalled.
 *
 * It implements the following public API of ADBScanner:
 * - enable
 * - disable
 * - scan
 * - listRuntimes
 * - event "runtime-list-updated"
 */
class AddonAwareADBScanner extends EventEmitter {
  /**
   * Parameters are provided only to allow tests to replace actual implementations with
   * mocks.
   *
   * @param {ADBScanner} scanner
   *        Only provided in tests for mocks
   * @param {ADBAddon} addon
   *        Only provided in tests for mocks
   */
  constructor(scanner = new ADBScanner(), addon = adbAddon) {
    super();

    this._onScannerListUpdated = this._onScannerListUpdated.bind(this);
    this._onAddonUpdate = this._onAddonUpdate.bind(this);

    this._scanner = scanner;
    this._scanner.on("runtime-list-updated", this._onScannerListUpdated);

    this._addon = addon;
  }

  /**
   * Only forward the enable() call if the addon is installed, because ADBScanner::enable
   * only works if the addon is installed.
   */
  enable() {
    if (this._addon.status === "installed") {
      this._scanner.enable();
    }

    // Remove any previous listener, to make sure we only add one listener if enable() is
    // called several times.
    this._addon.off("update", this._onAddonUpdate);

    this._addon.on("update", this._onAddonUpdate);
  }

  disable() {
    this._scanner.disable();

    this._addon.off("update", this._onAddonUpdate);
  }

  /**
   * Scan for USB devices.
   *
   * @return {Promise} Promise that will resolve when the scan is completed.
   */
  scan() {
    return this._scanner.scan();
  }

  /**
   * Get the list of currently detected runtimes.
   *
   * @return {Array} Array of currently detected runtimes.
   */
  listRuntimes() {
    return this._scanner.listRuntimes();
  }

  _onAddonUpdate() {
    if (this._addon.status === "installed") {
      this._scanner.enable();
    } else {
      this._scanner.disable();
    }
  }

  _onScannerListUpdated() {
    this.emit("runtime-list-updated");
  }
}
exports.AddonAwareADBScanner = AddonAwareADBScanner;
