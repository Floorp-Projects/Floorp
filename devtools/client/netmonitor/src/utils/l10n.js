/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { LocalizationHelper } = require("resource://devtools/shared/l10n.js");
loader.lazyRequireGetter(
  this,
  "BLOCKED_REASON_MESSAGES",
  "resource://devtools/client/netmonitor/src/constants.js",
  true
);

const NET_STRINGS_URI = "devtools/client/locales/netmonitor.properties";

exports.L10N = new LocalizationHelper(NET_STRINGS_URI);

function getBlockedReasonString(blockedReason, blockingExtension) {
  if (blockedReason && blockingExtension) {
    return exports.L10N.getFormatStr(
      "networkMenu.blockedby",
      blockingExtension
    );
  }

  if (blockedReason) {
    // If we receive a platform error code, print it as-is
    if (typeof blockedReason == "string" && blockedReason.startsWith("NS_")) {
      return blockedReason;
    }

    return (
      BLOCKED_REASON_MESSAGES[blockedReason] ||
      exports.L10N.getStr("networkMenu.blocked2")
    );
  }

  return null;
}

exports.getBlockedReasonString = getBlockedReasonString;
