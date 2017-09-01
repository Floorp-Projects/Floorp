/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Remove the "raw!" prefix used in some require which confuses webpack.

"use strict";

module.exports = function (content) {
  this.cacheable && this.cacheable();
  return content.replace(/raw\!/g, "");
};
