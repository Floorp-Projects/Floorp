/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { prepareSourcePayload, createWorker } from "./create";
import { supportsWorkers, updateWorkerClients } from "./workers";
import { features } from "../../utils/prefs";

import Reps from "devtools-reps";
import type { Node } from "devtools-reps";

import type {
  ActorId,
  BreakpointLocation,
  BreakpointOptions,
  PendingLocation,
  EventListenerBreakpoints,
  Frame,
  FrameId,
  GeneratedSourceData,
  Script,
  SourceId,
  SourceActor,
  Worker,
  Range,
} from "../../types";

import type {
  TabTarget,
  DebuggerClient,
  Grip,
  ThreadClient,
  ObjectClient,
  SourcesPacket,
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
  supportsWasm: boolean,
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

function hasWasmSupport() {
  return supportsWasm;
}

function createObjectClient(grip: Grip) {
  return debuggerClient.createObjectClient(grip);
}

async function loadObjectProperties(root: Node) {
  const utils = Reps.objectInspector.utils;
  const properties = await utils.loadProperties.loadItemProperties(
    root,
    createObjectClient
  );
  return utils.node.getChildren({
    item: root,
    loadedProperties: new Map([[root.path, properties]]),
  });
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

  // Do not return promises for the caller to wait on unless a flag is set.
  // Currently, worker threads are not guaranteed to respond to all requests,
  // if we send a request while they are shutting down. See bug 1529163.
  if (shouldWaitForWorkers) {
    return Promise.all(promises);
  }
}

function resume(thread: string): Promise<*> {
  return lookupThreadClient(thread).resume();
}

function stepIn(thread: string): Promise<*> {
  return lookupThreadClient(thread).stepIn();
}

function stepOver(thread: string): Promise<*> {
  return lookupThreadClient(thread).stepOver();
}

function stepOut(thread: string): Promise<*> {
  return lookupThreadClient(thread).stepOut();
}

function rewind(thread: string): Promise<*> {
  return lookupThreadClient(thread).rewind();
}

function reverseStepOver(thread: string): Promise<*> {
  return lookupThreadClient(thread).reverseStepOver();
}

function breakOnNext(thread: string): Promise<*> {
  return lookupThreadClient(thread).breakOnNext();
}

async function sourceContents({
  actor,
  thread,
}: SourceActor): Promise<{| source: any, contentType: ?string |}> {
  const sourceThreadClient = lookupThreadClient(thread);
  const sourceFront = sourceThreadClient.source({ actor });
  const { source, contentType } = await sourceFront.source();
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
  return `${(sourceUrl: any)}:${(sourceId: any)}:${line}:${(column: any)}`;
}

function waitForWorkers(shouldWait: boolean) {
  shouldWaitForWorkers = shouldWait;
}

function detachWorkers() {
  for (const thread of listWorkerThreadClients()) {
    thread.detach();
  }
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

function hasBreakpoint(location: BreakpointLocation) {
  return !!breakpoints[locationKey(location)];
}

async function setBreakpoint(
  location: BreakpointLocation,
  options: BreakpointOptions
) {
  maybeClearLogpoint(location);
  options = maybeGenerateLogGroupId(options);
  breakpoints[locationKey(location)] = { location, options };

  // We have to be careful here to atomically initiate the setBreakpoint() call
  // on every thread, with no intervening await. Otherwise, other code could run
  // and change or remove the breakpoint before we finish calling setBreakpoint
  // on all threads. Requests on server threads will resolve in FIFO order, and
  // this could result in the breakpoint state here being out of sync with the
  // breakpoints that are installed in the server.
  const mainThreadPromise = threadClient.setBreakpoint(location, options);

  await forEachWorkerThread(thread => thread.setBreakpoint(location, options));
  await mainThreadPromise;
}

async function removeBreakpoint(location: PendingLocation) {
  maybeClearLogpoint((location: any));
  delete breakpoints[locationKey((location: any))];

  // Delay waiting on this promise, for the same reason as in setBreakpoint.
  const mainThreadPromise = threadClient.removeBreakpoint(location);

  await forEachWorkerThread(thread => thread.removeBreakpoint(location));
  await mainThreadPromise;
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
): Promise<{ result: Grip | null }> {
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
  const sourceFront = threadClient.source({ actor: sourceActor.actor });
  if (isBlackBoxed) {
    await sourceFront.unblackBox(range);
  } else {
    await sourceFront.blackBox(range);
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

function registerSourceActor(sourceActorId: string, sourceId: SourceId) {
  sourceActors[sourceActorId] = sourceId;
}

async function getSources(
  client: ThreadClient
): Promise<Array<GeneratedSourceData>> {
  const { sources }: SourcesPacket = await client.getSources();

  return sources.map(source => prepareSourcePayload(client, source));
}

async function fetchSources(): Promise<Array<GeneratedSourceData>> {
  return getSources(threadClient);
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
      observeAsmJS: true,
    };

    const newWorkerClients = await updateWorkerClients({
      tabTarget,
      debuggerClient,
      threadClient,
      workerClients,
      options,
    });

    // Fetch the sources and install breakpoints on any new workers.
    const workerNames = Object.getOwnPropertyNames(newWorkerClients);
    for (const actor of workerNames) {
      if (!workerClients[actor]) {
        const client = newWorkerClients[actor].thread;
        getSources(client);
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
  actors: Array<SourceActor>,
  range: ?Range
): Promise<{ [string]: number[] }> {
  const sourcePositions = {};

  for (const { thread, actor } of actors) {
    const sourceThreadClient = lookupThreadClient(thread);
    const sourceFront = sourceThreadClient.source({ actor });
    const positions = await sourceFront.getBreakpointPositionsCompressed(range);

    for (const line of Object.keys(positions)) {
      let columns = positions[line];
      const existing = sourcePositions[line];
      if (existing) {
        columns = [...new Set([...existing, ...columns])];
      }

      sourcePositions[line] = columns;
    }
  }
  return sourcePositions;
}

async function getBreakableLines(actors: Array<SourceActor>) {
  let lines = [];
  for (const { thread, actor } of actors) {
    const sourceThreadClient = lookupThreadClient(thread);
    const sourceFront = sourceThreadClient.source({ actor });
    let actorLines = [];
    try {
      actorLines = await sourceFront.getBreakableLines();
    } catch (e) {
      // Handle backward compatibility
      if (
        e.message &&
        e.message.match(/does not recognize the packet type getBreakableLines/)
      ) {
        const pos = await sourceFront.getBreakpointPositionsCompressed();
        actorLines = Object.keys(pos).map(line => Number(line));
      } else if (!e.message || !e.message.match(/Connection closed/)) {
        throw e;
      }
    }

    lines = [...new Set([...lines, ...actorLines])];
  }

  return lines;
}

const clientCommands = {
  autocomplete,
  blackBox,
  createObjectClient,
  loadObjectProperties,
  releaseActor,
  interrupt,
  pauseGrip,
  resume,
  stepIn,
  stepOut,
  stepOver,
  rewind,
  reverseStepOver,
  breakOnNext,
  sourceContents,
  getSourceForActor,
  getBreakpointPositions,
  getBreakableLines,
  hasBreakpoint,
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
  waitForWorkers,
  detachWorkers,
  hasWasmSupport,
  lookupConsoleClient,
};

export { setupCommands, clientCommands };
