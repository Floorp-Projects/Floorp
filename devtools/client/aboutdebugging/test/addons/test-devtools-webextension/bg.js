/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env browser */
/* global browser */

"use strict";

document.body.innerText = "Background Page Body Test Content";

// These functions are called from the following about:debugging tests:
// - browser_addons_debug_webextension.js
// - browser_addons_debug_webextension_popup.js

// eslint-disable-next-line no-unused-vars
function myWebExtensionAddonFunction() {
  console.log("Background page function called", browser.runtime.getManifest());
}

// eslint-disable-next-line no-unused-vars
function myWebExtensionShowPopup() {
  browser.test.sendMessage("readyForOpenPopup");
}
