/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env browser */
/* global browser */

"use strict";

// This function is called from the following about:debugging test:
// browser_addons_debug_webextension.js
//
// eslint-disable-next-line no-unused-vars
function myWebExtensionPopupAddonFunction() {
  browser.test.sendMessage("popupPageFunctionCalled", browser.runtime.getManifest());
}
