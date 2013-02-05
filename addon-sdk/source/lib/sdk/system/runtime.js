/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

const { Cc, Ci } = require("chrome");
const runtime = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime);

exports.inSafeMode = runtime.inSafeMode;
exports.OS = runtime.OS;
exports.processType = runtime.processType;
exports.widgetToolkit = runtime.widgetToolkit;
exports.XPCOMABI = runtime.XPCOMABI;
