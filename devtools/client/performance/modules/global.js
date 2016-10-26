/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { MultiLocalizationHelper } = require("devtools/shared/l10n");
const { PrefsHelper } = require("devtools/client/shared/prefs");

/**
 * Localization convenience methods.
 */
exports.L10N = new MultiLocalizationHelper(
  "devtools/client/locales/markers.properties",
  "devtools/client/locales/performance.properties"
);

/**
 * A list of preferences for this tool. The values automatically update
 * if somebody edits edits about:config or the prefs change somewhere else.
 *
 * This needs to be registered and unregistered when used for the auto-update
 * functionality to work. The PerformanceController handles this, but if you
 * just use this module in a test independently, ensure you call
 * `registerObserver()` and `unregisterUnobserver()`.
 */
exports.PREFS = new PrefsHelper("devtools.performance", {
  "show-triggers-for-gc-types": ["Char", "ui.show-triggers-for-gc-types"],
  "show-platform-data": ["Bool", "ui.show-platform-data"],
  "hidden-markers": ["Json", "timeline.hidden-markers"],
  "memory-sample-probability": ["Float", "memory.sample-probability"],
  "memory-max-log-length": ["Int", "memory.max-log-length"],
  "profiler-buffer-size": ["Int", "profiler.buffer-size"],
  "profiler-sample-frequency": ["Int", "profiler.sample-frequency-khz"],
  // TODO: re-enable once we flame charts via bug 1148663.
  "enable-memory-flame": ["Bool", "ui.enable-memory-flame"],
});
