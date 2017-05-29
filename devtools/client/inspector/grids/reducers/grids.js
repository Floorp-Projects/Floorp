/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_GRID_COLOR,
  UPDATE_GRID_HIGHLIGHTED,
  UPDATE_GRIDS,
} = require("../actions/index");

const INITIAL_GRIDS = [];

let reducers = {

  [UPDATE_GRID_COLOR](grids, { nodeFront, color }) {
    let newGrids = grids.map(g => {
      if (g.nodeFront == nodeFront) {
        g.color = color;
      }

      return g;
    });

    return newGrids;
  },

  [UPDATE_GRID_HIGHLIGHTED](grids, { nodeFront, highlighted }) {
    return grids.map(g => {
      let isUpdatedNode = g.nodeFront === nodeFront;

      return Object.assign({}, g, {
        highlighted: isUpdatedNode && highlighted
      });
    });
  },

  [UPDATE_GRIDS](_, { grids }) {
    return grids;
  },

};

module.exports = function (grids = INITIAL_GRIDS, action) {
  let reducer = reducers[action.type];
  if (!reducer) {
    return grids;
  }
  return reducer(grids, action);
};
