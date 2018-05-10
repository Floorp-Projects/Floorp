"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getTokenLocation = getTokenLocation;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function getTokenLocation(codeMirror, tokenEl) {
  const {
    left,
    top,
    width,
    height
  } = tokenEl.getBoundingClientRect();
  const {
    line,
    ch
  } = codeMirror.coordsChar({
    left: left + width / 2,
    top: top + height / 2
  });
  return {
    line: line + 1,
    column: ch
  };
}