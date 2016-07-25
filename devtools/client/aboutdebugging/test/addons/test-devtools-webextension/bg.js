/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env browser */
/* global browser */

"use strict";

document.body.innerText = "Background Page Body Test Content";

// This function are called from the webconsole test:
// browser_addons_debug_webextension.js

function myWebExtensionAddonFunction() {  // eslint-disable-line no-unused-vars
  console.log("Background page function called", browser.runtime.getManifest());
}

function myWebExtensionShowPopup() {  // eslint-disable-line no-unused-vars
  console.log("readyForOpenPopup");
}
