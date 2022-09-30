/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  prepareTCPConnection,
} = require("resource://devtools/client/shared/remote-debugging/adb/commands/index.js");
const {
  shell,
} = require("resource://devtools/client/shared/remote-debugging/adb/commands/index.js");

class AdbRuntime {
  constructor(adbDevice, socketPath) {
    this._adbDevice = adbDevice;
    this._socketPath = socketPath;
    // Set a default version name in case versionName cannot be parsed.
    this._versionName = "";
  }

  async init() {
    const packageName = this._packageName();
    const query = `dumpsys package ${packageName} | grep versionName`;
    const versionNameString = await shell(this._adbDevice.id, query);

    // The versionName can have different formats depending on the channel
    // - `versionName=Nightly 191016 06:01\n` on Nightly
    // - `versionName=2.1.0\n` on Release
    // We use a very flexible regular expression to accommodate for those
    // different formats.
    const matches = versionNameString.match(/versionName=(.*)\n/);
    if (matches?.[1]) {
      this._versionName = matches[1];
    }
  }

  get id() {
    return this._adbDevice.id + "|" + this._socketPath;
  }

  get isFenix() {
    // Firefox Release uses "org.mozilla.firefox"
    // Firefox Beta uses "org.mozilla.firefox_beta"
    // Firefox Nightly uses "org.mozilla.fenix"
    const isFirefox =
      this._packageName().includes("org.mozilla.firefox") ||
      this._packageName().includes("org.mozilla.fenix");

    if (!isFirefox) {
      return false;
    }

    // Firefox Release (based on Fenix) is not released in all regions yet, so
    // we should still check for Fennec using the version number.
    // Note that Fennec's versionName followed Firefox versions (eg "68.11.0").
    // We can find the main version number in it. Fenix on the other hand has
    // version names such as "Nightly 200730 06:21".
    const mainVersion = Number(this.versionName.split(".")[0]);
    const isFennec = mainVersion === 68;

    // Application is Fenix if this is a Firefox application with a version
    // different from the Fennec version.
    return !isFennec;
  }

  get deviceId() {
    return this._adbDevice.id;
  }

  get deviceName() {
    return this._adbDevice.name;
  }

  get versionName() {
    return this._versionName;
  }

  get shortName() {
    const packageName = this._packageName();

    switch (packageName) {
      case "org.mozilla.firefox":
        if (!this.isFenix) {
          // Old Fennec release
          return "Firefox (Fennec)";
        }
        // Official Firefox app, based on Fenix
        return "Firefox";
      case "org.mozilla.firefox_beta":
        // Official Firefox Beta app, based on Fenix
        return "Firefox Beta";
      case "org.mozilla.fenix":
      case "org.mozilla.fenix.nightly":
        // Official Firefox Nightly app, based on Fenix
        return "Firefox Nightly";
      default:
        // Unknown package name
        return `Firefox (${packageName})`;
    }
  }

  get socketPath() {
    return this._socketPath;
  }

  get name() {
    return `${this.shortName} on Android (${this.deviceName})`;
  }

  connect(connection) {
    return prepareTCPConnection(this.deviceId, this._socketPath).then(port => {
      connection.host = "localhost";
      connection.port = port;
      connection.connect();
    });
  }

  _packageName() {
    // If using abstract socket address, it is "@org.mozilla.firefox/..."
    // If using path base socket, it is "/data/data/<package>...""
    // Until Fennec 62 only supports path based UNIX domain socket, but
    // Fennec 63+ supports both path based and abstract socket.
    return this._socketPath.startsWith("@")
      ? this._socketPath.substr(1).split("/")[0]
      : this._socketPath.split("/")[3];
  }
}
exports.AdbRuntime = AdbRuntime;
