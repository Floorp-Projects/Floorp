"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.locColumn = locColumn;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function locColumn(loc) {
  if (typeof loc.column !== "number") {
    // This shouldn't really happen with locations from the AST, but
    // the datatype we are using allows null/undefined column.
    return 0;
  }

  return loc.column;
}