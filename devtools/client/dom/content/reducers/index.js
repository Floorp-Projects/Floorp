/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { grips } = require("devtools/client/dom/content/reducers/grips");
const { filter } = require("devtools/client/dom/content/reducers/filter");

exports.reducers = {
  grips,
  filter,
};
