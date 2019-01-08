/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_RULES,
} = require("./index");

module.exports = {

  /**
   * Updates the rules state with the new list of CSS rules for the selected element.
   *
   * @param  {Array} rules
   *         Array of Rule objects containing the selected element's CSS rules.
   */
  updateRules(rules) {
    return {
      type: UPDATE_RULES,
      rules,
    };
  },

};
