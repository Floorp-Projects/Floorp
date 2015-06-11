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
loader.lazyRequireGetter(this, "PerformanceFront",
  "devtools/performance/front", true);

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
    this.panelWin.gToolbox = this._toolbox;
    this.panelWin.gTarget = this.target;
    this._onRecordingStartOrStop = this._onRecordingStartOrStop.bind(this);

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
    this.panelWin.gFront.on("recording-started", this._onRecordingStartOrStop);
    this.panelWin.gFront.on("recording-stopped", this._onRecordingStartOrStop);

    yield this.panelWin.startupPerformance();

    this.isReady = true;
    this.emit("ready");
    return this;
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

    this.panelWin.gFront.off("recording-started", this._onRecordingStartOrStop);
    this.panelWin.gFront.off("recording-stopped", this._onRecordingStartOrStop);
    yield this.panelWin.shutdownPerformance();
    this.emit("destroyed");
    this._destroyed = true;
  }),

  _onRecordingStartOrStop: function () {
    let front = this.panelWin.gFront;
    if (front.isRecording()) {
      this._toolbox.highlightTool("performance");
    } else {
      this._toolbox.unhighlightTool("performance");
    }
  }
};
