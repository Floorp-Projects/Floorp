"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.recordCoverage = recordCoverage;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function recordCoverage() {
  return async function ({
    dispatch,
    getState,
    client
  }) {
    const {
      coverage
    } = await client.recordCoverage();
    return dispatch({
      type: "RECORD_COVERAGE",
      value: {
        coverage
      }
    });
  };
}