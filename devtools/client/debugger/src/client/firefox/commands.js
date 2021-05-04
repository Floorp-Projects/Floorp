/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createThread, createFrame } from "./create";
import { makePendingLocationId } from "../../utils/breakpoint";

import Reps from "devtools/client/shared/components/reps/index";

let targets;
let commands;
let breakpoints;

const CALL_STACK_PAGE_SIZE = 1000;

function setupCommands(innerCommands) {
  commands = innerCommands;
  targets = {};
  breakpoints = {};
}

function currentTarget() {
  return commands.targetCommand.targetFront;
}

function currentThreadFront() {
  return currentTarget().threadFront;
}

function createObjectFront(grip) {
  if (!grip.actor) {
    throw new Error("Actor is missing");
  }

  return commands.client.createObjectFront(grip, currentThreadFront());
}

async function loadObjectProperties(root, threadActorID) {
  const { utils } = Reps.objectInspector;
  const properties = await utils.loadProperties.loadItemProperties(
    root,
    commands.client,
    undefined,
    threadActorID
  );
  return utils.node.getChildren({
    item: root,
    loadedProperties: new Map([[root.path, properties]]),
  });
}

function releaseActor(actor) {
  if (!actor) {
    return;
  }
  const objFront = commands.client.getFrontByID(actor);

  if (objFront) {
    return objFront.release().catch(() => {});
  }
}

// Get a copy of the current targets.
function getTargetsMap() {
  return Object.assign({}, targets);
}

function lookupTarget(thread) {
  if (thread == currentThreadFront().actor) {
    return currentTarget();
  }

  const targetsMap = getTargetsMap();
  if (!targetsMap[thread]) {
    throw new Error(`Unknown thread front: ${thread}`);
  }

  return targetsMap[thread];
}

function lookupThreadFront(thread) {
  const target = lookupTarget(thread);
  return target.threadFront;
}

function listThreadFronts() {
  const list = Object.values(getTargetsMap());
  return list.map(target => target.threadFront).filter(t => !!t);
}

function forEachThread(iteratee) {
  // We have to be careful here to atomically initiate the operation on every
  // thread, with no intervening await. Otherwise, other code could run and
  // trigger additional thread operations. Requests on server threads will
  // resolve in FIFO order, and this could result in client and server state
  // going out of sync.

  const promises = [currentThreadFront(), ...listThreadFronts()].map(
    // If a thread shuts down while sending the message then it will
    // throw. Ignore these exceptions.
    t => iteratee(t).catch(e => console.log(e))
  );

  return Promise.all(promises);
}

function resume(thread, frameId) {
  return lookupThreadFront(thread).resume();
}

function stepIn(thread, frameId) {
  return lookupThreadFront(thread).stepIn(frameId);
}

function stepOver(thread, frameId) {
  return lookupThreadFront(thread).stepOver(frameId);
}

function stepOut(thread, frameId) {
  return lookupThreadFront(thread).stepOut(frameId);
}

function restart(thread, frameId) {
  return lookupThreadFront(thread).restart(frameId);
}

function breakOnNext(thread) {
  return lookupThreadFront(thread).breakOnNext();
}

async function sourceContents({ actor, thread }) {
  const sourceThreadFront = lookupThreadFront(thread);
  const sourceFront = sourceThreadFront.source({ actor });
  const { source, contentType } = await sourceFront.source();
  return { source, contentType };
}

async function setXHRBreakpoint(path, method) {
  const hasWatcherSupport = commands.targetCommand.hasTargetWatcherSupport();
  if (!hasWatcherSupport) {
    // Without watcher support, forward setXHRBreakpoint to all threads.
    return forEachThread(thread => thread.setXHRBreakpoint(path, method));
  }
  const breakpointsFront = await commands.targetCommand.watcherFront.getBreakpointListActor();
  await breakpointsFront.setXHRBreakpoint(path, method);
}

async function removeXHRBreakpoint(path, method) {
  const hasWatcherSupport = commands.targetCommand.hasTargetWatcherSupport();
  if (!hasWatcherSupport) {
    // Without watcher support, forward removeXHRBreakpoint to all threads.
    return forEachThread(thread => thread.removeXHRBreakpoint(path, method));
  }
  const breakpointsFront = await commands.targetCommand.watcherFront.getBreakpointListActor();
  await breakpointsFront.removeXHRBreakpoint(path, method);
}

export function toggleJavaScriptEnabled(enabled) {
  return commands.targetConfigurationCommand.updateConfiguration({
    javascriptEnabled: enabled,
  });
}

function addWatchpoint(object, property, label, watchpointType) {
  if (currentTarget().traits.watchpoints) {
    const objectFront = createObjectFront(object);
    return objectFront.addWatchpoint(property, label, watchpointType);
  }
}

async function removeWatchpoint(object, property) {
  if (currentTarget().traits.watchpoints) {
    const objectFront = createObjectFront(object);
    await objectFront.removeWatchpoint(property);
  }
}

function hasBreakpoint(location) {
  return !!breakpoints[makePendingLocationId(location)];
}

async function setBreakpoint(location, options) {
  const breakpoint = breakpoints[makePendingLocationId(location)];
  if (
    breakpoint &&
    JSON.stringify(breakpoint.options) == JSON.stringify(options)
  ) {
    return;
  }
  breakpoints[makePendingLocationId(location)] = { location, options };

  // Map frontend options to a more restricted subset of what
  // the server supports. For example frontend uses `hidden` attribute
  // which isn't meant to be passed to the server.
  // (note that protocol.js specification isn't enough to filter attributes,
  //  all primitive attributes will be passed as-is)
  const serverOptions = {
    condition: options.condition,
    logValue: options.logValue,
  };
  const hasWatcherSupport = commands.targetCommand.hasTargetWatcherSupport();
  if (!hasWatcherSupport) {
    // Without watcher support, unconditionally forward setBreakpoint to all threads.
    return forEachThread(async thread =>
      thread.setBreakpoint(location, serverOptions)
    );
  }
  const breakpointsFront = await commands.targetCommand.watcherFront.getBreakpointListActor();
  await breakpointsFront.setBreakpoint(location, serverOptions);

  // Call setBreakpoint for threads linked to targets
  // not managed by the watcher.
  return forEachThread(async thread => {
    if (
      !commands.targetCommand.hasTargetWatcherSupport(
        thread.targetFront.targetType
      )
    ) {
      return thread.setBreakpoint(location, serverOptions);
    }
  });
}

async function removeBreakpoint(location) {
  delete breakpoints[makePendingLocationId(location)];

  const hasWatcherSupport = commands.targetCommand.hasTargetWatcherSupport();
  if (!hasWatcherSupport) {
    // Without watcher support, unconditionally forward removeBreakpoint to all threads.
    return forEachThread(async thread => thread.removeBreakpoint(location));
  }
  const breakpointsFront = await commands.targetCommand.watcherFront.getBreakpointListActor();
  await breakpointsFront.removeBreakpoint(location);

  // Call removeBreakpoint for threads linked to targets
  // not managed by the watcher.
  return forEachThread(async thread => {
    if (
      !commands.targetCommand.hasTargetWatcherSupport(
        thread.targetFront.targetType
      )
    ) {
      return thread.removeBreakpoint(location);
    }
  });
}

function evaluateInFrame(script, options) {
  return evaluate(script, options);
}

async function evaluateExpressions(scripts, options) {
  return Promise.all(scripts.map(script => evaluate(script, options)));
}

async function evaluate(script, { thread, frameId } = {}) {
  const params = { thread, frameActor: frameId };
  if (!currentTarget() || !script) {
    return { result: null };
  }

  const target = thread ? lookupTarget(thread) : currentTarget();
  const consoleFront = await target.getFront("console");
  if (!consoleFront) {
    return { result: null };
  }

  return consoleFront.evaluateJSAsync(script, params);
}

async function autocomplete(input, cursor, frameId) {
  if (!currentTarget() || !input) {
    return {};
  }
  const consoleFront = await currentTarget().getFront("console");
  if (!consoleFront) {
    return {};
  }

  return new Promise(resolve => {
    consoleFront.autocomplete(
      input,
      cursor,
      result => resolve(result),
      frameId
    );
  });
}

function navigate(url) {
  return currentTarget().navigateTo({ url });
}

function reload() {
  return currentTarget().reload();
}

function getProperties(thread, grip) {
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

async function getFrames(thread) {
  const threadFront = lookupThreadFront(thread);
  const response = await threadFront.getFrames(0, CALL_STACK_PAGE_SIZE);

  return Promise.all(
    response.frames.map((frame, i) => createFrame(thread, frame, i))
  );
}

async function getFrameScopes(frame) {
  const frameFront = lookupThreadFront(frame.thread).getActorByID(frame.id);
  return frameFront.getEnvironment();
}

async function pauseOnExceptions(
  shouldPauseOnExceptions,
  shouldPauseOnCaughtExceptions
) {
  if (commands.targetCommand.hasTargetWatcherSupport()) {
    const threadConfigurationActor = await commands.targetCommand.watcherFront.getThreadConfigurationActor();
    await threadConfigurationActor.updateConfiguration({
      pauseOnExceptions: shouldPauseOnExceptions,
      ignoreCaughtExceptions: !shouldPauseOnCaughtExceptions,
    });
  } else {
    return forEachThread(thread =>
      thread.pauseOnExceptions(
        shouldPauseOnExceptions,
        // Providing opposite value because server
        // uses "shouldIgnoreCaughtExceptions"
        !shouldPauseOnCaughtExceptions
      )
    );
  }
}

async function blackBox(sourceActor, isBlackBoxed, range) {
  const sourceFront = currentThreadFront().source({ actor: sourceActor.actor });
  if (isBlackBoxed) {
    await sourceFront.unblackBox(range);
  } else {
    await sourceFront.blackBox(range);
  }
}

function setSkipPausing(shouldSkip) {
  return forEachThread(thread => thread.skipBreakpoints(shouldSkip));
}

function interrupt(thread) {
  return lookupThreadFront(thread).interrupt();
}

function setEventListenerBreakpoints(ids) {
  return forEachThread(thread => thread.setActiveEventBreakpoints(ids));
}

// eslint-disable-next-line
async function getEventListenerBreakpointTypes() {
  let categories;
  try {
    categories = await currentThreadFront().getAvailableEventBreakpoints();

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

function pauseGrip(thread, func) {
  return lookupThreadFront(thread).pauseGrip(func);
}

async function toggleEventLogging(logEventBreakpoints) {
  return forEachThread(thread =>
    thread.toggleEventLogging(logEventBreakpoints)
  );
}

async function addThread(targetFront) {
  const threadActorID = targetFront.targetForm.threadActor;
  if (!targets[threadActorID]) {
    targets[threadActorID] = targetFront;
  }
  return createThread(threadActorID, targetFront);
}

function removeThread(thread) {
  delete targets[thread.actor];
}

function getMainThread() {
  return currentThreadFront().actor;
}

async function getSourceActorBreakpointPositions({ thread, actor }, range) {
  const sourceThreadFront = lookupThreadFront(thread);
  const sourceFront = sourceThreadFront.source({ actor });
  return sourceFront.getBreakpointPositionsCompressed(range);
}

async function getSourceActorBreakableLines({ thread, actor }) {
  let sourceFront;
  let actorLines = [];
  try {
    const sourceThreadFront = lookupThreadFront(thread);
    sourceFront = sourceThreadFront.source({ actor });
    actorLines = await sourceFront.getBreakableLines();
  } catch (e) {
    // Handle backward compatibility
    if (
      e.message &&
      e.message.match(/does not recognize the packet type getBreakableLines/)
    ) {
      const pos = await sourceFront.getBreakpointPositionsCompressed();
      actorLines = Object.keys(pos).map(line => Number(line));
    } else {
      // Other exceptions could be due to the target thread being shut down.
      console.warn(`getSourceActorBreakableLines failed: ${e}`);
    }
  }

  return actorLines;
}

function getFrontByID(actorID) {
  return commands.client.getFrontByID(actorID);
}

function fetchAncestorFramePositions(index) {
  currentThreadFront().fetchAncestorFramePositions(index);
}

const clientCommands = {
  autocomplete,
  blackBox,
  createObjectFront,
  loadObjectProperties,
  releaseActor,
  interrupt,
  pauseGrip,
  resume,
  stepIn,
  stepOut,
  stepOver,
  restart,
  breakOnNext,
  sourceContents,
  getSourceActorBreakpointPositions,
  getSourceActorBreakableLines,
  hasBreakpoint,
  setBreakpoint,
  setXHRBreakpoint,
  removeXHRBreakpoint,
  addWatchpoint,
  removeWatchpoint,
  removeBreakpoint,
  evaluate,
  evaluateInFrame,
  evaluateExpressions,
  navigate,
  reload,
  getProperties,
  getFrameScopes,
  getFrames,
  pauseOnExceptions,
  toggleEventLogging,
  addThread,
  removeThread,
  getMainThread,
  setSkipPausing,
  setEventListenerBreakpoints,
  getEventListenerBreakpointTypes,
  lookupTarget,
  getFrontByID,
  fetchAncestorFramePositions,
  toggleJavaScriptEnabled,
};

export { setupCommands, clientCommands };
