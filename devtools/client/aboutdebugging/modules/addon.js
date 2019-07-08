/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyImporter(
  this,
  "AddonManagerPrivate",
  "resource://gre/modules/AddonManager.jsm"
);

const {
  debugAddon,
  getExtensionUuid,
  openTemporaryExtension,
  parseFileUri,
  uninstallAddon,
} = require("devtools/client/aboutdebugging-new/src/modules/extensions-helper");

/**
 * Most of the implementation for this module has been moved to
 * devtools/client/aboutdebugging-new/src/modules/extensions-helper.js
 * The only methods implemented here are the ones used in the old aboutdebugging only.
 */

exports.isTemporaryID = function(addonID) {
  return AddonManagerPrivate.isTemporaryInstallID(addonID);
};

/**
 * See JSDoc in devtools/client/aboutdebugging-new/src/modules/extensions-helper for all
 * the methods exposed below.
 */

exports.debugAddon = debugAddon;
exports.getExtensionUuid = getExtensionUuid;
exports.openTemporaryExtension = openTemporaryExtension;
exports.parseFileUri = parseFileUri;
exports.uninstallAddon = uninstallAddon;
