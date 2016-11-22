/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  module.exports = {
    MODE: {
      TINY: Symbol("TINY"),
      SHORT: Symbol("SHORT"),
      LONG: Symbol("LONG"),
    }
  };
});
