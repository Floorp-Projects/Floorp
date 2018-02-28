/* globals buildSettings */
/* eslint-disable no-console */

"use strict";

this.log = (function() {
  const exports = {};

  const levels = ["debug", "info", "warn", "error"];
  if (!levels.includes(buildSettings.logLevel)) {
    console.warn("Invalid buildSettings.logLevel:", buildSettings.logLevel);
  }
  const shouldLog = {};

  {
    let startLogging = false;
    for (const level of levels) {
      if (buildSettings.logLevel === level) {
        startLogging = true;
      }
      if (startLogging) {
        shouldLog[level] = true;
      }
    }
  }

  function stub() {}
  exports.debug = exports.info = exports.warn = exports.error = stub;

  if (shouldLog.debug) {
    exports.debug = console.debug.bind(console);
  }

  if (shouldLog.info) {
    exports.info = console.info.bind(console);
  }

  if (shouldLog.warn) {
    exports.warn = console.warn.bind(console);
  }

  if (shouldLog.error) {
    exports.error = console.error.bind(console);
  }

  return exports;
})();
null;
