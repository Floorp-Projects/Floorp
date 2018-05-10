"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.convertToList = convertToList;
exports.formatPausePoints = formatPausePoints;

var _lodash = require("devtools/client/shared/vendor/lodash");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function insertStrtAt(string, index, newString) {
  const start = string.slice(0, index);
  const end = string.slice(index);
  return `${start}${newString}${end}`;
}

function convertToList(pausePoints) {
  const list = [];

  for (const line in pausePoints) {
    for (const column in pausePoints[line]) {
      const point = pausePoints[line][column];
      list.push({
        location: {
          line: parseInt(line, 10),
          column: parseInt(column, 10)
        },
        types: point
      });
    }
  }

  return list;
}

function formatPausePoints(text, pausePoints) {
  const nodes = (0, _lodash.reverse)(convertToList(pausePoints));
  const lines = text.split("\n");
  nodes.forEach((node, index) => {
    const {
      line,
      column
    } = node.location;
    const {
      break: breakPoint,
      step
    } = node.types;
    const num = nodes.length - index;
    const types = `${breakPoint ? "b" : ""}${step ? "s" : ""}`;
    const spacer = breakPoint || step ? " " : "";
    lines[line - 1] = insertStrtAt(lines[line - 1], column, `/*${types}${spacer}${num}*/`);
  });
  return lines.join("\n");
}