"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.positionCmp = positionCmp;

var _locColumn = require("./locColumn");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * * === 0 - Positions are equal.
 * * < 0 - first position before second position
 * * > 0 - first position after second position
 */
function positionCmp(p1, p2) {
  if (p1.line === p2.line) {
    const l1 = (0, _locColumn.locColumn)(p1);
    const l2 = (0, _locColumn.locColumn)(p2);

    if (l1 === l2) {
      return 0;
    }

    return l1 < l2 ? -1 : 1;
  }

  return p1.line < p2.line ? -1 : 1;
}