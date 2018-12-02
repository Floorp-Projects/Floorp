/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type {
  BreakpointId,
  BreakpointResult,
  Frame,
  FrameId,
  SourceLocation,
  Script,
  Source,
  SourceId,
  Worker
} from "../../types";

import type {
  TabTarget,
  DebuggerClient,
  Grip,
  ThreadClient,
  ObjectClient,
  BPClients
} from "./types";

import type { PausePoints } from "../../workers/parser";

import { makePendingLocationId } from "../../utils/breakpoint";

import { createSource, createBreakpointLocation } from "./create";

import Services from "devtools-services";

let bpClients: BPClients;
let threadClient: ThreadClient;
let tabTarget: TabTarget;
let debuggerClient: DebuggerClient;
let supportsWasm: boolean;

type Dependencies = {
  threadClient: ThreadClient,
  tabTarget: TabTarget,
  debuggerClient: DebuggerClient,
  supportsWasm: boolean
};

function setupCommands(dependencies: Dependencies): { bpClients: BPClients } {
  threadClient = dependencies.threadClient;
  tabTarget = dependencies.tabTarget;
  debuggerClient = dependencies.debuggerClient;
  supportsWasm = dependencies.supportsWasm;
  bpClients = {};

  return { bpClients };
}

function createObjectClient(grip: Grip) {
  return debuggerClient.createObjectClient(grip);
}

function releaseActor(actor: String) {
  if (!actor) {
    return;
  }

  return debuggerClient.release(actor);
}

function sendPacket(packet: Object, callback?: Function = r => r) {
  return debuggerClient.request(packet).then(callback);
}

function resume(): Promise<*> {
  return new Promise(resolve => {
    threadClient.resume(resolve);
  });
}

function stepIn(): Promise<*> {
  return new Promise(resolve => {
    threadClient.stepIn(resolve);
  });
}

function stepOver(): Promise<*> {
  return new Promise(resolve => {
    threadClient.stepOver(resolve);
  });
}

function stepOut(): Promise<*> {
  return new Promise(resolve => {
    threadClient.stepOut(resolve);
  });
}

function rewind(): Promise<*> {
  return new Promise(resolve => {
    threadClient.rewind(resolve);
  });
}

function reverseStepIn(): Promise<*> {
  return new Promise(resolve => {
    threadClient.reverseStepIn(resolve);
  });
}

function reverseStepOver(): Promise<*> {
  return new Promise(resolve => {
    threadClient.reverseStepOver(resolve);
  });
}

function reverseStepOut(): Promise<*> {
  return new Promise(resolve => {
    threadClient.reverseStepOut(resolve);
  });
}

function breakOnNext(): Promise<*> {
  return threadClient.breakOnNext();
}

function sourceContents(sourceId: SourceId): Source {
  const sourceClient = threadClient.source({ actor: sourceId });
  return sourceClient.source();
}

function getBreakpointByLocation(location: SourceLocation) {
  const id = makePendingLocationId(location);
  const bpClient = bpClients[id];

  if (bpClient) {
    const { actor, url, line, column, condition } = bpClient.location;
    return {
      id: bpClient.actor,
      condition,
      actualLocation: {
        line,
        column,
        sourceId: actor,
        sourceUrl: url
      }
    };
  }
  return null;
}

function setXHRBreakpoint(path: string, method: string) {
  return threadClient.setXHRBreakpoint(path, method);
}

function removeXHRBreakpoint(path: string, method: string) {
  return threadClient.removeXHRBreakpoint(path, method);
}

function setBreakpoint(
  location: SourceLocation,
  condition: boolean,
  noSliding: boolean
): Promise<BreakpointResult> {
  const sourceClient = threadClient.source({ actor: location.sourceId });

  return sourceClient
    .setBreakpoint({
      line: location.line,
      column: location.column,
      condition,
      noSliding
    })
    .then(([{ actualLocation }, bpClient]) => {
      actualLocation = createBreakpointLocation(location, actualLocation);
      const id = makePendingLocationId(actualLocation);
      bpClients[id] = bpClient;
      bpClient.location.line = actualLocation.line;
      bpClient.location.column = actualLocation.column;
      bpClient.location.url = actualLocation.sourceUrl || "";

      return { id, actualLocation };
    });
}

function removeBreakpoint(
  generatedLocation: SourceLocation
): Promise<void> | ?BreakpointResult {
  try {
    const id = makePendingLocationId(generatedLocation);
    const bpClient = bpClients[id];
    if (!bpClient) {
      console.warn("No breakpoint to delete on server");
      return Promise.resolve();
    }
    delete bpClients[id];
    return bpClient.remove();
  } catch (_error) {
    console.warn("No breakpoint to delete on server");
  }
}

function setBreakpointCondition(
  breakpointId: BreakpointId,
  location: SourceLocation,
  condition: boolean,
  noSliding: boolean
) {
  const bpClient = bpClients[breakpointId];
  delete bpClients[breakpointId];

  return bpClient
    .setCondition(threadClient, condition, noSliding)
    .then(_bpClient => {
      bpClients[breakpointId] = _bpClient;
      return { id: breakpointId };
    });
}

async function evaluateInFrame(script: Script, frameId: string) {
  return evaluate(script, { frameId });
}

async function evaluateExpressions(scripts: Script[], frameId?: string) {
  return Promise.all(scripts.map(script => evaluate(script, { frameId })));
}

type EvaluateParam = { frameId?: FrameId };

function evaluate(
  script: ?Script,
  { frameId }: EvaluateParam = {}
): Promise<mixed> {
  const params = frameId ? { frameActor: frameId } : {};
  if (!tabTarget || !tabTarget.activeConsole || !script) {
    return Promise.resolve({});
  }

  return new Promise(resolve => {
    tabTarget.activeConsole.evaluateJSAsync(
      script,
      result => resolve(result),
      params
    );
  });
}

function autocomplete(
  input: string,
  cursor: number,
  frameId: string
): Promise<mixed> {
  if (!tabTarget || !tabTarget.activeConsole || !input) {
    return Promise.resolve({});
  }
  return new Promise(resolve => {
    tabTarget.activeConsole.autocomplete(
      input,
      cursor,
      result => resolve(result),
      frameId
    );
  });
}

function debuggeeCommand(script: Script): ?Promise<void> {
  tabTarget.activeConsole.evaluateJS(script, () => {}, {});

  if (!debuggerClient) {
    return;
  }

  const consoleActor = tabTarget.form.consoleActor;
  const request = debuggerClient._activeRequests.get(consoleActor);
  request.emit("json-reply", {});
  debuggerClient._activeRequests.delete(consoleActor);

  return Promise.resolve();
}

function navigate(url: string): Promise<*> {
  return tabTarget.activeTab.navigateTo({ url });
}

function reload(): Promise<*> {
  return tabTarget.activeTab.reload();
}

function getProperties(grip: Grip): Promise<*> {
  const objClient = threadClient.pauseGrip(grip);

  return objClient.getPrototypeAndProperties().then(resp => {
    const { ownProperties, safeGetterValues } = resp;
    for (const name in safeGetterValues) {
      const { enumerable, writable, getterValue } = safeGetterValues[name];
      ownProperties[name] = { enumerable, writable, value: getterValue };
    }
    return resp;
  });
}

async function getFrameScopes(frame: Frame): Promise<*> {
  if (frame.scope) {
    return frame.scope;
  }

  return threadClient.getEnvironment(frame.id);
}

function pauseOnExceptions(
  shouldPauseOnExceptions: boolean,
  shouldPauseOnCaughtExceptions: boolean
): Promise<*> {
  return threadClient.pauseOnExceptions(
    shouldPauseOnExceptions,
    // Providing opposite value because server
    // uses "shouldIgnoreCaughtExceptions"
    !shouldPauseOnCaughtExceptions
  );
}

function prettyPrint(sourceId: SourceId, indentSize: number): Promise<*> {
  const sourceClient = threadClient.source({ actor: sourceId });
  return sourceClient.prettyPrint(indentSize);
}

async function blackBox(sourceId: SourceId, isBlackBoxed: boolean): Promise<*> {
  const sourceClient = threadClient.source({ actor: sourceId });
  if (isBlackBoxed) {
    await sourceClient.unblackBox();
  } else {
    await sourceClient.blackBox();
  }

  return { isBlackBoxed: !isBlackBoxed };
}

function disablePrettyPrint(sourceId: SourceId): Promise<*> {
  const sourceClient = threadClient.source({ actor: sourceId });
  return sourceClient.disablePrettyPrint();
}

async function setPausePoints(sourceId: SourceId, pausePoints: PausePoints) {
  return sendPacket({ to: sourceId, type: "setPausePoints", pausePoints });
}

async function setSkipPausing(shouldSkip: boolean) {
  return threadClient.request({
    skip: shouldSkip,
    to: threadClient.actor,
    type: "skipBreakpoints"
  });
}

function interrupt(): Promise<*> {
  return threadClient.interrupt();
}

function eventListeners(): Promise<*> {
  return threadClient.eventListeners();
}

function pauseGrip(func: Function): ObjectClient {
  return threadClient.pauseGrip(func);
}

async function fetchSources() {
  const { sources } = await threadClient.getSources();

  // NOTE: this happens when we fetch sources and then immediately navigate
  if (!sources) {
    return;
  }

  return sources.map(source => createSource(source, { supportsWasm }));
}

/**
 * Temporary helper to check if the current server will support a call to
 * listWorkers. On Fennec 60 or older, the call will silently crash and prevent
 * the client from resuming.
 * XXX: Remove when FF60 for Android is no longer used or available.
 *
 * See https://bugzilla.mozilla.org/show_bug.cgi?id=1443550 for more details.
 */
async function checkServerSupportsListWorkers() {
  const root = await tabTarget.root;
  // root is not available on all debug targets.
  if (!root) {
    return false;
  }

  const deviceFront = await debuggerClient.mainRoot.getFront("device");
  const description = await deviceFront.getDescription();

  const isFennec = description.apptype === "mobile/android";
  if (!isFennec) {
    // Explicitly return true early to avoid calling Services.vs.compare.
    // This would force us to extent the Services shim provided by
    // devtools-modules, used when this code runs in a tab.
    return true;
  }

  // We are only interested in Fennec release versions here.
  // We assume that the server fix for Bug 1443550 will land in FF61.
  const version = description.platformversion;
  return Services.vc.compare(version, "61.0") >= 0;
}

async function fetchWorkers(): Promise<{ workers: Worker[] }> {
  // Temporary workaround for Bug 1443550
  // XXX: Remove when FF60 for Android is no longer used or available.
  const supportsListWorkers = await checkServerSupportsListWorkers();

  // NOTE: The Worker and Browser Content toolboxes do not have a parent
  // with a listWorkers function
  // TODO: there is a listWorkers property, but it is not a function on the
  // parent. Investigate what it is
  if (
    !threadClient._parent ||
    typeof threadClient._parent.listWorkers != "function" ||
    !supportsListWorkers
  ) {
    return Promise.resolve({ workers: [] });
  }

  return threadClient._parent.listWorkers();
}

const clientCommands = {
  autocomplete,
  blackBox,
  createObjectClient,
  releaseActor,
  interrupt,
  eventListeners,
  pauseGrip,
  resume,
  stepIn,
  stepOut,
  stepOver,
  rewind,
  reverseStepIn,
  reverseStepOut,
  reverseStepOver,
  breakOnNext,
  sourceContents,
  getBreakpointByLocation,
  setBreakpoint,
  setXHRBreakpoint,
  removeXHRBreakpoint,
  removeBreakpoint,
  setBreakpointCondition,
  evaluate,
  evaluateInFrame,
  evaluateExpressions,
  debuggeeCommand,
  navigate,
  reload,
  getProperties,
  getFrameScopes,
  pauseOnExceptions,
  prettyPrint,
  disablePrettyPrint,
  fetchSources,
  fetchWorkers,
  sendPacket,
  setPausePoints,
  setSkipPausing
};

export { setupCommands, clientCommands };
