/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const Services = require("Services");
const { Preferences } = require("resource://gre/modules/Preferences.jsm");

// Prefs to revert to default once tests finish. Keep these in sync with
// all the preferences defined in devtools/client/preferences/devtools-client.js.
exports.MEMORY_SAMPLE_PROB_PREF = "devtools.performance.memory.sample-probability";
exports.MEMORY_MAX_LOG_LEN_PREF = "devtools.performance.memory.max-log-length";
exports.PROFILER_BUFFER_SIZE_PREF = "devtools.performance.profiler.buffer-size";
exports.PROFILER_SAMPLE_RATE_PREF = "devtools.performance.profiler.sample-frequency-khz";

exports.UI_EXPERIMENTAL_PREF = "devtools.performance.ui.experimental";
exports.UI_INVERT_CALL_TREE_PREF = "devtools.performance.ui.invert-call-tree";
exports.UI_INVERT_FLAME_PREF = "devtools.performance.ui.invert-flame-graph";
exports.UI_FLATTEN_RECURSION_PREF = "devtools.performance.ui.flatten-tree-recursion";
exports.UI_SHOW_PLATFORM_DATA_PREF = "devtools.performance.ui.show-platform-data";
exports.UI_SHOW_IDLE_BLOCKS_PREF = "devtools.performance.ui.show-idle-blocks";
exports.UI_ENABLE_FRAMERATE_PREF = "devtools.performance.ui.enable-framerate";
exports.UI_ENABLE_MEMORY_PREF = "devtools.performance.ui.enable-memory";
exports.UI_ENABLE_ALLOCATIONS_PREF = "devtools.performance.ui.enable-allocations";
exports.UI_ENABLE_MEMORY_FLAME_CHART = "devtools.performance.ui.enable-memory-flame";

exports.DEFAULT_PREF_VALUES = [
  "devtools.debugger.log",
  "devtools.performance.enabled",
  "devtools.performance.timeline.hidden-markers",
  exports.MEMORY_SAMPLE_PROB_PREF,
  exports.MEMORY_MAX_LOG_LEN_PREF,
  exports.PROFILER_BUFFER_SIZE_PREF,
  exports.PROFILER_SAMPLE_RATE_PREF,
  exports.UI_EXPERIMENTAL_PREF,
  exports.UI_INVERT_CALL_TREE_PREF,
  exports.UI_INVERT_FLAME_PREF,
  exports.UI_FLATTEN_RECURSION_PREF,
  exports.UI_SHOW_PLATFORM_DATA_PREF,
  exports.UI_SHOW_IDLE_BLOCKS_PREF,
  exports.UI_ENABLE_FRAMERATE_PREF,
  exports.UI_ENABLE_MEMORY_PREF,
  exports.UI_ENABLE_ALLOCATIONS_PREF,
  exports.UI_ENABLE_MEMORY_FLAME_CHART,
  "devtools.performance.ui.show-jit-optimizations",
  "devtools.performance.ui.show-triggers-for-gc-types",
].reduce((prefValues, prefName) => {
  prefValues[prefName] = Preferences.get(prefName);
  return prefValues;
}, {});

/**
 * Invokes callback when a pref which is not in the `DEFAULT_PREF_VALUES` store
 * is changed. Returns a cleanup function.
 */
exports.whenUnknownPrefChanged = function(branch, callback) {
  function onObserve(subject, topic, data) {
    if (!(data in exports.DEFAULT_PREF_VALUES)) {
      callback(data);
    }
  }
  Services.prefs.addObserver(branch, onObserve);
  return () => Services.prefs.removeObserver(branch, onObserve);
};

/**
 * Reverts all known preferences to their default values.
 */
exports.rollbackPrefsToDefault = function() {
  for (const prefName of Object.keys(exports.DEFAULT_PREF_VALUES)) {
    Preferences.set(prefName, exports.DEFAULT_PREF_VALUES[prefName]);
  }
};
