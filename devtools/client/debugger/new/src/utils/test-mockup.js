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
  Frame,
  FrameId,
  Scope,
  Source,
  SourceId,
  Why
} from "../types";

function makeMockSource(id: SourceId = "source", url: string): Source {
  return {
    id,
    url,
    thread: "FakeThread",
    isBlackBoxed: false,
    isPrettyPrinted: false,
    loadedState: "unloaded",
    relativeUrl: url,
    introductionUrl: null,
    isWasm: false
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

function makeMockFrame(
  id: FrameId = "frame",
  source: Source = makeMockSource(undefined, "url"),
  scope: Scope = makeMockScope()
): Frame {
  const location = { sourceId: source.id, line: 4 };
  return {
    id,
    thread: "FakeThread",
    displayName: `display-${id}`,
    location,
    generatedLocation: location,
    source,
    scope,
    this: {}
  };
}

function makeMockFrameWithURL(url: string): Frame {
  return makeMockFrame(undefined, makeMockSource(undefined, url));
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

export {
  makeMockSource,
  makeMockScope,
  mockScopeAddVariable,
  makeMockFrame,
  makeMockFrameWithURL,
  makeWhyNormal,
  makeWhyThrow
};
