/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { xor, range } from "lodash";
import type { SourceLocation, Position } from "../types";
import type { Symbols } from "../reducers/ast";

import type {
  AstPosition,
  AstLocation,
  FunctionDeclaration,
  ClassDeclaration
} from "../workers/parser";
import type { PausePoints } from "../reducers/types";

export function findBestMatchExpression(symbols: Symbols, tokenPos: Position) {
  if (symbols.loading) {
    return null;
  }

  const { line, column } = tokenPos;
  const { memberExpressions, identifiers, literals } = symbols;
  const members = memberExpressions.filter(({ computed }) => !computed);

  return []
    .concat(identifiers, members, literals)
    .reduce((found, expression) => {
      const overlaps =
        expression.location.start.line == line &&
        expression.location.start.column <= column &&
        expression.location.end.column >= column;

      if (overlaps) {
        return expression;
      }

      return found;
    }, null);
}

export function findEmptyLines(
  sourceText: string,
  pausePoints: PausePoints
): number[] {
  if (!pausePoints || !sourceText) {
    return [];
  }

  const breakpoints = pausePoints.filter(point => point.types.break);
  const breakpointLines = breakpoints.map(point => point.location.line);

  if (!sourceText || breakpointLines.length == 0) {
    return [];
  }

  const lineCount = sourceText.split("\n").length;
  const sourceLines = range(1, lineCount + 1);
  return xor(sourceLines, breakpointLines);
}

export function containsPosition(a: AstLocation, b: AstPosition) {
  const startsBefore =
    a.start.line < b.line ||
    (a.start.line === b.line && a.start.column <= b.column);
  const endsAfter =
    a.end.line > b.line || (a.end.line === b.line && a.end.column >= b.column);

  return startsBefore && endsAfter;
}

function findClosestofSymbol(declarations: any[], location: SourceLocation) {
  if (!declarations) {
    return null;
  }

  return declarations.reduce((found, currNode) => {
    if (
      currNode.name === "anonymous" ||
      !containsPosition(currNode.location, {
        line: location.line,
        column: location.column || 0
      })
    ) {
      return found;
    }

    if (!found) {
      return currNode;
    }

    if (found.location.start.line > currNode.location.start.line) {
      return found;
    }
    if (
      found.location.start.line === currNode.location.start.line &&
      found.location.start.column > currNode.location.start.column
    ) {
      return found;
    }

    return currNode;
  }, null);
}

export function findClosestFunction(
  symbols: ?Symbols,
  location: SourceLocation
): FunctionDeclaration | null {
  if (!symbols || symbols.loading) {
    return null;
  }

  return findClosestofSymbol(symbols.functions, location);
}

export function findClosestClass(
  symbols: Symbols,
  location: SourceLocation
): ClassDeclaration | null {
  if (!symbols || symbols.loading) {
    return null;
  }

  return findClosestofSymbol(symbols.classes, location);
}
