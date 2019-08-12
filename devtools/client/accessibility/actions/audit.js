/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  accessibility: { AUDIT_TYPE },
} = require("devtools/shared/constants");
const {
  AUDIT,
  AUDIT_PROGRESS,
  AUDITING,
  FILTER_TOGGLE,
  FILTERS,
} = require("../constants");

exports.filterToggle = filter => dispatch =>
  dispatch({ filter, type: FILTER_TOGGLE });

exports.auditing = filter => dispatch => {
  const auditing = filter === FILTERS.ALL ? Object.values(FILTERS) : [filter];
  return dispatch({ auditing, type: AUDITING });
};

exports.audit = (accessibilityWalker, filter) => dispatch =>
  new Promise(resolve => {
    const types = filter === FILTERS.ALL ? Object.values(AUDIT_TYPE) : [filter];
    const auditEventHandler = ({ type, ancestries, progress }) => {
      switch (type) {
        case "error":
          accessibilityWalker.off("audit-event", auditEventHandler);
          dispatch({ type: AUDIT, error: true });
          resolve();
          break;
        case "completed":
          accessibilityWalker.off("audit-event", auditEventHandler);
          dispatch({ type: AUDIT, response: ancestries });
          resolve();
          break;
        case "progress":
          dispatch({ type: AUDIT_PROGRESS, progress });
          break;
        default:
          break;
      }
    };

    accessibilityWalker.on("audit-event", auditEventHandler);
    accessibilityWalker.startAudit({ types });
  });
