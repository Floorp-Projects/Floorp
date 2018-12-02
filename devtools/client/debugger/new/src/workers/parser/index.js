/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { workerUtils } from "devtools-utils";
const { WorkerDispatcher } = workerUtils;

import type { AstLocation, AstPosition, PausePoints } from "./types";
import type { SourceLocation, Source, SourceId } from "../../types";
import type { SourceScope } from "./getScopes/visitor";
import type { SymbolDeclarations } from "./getSymbols";

const dispatcher = new WorkerDispatcher();
export const start = (url: string, win: any = window) =>
  dispatcher.start(url, win);
export const stop = () => dispatcher.stop();

export const findOutOfScopeLocations = async (
  sourceId: string,
  position: AstPosition
): Promise<AstLocation[]> =>
  dispatcher.invoke("findOutOfScopeLocations", sourceId, position);

export const getNextStep = async (
  sourceId: SourceId,
  pausedPosition: AstPosition
): Promise<?SourceLocation> =>
  dispatcher.invoke("getNextStep", sourceId, pausedPosition);

export const clearASTs = async (): Promise<void> =>
  dispatcher.invoke("clearASTs");

export const getScopes = async (
  location: SourceLocation
): Promise<SourceScope[]> => dispatcher.invoke("getScopes", location);

export const clearScopes = async (): Promise<void> =>
  dispatcher.invoke("clearScopes");

export const clearSymbols = async (): Promise<void> =>
  dispatcher.invoke("clearSymbols");

export const getSymbols = async (
  sourceId: string
): Promise<SymbolDeclarations> => dispatcher.invoke("getSymbols", sourceId);

export const hasSource = async (sourceId: SourceId): Promise<Source> =>
  dispatcher.invoke("hasSource", sourceId);

export const setSource = async (source: Source): Promise<void> =>
  dispatcher.invoke("setSource", source);

export const clearSources = async (): Promise<void> =>
  dispatcher.invoke("clearSources");

export const hasSyntaxError = async (input: string): Promise<string | false> =>
  dispatcher.invoke("hasSyntaxError", input);

export const mapExpression = async (
  expression: string,
  mappings: {
    [string]: string | null
  } | null,
  bindings: string[],
  shouldMapBindings?: boolean,
  shouldMapAwait?: boolean
): Promise<{ expression: string }> =>
  dispatcher.invoke(
    "mapExpression",
    expression,
    mappings,
    bindings,
    shouldMapBindings,
    shouldMapAwait
  );

export const getFramework = async (sourceId: string): Promise<?string> =>
  dispatcher.invoke("getFramework", sourceId);

export const getPausePoints = async (sourceId: string): Promise<PausePoints> =>
  dispatcher.invoke("getPausePoints", sourceId);

export type {
  SourceScope,
  BindingData,
  BindingLocation,
  BindingLocationType,
  BindingDeclarationLocation,
  BindingMetaValue,
  BindingType
} from "./getScopes";

export type {
  AstLocation,
  AstPosition,
  PausePoint,
  PausePoints
} from "./types";

export type {
  ClassDeclaration,
  SymbolDeclaration,
  SymbolDeclarations,
  FunctionDeclaration
} from "./getSymbols";
