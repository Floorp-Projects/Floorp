/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const worker = {
  id: PropTypes.string.isRequired,
  isActive: PropTypes.bool.isRequired,
  stateText: PropTypes.string.isRequired,
  url: PropTypes.string.isRequired,
  workerTargetFront: PropTypes.object,
  registrationFront: PropTypes.object,
};

const workerArray = PropTypes.arrayOf(PropTypes.shape(worker));

const registration = {
  id: PropTypes.string.isRequired,
  lastUpdateTime: PropTypes.number,
  registrationFront: PropTypes.object.isRequired,
  scope: PropTypes.string.isRequired,
  workers: workerArray.isRequired,
};

const registrationArray = PropTypes.arrayOf(PropTypes.shape(registration));

module.exports = {
  registration,
  registrationArray,
  worker,
  workerArray,
};
