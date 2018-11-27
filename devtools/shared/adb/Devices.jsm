/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
const EventEmitter = require("devtools/shared/event-emitter");
const { adbAddon, ADB_ADDON_STATES } = require("devtools/shared/adb/adb-addon");

/* exported EXPORTED_SYMBOLS */

const EXPORTED_SYMBOLS = ["Devices"];

const Devices = {
  _devices: {},

  register: function(name, device) {
    this._devices[name] = device;
    this.emit("register");
  },

  unregister: function(name) {
    delete this._devices[name];
    this.emit("unregister");
  },

  available: function() {
    return Object.keys(this._devices).sort();
  },

  getByName: function(name) {
    return this._devices[name];
  },

  updateAdbAddonStatus: function() {
    const installed = adbAddon.status === ADB_ADDON_STATES.INSTALLED;
    if (!installed) {
      for (const name in this._devices) {
        this.unregister(name);
      }
    }
  },
};

Object.defineProperty(this, "Devices", {
  value: Devices,
  enumerable: true,
  writable: false,
});

EventEmitter.decorate(Devices);

adbAddon.on("update", () => Devices.updateAdbAddonStatus());
