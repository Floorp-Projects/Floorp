/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  AUDIT,
  AUDIT_PROGRESS,
  AUDITING,
  FILTER_TOGGLE,
  FILTERS,
} = require("resource://devtools/client/accessibility/constants.js");

exports.filterToggle =
  filter =>
  ({ dispatch }) =>
    dispatch({ filter, type: FILTER_TOGGLE });

exports.auditing =
  filter =>
  ({ dispatch }) => {
    const auditing = filter === FILTERS.ALL ? Object.values(FILTERS) : [filter];
    return dispatch({ auditing, type: AUDITING });
  };

exports.audit =
  (auditFunc, filter) =>
  ({ dispatch }) =>
    auditFunc(filter, progress =>
      dispatch({ type: AUDIT_PROGRESS, progress })
    ).then(({ error, ancestries }) => {
      return error
        ? dispatch({ type: AUDIT, error: true })
        : dispatch({ type: AUDIT, response: ancestries });
    });
