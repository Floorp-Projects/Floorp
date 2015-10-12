/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { on, once, off, emit } = require("sdk/event/core");
const { Class } = require("sdk/core/heritage");

/**
 * A very simple utility for monitoring framerate. Takes a `tabActor`
 * and monitors framerate over time. The actor wrapper around this
 * can be found at devtools/server/actors/framerate.js
 */
var Framerate = exports.Framerate = Class({
  initialize: function (tabActor) {
    this.tabActor = tabActor;
    this._contentWin = tabActor.window;
    this._onRefreshDriverTick = this._onRefreshDriverTick.bind(this);
    this._onGlobalCreated = this._onGlobalCreated.bind(this);
    on(this.tabActor, "window-ready", this._onGlobalCreated);
  },
  destroy: function(conn) {
    off(this.tabActor, "window-ready", this._onGlobalCreated);
    this.stopRecording();
  },

  /**
   * Starts monitoring framerate, storing the frames per second.
   */
  startRecording: function () {
    if (this._recording) {
      return;
    }
    this._recording = true;
    this._ticks = [];
    this._startTime = this.tabActor.docShell.now();
    this._rafID = this._contentWin.requestAnimationFrame(this._onRefreshDriverTick);
  },

  /**
   * Stops monitoring framerate, returning the recorded values.
   */
  stopRecording: function (beginAt = 0, endAt = Number.MAX_SAFE_INTEGER) {
    if (!this._recording) {
      return [];
    }
    let ticks = this.getPendingTicks(beginAt, endAt);
    this.cancelRecording();
    return ticks;
  },

  /**
   * Stops monitoring framerate, without returning the recorded values.
   */
  cancelRecording: function () {
    this._contentWin.cancelAnimationFrame(this._rafID);
    this._recording = false;
    this._ticks = null;
    this._rafID = -1;
  },

  /**
   * Returns whether this instance is currently recording.
   */
  isRecording: function () {
    return !!this._recording;
  },

  /**
   * Gets the refresh driver ticks recorded so far.
   */
  getPendingTicks: function (beginAt = 0, endAt = Number.MAX_SAFE_INTEGER) {
    if (!this._ticks) {
      return [];
    }
    return this._ticks.filter(e => e >= beginAt && e <= endAt);
  },

  /**
   * Function invoked along with the refresh driver.
   */
  _onRefreshDriverTick: function () {
    if (!this._recording) {
      return;
    }
    this._rafID = this._contentWin.requestAnimationFrame(this._onRefreshDriverTick);
    this._ticks.push(this.tabActor.docShell.now() - this._startTime);
  },

  /**
   * When the content window for the tab actor is created.
   */
  _onGlobalCreated: function (win) {
    if (this._recording) {
      this._contentWin.cancelAnimationFrame(this._rafID);
      this._rafID = this._contentWin.requestAnimationFrame(this._onRefreshDriverTick);
    }
  }
});
