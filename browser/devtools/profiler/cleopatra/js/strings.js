const Cu = Components.utils;
Cu.import("resource:///modules/devtools/ProfilerHelpers.jsm");

/**
 * Shortcuts for the L10N helper functions. Used in Cleopatra.
 */
var gStrings = {
  // This strings are here so that Cleopatra code could use a simple object
  // lookup. This makes it easier to merge upstream changes.
  "Complete Profile": L10N.getStr("profiler.completeProfile"),
  "Sample Range": L10N.getStr("profiler.sampleRange"),
  "Running Time": L10N.getStr("profiler.runningTime"),
  "Self": L10N.getStr("profiler.self"),
  "Symbol Name": L10N.getStr("profiler.symbolName"),

  getStr: function (name) {
    return L10N.getStr(name);
  },

  getFormatStr: function (name, params) {
    return L10N.getFormatStr(name, params);
  }
};