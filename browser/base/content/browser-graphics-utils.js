/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

/**
 * Global browser interface with graphics utilities.
 */
var gGfxUtils = {
  _isRecording: false,
  _isTransactionLogging: false,

  init() {
    if (Services.prefs.getBoolPref("gfx.webrender.enable-capture")) {
      document.getElementById("wrCaptureCmd").removeAttribute("disabled");
      document
        .getElementById("wrToggleCaptureSequenceCmd")
        .removeAttribute("disabled");
    }
  },

  /**
   * Toggle composition recording for the current window.
   */
  toggleWindowRecording() {
    window.windowUtils.setCompositionRecording(!this._isRecording);
    this._isRecording = !this._isRecording;
  },
  /**
   * Trigger a WebRender capture of the current state into a local folder.
   */
  webrenderCapture() {
    window.windowUtils.wrCapture();
  },
  /**
   * Trigger a WebRender capture of the current state and future state
   * into a local folder. If called again, it will stop capturing.
   */
  toggleWebrenderCaptureSequence() {
    window.windowUtils.wrToggleCaptureSequence();
  },

  /**
   * Toggle transaction logging to text file.
   */
  toggleTransactionLogging() {
    window.windowUtils.setTransactionLogging(!this._isTransactionLogging);
    this._isTransactionLogging = !this._isTransactionLogging;
  },
};
