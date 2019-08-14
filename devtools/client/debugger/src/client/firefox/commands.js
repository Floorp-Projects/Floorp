/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { prepareSourcePayload, createWorker } from "./create";
import { supportsWorkers, updateWorkerTargets } from "./workers";
import { features } from "../../utils/prefs";

import Reps from "devtools-reps";
import type { Node } from "devtools-reps";

import type {
  ActorId,
  BreakpointLocation,
  BreakpointOptions,
  PendingLocation,
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
  ThreadFront,
  ObjectClient,
  SourcesPacket,
} from "./types";

import type {
  EventListenerCategoryList,
  EventListenerActiveList,
} from "../../actions/types";

let workerTargets: Object;
let threadFront: ThreadFront;
let tabTarget: TabTarget;
let debuggerClient: DebuggerClient;
let sourceActors: { [ActorId]: SourceId };
let breakpoints: { [string]: Object };
let eventBreakpoints: ?EventListenerActiveList;
let supportsWasm: boolean;

type Dependencies = {
  threadFront: ThreadFront,
  tabTarget: TabTarget,
  debuggerClient: DebuggerClient,
  supportsWasm: boolean,
};

function setupCommands(dependencies: Dependencies) {
  threadFront = dependencies.threadFront;
  tabTarget = dependencies.tabTarget;
  debuggerClient = dependencies.debuggerClient;
  supportsWasm = dependencies.supportsWasm;
  workerTargets = {};
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

function lookupTarget(thread: string) {
  if (thread == threadFront.actor) {
    return tabTarget;
  }
  if (!workerTargets[thread]) {
    throw new Error(`Unknown thread front: ${thread}`);
  }
  return workerTargets[thread];
}

function lookupThreadFront(thread: string) {
  const target = lookupTarget(thread);
  return target.threadFront;
}

function listWorkerThreadFronts() {
  return (Object.values(workerTargets): any).map(target => target.threadFront);
}

function forEachThread(iteratee) {
  // We have to be careful here to atomically initiate the operation on every
  // thread, with no intervening await. Otherwise, other code could run and
  // trigger additional thread operations. Requests on server threads will
  // resolve in FIFO order, and this could result in client and server state
  // going out of sync.

  const promises = [threadFront, ...listWorkerThreadFronts()].map(
    // If a thread shuts down while sending the message then it will
    // throw. Ignore these exceptions.
    t => iteratee(t).catch(e => console.log(e))
  );

  return Promise.all(promises);
}

function resume(thread: string): Promise<*> {
  return lookupThreadFront(thread).resume();
}

function stepIn(thread: string): Promise<*> {
  return lookupThreadFront(thread).stepIn();
}

function stepOver(thread: string): Promise<*> {
  return lookupThreadFront(thread).stepOver();
}

function stepOut(thread: string): Promise<*> {
  return lookupThreadFront(thread).stepOut();
}

function rewind(thread: string): Promise<*> {
  return lookupThreadFront(thread).rewind();
}

function reverseStepOver(thread: string): Promise<*> {
  return lookupThreadFront(thread).reverseStepOver();
}

function breakOnNext(thread: string): Promise<*> {
  return lookupThreadFront(thread).breakOnNext();
}

async function sourceContents({
  actor,
  thread,
}: SourceActor): Promise<{| source: any, contentType: ?string |}> {
  const sourceThreadFront = lookupThreadFront(thread);
  const sourceFront = sourceThreadFront.source({ actor });
  const { source, contentType } = await sourceFront.source();
  return { source, contentType };
}

function setXHRBreakpoint(path: string, method: string) {
  return threadFront.setXHRBreakpoint(path, method);
}

function removeXHRBreakpoint(path: string, method: string) {
  return threadFront.removeXHRBreakpoint(path, method);
}

// Get the string key to use for a breakpoint location.
// See also duplicate code in breakpoint-actor-map.js :(
function locationKey(location: BreakpointLocation) {
  const { sourceUrl, line, column } = location;
  const sourceId = location.sourceId || "";
  // $FlowIgnore
  return `${sourceUrl}:${sourceId}:${line}:${column}`;
}

function detachWorkers() {
  for (const thread of listWorkerThreadFronts()) {
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

function setBreakpoint(
  location: BreakpointLocation,
  options: BreakpointOptions
) {
  maybeClearLogpoint(location);
  options = maybeGenerateLogGroupId(options);
  breakpoints[locationKey(location)] = { location, options };

  return forEachThread(thread => thread.setBreakpoint(location, options));
}

function removeBreakpoint(location: PendingLocation) {
  maybeClearLogpoint((location: any));
  delete breakpoints[locationKey((location: any))];

  return forEachThread(thread => thread.removeBreakpoint(location));
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

  const target = thread ? lookupTarget(thread) : tabTarget;
  const console = target.activeConsole;
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
  const objClient = lookupThreadFront(thread).pauseGrip(grip);

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

  const sourceThreadFront = lookupThreadFront(frame.thread);
  return sourceThreadFront.getEnvironment(frame.id);
}

function pauseOnExceptions(
  shouldPauseOnExceptions: boolean,
  shouldPauseOnCaughtExceptions: boolean
): Promise<*> {
  return forEachThread(thread =>
    thread.pauseOnExceptions(
      shouldPauseOnExceptions,
      // Providing opposite value because server
      // uses "shouldIgnoreCaughtExceptions"
      !shouldPauseOnCaughtExceptions
    )
  );
}

async function blackBox(
  sourceActor: SourceActor,
  isBlackBoxed: boolean,
  range?: Range
): Promise<*> {
  const sourceFront = threadFront.source({ actor: sourceActor.actor });
  if (isBlackBoxed) {
    await sourceFront.unblackBox(range);
  } else {
    await sourceFront.blackBox(range);
  }
}

function setSkipPausing(shouldSkip: boolean) {
  return forEachThread(thread => thread.skipBreakpoints(shouldSkip));
}

function interrupt(thread: string): Promise<*> {
  return lookupThreadFront(thread).interrupt();
}

function setEventListenerBreakpoints(ids: string[]) {
  eventBreakpoints = ids;

  return forEachThread(thread => thread.setActiveEventBreakpoints(ids));
}

// eslint-disable-next-line
async function getEventListenerBreakpointTypes(): Promise<
  EventListenerCategoryList
> {
  let categories;
  try {
    categories = await threadFront.getAvailableEventBreakpoints();

    if (!Array.isArray(categories)) {
      // When connecting to older browser that had our placeholder
      // implementation of the 'getAvailableEventBreakpoints' endpoint, we
      // actually get back an object with a 'value' property containing
      // the categories. Since that endpoint wasn't actually backed with a
      // functional implementation, we just bail here instead of storing the
      // 'value' property into the categories.
      categories = null;
    }
  } catch (err) {
    // Event bps aren't supported on this firefox version.
  }
  return categories || [];
}

function pauseGrip(thread: string, func: Function): ObjectClient {
  return lookupThreadFront(thread).pauseGrip(func);
}

function registerSourceActor(sourceActorId: string, sourceId: SourceId) {
  sourceActors[sourceActorId] = sourceId;
}

async function getSources(
  client: ThreadFront
): Promise<Array<GeneratedSourceData>> {
  const { sources }: SourcesPacket = await client.getSources();

  return sources.map(source => prepareSourcePayload(client, source));
}

async function fetchSources(): Promise<Array<GeneratedSourceData>> {
  return getSources(threadFront);
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
      eventBreakpoints,
      observeAsmJS: true,
    };

    const newWorkerTargets = await updateWorkerTargets({
      tabTarget,
      debuggerClient,
      threadFront,
      workerTargets,
      options,
    });

    // Fetch the sources and install breakpoints on any new workers.
    const workerNames = Object.getOwnPropertyNames(newWorkerTargets);
    for (const actor of workerNames) {
      if (!workerTargets[actor]) {
        const front = newWorkerTargets[actor].threadFront;

        // This runs in the background and populates some data, but we also
        // want to allow it to fail quietly. For instance, it is pretty easy
        // for source clients to throw during the fetch if their thread
        // shuts down, and this would otherwise cause test failures.
        getSources(front).catch(e => console.error(e));
      }
    }

    workerTargets = newWorkerTargets;

    return workerNames.map(actor =>
      createWorker(actor, workerTargets[actor].url)
    );
  }

  if (!supportsWorkers(tabTarget)) {
    return Promise.resolve([]);
  }

  const { workers } = await tabTarget.listWorkers();
  return workers;
}

function getMainThread() {
  return threadFront.actor;
}

async function getBreakpointPositions(
  actors: Array<SourceActor>,
  range: ?Range
): Promise<{ [string]: number[] }> {
  const sourcePositions = {};

  for (const { thread, actor } of actors) {
    const sourceThreadFront = lookupThreadFront(thread);
    const sourceFront = sourceThreadFront.source({ actor });
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
    const sourceThreadFront = lookupThreadFront(thread);
    const sourceFront = sourceThreadFront.source({ actor });
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
  getEventListenerBreakpointTypes,
  detachWorkers,
  hasWasmSupport,
  lookupTarget,
};

export { setupCommands, clientCommands };
