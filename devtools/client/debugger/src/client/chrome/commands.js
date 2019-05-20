/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  toServerLocation,
  fromServerLocation,
  createLoadedObject,
} from "./create";

import type { SourceLocation } from "../../types";
import type { ServerLocation, Agents } from "./types";

type setBreakpointResponseType = {
  breakpointId: string,
  serverLocation?: ServerLocation,
};

let debuggerAgent;
let runtimeAgent;
let pageAgent;

function setupCommands({ Debugger, Runtime, Page }: Agents) {
  debuggerAgent = Debugger;
  runtimeAgent = Runtime;
  pageAgent = Page;
}

function resume() {
  return debuggerAgent.resume();
}

function stepIn() {
  return debuggerAgent.stepInto();
}

function stepOver() {
  return debuggerAgent.stepOver();
}

function stepOut() {
  return debuggerAgent.stepOut();
}

function pauseOnExceptions(
  shouldPauseOnExceptions: boolean,
  shouldIgnoreCaughtExceptions: boolean
) {
  if (!shouldPauseOnExceptions) {
    return debuggerAgent.setPauseOnExceptions({ state: "none" });
  }
  const state = shouldIgnoreCaughtExceptions ? "uncaught" : "all";
  return debuggerAgent.setPauseOnExceptions({ state });
}

function breakOnNext() {
  return debuggerAgent.pause();
}

function sourceContents(sourceId: string) {
  return debuggerAgent
    .getScriptSource({ scriptId: sourceId })
    .then(({ scriptSource }) => ({
      source: scriptSource,
      contentType: null,
    }));
}

async function setBreakpoint(location: SourceLocation, condition: string) {
  const {
    breakpointId,
    serverLocation,
  }: setBreakpointResponseType = await debuggerAgent.setBreakpoint({
    location: toServerLocation(location),
    columnNumber: location.column,
  });

  const actualLocation = fromServerLocation(serverLocation) || location;

  return {
    id: breakpointId,
    actualLocation: actualLocation,
  };
}

function removeBreakpoint(breakpointId: string) {
  return debuggerAgent.removeBreakpoint({ breakpointId });
}

async function getProperties(object: any) {
  const { result } = await runtimeAgent.getProperties({
    objectId: object.objectId,
  });

  const loadedObjects = result.map(createLoadedObject);

  return { loadedObjects };
}

function evaluate(script: string) {
  return runtimeAgent.evaluate({ expression: script });
}

function debuggeeCommand(script: string): Promise<void> {
  evaluate(script);
  return Promise.resolve();
}

function navigate(url: string) {
  return pageAgent.navigate({ url });
}

function getBreakpointByLocation(location: SourceLocation) {}

function getFrameScopes() {}
function evaluateInFrame() {}
function evaluateExpressions() {}

const clientCommands = {
  resume,
  stepIn,
  stepOut,
  stepOver,
  pauseOnExceptions,
  breakOnNext,
  sourceContents,
  setBreakpoint,
  removeBreakpoint,
  evaluate,
  debuggeeCommand,
  navigate,
  getProperties,
  getBreakpointByLocation,
  getFrameScopes,
  evaluateInFrame,
  evaluateExpressions,
};

export { setupCommands, clientCommands };
