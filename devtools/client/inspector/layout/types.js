/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PropTypes } = require("devtools/client/shared/vendor/react");

/**
 * A single grid container in the document.
 */
exports.grid = {

  // The id of the grid
  id: PropTypes.number,

  // The grid fragment object of the grid container
  gridFragments: PropTypes.array,

  // Whether or not the grid highlighter is highlighting the grid
  highlighted: PropTypes.bool,

  // The node front of the grid container
  nodeFront: PropTypes.object,

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
