"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = getMatches;

var _buildQuery = require("./build-query");

var _buildQuery2 = _interopRequireDefault(_buildQuery);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function getMatches(query, text, modifiers) {
  if (!query || !text || !modifiers) {
    return [];
  }

  const regexQuery = (0, _buildQuery2.default)(query, modifiers, {
    isGlobal: true
  });
  const matchedLocations = [];
  const lines = text.split("\n");

  for (let i = 0; i < lines.length; i++) {
    let singleMatch;
    const line = lines[i];

    while ((singleMatch = regexQuery.exec(line)) !== null) {
      matchedLocations.push({
        line: i,
        ch: singleMatch.index
      });
    }
  }

  return matchedLocations;
}