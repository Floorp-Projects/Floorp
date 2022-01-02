/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci } = require("chrome");
const { dumpn } = require("devtools/shared/DevToolsUtils");
const EventEmitter = require("devtools/shared/event-emitter");
const {
  getFileForBinary,
} = require("devtools/client/shared/remote-debugging/adb/adb-binary");
const { setTimeout } = require("resource://gre/modules/Timer.jsm");

loader.lazyRequireGetter(
  this,
  "runCommand",
  "devtools/client/shared/remote-debugging/adb/commands/index",
  true
);
loader.lazyRequireGetter(
  this,
  "check",
  "devtools/client/shared/remote-debugging/adb/adb-running-checker",
  true
);

// Waits until a predicate returns true or re-tries the predicate calls
// |retry| times, we wait for 100ms between each calls.
async function waitUntil(predicate, retry = 20) {
  let count = 0;
  while (count++ < retry) {
    if (await predicate()) {
      return true;
    }
    // Wait for 100 milliseconds.
    await new Promise(resolve => setTimeout(resolve, 100));
  }
  // Timed out after trying too many times.
  return false;
}

// Class representing the ADB process.
class AdbProcess extends EventEmitter {
  constructor() {
    super();

    this._ready = false;
    this._didRunInitially = false;
  }

  get ready() {
    return this._ready;
  }

  _getAdbFile() {
    if (this._adbFilePromise) {
      return this._adbFilePromise;
    }
    this._adbFilePromise = getFileForBinary();
    return this._adbFilePromise;
  }

  async _runProcess(process, params) {
    return new Promise((resolve, reject) => {
      process.runAsync(
        params,
        params.length,
        {
          observe(subject, topic, data) {
            switch (topic) {
              case "process-finished":
                resolve();
                break;
              case "process-failed":
                reject();
                break;
            }
          },
        },
        false
      );
    });
  }

  // We startup by launching adb in server mode, and setting
  // the tcp socket preference to |true|
  async start() {
    const onSuccessfulStart = () => {
      this._ready = true;
      this.emit("adb-ready");
    };

    const isAdbRunning = await check();
    if (isAdbRunning) {
      dumpn("Found ADB process running, not restarting");
      onSuccessfulStart();
      return;
    }
    dumpn("Didn't find ADB process running, restarting");

    this._didRunInitially = true;
    const process = Cc["@mozilla.org/process/util;1"].createInstance(
      Ci.nsIProcess
    );

    // FIXME: Bug 1481691 - We should avoid extracting files every time.
    const adbFile = await this._getAdbFile();
    process.init(adbFile);
    // Hide command prompt window on Windows
    process.startHidden = true;
    process.noShell = true;
    const params = ["start-server"];
    let isStarted = false;
    try {
      await this._runProcess(process, params);
      isStarted = await waitUntil(check);
    } catch (e) {}

    if (isStarted) {
      onSuccessfulStart();
    } else {
      this._ready = false;
      throw new Error("ADB Process didn't start");
    }
  }

  /**
   * Stop the ADB server, but only if we started it.  If it was started before
   * us, we return immediately.
   */
  async stop() {
    if (!this._didRunInitially) {
      return; // We didn't start the server, nothing to do
    }
    await this.kill();
  }

  /**
   * Kill the ADB server.
   */
  async kill() {
    try {
      await runCommand("host:kill");
    } catch (e) {
      dumpn("Failed to send host:kill command");
    }
    dumpn("adb server was terminated by host:kill");
    this._ready = false;
    this._didRunInitially = false;
  }
}

exports.adbProcess = new AdbProcess();
