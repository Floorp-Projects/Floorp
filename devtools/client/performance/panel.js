/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");

function PerformancePanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this.toolbox = toolbox;
  this._targetAvailablePromise = Promise.resolve();

  this._onTargetAvailable = this._onTargetAvailable.bind(this);

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

    const { PerformanceController, EVENTS } = this.panelWin;
    PerformanceController.on(
      EVENTS.RECORDING_ADDED,
      this._checkRecordingStatus
    );
    PerformanceController.on(
      EVENTS.RECORDING_STATE_CHANGE,
      this._checkRecordingStatus
    );

    // In case that the target is switched across process, the corresponding front also
    // will be changed. In order to detect that, watch the change.
    // Also, we wait for `watchTargets` to end. Indeed the function `_onTargetAvailable
    // will be called synchronously with current target as a parameter by
    // the `watchTargets` function.
    // So this `await` waits for initialization with current target, happening
    // in `_onTargetAvailable`.
    await this.toolbox.targetList.watchTargets(
      [this.toolbox.targetList.TYPES.FRAME],
      this._onTargetAvailable
    );

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

    await this.toolbox.targetList.unwatchTargets(
      [this.toolbox.targetList.TYPES.FRAME],
      this._onTargetAvailable
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

  /**
   * This function executes actual logic for the target-switching.
   *
   * @param {TargetFront} - targetFront
   *        As we are watching only FRAME type for this panel,
   *        the target should be a instance of BrowsingContextTarget.
   * @param {Boolean} - isTopLevel
   *        true if the target is a full page.
   */
  async _handleTargetAvailable({ targetFront, isTopLevel }) {
    if (isTopLevel) {
      const { PerformanceController, PerformanceView } = this.panelWin;
      const performanceFront = await targetFront.getFront("performance");

      if (!this._isPanelInitialized) {
        await PerformanceController.initialize(targetFront, performanceFront);
        await PerformanceView.initialize();
        PerformanceController.enableFrontEventListeners();
        this._isPanelInitialized = true;
      } else {
        const isRecording = PerformanceController.isRecording();
        if (isRecording) {
          await PerformanceController.stopRecording();
        }

        PerformanceView.resetBufferStatus();
        PerformanceController.updateFronts(targetFront, performanceFront);

        if (isRecording) {
          await PerformanceController.startRecording();
        }
      }

      // Keep references on window for front for tests.
      this.panelWin.gFront = performanceFront;
    }
  },

  /**
   * This function is called for every target is available.
   */
  _onTargetAvailable(parameters) {
    // As this function is called asynchronous, while previous processing, this might be
    // called. Thus, we wait until finishing previous one before starting next.
    this._targetAvailablePromise = this._targetAvailablePromise.then(() =>
      this._handleTargetAvailable(parameters)
    );

    return this._targetAvailablePromise;
  },
};
