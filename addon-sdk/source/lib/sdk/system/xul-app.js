/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "experimental"
};

var { Cu } = require("chrome");
var { XulApp } = Cu.import("resource://gre/modules/sdk/system/XulApp.js", {});

Object.keys(XulApp).forEach(k => exports[k] = XulApp[k]);
