/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ADB } = require("devtools/shared/adb/adb");

/**
 * A Device instance is created and registered with the Devices module whenever
 * ADB notices a new device is connected.
 */
function Device(id) {
  this.id = id;
}

Device.prototype = {
  type: "adb",

  shell: ADB.shell.bind(ADB),
  forwardPort: ADB.forwardPort.bind(ADB),
  push: ADB.push.bind(ADB),
  pull: ADB.pull.bind(ADB),
  reboot: ADB.reboot.bind(ADB),
  rebootRecovery: ADB.rebootRecovery.bind(ADB),
  rebootBootloader: ADB.rebootBootloader.bind(ADB),

  isRoot() {
    return ADB.shell("id").then(stdout => {
      if (stdout) {
        let uid = stdout.match(/uid=(\d+)/)[1];
        return uid == "0";
      }
      return false;
    });
  },

  summonRoot() {
    return ADB.root();
  },

  getModel() {
    if (this._modelPromise) {
      return this._modelPromise;
    }
    this._modelPromise = this.shell("getprop ro.product.model")
                             .then(model => model.trim());
    return this._modelPromise;
  }
};

module.exports = Device;
