/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const PREF_LOGLEVEL = "browser.policies.loglevel";

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let { ConsoleAPI } = ChromeUtils.import("resource://gre/modules/Console.jsm", {});
  return new ConsoleAPI({
    prefix: "macOSPoliciesParser.jsm",
    // tip: set maxLogLevel to "debug" and use log.debug() to create detailed
    // messages during development. See LOG_LEVELS in Console.jsm for details.
    maxLogLevel: "error",
    maxLogLevelPref: PREF_LOGLEVEL,
  });
});

var EXPORTED_SYMBOLS = ["macOSPoliciesParser"];

var macOSPoliciesParser = {
  readPolicies(reader) {
    let nativePolicies = reader.readPreferences();
    if (!nativePolicies) {
      return null;
    }

    // Need an extra check here so we don't
    // JSON.stringify if we aren't in debug mode
    if (log.maxLogLevel == "debug") {
      log.debug(JSON.stringify(nativePolicies, null, 2));
    }

    return nativePolicies;
  }
};
