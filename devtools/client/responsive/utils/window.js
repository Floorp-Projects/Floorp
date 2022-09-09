/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Returns the `nsIDOMWindow` toplevel window for any child/inner window
 */
function getTopLevelWindow(window) {
  return window.browsingContext.topChromeWindow;
}
exports.getTopLevelWindow = getTopLevelWindow;

function getDOMWindowUtils(window) {
  return window.windowUtils;
}
exports.getDOMWindowUtils = getDOMWindowUtils;

/**
 * Check if the given browser window has finished the startup.
 * @params {nsIDOMWindow} window
 */
const isStartupFinished = window => window.gBrowserInit?.delayedStartupFinished;

function startup(window) {
  return new Promise(resolve => {
    if (isStartupFinished(window)) {
      resolve(window);
      return;
    }
    Services.obs.addObserver(function listener({ subject }) {
      if (subject === window) {
        Services.obs.removeObserver(
          listener,
          "browser-delayed-startup-finished"
        );
        resolve(window);
      }
    }, "browser-delayed-startup-finished");
  });
}
exports.startup = startup;
