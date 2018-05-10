"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.containsPosition = containsPosition;
exports.containsLocation = containsLocation;
exports.nodeContainsPosition = nodeContainsPosition;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function startsBefore(a, b) {
  let before = a.start.line < b.line;

  if (a.start.line === b.line) {
    before = a.start.column >= 0 && b.column >= 0 ? a.start.column <= b.column : true;
  }

  return before;
}

function endsAfter(a, b) {
  let after = a.end.line > b.line;

  if (a.end.line === b.line) {
    after = a.end.column >= 0 && b.column >= 0 ? a.end.column >= b.column : true;
  }

  return after;
}

function containsPosition(a, b) {
  return startsBefore(a, b) && endsAfter(a, b);
}

function containsLocation(a, b) {
  return containsPosition(a, b.start) && containsPosition(a, b.end);
}

function nodeContainsPosition(node, position) {
  return containsPosition(node.loc, position);
}