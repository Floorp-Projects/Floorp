/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Currently supported HAR version.
 */
const HAR_VERSION = "1.1";

function buildHarLog(appInfo) {
  return {
    log: {
      version: HAR_VERSION,
      creator: {
        name: appInfo.name,
        version: appInfo.version
      },
      browser: {
        name: appInfo.name,
        version: appInfo.version
      },
      pages: [],
      entries: [],
    }
  };
}

exports.buildHarLog = buildHarLog;
