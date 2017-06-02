/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_SHOW_GRID_AREAS,
  UPDATE_SHOW_GRID_LINE_NUMBERS,
  UPDATE_SHOW_INFINITE_LINES,
} = require("./index");

module.exports = {

  /**
   * Update the grid highlighter's show grid areas preference.
   *
   * @param  {Boolean} enabled
   *         Whether or not the grid highlighter should show the grid areas.
   */
  updateShowGridAreas(enabled) {
    return {
      type: UPDATE_SHOW_GRID_AREAS,
      enabled,
    };
  },

  /**
   * Update the grid highlighter's show grid line numbers preference.
   *
   * @param  {Boolean} enabled
   *         Whether or not the grid highlighter should show the grid line numbers.
   */
  updateShowGridLineNumbers(enabled) {
    return {
      type: UPDATE_SHOW_GRID_LINE_NUMBERS,
      enabled,
    };
  },

  /**
   * Update the grid highlighter's show infinite lines preference.
   *
   * @param  {Boolean} enabled
   *         Whether or not the grid highlighter should extend grid lines infinitely.
   */
  updateShowInfiniteLines(enabled) {
    return {
      type: UPDATE_SHOW_INFINITE_LINES,
      enabled,
    };
  },

};
