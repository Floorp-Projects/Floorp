/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { RuntimeTypes } = require("devtools/client/webide/modules/runtime-types");
const { prepareTCPConnection } = require("devtools/shared/adb/commands/index");
const { shell } = require("devtools/shared/adb/commands/index");

class AdbRuntime {
  constructor(adbDevice, socketPath) {
    this.type = RuntimeTypes.USB;

    this._adbDevice = adbDevice;
    this._socketPath = socketPath;
  }

  async init() {
    const packageName = this._packageName();
    const query = `dumpsys package ${packageName} | grep versionName`;
    const versionNameString = await shell(this._adbDevice.id, query);
    const matches = versionNameString.match(/versionName=([\d.]+)/);
    if (matches && matches[1]) {
      this._versionName = matches[1];
    }
  }

  get id() {
    return this._adbDevice.id + "|" + this._socketPath;
  }

  get isFenix() {
    return this._packageName().includes("org.mozilla.fenix");
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
        return "Firefox";
      case "org.mozilla.firefox_beta":
        return "Firefox Beta";
      case "org.mozilla.fennec":
      case "org.mozilla.fennec_aurora":
        // This package name is now the one for Firefox Nightly distributed
        // through the Google Play Store since "dawn project"
        // cf. https://bugzilla.mozilla.org/show_bug.cgi?id=1357351#c8
        return "Firefox Nightly";
      case "org.mozilla.fenix":
        // The current Nightly build for Fenix is available under this package name
        // but the official packages will use fenix, fenix.beta and fenix.nightly.
        return "Firefox Preview";
      case "org.mozilla.fenix.beta":
        return "Firefox Preview Beta";
      case "org.mozilla.fenix.nightly":
        return "Firefox Preview Nightly";
      default:
        return "Firefox Custom";
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
    return this._socketPath.startsWith("@") ?
      this._socketPath.substr(1).split("/")[0] :
      this._socketPath.split("/")[3];
  }
}
exports.AdbRuntime = AdbRuntime;
