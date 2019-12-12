/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var Services = require("Services");

exports.defaultThreadOptions = function() {
  return {
    autoBlackBox: false,
    pauseOnExceptions: Services.prefs.getBoolPref(
      "devtools.debugger.pause-on-exceptions"
    ),
    ignoreCaughtExceptions: Services.prefs.getBoolPref(
      "devtools.debugger.ignore-caught-exceptions"
    ),
    shouldShowOverlay: Services.prefs.getBoolPref(
      "devtools.debugger.features.overlay"
    ),
    shouldIncludeAsyncLiveFrames: Services.prefs.getBoolPref(
      "devtools.debugger.features.async-live-stacks"
    ),
    skipBreakpoints: Services.prefs.getBoolPref(
      "devtools.debugger.skip-pausing"
    ),
    logEventBreakpoints: Services.prefs.getBoolPref(
      "devtools.debugger.log-event-breakpoints"
    ),
  };
};
