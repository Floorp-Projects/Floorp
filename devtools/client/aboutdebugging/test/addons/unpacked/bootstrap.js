/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env browser */
/* exported startup, shutdown, install, uninstall */

"use strict";

Components.utils.import("resource://gre/modules/Services.jsm");

// This function is called from the webconsole test:
// browser_addons_debug_bootstrapped.js
function myBootstrapAddonFunction() { // eslint-disable-line no-unused-vars
  Services.obs.notifyObservers(null, "addon-console-works");
}

function startup() {
  Services.obs.notifyObservers(null, "test-devtools");
}
function shutdown() {}
function install() {}
function uninstall() {}
