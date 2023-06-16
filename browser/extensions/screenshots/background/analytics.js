/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals main, browser, catcher, log */

"use strict";

this.analytics = (function () {
  const exports = {};

  let telemetryEnabled;

  exports.incrementCount = function (scalar) {
    const allowedScalars = [
      "download",
      "upload",
      "copy",
      "visible",
      "full_page",
      "custom",
      "element",
    ];
    if (!allowedScalars.includes(scalar)) {
      const err = `incrementCount passed an unrecognized scalar ${scalar}`;
      log.warn(err);
      return Promise.resolve();
    }
    return browser.telemetry
      .scalarAdd(`screenshots.${scalar}`, 1)
      .catch(err => {
        log.warn(`incrementCount failed with error: ${err}`);
      });
  };

  exports.refreshTelemetryPref = function () {
    return browser.telemetry.canUpload().then(
      result => {
        telemetryEnabled = result;
      },
      error => {
        // If there's an error reading the pref, we should assume that we shouldn't send data
        telemetryEnabled = false;
        throw error;
      }
    );
  };

  exports.isTelemetryEnabled = function () {
    catcher.watchPromise(exports.refreshTelemetryPref());
    return telemetryEnabled;
  };

  return exports;
})();
