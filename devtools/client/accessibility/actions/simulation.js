/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { SIMULATE } = require("devtools/client/accessibility/constants");

exports.simulate = (simulator, simTypes = []) => dispatch =>
  simulator
    .simulate({ types: simTypes })
    .then(success => dispatch({ error: !success, simTypes, type: SIMULATE }))
    .catch(error => dispatch({ error, type: SIMULATE }));
