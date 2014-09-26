/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "experimental"
};

const { AddonManager } = require("resource://gre/modules/AddonManager.jsm");
const { defer } = require("../core/promise");

function getAddonByID(id) {
  let { promise, resolve } = defer();
  AddonManager.getAddonByID(id, resolve);
  return promise;
}
exports.getAddonByID = getAddonByID;
