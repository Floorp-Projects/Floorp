/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type {
  BreakpointId,
  BreakpointResult,
  EventListenerBreakpoints,
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

import type { PausePointsMap } from "../../workers/parser";

import { makePendingLocationId } from "../../utils/breakpoint";

import { createSource, createBreakpointLocation, createWorker } from "./create";
import { originalToGeneratedId, isOriginalId } from "devtools-source-map";
import { supportsWorkers, updateWorkerClients } from "./workers";

import { features } from "../../utils/prefs";

let bpClients: BPClients;
let workerClients: Object;
let sourceThreads: Object;
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
  workerClients = {};
  sourceThreads = {};

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

function lookupThreadClient(thread: string) {
  if (thread == threadClient.actor) {
    return threadClient;
  }
  if (!workerClients[thread]) {
    throw new Error(`Unknown thread client: ${thread}`);
  }
  return workerClients[thread].thread;
}

function lookupConsoleClient(thread: string) {
  if (thread == threadClient.actor) {
    return tabTarget.activeConsole;
  }
  return workerClients[thread].console;
}

function resume(thread: string): Promise<*> {
  return new Promise(resolve => {
    lookupThreadClient(thread).resume(resolve);
  });
}

function stepIn(thread: string): Promise<*> {
  return new Promise(resolve => {
    lookupThreadClient(thread).stepIn(resolve);
  });
}

function stepOver(thread: string): Promise<*> {
  return new Promise(resolve => {
    lookupThreadClient(thread).stepOver(resolve);
  });
}

function stepOut(thread: string): Promise<*> {
  return new Promise(resolve => {
    lookupThreadClient(thread).stepOut(resolve);
  });
}

function rewind(thread: string): Promise<*> {
  return new Promise(resolve => {
    lookupThreadClient(thread).rewind(resolve);
  });
}

function reverseStepIn(thread: string): Promise<*> {
  return new Promise(resolve => {
    lookupThreadClient(thread).reverseStepIn(resolve);
  });
}

function reverseStepOver(thread: string): Promise<*> {
  return new Promise(resolve => {
    lookupThreadClient(thread).reverseStepOver(resolve);
  });
}

function reverseStepOut(thread: string): Promise<*> {
  return new Promise(resolve => {
    lookupThreadClient(thread).reverseStepOut(resolve);
  });
}

function breakOnNext(thread: string): Promise<*> {
  return lookupThreadClient(thread).breakOnNext();
}

function sourceContents(sourceId: SourceId): Source {
  const sourceThreadClient = sourceThreads[sourceId];
  const sourceClient = sourceThreadClient.source({ actor: sourceId });
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
  const sourceThreadClient = sourceThreads[location.sourceId];
  const sourceClient = sourceThreadClient.source({ actor: location.sourceId });

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

  const sourceThreadClient = sourceThreads[bpClient.source.actor];
  return bpClient
    .setCondition(sourceThreadClient, condition, noSliding)
    .then(_bpClient => {
      bpClients[breakpointId] = _bpClient;
      return { id: breakpointId };
    });
}

async function evaluateInFrame(script: Script, options: EvaluateParam) {
  return evaluate(script, options);
}

async function evaluateExpressions(scripts: Script[], options: EvaluateParam) {
  return Promise.all(scripts.map(script => evaluate(script, options)));
}

type EvaluateParam = { thread?: string, frameId?: FrameId };

function evaluate(
  script: ?Script,
  { thread, frameId }: EvaluateParam = {}
): Promise<mixed> {
  const params = { thread, frameActor: frameId };
  if (!tabTarget || !script) {
    return Promise.resolve({});
  }

  const console = thread
    ? lookupConsoleClient(thread)
    : tabTarget.activeConsole;
  if (!console) {
    return Promise.resolve({});
  }

  return console.evaluateJSAsync(script, params);
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

function navigate(url: string): Promise<*> {
  return tabTarget.activeTab.navigateTo({ url });
}

function reload(): Promise<*> {
  return tabTarget.activeTab.reload();
}

function getProperties(thread: string, grip: Grip): Promise<*> {
  const objClient = lookupThreadClient(thread).pauseGrip(grip);

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

  let sourceId = frame.location.sourceId;
  if (isOriginalId(sourceId)) {
    sourceId = originalToGeneratedId(sourceId);
  }

  const sourceThreadClient = sourceThreads[sourceId];
  return sourceThreadClient.getEnvironment(frame.id);
}

function pauseOnExceptions(
  thread: string,
  shouldPauseOnExceptions: boolean,
  shouldPauseOnCaughtExceptions: boolean
): Promise<*> {
  return lookupThreadClient(thread).pauseOnExceptions(
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

async function blackBox(
  sourceId: SourceId,
  isBlackBoxed: boolean,
  range?: Range
): Promise<*> {
  const sourceClient = threadClient.source({ actor: sourceId });
  if (isBlackBoxed) {
    await sourceClient.unblackBox(range);
  } else {
    await sourceClient.blackBox(range);
  }

  return { isBlackBoxed: !isBlackBoxed };
}

function disablePrettyPrint(sourceId: SourceId): Promise<*> {
  const sourceClient = threadClient.source({ actor: sourceId });
  return sourceClient.disablePrettyPrint();
}

async function setPausePoints(sourceId: SourceId, pausePoints: PausePointsMap) {
  return sendPacket({ to: sourceId, type: "setPausePoints", pausePoints });
}

async function setSkipPausing(thread: string, shouldSkip: boolean) {
  const client = lookupThreadClient(thread);
  return client.request({
    skip: shouldSkip,
    to: client.actor,
    type: "skipBreakpoints"
  });
}

function interrupt(thread: string): Promise<*> {
  return lookupThreadClient(thread).interrupt();
}

function eventListeners(): Promise<*> {
  return threadClient.eventListeners();
}

function setEventListenerBreakpoints(eventTypes: EventListenerBreakpoints) {
  // TODO: Figure out what sendpoint we want to hit
}

function pauseGrip(thread: string, func: Function): ObjectClient {
  return lookupThreadClient(thread).pauseGrip(func);
}

function registerSource(source: Source) {
  if (isOriginalId(source.id)) {
    throw new Error("registerSource called with original ID");
  }
  sourceThreads[source.id] = lookupThreadClient(source.thread);
}

async function createSources(client: ThreadClient) {
  const { sources } = await client.getSources();
  return (
    sources &&
    sources.map(packet => createSource(client.actor, packet, { supportsWasm }))
  );
}

async function fetchSources(): Promise<any[]> {
  const sources = await createSources(threadClient);

  // NOTE: this happens when we fetch sources and then immediately navigate
  if (!sources) {
    return [];
  }

  return sources;
}

async function fetchWorkers(): Promise<Worker[]> {
  if (features.windowlessWorkers) {
    workerClients = await updateWorkerClients({
      tabTarget,
      debuggerClient,
      threadClient,
      workerClients
    });

    const workerNames = Object.getOwnPropertyNames(workerClients);

    workerNames.forEach(actor => {
      createSources(workerClients[actor].thread);
    });

    return workerNames.map(actor =>
      createWorker(actor, workerClients[actor].url)
    );
  }

  if (!supportsWorkers(tabTarget)) {
    return Promise.resolve([]);
  }

  const { workers } = await tabTarget.activeTab.listWorkers();
  return workers;
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
  setSkipPausing,
  setEventListenerBreakpoints,
  registerSource
};

export { setupCommands, clientCommands };
