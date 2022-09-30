/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  accessibles,
} = require("resource://devtools/client/accessibility/reducers/accessibles.js");
const {
  audit,
} = require("resource://devtools/client/accessibility/reducers/audit.js");
const {
  details,
} = require("resource://devtools/client/accessibility/reducers/details.js");
const {
  simulation,
} = require("resource://devtools/client/accessibility/reducers/simulation.js");
const {
  ui,
} = require("resource://devtools/client/accessibility/reducers/ui.js");

exports.reducers = {
  accessibles,
  audit,
  details,
  simulation,
  ui,
};
