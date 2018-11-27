/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyImporter(this, "AddonManagerPrivate", "resource://gre/modules/AddonManager.jsm");

const {
  debugLocalAddon,
  debugRemoteAddon,
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

exports.isLegacyTemporaryExtension = function(addonForm) {
  if (!addonForm.type) {
    // If about:debugging is connected to an older then 59 remote Firefox, and type is
    // not available on the addon/webextension actors, return false to avoid showing
    // irrelevant warning messages.
    return false;
  }
  return addonForm.type == "extension" &&
         addonForm.temporarilyInstalled &&
         !addonForm.isWebExtension &&
         !addonForm.isAPIExtension;
};

/**
 * See JSDoc in devtools/client/aboutdebugging-new/src/modules/extensions-helper for all
 * the methods exposed below.
 */

exports.debugLocalAddon = debugLocalAddon;
exports.debugRemoteAddon = debugRemoteAddon;
exports.getExtensionUuid = getExtensionUuid;
exports.openTemporaryExtension = openTemporaryExtension;
exports.parseFileUri = parseFileUri;
exports.uninstallAddon = uninstallAddon;
