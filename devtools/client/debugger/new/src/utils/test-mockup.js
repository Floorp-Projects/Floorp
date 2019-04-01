/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * This file is for use by unit tests for isolated debugger components that do
 * not need to interact with the redux store. When these tests need to construct
 * debugger objects, these interfaces should be used instead of plain object
 * literals.
 */

import type {
  ActorId,
  Breakpoint,
  Expression,
  Frame,
  FrameId,
  Scope,
  JsSource,
  WasmSource,
  Source,
  SourceId,
  Why
} from "../types";

function makeMockSource(
  url: string = "url",
  id: SourceId = "source",
  contentType: string = "text/javascript",
  text: string = ""
): JsSource {
  return {
    id,
    url,
    isBlackBoxed: false,
    isPrettyPrinted: false,
    loadedState: text ? "loaded" : "unloaded",
    relativeUrl: url,
    introductionUrl: null,
    introductionType: undefined,
    actors: [],
    isWasm: false,
    contentType,
    isExtension: false,
    text
  };
}

function makeMockWasmSource(text: {| binary: Object |}): WasmSource {
  return {
    id: "wasm-source-id",
    url: "url",
    isBlackBoxed: false,
    isPrettyPrinted: false,
    loadedState: "unloaded",
    relativeUrl: "url",
    introductionUrl: null,
    introductionType: undefined,
    actors: [],
    isWasm: true,
    isExtension: false,
    text
  };
}

function makeMockScope(
  actor: ActorId = "scope-actor",
  type: string = "block",
  parent: ?Scope = null
): Scope {
  return {
    actor,
    parent,
    bindings: {
      arguments: [],
      variables: {}
    },
    object: null,
    function: null,
    type
  };
}

function mockScopeAddVariable(scope: Scope, name: string) {
  if (!scope.bindings) {
    throw new Error("no scope bindings");
  }
  scope.bindings.variables[name] = { value: null };
}

function makeMockBreakpoint(
  source: Source = makeMockSource(),
  line: number = 1,
  column: ?number
): Breakpoint {
  const location = column
    ? { sourceId: source.id, line, column }
    : { sourceId: source.id, line };
  return {
    id: "breakpoint",
    location,
    astLocation: null,
    generatedLocation: location,
    disabled: false,
    text: "text",
    originalText: "text",
    options: {}
  };
}

function makeMockFrame(
  id: FrameId = "frame",
  source: Source = makeMockSource("url"),
  scope: Scope = makeMockScope(),
  line: number = 4,
  displayName: string = `display-${id}`
): Frame {
  const location = { sourceId: source.id, line };
  return {
    id,
    thread: "FakeThread",
    displayName,
    location,
    generatedLocation: location,
    source,
    scope,
    this: {}
  };
}

function makeMockFrameWithURL(url: string): Frame {
  return makeMockFrame(undefined, makeMockSource(url));
}

function makeWhyNormal(frameReturnValue: any = undefined): Why {
  if (frameReturnValue) {
    return { type: "why-normal", frameFinished: { return: frameReturnValue } };
  }
  return { type: "why-normal" };
}

function makeWhyThrow(frameThrowValue: any): Why {
  return { type: "why-throw", frameFinished: { throw: frameThrowValue } };
}

function makeMockExpression(value: Object): Expression {
  return {
    input: "input",
    value,
    from: "from",
    updating: false
  };
}

export {
  makeMockSource,
  makeMockWasmSource,
  makeMockScope,
  mockScopeAddVariable,
  makeMockBreakpoint,
  makeMockFrame,
  makeMockFrameWithURL,
  makeWhyNormal,
  makeWhyThrow,
  makeMockExpression
};
