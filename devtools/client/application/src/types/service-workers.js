/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const worker = {
  active: PropTypes.bool,
  name: PropTypes.string.isRequired,
  scope: PropTypes.string.isRequired,
  lastUpdateTime: PropTypes.number.isRequired,
  url: PropTypes.string.isRequired,
  // registrationFront can be missing in e10s.
  registrationFront: PropTypes.object,
  workerTargetFront: PropTypes.object,
};

const workerArray = PropTypes.arrayOf(PropTypes.shape(worker));

module.exports = {
  worker,
  workerArray,
};
