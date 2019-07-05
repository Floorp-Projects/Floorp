/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Initialize the require function through a BrowserLoader. This loader ensures
 * that the popup can use require and has access to the window object.
 */
const { BrowserLoader } = ChromeUtils.import(
  "resource://devtools/client/shared/browser-loader.js"
);
const { require } = BrowserLoader({
  baseURI: "resource://devtools/client/performance-new/popup/",
  window,
});

/**
 * The background.jsm manages the profiler state, and can be loaded multiple time
 * for various components. This pop-up needs a copy, and it is also used by the
 * profiler shortcuts. In order to do this, the background code needs to live in a
 * JSM module, that can be shared with the DevTools keyboard shortcut manager.
 */

/**
 * Require the popup code, and initialize it.
 */
const { initializePopup } = require("./popup");

document.addEventListener("DOMContentLoaded", () => {
  initializePopup();
});
