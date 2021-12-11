/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const manifestTypes = require("devtools/client/application/src/types/manifest");
const routingTypes = require("devtools/client/application/src/types/routing");
const workersTypes = require("devtools/client/application/src/types/service-workers");

module.exports = Object.assign(
  {},
  {
    ...manifestTypes,
    ...routingTypes,
    ...workersTypes,
  }
);
