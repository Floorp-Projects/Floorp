/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import { isFulfilled } from "./async-value";
import { findClosestFunction } from "./ast";
import { correctIndentation } from "./indentation";
import type { SourceWithContent } from "../types";
import type { Symbols } from "../reducers/ast";

export function findFunctionText(
  line: number,
  source: SourceWithContent,
  symbols: ?Symbols
): ?string {
  const func = findClosestFunction(symbols, {
    sourceId: source.id,
    line,
    column: Infinity,
  });

  if (
    source.isWasm ||
    !func ||
    !source.content ||
    !isFulfilled(source.content) ||
    source.content.value.type !== "text"
  ) {
    return null;
  }

  const {
    location: { start, end },
  } = func;
  const lines = source.content.value.value.split("\n");
  const firstLine = lines[start.line - 1].slice(start.column);
  const lastLine = lines[end.line - 1].slice(0, end.column);
  const middle = lines.slice(start.line, end.line - 1);
  const functionText = [firstLine, ...middle, lastLine].join("\n");
  const indentedFunctionText = correctIndentation(functionText);

  return indentedFunctionText;
}
