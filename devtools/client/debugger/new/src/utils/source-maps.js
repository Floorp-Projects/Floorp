"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getGeneratedLocation = getGeneratedLocation;

var _selectors = require("../selectors/index");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
async function getGeneratedLocation(state, source, location, sourceMaps) {
  if (!sourceMaps.isOriginalId(location.sourceId)) {
    return location;
  }

  const {
    line,
    sourceId,
    column
  } = await sourceMaps.getGeneratedLocation(location, source.toJS());
  const generatedSource = (0, _selectors.getSource)(state, sourceId);

  if (!generatedSource) {
    return location;
  }

  return {
    line,
    sourceId,
    column: column === 0 ? undefined : column,
    sourceUrl: generatedSource.url
  };
}