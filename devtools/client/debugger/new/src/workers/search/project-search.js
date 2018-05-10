"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.findSourceMatches = findSourceMatches;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
// Maybe reuse file search's functions?
function findSourceMatches(source, queryText) {
  const {
    id,
    loadedState,
    text
  } = source;

  if (loadedState != "loaded" || !text || queryText == "") {
    return [];
  }

  const lines = text.split("\n");
  let result = undefined;
  const query = new RegExp(queryText, "g");
  const matches = lines.map((_text, line) => {
    const indices = [];

    while (result = query.exec(_text)) {
      indices.push({
        sourceId: id,
        line: line + 1,
        column: result.index,
        match: result[0],
        value: _text,
        text: result.input
      });
    }

    return indices;
  }).filter(_matches => _matches.length > 0);
  return [].concat(...matches);
}