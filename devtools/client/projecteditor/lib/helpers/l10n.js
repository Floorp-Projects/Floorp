/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file contains helper functions for internationalizing projecteditor strings
 */

const { Cu, Cc, Ci } = require("chrome");
const { ViewHelpers } = Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm", {});
const ITCHPAD_STRINGS_URI = "chrome://devtools/locale/projecteditor.properties";
const L10N = new ViewHelpers.L10N(ITCHPAD_STRINGS_URI).stringBundle;

function getLocalizedString (name) {
  try {
    return L10N.GetStringFromName(name);
  } catch (ex) {
    console.log("Error reading '" + name + "'");
    throw new Error("l10n error with " + name);
  }
}

exports.getLocalizedString = getLocalizedString;
