/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { shell } = require("devtools/shared/adb/commands/index");

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
    const model = await shell("getprop ro.product.model");
    this._model = model.trim();
    return this._model;
  }

  // This method is not using any information from the instance, but in theory getting
  // runtime socket paths (as well as model) should be device specific. So we should use
  // information available on the instance when implementing multi device support.
  // See Bug 1507126.
  async getRuntimeSocketPaths() {
    // A matching entry looks like:
    // 00000000: 00000002 00000000 00010000 0001 01 6551588
    //  /data/data/org.mozilla.fennec/firefox-debugger-socket
    const query = "cat /proc/net/unix";
    const rawSocketInfo = await shell(query);

    // Filter to lines with "firefox-debugger-socket"
    let socketInfos = rawSocketInfo.split(/\r?\n/);
    socketInfos = socketInfos.filter(l => l.includes("firefox-debugger-socket"));

    // It's possible to have multiple lines with the same path, so de-dupe them
    const socketPaths = new Set();
    for (const socketInfo of socketInfos) {
      const socketPath = socketInfo.split(" ").pop();
      socketPaths.add(socketPath);
    }

    return socketPaths;
  }
}

module.exports = AdbDevice;
