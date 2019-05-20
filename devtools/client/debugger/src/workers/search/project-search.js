/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

// Maybe reuse file search's functions?

import getMatches from "./get-matches";

import type { SourceId, TextSourceContent } from "../../types";

export function findSourceMatches(
  sourceId: SourceId,
  content: TextSourceContent,
  queryText: string
): Object[] {
  if (queryText == "") {
    return [];
  }

  const modifiers = {
    caseSensitive: false,
    regexMatch: false,
    wholeWord: false,
  };

  const text = content.value;
  const lines = text.split("\n");

  return getMatches(queryText, text, modifiers).map(({ line, ch }) => {
    const { value, matchIndex } = truncateLine(lines[line], ch);
    return {
      sourceId,
      line: line + 1,
      column: ch,
      matchIndex,
      match: queryText,
      value,
    };
  });
}

// This is used to find start of a word, so that cropped string look nice
const startRegex = /([ !@#$%^&*()_+\-=\[\]{};':"\\|,.<>\/?])/g;
// Similarly, find
const endRegex = new RegExp(
  [
    "([ !@#$%^&*()_+-=[]{};':\"\\|,.<>/?])",
    '[^ !@#$%^&*()_+-=[]{};\':"\\|,.<>/?]*$"/',
  ].join("")
);

function truncateLine(text: string, column: number) {
  if (text.length < 100) {
    return {
      matchIndex: column,
      value: text,
    };
  }

  // Initially take 40 chars left to the match
  const offset = Math.max(column - 40, 0);
  // 400 characters should be enough to figure out the context of the match
  const truncStr = text.slice(offset, column + 400);
  let start = truncStr.search(startRegex);
  let end = truncStr.search(endRegex);

  if (start > column) {
    // No word separator found before the match, so we take all characters
    // before the match
    start = -1;
  }
  if (end < column) {
    end = truncStr.length;
  }
  const value = truncStr.slice(start + 1, end);

  return {
    matchIndex: column - start - offset - 1,
    value,
  };
}
