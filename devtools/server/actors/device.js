/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci, Cc } = require("chrome");
const Services = require("Services");
const protocol = require("devtools/shared/protocol");

const { DevToolsServer } = require("devtools/server/devtools-server");
const { getSystemInfo } = require("devtools/shared/system");
const { deviceSpec } = require("devtools/shared/specs/device");
const { AppConstants } = require("resource://gre/modules/AppConstants.jsm");

exports.DeviceActor = protocol.ActorClassWithSpec(deviceSpec, {
  initialize: function(conn) {
    protocol.Actor.prototype.initialize.call(this, conn);
    // pageshow and pagehide event release wake lock, so we have to acquire
    // wake lock again by pageshow event
    this._onPageShow = this._onPageShow.bind(this);
    if (this._window) {
      this._window.addEventListener("pageshow", this._onPageShow, true);
    }
    this._acquireWakeLock();
  },

  destroy: function() {
    protocol.Actor.prototype.destroy.call(this);
    this._releaseWakeLock();
    if (this._window) {
      this._window.removeEventListener("pageshow", this._onPageShow, true);
    }
  },

  getDescription: function() {
    return Object.assign({}, getSystemInfo(), {
      canDebugServiceWorkers: true,
    });
  },

  _acquireWakeLock: function() {
    if (AppConstants.platform !== "android") {
      return;
    }

    const pm = Cc["@mozilla.org/power/powermanagerservice;1"].getService(
      Ci.nsIPowerManagerService
    );
    this._wakelock = pm.newWakeLock("screen", this._window);
  },

  _releaseWakeLock: function() {
    if (this._wakelock) {
      try {
        this._wakelock.unlock();
      } catch (e) {
        // Ignore error since wake lock is already unlocked
      }
      this._wakelock = null;
    }
  },

  _onPageShow: function() {
    this._releaseWakeLock();
    this._acquireWakeLock();
  },

  get _window() {
    return Services.wm.getMostRecentWindow(DevToolsServer.chromeWindowType);
  },
});
