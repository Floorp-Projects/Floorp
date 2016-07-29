/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env browser */
/* global browser */

"use strict";

// This function is called from the webconsole test:
// browser_addons_debug_webextension.js
function myWebExtensionPopupAddonFunction() {  // eslint-disable-line no-unused-vars
  console.log("Popup page function called", browser.runtime.getManifest());
}
