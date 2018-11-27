/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ADB } = require("devtools/shared/adb/adb");

/**
 * A Device instance is created and registered with the Devices module whenever
 * ADB notices a new device is connected.
 */
class AdbDevice {
  constructor(id) {
    this.id = id;
  }

  async getModel() {
    if (this._model) {
      return this._model;
    }
    const model = await ADB.shell("getprop ro.product.model");
    this._model = model.trim();
    return this._model;
  }
}

module.exports = AdbDevice;
