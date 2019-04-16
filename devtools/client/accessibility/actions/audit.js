/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AUDIT, FILTER_TOGGLE } = require("../constants");

exports.filterToggle = filter =>
  dispatch => dispatch({ filter, type: FILTER_TOGGLE });

exports.audit = walker =>
  dispatch => walker.audit()
    .then(response => dispatch({ type: AUDIT, response }))
    .catch(error => dispatch({ type: AUDIT, error }));
