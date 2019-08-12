/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

/**
 * This module controls various global flags that can be toggled on and off.
 * These flags are generally used to change the behavior of the code during
 * testing. They are tracked by preferences so that they are propagated
 * between the parent and content processes. The flags are exposed via a module
 * as a conveniene and to stop from littering preference names throughout the
 * code ase.
 *
 * Each of the flags is documented where it is defined.
 */

/**
 * We cannot make a normal property writeable on `exports` because
 * the module system freezes it. This function observes a preference
 * and provides the latest value through a getter.
 */
function makePrefTrackedFlag(exports, name, pref) {
  let flag;
  // We don't have access to pref in worker, so disable all logs by default
  if (isWorker) {
    flag = false;
  } else {
    flag = Services.prefs.getBoolPref(pref, false);
    const prefObserver = () => {
      flag = Services.prefs.getBoolPref(pref, false);
    };
    Services.prefs.addObserver(pref, prefObserver);

    // Also listen for Loader unload to unregister the pref observer and prevent leaking
    const unloadObserver = function() {
      Services.prefs.removeObserver(pref, prefObserver);
      Services.obs.removeObserver(unloadObserver, "devtools:loader:destroy");
    };
    Services.obs.addObserver(unloadObserver, "devtools:loader:destroy");
  }
  Object.defineProperty(exports, name, {
    get: function() {
      return flag;
    },
  });
}

/**
 * Setting the "devtools.debugger.log" preference to true will enable logging of
 * the RDP calls to the debugger server.
 */
makePrefTrackedFlag(exports, "wantLogging", "devtools.debugger.log");

/**
 * Setting the "devtools.debugger.log.verbose" preference to true will enable a
 * more verbose logging of the the RDP. The "devtools.debugger.log" preference
 * must be set to true as well for this to have any effect.
 */
makePrefTrackedFlag(exports, "wantVerbose", "devtools.debugger.log.verbose");

/**
 * Setting the "devtools.testing" preference to true will toggle on certain
 * behaviors that can differ from the production version of the code. These
 * behaviors typically enable easier testing or enhanced debugging features.
 */
makePrefTrackedFlag(exports, "testing", "devtools.testing");
