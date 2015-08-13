/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu, Cr } = require("chrome");
const { Task } = require("resource://gre/modules/Task.jsm");

loader.lazyRequireGetter(this, "promise");
loader.lazyRequireGetter(this, "EventEmitter",
  "devtools/toolkit/event-emitter");

function PerformancePanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this._toolbox = toolbox;

  EventEmitter.decorate(this);
}

exports.PerformancePanel = PerformancePanel;

PerformancePanel.prototype = {
  /**
   * Open is effectively an asynchronous constructor.
   *
   * @return object
   *         A promise that is resolved when the Performance tool
   *         completes opening.
   */
  open: Task.async(function*() {
    if (this._opening) {
      return this._opening;
    }
    let deferred = promise.defer();
    this._opening = deferred.promise;

    this.panelWin.gToolbox = this._toolbox;
    this.panelWin.gTarget = this.target;
    this._checkRecordingStatus = this._checkRecordingStatus.bind(this);

    // Actor is already created in the toolbox; reuse
    // the same front, and the toolbox will also initialize the front,
    // but redo it here so we can hook into the same event to prevent race conditions
    // in the case of the front still being in the process of opening.
    let front = yield this.panelWin.gToolbox.initPerformance();

    // This should only happen if this is completely unsupported (when profiler
    // does not exist), and in that case, the tool shouldn't be available,
    // so let's ensure this assertion.
    if (!front) {
      Cu.reportError("No PerformanceFront found in toolbox.");
    }

    this.panelWin.gFront = front;
    let { PerformanceController, EVENTS } = this.panelWin;
    PerformanceController.on(EVENTS.NEW_RECORDING, this._checkRecordingStatus);
    PerformanceController.on(EVENTS.RECORDING_STATE_CHANGE, this._checkRecordingStatus);
    yield this.panelWin.startupPerformance();

    // Fire this once incase we have an in-progress recording (console profile)
    // that caused this start up, and no state change yet, so we can highlight the
    // tab if we need.
    this._checkRecordingStatus();

    this.isReady = true;
    this.emit("ready");

    deferred.resolve(this);
    return this._opening;
  }),

  // DevToolPanel API

  get target() {
    return this._toolbox.target;
  },

  destroy: Task.async(function*() {
    // Make sure this panel is not already destroyed.
    if (this._destroyed) {
      return;
    }

    let { PerformanceController, EVENTS } = this.panelWin;
    PerformanceController.off(EVENTS.NEW_RECORDING, this._checkRecordingStatus);
    PerformanceController.off(EVENTS.RECORDING_STATE_CHANGE, this._checkRecordingStatus);
    yield this.panelWin.shutdownPerformance();
    this.emit("destroyed");
    this._destroyed = true;
  }),

  _checkRecordingStatus: function () {
    if (this.panelWin.PerformanceController.isRecording()) {
      this._toolbox.highlightTool("performance");
    } else {
      this._toolbox.unhighlightTool("performance");
    }
  }
};
