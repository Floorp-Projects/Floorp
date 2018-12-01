/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Ast reducer
 * @module reducers/ast
 */

import * as I from "immutable";
import makeRecord from "../utils/makeRecord";
import { findEmptyLines } from "../utils/ast";

import type {
  AstLocation,
  SymbolDeclarations,
  PausePoints,
  PausePoint
} from "../workers/parser";

import type { Map } from "immutable";
import type { Location, Source } from "../types";
import type { Action, DonePromiseAction } from "../actions/types";
import type { Record } from "../utils/makeRecord";

type EmptyLinesType = number[];

export type Symbols = SymbolDeclarations | {| loading: true |};
export type SymbolsMap = Map<string, Symbols>;
export type EmptyLinesMap = Map<string, EmptyLinesType>;

export type SourceMetaDataType = {
  framework: ?string
};

export type SourceMetaDataMap = Map<string, SourceMetaDataType>;
export type PausePointsMap = Map<string, PausePoints>;

export type Preview =
  | {| updating: true |}
  | null
  | {|
      updating: false,
      expression: string,
      location: AstLocation,
      cursorPos: any,
      tokenPos: AstLocation,
      result: Object,
      extra: Object
    |};

export type ASTState = {
  symbols: SymbolsMap,
  emptyLines: EmptyLinesMap,
  outOfScopeLocations: ?Array<AstLocation>,
  inScopeLines: ?Array<Number>,
  preview: Preview,
  pausePoints: PausePointsMap,
  sourceMetaData: SourceMetaDataMap
};

export function initialASTState() {
  return makeRecord(
    ({
      symbols: I.Map(),
      emptyLines: I.Map(),
      outOfScopeLocations: null,
      inScopeLines: null,
      preview: null,
      pausePoints: I.Map(),
      sourceMetaData: I.Map()
    }: ASTState)
  )();
}

function update(
  state: Record<ASTState> = initialASTState(),
  action: Action
): Record<ASTState> {
  switch (action.type) {
    case "SET_SYMBOLS": {
      const { sourceId } = action;
      if (action.status === "start") {
        return state.setIn(["symbols", sourceId], { loading: true });
      }

      const value = ((action: any): DonePromiseAction).value;
      return state.setIn(["symbols", sourceId], value);
    }

    case "SET_PAUSE_POINTS": {
      const { sourceText, sourceId, pausePoints } = action;
      const emptyLines = findEmptyLines(sourceText, pausePoints);

      return state
        .setIn(["pausePoints", sourceId], pausePoints)
        .setIn(["emptyLines", sourceId], emptyLines);
    }

    case "OUT_OF_SCOPE_LOCATIONS": {
      return state.set("outOfScopeLocations", action.locations);
    }

    case "IN_SCOPE_LINES": {
      return state.set("inScopeLines", action.lines);
    }

    case "CLEAR_SELECTION": {
      return state.set("preview", null);
    }

    case "SET_PREVIEW": {
      if (action.status == "start") {
        return state.set("preview", { updating: true });
      }

      if (!action.value) {
        return state.set("preview", null);
      }

      // NOTE: if the preview does not exist, it has been cleared
      if (state.get("preview")) {
        return state.set("preview", {
          ...action.value,
          updating: false
        });
      }

      return state;
    }

    case "RESUME": {
      return state.set("outOfScopeLocations", null);
    }

    case "NAVIGATE": {
      return initialASTState();
    }

    case "SET_SOURCE_METADATA": {
      return state.setIn(
        ["sourceMetaData", action.sourceId],
        action.sourceMetaData
      );
    }

    default: {
      return state;
    }
  }
}

// NOTE: we'd like to have the app state fully typed
// https://github.com/devtools-html/debugger.html/blob/master/src/reducers/sources.js#L179-L185
type OuterState = { ast: Record<ASTState> };

export function getSymbols(state: OuterState, source?: Source): ?Symbols {
  if (!source) {
    return null;
  }

  return state.ast.symbols.get(source.id) || null;
}

export function hasSymbols(state: OuterState, source: Source): boolean {
  const symbols = getSymbols(state, source);

  if (!symbols) {
    return false;
  }

  return !symbols.loading;
}

export function isSymbolsLoading(state: OuterState, source: Source): boolean {
  const symbols = getSymbols(state, source);
  if (!symbols) {
    return false;
  }

  return symbols.loading;
}

export function isEmptyLineInSource(
  state: OuterState,
  line: number,
  selectedSourceId: string
) {
  const emptyLines = getEmptyLines(state, selectedSourceId);
  return emptyLines && emptyLines.includes(line);
}

export function getEmptyLines(state: OuterState, sourceId: string) {
  if (!sourceId) {
    return null;
  }

  return state.ast.emptyLines.get(sourceId);
}

export function getPausePoints(
  state: OuterState,
  sourceId: string
): ?PausePoints {
  return state.ast.pausePoints.get(sourceId);
}

export function getPausePoint(
  state: OuterState,
  location: ?Location
): ?PausePoint {
  if (!location) {
    return;
  }

  const { column, line, sourceId } = location;
  const pausePoints = getPausePoints(state, sourceId);
  if (!pausePoints) {
    return;
  }

  const linePoints = pausePoints[String(line)];
  if (linePoints && column) {
    return linePoints[String(column)];
  }
}

export function hasPausePoints(state: OuterState, sourceId: string): boolean {
  const pausePoints = getPausePoints(state, sourceId);
  return !!pausePoints;
}

export function getOutOfScopeLocations(state: OuterState) {
  return state.ast.get("outOfScopeLocations");
}

export function getPreview(state: OuterState) {
  return state.ast.get("preview");
}

const emptySourceMetaData = {};
export function getSourceMetaData(state: OuterState, sourceId: string) {
  return state.ast.sourceMetaData.get(sourceId) || emptySourceMetaData;
}

export function hasSourceMetaData(state: OuterState, sourceId: string) {
  return state.ast.hasIn(["sourceMetaData", sourceId]);
}

export function getInScopeLines(state: OuterState) {
  return state.ast.get("inScopeLines");
}

export function isLineInScope(state: OuterState, line: number) {
  const linesInScope = state.ast.get("inScopeLines");
  return linesInScope && linesInScope.includes(line);
}

export default update;
