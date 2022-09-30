/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const worker = {
  id: PropTypes.string.isRequired,
  state: PropTypes.number.isRequired,
  stateText: PropTypes.string.isRequired,
  url: PropTypes.string.isRequired,
  workerDescriptorFront: PropTypes.object,
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
