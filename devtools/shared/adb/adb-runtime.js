/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { RuntimeTypes } = require("devtools/client/webide/modules/runtime-types");
const { prepareTCPConnection } = require("devtools/shared/adb/commands/index");

class AdbRuntime {
  constructor(adbDevice, socketPath) {
    this.type = RuntimeTypes.USB;

    this._adbDevice = adbDevice;
    this._socketPath = socketPath;
  }

  get id() {
    return this._adbDevice.id + "|" + this._socketPath;
  }

  get deviceName() {
    return this._adbDevice.name;
  }

  get shortName() {
    return `Firefox ${this._channel()}`;
  }

  get name() {
    return `${this.shortName} on Android (${this.deviceName})`;
  }

  connect(connection) {
    return prepareTCPConnection(this._socketPath).then(port => {
      connection.host = "localhost";
      connection.port = port;
      connection.connect();
    });
  }

  isUnknown() {
    return false;
  }

  _channel() {
    const packageName = this._packageName();

    switch (packageName) {
      case "org.mozilla.firefox":
        return "";
      case "org.mozilla.firefox_beta":
        return "Beta";
      case "org.mozilla.fennec":
      case "org.mozilla.fennec_aurora":
        // This package name is now the one for Firefox Nightly distributed
        // through the Google Play Store since "dawn project"
        // cf. https://bugzilla.mozilla.org/show_bug.cgi?id=1357351#c8
        return "Nightly";
      default:
        return "Custom";
    }
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

/**
 * UnknownAdbRuntime instance will be used to represent devices which have USB debugging
 * enabled but where Firefox (or another debuggable gecko-based runtime) is either not
 * started or not ready for USB debugging.
 */
class UnknownAdbRuntime extends AdbRuntime {
  constructor(adbDevice) {
    super(adbDevice);
  }

  get id() {
    return this._adbDevice.id;
  }

  get shortName() {
    return "Unknown runtime";
  }

  connect(connection) {
    throw new Error("Cannot connect on unknown runtime");
  }

  isUnknown() {
    return true;
  }
}
exports.UnknownAdbRuntime = UnknownAdbRuntime;
