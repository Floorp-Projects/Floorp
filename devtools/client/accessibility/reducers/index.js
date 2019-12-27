/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  accessibles,
} = require("devtools/client/accessibility/reducers/accessibles");
const { audit } = require("devtools/client/accessibility/reducers/audit");
const { details } = require("devtools/client/accessibility/reducers/details");
const {
  simulation,
} = require("devtools/client/accessibility/reducers/simulation");
const { ui } = require("devtools/client/accessibility/reducers/ui");

exports.reducers = {
  accessibles,
  audit,
  details,
  simulation,
  ui,
};
