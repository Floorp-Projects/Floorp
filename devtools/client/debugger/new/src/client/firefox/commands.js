/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { createSource, createWorker } from "./create";
import { supportsWorkers, updateWorkerClients } from "./workers";
import { features } from "../../utils/prefs";

import type {
  ActorId,
  BreakpointLocation,
  BreakpointOptions,
  PendingLocation,
  EventListenerBreakpoints,
  Frame,
  FrameId,
  Script,
  SourceId,
  SourceActor,
  Source,
  Worker,
  Range
} from "../../types";

import type {
  TabTarget,
  DebuggerClient,
  Grip,
  ThreadClient,
  ObjectClient,
  SourcesPacket
} from "./types";

let workerClients: Object;
let threadClient: ThreadClient;
let tabTarget: TabTarget;
let debuggerClient: DebuggerClient;
let sourceActors: { [ActorId]: SourceId };
let breakpoints: { [string]: Object };
let supportsWasm: boolean;

let shouldWaitForWorkers = false;

type Dependencies = {
  threadClient: ThreadClient,
  tabTarget: TabTarget,
  debuggerClient: DebuggerClient,
  supportsWasm: boolean
};

function setupCommands(dependencies: Dependencies) {
  threadClient = dependencies.threadClient;
  tabTarget = dependencies.tabTarget;
  debuggerClient = dependencies.debuggerClient;
  supportsWasm = dependencies.supportsWasm;
  workerClients = {};
  sourceActors = {};
  breakpoints = {};
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

function sendPacket(packet: Object) {
  return debuggerClient.request(packet);
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

function listWorkerThreadClients() {
  return (Object.values(workerClients): any).map(({ thread }) => thread);
}

function forEachWorkerThread(iteratee) {
  const promises = listWorkerThreadClients().map(thread => iteratee(thread));

  if (shouldWaitForWorkers) {
    return Promise.all(promises);
  }
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

function setXHRBreakpoint(path: string, method: string) {
  return threadClient.setXHRBreakpoint(path, method);
}

function removeXHRBreakpoint(path: string, method: string) {
  return threadClient.removeXHRBreakpoint(path, method);
}

// Get the string key to use for a breakpoint location.
// See also duplicate code in breakpoint-actor-map.js :(
function locationKey(location: BreakpointLocation) {
  const { sourceUrl, line, column } = location;
  const sourceId = location.sourceId || "";
  return `${(sourceUrl: any)}:${sourceId}:${line}:${(column: any)}`;
}

function waitForWorkers(shouldWait: boolean) {
  shouldWaitForWorkers = shouldWait;
}

function maybeGenerateLogGroupId(options) {
  if (options.logValue && tabTarget.traits && tabTarget.traits.canRewind) {
    return { ...options, logGroupId: `logGroup-${Math.random()}` };
  }
  return options;
}

function maybeClearLogpoint(location: BreakpointLocation) {
  const bp = breakpoints[locationKey(location)];
  if (bp && bp.options.logGroupId && tabTarget.activeConsole) {
    tabTarget.activeConsole.emit(
      "clearLogpointMessages",
      bp.options.logGroupId
    );
  }
}

async function setBreakpoint(
  location: BreakpointLocation,
  options: BreakpointOptions
) {
  maybeClearLogpoint(location);
  options = maybeGenerateLogGroupId(options);
  breakpoints[locationKey(location)] = { location, options };
  await threadClient.setBreakpoint(location, options);

  // Set breakpoints in other threads as well, but do not wait for the requests
  // to complete, so that we don't get hung up if one of the threads stops
  // responding. We don't strictly need to wait for the main thread to finish
  // setting its breakpoint, but this leads to more consistent behavior if the
  // user sets a breakpoint and immediately starts interacting with the page.
  // If the main thread stops responding then we're toast regardless.
  await forEachWorkerThread(thread => thread.setBreakpoint(location, options));
}

async function removeBreakpoint(location: PendingLocation) {
  maybeClearLogpoint((location: any));
  delete breakpoints[locationKey((location: any))];
  await threadClient.removeBreakpoint(location);

  // Remove breakpoints without waiting for the thread to respond, for the same
  // reason as in setBreakpoint.
  await forEachWorkerThread(thread => thread.removeBreakpoint(location));
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

async function pauseOnExceptions(
  shouldPauseOnExceptions: boolean,
  shouldPauseOnCaughtExceptions: boolean
): Promise<*> {
  await threadClient.pauseOnExceptions(
    shouldPauseOnExceptions,
    // Providing opposite value because server
    // uses "shouldIgnoreCaughtExceptions"
    !shouldPauseOnCaughtExceptions
  );

  await forEachWorkerThread(thread =>
    thread.pauseOnExceptions(
      shouldPauseOnExceptions,
      !shouldPauseOnCaughtExceptions
    )
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

async function setSkipPausing(shouldSkip: boolean) {
  await threadClient.skipBreakpoints(shouldSkip);
  await forEachWorkerThread(thread => thread.skipBreakpoints(shouldSkip));
}

function interrupt(thread: string): Promise<*> {
  return lookupThreadClient(thread).interrupt();
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
    const options = {
      breakpoints,
      observeAsmJS: true
    };

    const newWorkerClients = await updateWorkerClients({
      tabTarget,
      debuggerClient,
      threadClient,
      workerClients,
      options
    });

    // Fetch the sources and install breakpoints on any new workers.
    const workerNames = Object.getOwnPropertyNames(newWorkerClients);
    for (const actor of workerNames) {
      if (!workerClients[actor]) {
        const client = newWorkerClients[actor].thread;
        createSources(client);
      }
    }

    workerClients = newWorkerClients;

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

async function getBreakpointPositions(
  source: Source,
  range: ?Range
): Promise<{ [string]: number[] }> {
  const sourceActor = source.actors[0];
  const { thread, actor } = sourceActor;
  const sourceThreadClient = lookupThreadClient(thread);
  const sourceClient = sourceThreadClient.source({ actor });
  const { positions } = await sourceClient.getBreakpointPositionsCompressed(
    range
  );
  return positions;
}

const clientCommands = {
  autocomplete,
  blackBox,
  createObjectClient,
  releaseActor,
  interrupt,
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
  getBreakpointPositions,
  setBreakpoint,
  setXHRBreakpoint,
  removeXHRBreakpoint,
  removeBreakpoint,
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
  setSkipPausing,
  setEventListenerBreakpoints,
  waitForWorkers
};

export { setupCommands, clientCommands };
