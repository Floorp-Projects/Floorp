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

  // The node front of the grid container
  nodeFront: PropTypes.object,

  // The grid fragment object of the grid container
  gridFragments: PropTypes.object

};
