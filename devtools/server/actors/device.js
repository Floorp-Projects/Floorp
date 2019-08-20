/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci, Cc } = require("chrome");
const Services = require("Services");
const protocol = require("devtools/shared/protocol");
const { LongStringActor } = require("devtools/server/actors/string");
const {
  addDebugServiceWorkersListener,
  canDebugServiceWorkers,
  isParentInterceptEnabled,
  removeDebugServiceWorkersListener,
} = require("devtools/shared/service-workers-debug-helper");

const { DebuggerServer } = require("devtools/server/debugger-server");
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

    this._onDebugServiceWorkersUpdated = this._onDebugServiceWorkersUpdated.bind(
      this
    );
    addDebugServiceWorkersListener(this._onDebugServiceWorkersUpdated);
  },

  destroy: function() {
    protocol.Actor.prototype.destroy.call(this);
    this._releaseWakeLock();
    if (this._window) {
      this._window.removeEventListener("pageshow", this._onPageShow, true);
    }
    removeDebugServiceWorkersListener(this._onDebugServiceWorkersUpdated);
  },

  _onDebugServiceWorkersUpdated: function() {
    this.emit("can-debug-sw-updated", canDebugServiceWorkers());
  },

  getDescription: function() {
    return Object.assign({}, getSystemInfo(), {
      canDebugServiceWorkers: canDebugServiceWorkers(),
      isParentInterceptEnabled: isParentInterceptEnabled(),
    });
  },

  screenshotToDataURL: function() {
    const window = this._window;
    const { devicePixelRatio } = window;
    const canvas = window.document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "canvas"
    );
    const width = window.innerWidth;
    const height = window.innerHeight;
    canvas.setAttribute("width", Math.round(width * devicePixelRatio));
    canvas.setAttribute("height", Math.round(height * devicePixelRatio));
    const context = canvas.getContext("2d");
    const flags =
      context.DRAWWINDOW_DRAW_CARET |
      context.DRAWWINDOW_DRAW_VIEW |
      context.DRAWWINDOW_USE_WIDGET_LAYERS;
    context.scale(devicePixelRatio, devicePixelRatio);
    context.drawWindow(window, 0, 0, width, height, "rgb(255,255,255)", flags);
    const dataURL = canvas.toDataURL("image/png");
    return new LongStringActor(this.conn, dataURL);
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
    return Services.wm.getMostRecentWindow(DebuggerServer.chromeWindowType);
  },
});
