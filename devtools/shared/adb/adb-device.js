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

  getModel() {
    if (this._modelPromise) {
      return this._modelPromise;
    }
    this._modelPromise = this.shell("getprop ro.product.model")
                             .then(model => model.trim());
    return this._modelPromise;
  },
  // push, pull were removed in Bug 1481691.
};

module.exports = Device;
