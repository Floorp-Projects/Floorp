/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { isFulfilled } from "./async-value";
import { findClosestFunction } from "./ast";
import { correctIndentation } from "./indentation";

export function findFunctionText(line, source, sourceTextContent, symbols) {
  const func = findClosestFunction(symbols, {
    sourceId: source.id,
    line,
    column: Infinity,
  });

  if (
    source.isWasm ||
    !func ||
    !sourceTextContent ||
    !isFulfilled(sourceTextContent) ||
    sourceTextContent.value.type !== "text"
  ) {
    return null;
  }

  const {
    location: { start, end },
  } = func;
  const lines = sourceTextContent.value.value.split("\n");
  const firstLine = lines[start.line - 1].slice(start.column);
  const lastLine = lines[end.line - 1].slice(0, end.column);
  const middle = lines.slice(start.line, end.line - 1);
  const functionText = [firstLine, ...middle, lastLine].join("\n");
  const indentedFunctionText = correctIndentation(functionText);

  return indentedFunctionText;
}
