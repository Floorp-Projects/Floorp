/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals buildSettings */
/* eslint-disable no-console */

"use strict";

this.log = (function() {
  const exports = {};
  const logLevel = "warn";

  const levels = ["debug", "info", "warn", "error"];
  const shouldLog = {};

  {
    let startLogging = false;
    for (const level of levels) {
      if (logLevel === level) {
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
    exports.debug = console.debug;
  }

  if (shouldLog.info) {
    exports.info = console.info;
  }

  if (shouldLog.warn) {
    exports.warn = console.warn;
  }

  if (shouldLog.error) {
    exports.error = console.error;
  }

  return exports;
})();
null;
