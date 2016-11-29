/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_GRID_HIGHLIGHTED,
  UPDATE_GRIDS,
} = require("./index");

module.exports = {

  /**
   * Update the grid highlighted state.
   *
   * @param  {NodeFront} nodeFront
   *         The NodeFront of the DOM node to toggle the grid highlighter.
   * @param  {Boolean} highlighted
   *         Whether or not the grid highlighter is highlighting the grid.
   */
  updateGridHighlighted(nodeFront, highlighted) {
    return {
      type: UPDATE_GRID_HIGHLIGHTED,
      nodeFront,
      highlighted,
    };
  },

  /**
   * Update the grid state with the new list of grids.
   */
  updateGrids(grids) {
    return {
      type: UPDATE_GRIDS,
      grids,
    };
  },

};
