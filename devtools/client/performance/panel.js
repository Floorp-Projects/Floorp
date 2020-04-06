/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");

function PerformancePanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this.toolbox = toolbox;

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
  async open() {
    if (this._opening) {
      return this._opening;
    }

    this._checkRecordingStatus = this._checkRecordingStatus.bind(this);

    // Actor is already created in the toolbox; reuse
    // the same front, and the toolbox will also initialize the front,
    // but redo it here so we can hook into the same event to prevent race conditions
    // in the case of the front still being in the process of opening.
    const front = await this.target.getFront("performance");

    // Keep references on window for front for tests.
    this.panelWin.gFront = front;

    // This should only happen if this is completely unsupported (when profiler
    // does not exist), and in that case, the tool shouldn't be available,
    // so let's ensure this assertion.
    if (!front) {
      console.error("No PerformanceFront found in toolbox.");
    }

    const { PerformanceController, PerformanceView, EVENTS } = this.panelWin;
    PerformanceController.on(
      EVENTS.RECORDING_ADDED,
      this._checkRecordingStatus
    );
    PerformanceController.on(
      EVENTS.RECORDING_STATE_CHANGE,
      this._checkRecordingStatus
    );

    await PerformanceController.initialize(this.toolbox, this.target, front);
    await PerformanceView.initialize();
    PerformanceController.enableFrontEventListeners();

    // Fire this once incase we have an in-progress recording (console profile)
    // that caused this start up, and no state change yet, so we can highlight the
    // tab if we need.
    this._checkRecordingStatus();

    this.isReady = true;
    this.emit("ready");

    this._opening = new Promise(resolve => {
      resolve(this);
    });
    return this._opening;
  },

  // DevToolPanel API

  get target() {
    return this.toolbox.target;
  },

  async destroy() {
    // Make sure this panel is not already destroyed.
    if (this._destroyed) {
      return;
    }

    const { PerformanceController, PerformanceView, EVENTS } = this.panelWin;
    PerformanceController.off(
      EVENTS.RECORDING_ADDED,
      this._checkRecordingStatus
    );
    PerformanceController.off(
      EVENTS.RECORDING_STATE_CHANGE,
      this._checkRecordingStatus
    );

    await PerformanceController.destroy();
    await PerformanceView.destroy();
    PerformanceController.disableFrontEventListeners();

    this.emit("destroyed");
    this._destroyed = true;
  },

  _checkRecordingStatus: function() {
    if (this.panelWin.PerformanceController.isRecording()) {
      this.toolbox.highlightTool("performance");
    } else {
      this.toolbox.unhighlightTool("performance");
    }
  },
};
