/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var Services = require("Services");
const asyncStoreHelper = require("devtools/client/shared/async-store-helper");

const asyncStore = asyncStoreHelper("debugger", {
  pendingBreakpoints: ["pending-breakpoints", {}],
  tabs: ["tabs", []],
  xhrBreakpoints: ["xhr-breakpoints", []],
  eventListenerBreakpoints: ["event-listener-breakpoints", undefined],
  tabsBlackBoxed: ["tabsBlackBoxed", []],
});
exports.asyncStore = asyncStore;

exports.getThreadOptions = async function() {
  return {
    pauseOnExceptions: Services.prefs.getBoolPref(
      "devtools.debugger.pause-on-exceptions"
    ),
    ignoreCaughtExceptions: Services.prefs.getBoolPref(
      "devtools.debugger.ignore-caught-exceptions"
    ),
    shouldShowOverlay: Services.prefs.getBoolPref(
      "devtools.debugger.features.overlay"
    ),
    shouldIncludeSavedFrames: Services.prefs.getBoolPref(
      "devtools.debugger.features.async-captured-stacks"
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
    // This option is always true. See Bug 1654590 for removal.
    observeAsmJS: true,
    breakpoints: await asyncStore.pendingBreakpoints,
    // XXX: `event-listener-breakpoints` is a copy of the event-listeners state
    // of the debugger panel. The `active` property is therefore linked to
    // the `active` property of the state.
    // See devtools/client/debugger/src/reducers/event-listeners.js
    eventBreakpoints:
      ((await asyncStore.eventListenerBreakpoints) || {}).active || [],
  };
};
