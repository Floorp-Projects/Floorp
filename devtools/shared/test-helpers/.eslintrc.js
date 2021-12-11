/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// General rule from /.eslintrc.js only accept folders matching **/test*/browser*/
// where is this folder doesn't match, so manually apply browser test config
module.exports = {
  extends: ["plugin:mozilla/browser-test"],
};
