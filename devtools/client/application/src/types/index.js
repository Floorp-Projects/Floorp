/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const manifestTypes = require("resource://devtools/client/application/src/types/manifest.js");
const routingTypes = require("resource://devtools/client/application/src/types/routing.js");
const workersTypes = require("resource://devtools/client/application/src/types/service-workers.js");

module.exports = Object.assign(
  {},
  {
    ...manifestTypes,
    ...routingTypes,
    ...workersTypes,
  }
);
