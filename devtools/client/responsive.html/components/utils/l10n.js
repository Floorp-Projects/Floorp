/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const STRINGS_URI = "chrome://devtools/locale/responsive.properties";

const {
  ViewHelpers
} = require("resource://devtools/client/shared/widgets/ViewHelpers.jsm");

const L10N = new ViewHelpers.L10N(STRINGS_URI);

module.exports = {
  getStr: (...args) => L10N.getStr(...args),
  getFormatStr: (...args) => L10N.getFormatStr(...args),
  getFormatStrWithNumbers: (...args) => L10N.getFormatStrWithNumbers(...args),
  numberWithDecimals: (...args) => L10N.numberWithDecimals(...args),
};
