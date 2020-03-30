/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { workerUtils } from "devtools-utils";
const { WorkerDispatcher } = workerUtils;

import type { AstSource, AstLocation, AstPosition } from "./types";
import type { SourceLocation, SourceId, SourceContent } from "../../types";
import type { SourceScope } from "./getScopes/visitor";
import type { SymbolDeclarations } from "./getSymbols";

export class ParserDispatcher extends WorkerDispatcher {
  async findOutOfScopeLocations(
    sourceId: SourceId,
    position: AstPosition
  ): Promise<AstLocation[]> {
    return this.invoke("findOutOfScopeLocations", sourceId, position);
  }

  async getNextStep(
    sourceId: SourceId,
    pausedPosition: AstPosition
  ): Promise<?SourceLocation> {
    return this.invoke("getNextStep", sourceId, pausedPosition);
  }

  async clearState(): Promise<void> {
    return this.invoke("clearState");
  }

  async getScopes(location: SourceLocation): Promise<SourceScope[]> {
    return this.invoke("getScopes", location);
  }

  async getSymbols(sourceId: SourceId): Promise<SymbolDeclarations> {
    return this.invoke("getSymbols", sourceId);
  }

  async setSource(sourceId: SourceId, content: SourceContent): Promise<void> {
    const astSource: AstSource = {
      id: sourceId,
      text: content.type === "wasm" ? "" : content.value,
      contentType: content.contentType || null,
      isWasm: content.type === "wasm",
    };

    return this.invoke("setSource", astSource);
  }

  async hasSyntaxError(input: string): Promise<string | false> {
    return this.invoke("hasSyntaxError", input);
  }

  async mapExpression(
    expression: string,
    mappings: {
      [string]: string | null,
    } | null,
    bindings: string[],
    shouldMapBindings?: boolean,
    shouldMapAwait?: boolean
  ): Promise<{ expression: string }> {
    return this.invoke(
      "mapExpression",
      expression,
      mappings,
      bindings,
      shouldMapBindings,
      shouldMapAwait
    );
  }

  async clear() {
    await this.clearState();
  }
}

export type {
  SourceScope,
  BindingData,
  BindingLocation,
  BindingLocationType,
  BindingDeclarationLocation,
  BindingMetaValue,
  BindingType,
} from "./getScopes";

export type { AstLocation, AstPosition } from "./types";

export type {
  ClassDeclaration,
  SymbolDeclaration,
  SymbolDeclarations,
  IdentifierDeclaration,
  FunctionDeclaration,
} from "./getSymbols";
