"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.mappingContains = mappingContains;

var _positionCmp = require("./positionCmp");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function mappingContains(mapped, item) {
  return (0, _positionCmp.positionCmp)(item.start, mapped.start) >= 0 && (0, _positionCmp.positionCmp)(item.end, mapped.end) <= 0;
}