/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("resource://devtools/shared/protocol.js");

const {
  DevToolsServer,
} = require("resource://devtools/server/devtools-server.js");
const { getSystemInfo } = require("resource://devtools/shared/system.js");
const { deviceSpec } = require("resource://devtools/shared/specs/device.js");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

exports.DeviceActor = protocol.ActorClassWithSpec(deviceSpec, {
  initialize(conn) {
    protocol.Actor.prototype.initialize.call(this, conn);
    // pageshow and pagehide event release wake lock, so we have to acquire
    // wake lock again by pageshow event
    this._onPageShow = this._onPageShow.bind(this);
    if (this._window) {
      this._window.addEventListener("pageshow", this._onPageShow, true);
    }
    this._acquireWakeLock();
  },

  destroy() {
    protocol.Actor.prototype.destroy.call(this);
    this._releaseWakeLock();
    if (this._window) {
      this._window.removeEventListener("pageshow", this._onPageShow, true);
    }
  },

  getDescription() {
    return Object.assign({}, getSystemInfo(), {
      canDebugServiceWorkers: true,
    });
  },

  _acquireWakeLock() {
    if (AppConstants.platform !== "android") {
      return;
    }

    const pm = Cc["@mozilla.org/power/powermanagerservice;1"].getService(
      Ci.nsIPowerManagerService
    );
    this._wakelock = pm.newWakeLock("screen", this._window);
  },

  _releaseWakeLock() {
    if (this._wakelock) {
      try {
        this._wakelock.unlock();
      } catch (e) {
        // Ignore error since wake lock is already unlocked
      }
      this._wakelock = null;
    }
  },

  _onPageShow() {
    this._releaseWakeLock();
    this._acquireWakeLock();
  },

  get _window() {
    return Services.wm.getMostRecentWindow(DevToolsServer.chromeWindowType);
  },
});
