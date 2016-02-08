/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env browser */
/* exported startup, shutdown, install, uninstall */

"use strict";

Components.utils.import("resource://gre/modules/Services.jsm");
function startup() {
  Services.obs.notifyObservers(null, "test-devtools", null);
}
function shutdown() {}
function install() {}
function uninstall() {}
