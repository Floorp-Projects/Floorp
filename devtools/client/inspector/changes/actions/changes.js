/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  RESET_CHANGES,
  TRACK_CHANGE,
} = require("./index");

module.exports = {

  resetChanges() {
    return {
      type: RESET_CHANGES,
    };
  },

  trackChange(data) {
    return {
      type: TRACK_CHANGE,
      data,
    };
  },

};
