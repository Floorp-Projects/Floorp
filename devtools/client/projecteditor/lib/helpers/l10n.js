/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This file contains helper functions for internationalizing projecteditor strings
 */

const { LocalizationHelper } = require("devtools/shared/l10n");
const ITCHPAD_STRINGS_URI = "devtools/client/locales/projecteditor.properties";
const L10N = new LocalizationHelper(ITCHPAD_STRINGS_URI);

function getLocalizedString(name) {
  try {
    return L10N.getStr(name);
  } catch (ex) {
    console.log("Error reading '" + name + "'");
    throw new Error("l10n error with " + name);
  }
}

exports.getLocalizedString = getLocalizedString;
