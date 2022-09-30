/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

/**
 * A single grid container in the document.
 */
exports.grid = {
  // The id of the grid
  id: PropTypes.number,

  // The color for the grid overlay highlighter
  color: PropTypes.string,

  // The text direction of the grid container
  direction: PropTypes.string,

  // Whether or not the grid checkbox is disabled as a result of hitting the
  // maximum number of grid highlighters shown.
  disabled: PropTypes.bool,

  // The grid fragment object of the grid container
  gridFragments: PropTypes.array,

  // Whether or not the grid highlighter is highlighting the grid
  highlighted: PropTypes.bool,

  // Whether or not the grid is a subgrid
  isSubgrid: PropTypes.bool,

  // The node front of the grid container
  nodeFront: PropTypes.object,

  // If the grid is a subgrid, this references the parent node front actor ID
  parentNodeActorID: PropTypes.string,

  // Array of ids belonging to the subgrid within the grid container
  subgrids: PropTypes.arrayOf(PropTypes.number),

  // The writing mode of the grid container
  writingMode: PropTypes.string,
};

/**
 * The grid highlighter settings on what to display in its grid overlay in the document.
 */
exports.highlighterSettings = {
  // Whether or not the grid highlighter should show the grid line numbers
  showGridLineNumbers: PropTypes.bool,

  // Whether or not the grid highlighter extends the grid lines infinitely
  showInfiniteLines: PropTypes.bool,
};
