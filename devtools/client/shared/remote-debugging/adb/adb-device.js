/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  shell,
} = require("devtools/client/shared/remote-debugging/adb/commands/index");

/**
 * A Device instance is created and registered with the Devices module whenever
 * ADB notices a new device is connected.
 */
class AdbDevice {
  constructor(id) {
    this.id = id;
  }

  async initialize() {
    const model = await shell(this.id, "getprop ro.product.model");
    this.model = model.trim();
  }

  get name() {
    return this.model || this.id;
  }

  async getRuntimeSocketPaths() {
    // A matching entry looks like:
    // 00000000: 00000002 00000000 00010000 0001 01 6551588
    //  /data/data/org.mozilla.fennec/firefox-debugger-socket
    const query = "cat /proc/net/unix";
    const rawSocketInfo = await shell(this.id, query);

    // Filter to lines with "firefox-debugger-socket"
    let socketInfos = rawSocketInfo.split(/\r?\n/);
    socketInfos = socketInfos.filter(l =>
      l.includes("firefox-debugger-socket")
    );

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
