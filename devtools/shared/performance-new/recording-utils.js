/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check
"use strict";

/**
 * This file is for the new performance panel that targets profiler.firefox.com,
 * not the default-enabled DevTools performance panel.
 */

/**
 * @typedef {import("../../client/performance-new/@types/perf").GetActiveBrowserID} GetActiveBrowserID
 */

/**
 * Gets the ID of active tab from the browser.
 *
 * @type {GetActiveBrowserID}
 */
function getActiveBrowserID() {
  const win = Services.wm.getMostRecentWindow("navigator:browser");

  if (win?.gBrowser?.selectedBrowser?.browsingContext?.browserId) {
    return win.gBrowser.selectedBrowser.browsingContext.browserId;
  }

  console.error(
    "Failed to get the active browserId while starting the profiler."
  );
  // `0` mean that we failed to ge the active browserId, and it's
  // treated as null value in the platform.
  return 0;
}

module.exports = {
  getActiveBrowserID,
};
