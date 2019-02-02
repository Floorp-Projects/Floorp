/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type {
  ActorId,
  BreakpointOptions,
  BreakpointResult,
  EventListenerBreakpoints,
  Frame,
  FrameId,
  Script,
  SourceId,
  SourceActor,
  SourceActorLocation,
  Worker
} from "../../types";

import type {
  TabTarget,
  DebuggerClient,
  Grip,
  ThreadClient,
  ObjectClient,
  BPClients,
  SourcesPacket
} from "./types";

import type { PausePointsMap } from "../../workers/parser";

import { makeBreakpointActorId } from "../../utils/breakpoint";

import { createSource, createBreakpointLocation, createWorker } from "./create";
import { supportsWorkers, updateWorkerClients } from "./workers";

import { features } from "../../utils/prefs";

let bpClients: BPClients;
let workerClients: Object;
let threadClient: ThreadClient;
let tabTarget: TabTarget;
let debuggerClient: DebuggerClient;
let sourceActors: { [ActorId]: SourceId };
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
  sourceActors = {};

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

async function sourceContents({
  actor,
  thread
}: SourceActor): Promise<{| source: any, contentType: ?string |}> {
  const sourceThreadClient = lookupThreadClient(thread);
  const sourceClient = sourceThreadClient.source({ actor });
  const { source, contentType } = await sourceClient.source();
  return { source, contentType };
}

function getBreakpointByLocation(location: SourceActorLocation) {
  const id = makeBreakpointActorId(location);
  const bpClient = bpClients[id];

  if (bpClient) {
    const { actor, url, line, column } = bpClient.location;
    return {
      id: bpClient.actor,
      options: bpClient.options,
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
  location: SourceActorLocation,
  options: BreakpointOptions,
  noSliding: boolean
): Promise<BreakpointResult> {
  const sourceThreadClient = lookupThreadClient(location.sourceActor.thread);
  const sourceClient = sourceThreadClient.source({
    actor: location.sourceActor.actor
  });

  return sourceClient
    .setBreakpoint({
      line: location.line,
      column: location.column,
      options,
      noSliding
    })
    .then(([{ actualLocation }, bpClient]) => {
      actualLocation = createBreakpointLocation(location, actualLocation);

      const id = makeBreakpointActorId(actualLocation);
      bpClients[id] = bpClient;
      bpClient.location.line = actualLocation.line;
      bpClient.location.column = actualLocation.column;

      return { id, actualLocation };
    });
}

function removeBreakpoint(
  location: SourceActorLocation
): Promise<void> | ?BreakpointResult {
  try {
    const id = makeBreakpointActorId(location);
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

function setBreakpointOptions(
  location: SourceActorLocation,
  options: BreakpointOptions
) {
  const id = makeBreakpointActorId(location);
  const bpClient = bpClients[id];

  if (debuggerClient.mainRoot.traits.nativeLogpoints) {
    bpClient.setOptions(options);
  } else {
    // Older server breakpoints destroy themselves when changing options.
    delete bpClients[id];
    bpClient
      .setOptions(options)
      .then(_bpClient => {
        bpClients[id] = _bpClient;
      });
  }
}

async function evaluateInFrame(script: Script, options: EvaluateParam) {
  return evaluate(script, options);
}

async function evaluateExpressions(scripts: Script[], options: EvaluateParam) {
  return Promise.all(scripts.map(script => evaluate(script, options)));
}

type EvaluateParam = { thread: string, frameId: ?FrameId };

function evaluate(
  script: ?Script,
  { thread, frameId }: EvaluateParam = {}
): Promise<{ result: ?Object }> {
  const params = { thread, frameActor: frameId };
  if (!tabTarget || !script) {
    return Promise.resolve({ result: null });
  }

  const console = thread
    ? lookupConsoleClient(thread)
    : tabTarget.activeConsole;
  if (!console) {
    return Promise.resolve({ result: null });
  }

  return console.evaluateJSAsync(script, params);
}

function autocomplete(
  input: string,
  cursor: number,
  frameId: ?string
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
  return tabTarget.navigateTo({ url });
}

function reload(): Promise<*> {
  return tabTarget.reload();
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

  const sourceThreadClient = lookupThreadClient(frame.thread);
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

async function blackBox(
  sourceActor: SourceActor,
  isBlackBoxed: boolean,
  range?: Range
): Promise<*> {
  const sourceClient = threadClient.source({ actor: sourceActor.actor });
  if (isBlackBoxed) {
    await sourceClient.unblackBox(range);
  } else {
    await sourceClient.blackBox(range);
  }
}

async function setPausePoints(
  sourceActor: SourceActor,
  pausePoints: PausePointsMap
) {
  return sendPacket({
    to: sourceActor.actor,
    type: "setPausePoints",
    pausePoints
  });
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

function registerSourceActor(sourceActor: SourceActor) {
  sourceActors[sourceActor.actor] = sourceActor.source;
}

async function createSources(client: ThreadClient) {
  const { sources }: SourcesPacket = await client.getSources();
  if (!sources) {
    return null;
  }
  return sources.map(packet =>
    createSource(client.actor, packet, { supportsWasm })
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

function getSourceForActor(actor: ActorId) {
  if (!sourceActors[actor]) {
    throw new Error(`Unknown source actor: ${actor}`);
  }
  return sourceActors[actor];
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

  const { workers } = await tabTarget.listWorkers();
  return workers;
}

function getMainThread() {
  return threadClient.actor;
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
  getSourceForActor,
  getBreakpointByLocation,
  setBreakpoint,
  setXHRBreakpoint,
  removeXHRBreakpoint,
  removeBreakpoint,
  setBreakpointOptions,
  evaluate,
  evaluateInFrame,
  evaluateExpressions,
  navigate,
  reload,
  getProperties,
  getFrameScopes,
  pauseOnExceptions,
  fetchSources,
  registerSourceActor,
  fetchWorkers,
  getMainThread,
  sendPacket,
  setPausePoints,
  setSkipPausing,
  setEventListenerBreakpoints
};

export { setupCommands, clientCommands };
