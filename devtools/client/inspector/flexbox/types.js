/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

exports.flexbox = {

  // The actor ID of the flex container.
  actorID: PropTypes.number,

  // Whether or not the flexbox highlighter is highlighting the flex container.
  highlighted: PropTypes.bool,

  // The NodeFront of the flex container.
  nodeFront: PropTypes.object,

};
