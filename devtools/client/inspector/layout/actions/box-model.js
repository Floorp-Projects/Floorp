/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_LAYOUT,
} = require("./index");

module.exports = {

  /**
   * Update the layout state with the new layout properties.
   */
  updateLayout(layout) {
    return {
      type: UPDATE_LAYOUT,
      layout,
    };
  },

};
