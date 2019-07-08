/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Get the source text offset equivalent to a given line/column pair.
 *
 * @param {Debugger.Source} source
 * @param {number} line The 1-based line number.
 * @param {number} column The 0-based column number.
 * @returns {number} The codepoint offset into the source's text.
 */
function findSourceOffset(source, line, column) {
  const offsets = getSourceLineOffsets(source);
  const offset = offsets[line - 1];

  if (offset) {
    // Make sure that columns that technically don't exist in the line text
    // don't cause the offset to wrap to the next line.
    return Math.min(offset.start + column, offset.textEnd);
  }

  return line < 0 ? 0 : offsets[offsets.length - 1].end;
}
exports.findSourceOffset = findSourceOffset;

const NEWLINE = /(\r?\n|\r|\u2028|\u2029)/g;
const SOURCE_OFFSETS = new WeakMap();
/**
 * Generate and cache line information for a given source to track what
 * text offsets mark the start and end of lines. Each entry in the array
 * represents a line in the source text.
 *
 * @param {Debugger.Source} source
 * @returns {Array<{ start, textEnd, end }>}
 *    - start - The codepoint offset of the start of the line.
 *    - textEnd - The codepoint offset just after the last non-newline character.
 *    - end - The codepoint offset of the end of the line. This will be
 *            be the same as the 'start' value of the next offset object,
 *            and this includes the newlines for the line itself, where
 *            'textEnd' excludes newline characters.
 */
function getSourceLineOffsets(source) {
  const cached = SOURCE_OFFSETS.get(source);
  if (cached) {
    return cached;
  }

  const { text } = source;

  const lines = text.split(NEWLINE);

  const offsets = [];
  let offset = 0;
  for (let i = 0; i < lines.length; i += 2) {
    const line = lines[i];
    const start = offset;

    // Calculate the end codepoint offset.
    let end = offset;
    // eslint-disable-next-line no-unused-vars
    for (const c of line) {
      end++;
    }
    const textEnd = end;

    if (i + 1 < lines.length) {
      end += lines[i + 1].length;
    }

    offsets.push(Object.freeze({ start, textEnd, end }));
    offset = end;
  }
  Object.freeze(offsets);

  SOURCE_OFFSETS.set(source, offsets);
  return offsets;
}
