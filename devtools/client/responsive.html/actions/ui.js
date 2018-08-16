/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TOGGLE_LEFT_ALIGNMENT,
} = require("./index");

module.exports = {

  toggleLeftAlignment(enabled) {
    return {
      type: TOGGLE_LEFT_ALIGNMENT,
      enabled,
    };
  },

};
