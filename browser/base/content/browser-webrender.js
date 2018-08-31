/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

/**
 * Global browser interface with the WebRender backend.
 */
var gWebRender = {
  /**
   * Trigger a WebRender capture of the current state into a local folder.
   */
  capture() {
    window.windowUtils.wrCapture();
  },
};
