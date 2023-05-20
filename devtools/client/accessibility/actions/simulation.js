/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  SIMULATE,
} = require("resource://devtools/client/accessibility/constants.js");

exports.simulate =
  (simulateFunc, simTypes = []) =>
  ({ dispatch }) =>
    simulateFunc(simTypes)
      .then(success => dispatch({ error: !success, simTypes, type: SIMULATE }))
      .catch(error => dispatch({ error, type: SIMULATE }));
