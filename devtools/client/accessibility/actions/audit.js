/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AUDIT, AUDIT_PROGRESS, AUDITING, FILTER_TOGGLE } = require("../constants");

exports.filterToggle = filter =>
  dispatch => dispatch({ filter, type: FILTER_TOGGLE });

exports.auditing = filter =>
  dispatch => dispatch({ auditing: filter, type: AUDITING });

exports.audit = (walker, filter) =>
  dispatch => new Promise(resolve => {
    const auditEventHandler = ({ type, ancestries, progress }) => {
      switch (type) {
        case "error":
          walker.off("audit-event", auditEventHandler);
          dispatch({ type: AUDIT, error: true });
          resolve();
          break;
        case "completed":
          walker.off("audit-event", auditEventHandler);
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

    walker.on("audit-event", auditEventHandler);
    walker.startAudit();
  });
