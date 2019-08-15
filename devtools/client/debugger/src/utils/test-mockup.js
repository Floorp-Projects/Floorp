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
  Source,
  SourceId,
  SourceWithContentAndType,
  SourceWithContent,
  TextSourceContent,
  WasmSourceContent,
  Why,
} from "../types";
import * as asyncValue from "./async-value";
import type { SourceBase } from "../reducers/sources";

function makeMockSource(
  url: string = "url",
  id: SourceId = "source"
): SourceBase {
  return {
    id,
    url,
    isBlackBoxed: false,
    isPrettyPrinted: false,
    relativeUrl: url,
    introductionUrl: null,
    introductionType: undefined,
    isWasm: false,
    extensionName: null,
    isExtension: false,
  };
}

function makeMockSourceWithContent(
  url?: string,
  id?: SourceId,
  contentType?: string = "text/javascript",
  text?: string = ""
): SourceWithContent {
  const source = makeMockSource(url, id);

  return {
    source,
    content: text
      ? asyncValue.fulfilled({
          type: "text",
          value: text,
          contentType,
        })
      : null,
  };
}

function makeMockSourceAndContent(
  url?: string,
  id?: SourceId,
  contentType?: string = "text/javascript",
  text: string = ""
): { source: Source, content: TextSourceContent } {
  const source = makeMockSource(url, id);

  return {
    source,
    content: {
      type: "text",
      value: text,
      contentType,
    },
  };
}

function makeMockWasmSource(): Source {
  return {
    id: "wasm-source-id",
    url: "url",
    isBlackBoxed: false,
    isPrettyPrinted: false,
    relativeUrl: "url",
    introductionUrl: null,
    introductionType: undefined,
    isWasm: true,
    extensionName: null,
    isExtension: false,
  };
}

function makeMockWasmSourceWithContent(text: {|
  binary: Object,
|}): SourceWithContentAndType<WasmSourceContent> {
  const source = makeMockWasmSource();

  return {
    source,
    content: asyncValue.fulfilled({
      type: "wasm",
      value: text,
    }),
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
      variables: {},
    },
    object: null,
    function: null,
    type,
    scopeKind: "",
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
    options: {},
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
    this: {},
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
    updating: false,
  };
}

// Mock contexts for use in tests that do not create a redux store.
const mockcx = { navigateCounter: 0 };
const mockthreadcx = {
  navigateCounter: 0,
  thread: "FakeThread",
  pauseCounter: 0,
  isPaused: false,
};

export {
  makeMockSource,
  makeMockSourceWithContent,
  makeMockSourceAndContent,
  makeMockWasmSource,
  makeMockWasmSourceWithContent,
  makeMockScope,
  mockScopeAddVariable,
  makeMockBreakpoint,
  makeMockFrame,
  makeMockFrameWithURL,
  makeWhyNormal,
  makeWhyThrow,
  makeMockExpression,
  mockcx,
  mockthreadcx,
};
