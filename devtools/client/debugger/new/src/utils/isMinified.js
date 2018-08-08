"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.isMinified = isMinified;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
// Used to detect minification for automatic pretty printing
const SAMPLE_SIZE = 50;
const INDENT_COUNT_THRESHOLD = 5;
const CHARACTER_LIMIT = 250;

const _minifiedCache = new Map();

function isMinified(source) {
  if (_minifiedCache.has(source.id)) {
    return _minifiedCache.get(source.id);
  }

  let text = source.text;

  if (!text) {
    return false;
  }

  let lineEndIndex = 0;
  let lineStartIndex = 0;
  let lines = 0;
  let indentCount = 0;
  let overCharLimit = false; // Strip comments.

  text = text.replace(/\/\*[\S\s]*?\*\/|\/\/(.+|\n)/g, "");

  while (lines++ < SAMPLE_SIZE) {
    lineEndIndex = text.indexOf("\n", lineStartIndex);

    if (lineEndIndex == -1) {
      break;
    }

    if (/^\s+/.test(text.slice(lineStartIndex, lineEndIndex))) {
      indentCount++;
    } // For files with no indents but are not minified.


    if (lineEndIndex - lineStartIndex > CHARACTER_LIMIT) {
      overCharLimit = true;
      break;
    }

    lineStartIndex = lineEndIndex + 1;
  }

  const minified = indentCount / lines * 100 < INDENT_COUNT_THRESHOLD || overCharLimit;

  _minifiedCache.set(source.id, minified);

  return minified;
}