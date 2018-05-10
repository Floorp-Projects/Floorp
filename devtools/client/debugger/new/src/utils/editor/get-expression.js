"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.tokenAtTextPosition = tokenAtTextPosition;
exports.getExpressionFromCoords = getExpressionFromCoords;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function tokenAtTextPosition(cm, {
  line,
  column
}) {
  if (line < 0 || line >= cm.lineCount()) {
    return null;
  }

  const token = cm.getTokenAt({
    line: line - 1,
    ch: column
  });

  if (!token) {
    return null;
  }

  return {
    startColumn: token.start,
    endColumn: token.end,
    type: token.type
  };
} // The strategy of querying codeMirror tokens was borrowed
// from Chrome's inital implementation in JavaScriptSourceFrame.js#L414


function getExpressionFromCoords(cm, coord) {
  const token = tokenAtTextPosition(cm, coord);

  if (!token) {
    return null;
  }

  let startHighlight = token.startColumn;
  const endHighlight = token.endColumn;
  const lineNumber = coord.line;
  const line = cm.doc.getLine(coord.line - 1);

  while (startHighlight > 1 && line.charAt(startHighlight - 1) === ".") {
    const tokenBefore = tokenAtTextPosition(cm, {
      line: coord.line,
      column: startHighlight - 2
    });

    if (!tokenBefore || !tokenBefore.type) {
      return null;
    }

    startHighlight = tokenBefore.startColumn;
  }

  const expression = line.substring(startHighlight, endHighlight) || "";

  if (!expression) {
    return null;
  }

  const location = {
    start: {
      line: lineNumber,
      column: startHighlight
    },
    end: {
      line: lineNumber,
      column: endHighlight
    }
  };
  return {
    expression,
    location
  };
}