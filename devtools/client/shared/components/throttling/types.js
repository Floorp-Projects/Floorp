/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

/**
 * Network throttling state.
 */
exports.networkThrottling = {
  // Whether or not network throttling is enabled
  enabled: PropTypes.bool,
  // Name of the selected throttling profile
  profile: PropTypes.string,
};
