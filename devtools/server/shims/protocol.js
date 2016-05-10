/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This file only exists to support add-ons which import this module at a
 * specific path.
 */

const { Cu } = require("chrome");
const Services = require("Services");

const WARNING_PREF = "devtools.migration.warnings";
if (Services.prefs.getBoolPref(WARNING_PREF)) {
  const { Deprecated } = Cu.import("resource://gre/modules/Deprecated.jsm", {});
  Deprecated.warning("This path to protocol.js is deprecated. Please use " +
                     "require(\"devtools/shared/protocol\") to load this " +
                     "module.",
                     "https://bugzil.la/1270173");
}

module.exports = require("devtools/shared/protocol");
