"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.createLocation = createLocation;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function createLocation({
  sourceId,
  line,
  column,
  sourceUrl
}) {
  return {
    sourceId,
    line,
    column,
    sourceUrl: sourceUrl || null
  };
}