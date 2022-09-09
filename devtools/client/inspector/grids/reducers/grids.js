/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_GRID_COLOR,
  UPDATE_GRID_HIGHLIGHTED,
  UPDATE_GRIDS,
} = require("devtools/client/inspector/grids/actions/index");

const INITIAL_GRIDS = [];

const reducers = {
  [UPDATE_GRID_COLOR](grids, { nodeFront, color }) {
    const newGrids = grids.map(g => {
      if (g.nodeFront === nodeFront) {
        g = Object.assign({}, g, { color });
      }

      return g;
    });

    return newGrids;
  },

  [UPDATE_GRID_HIGHLIGHTED](grids, { nodeFront, highlighted }) {
    const maxHighlighters = Services.prefs.getIntPref(
      "devtools.gridinspector.maxHighlighters"
    );
    const highlightedNodeFronts = grids
      .filter(g => g.highlighted)
      .map(g => g.nodeFront);
    let numHighlighted = highlightedNodeFronts.length;

    // Get the total number of highlighted grids including the one that will be
    // highlighted/unhighlighted.
    if (!highlightedNodeFronts.includes(nodeFront) && highlighted) {
      numHighlighted += 1;
    } else if (highlightedNodeFronts.includes(nodeFront) && !highlighted) {
      numHighlighted -= 1;
    }

    return grids.map(g => {
      if (maxHighlighters === 1) {
        // When there is only one grid highlighter available, only the given grid
        // container nodeFront can be highlighted, and all the other grid containers
        // are unhighlighted.
        return Object.assign({}, g, {
          highlighted: g.nodeFront === nodeFront && highlighted,
        });
      } else if (
        numHighlighted === maxHighlighters &&
        g.nodeFront !== nodeFront
      ) {
        // The maximum number of highlighted grids have been reached. Disable all the
        // other non-highlighted grids.
        return Object.assign({}, g, {
          disabled: !g.highlighted,
        });
      } else if (g.nodeFront === nodeFront) {
        // This is the provided grid nodeFront to highlight/unhighlight.
        return Object.assign({}, g, {
          disabled: false,
          highlighted,
        });
      }

      return Object.assign({}, g, {
        disabled: false,
      });
    });
  },

  [UPDATE_GRIDS](_, { grids }) {
    return grids;
  },
};

module.exports = function(grids = INITIAL_GRIDS, action) {
  const reducer = reducers[action.type];
  if (!reducer) {
    return grids;
  }
  return reducer(grids, action);
};
