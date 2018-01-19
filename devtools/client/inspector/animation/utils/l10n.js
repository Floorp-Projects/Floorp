/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N =
  new LocalizationHelper("devtools/client/locales/animationinspector.properties");

module.exports = {
  getFormatStr: (...args) => L10N.getFormatStr(...args),
  getStr: (...args) => L10N.getStr(...args),
  numberWithDecimals: (...args) => L10N.numberWithDecimals(...args),
};
