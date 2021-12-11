/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createEnum } = require("devtools/client/shared/enum");

createEnum(
  [
    // Updates the color used for the overlay of a grid.
    "UPDATE_GRID_COLOR",

    // Updates the grid highlighted state.
    "UPDATE_GRID_HIGHLIGHTED",

    // Updates the entire grids state with the new list of grids.
    "UPDATE_GRIDS",

    // Updates the grid highlighter's show grid areas state.
    "UPDATE_SHOW_GRID_AREAS",

    // Updates the grid highlighter's show grid line numbers state.
    "UPDATE_SHOW_GRID_LINE_NUMBERS",

    // Updates the grid highlighter's show infinite lines state.
    "UPDATE_SHOW_INFINITE_LINES",
  ],
  module.exports
);
