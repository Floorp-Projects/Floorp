/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createFrame } from "./create";
import { makeBreakpointServerLocationId } from "../../utils/breakpoint/index";

import Reps from "devtools/client/shared/components/reps/index";

let commands;
let breakpoints;

// The maximal number of stackframes to retrieve when pausing
const CALL_STACK_PAGE_SIZE = 1000;

function setupCommands(innerCommands) {
  commands = innerCommands;
  breakpoints = {};
}

function currentTarget() {
  return commands.targetCommand.targetFront;
}

function currentThreadFront() {
  return currentTarget().threadFront;
}

/**
 * Create an object front for the passed grip
 *
 * @param {Object} grip
 * @param {Object} frame: An optional frame that will manage the created object front.
 *                        if not passed, the current thread front will manage the object.
 * @returns {ObjectFront}
 */
function createObjectFront(grip, frame) {
  if (!grip.actor) {
    throw new Error("Actor is missing");
  }
  const threadFront = frame?.thread
    ? lookupThreadFront(frame.thread)
    : currentThreadFront();
  const frameFront = frame ? threadFront.getActorByID(frame.id) : null;
  return commands.client.createObjectFront(grip, threadFront, frameFront);
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
    return Promise.resolve();
  }
  const objFront = commands.client.getFrontByID(actor);

  if (!objFront) {
    return Promise.resolve();
  }

  return objFront.release().catch(() => {});
}

function lookupTarget(thread) {
  if (thread == currentThreadFront().actor) {
    return currentTarget();
  }

  const targets = commands.targetCommand.getAllTargets(
    commands.targetCommand.ALL_TYPES
  );
  return targets.find(target => target.targetForm.threadActor == thread);
}

function lookupThreadFront(thread) {
  const target = lookupTarget(thread);
  return target.threadFront;
}

function listThreadFronts() {
  const targets = commands.targetCommand.getAllTargets(
    commands.targetCommand.ALL_TYPES
  );
  return targets.map(target => target.threadFront).filter(front => !!front);
}

function forEachThread(iteratee) {
  // We have to be careful here to atomically initiate the operation on every
  // thread, with no intervening await. Otherwise, other code could run and
  // trigger additional thread operations. Requests on server threads will
  // resolve in FIFO order, and this could result in client and server state
  // going out of sync.

  const promises = listThreadFronts().map(
    // If a thread shuts down while sending the message then it will
    // throw. Ignore these exceptions.
    t => iteratee(t).catch(e => console.log(e))
  );

  return Promise.all(promises);
}

async function toggleTracing(logMethod) {
  return commands.tracerCommand.toggle(logMethod);
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
    await forEachThread(thread => thread.setXHRBreakpoint(path, method));
    return;
  }
  const breakpointsFront =
    await commands.targetCommand.watcherFront.getBreakpointListActor();
  await breakpointsFront.setXHRBreakpoint(path, method);
}

async function removeXHRBreakpoint(path, method) {
  const hasWatcherSupport = commands.targetCommand.hasTargetWatcherSupport();
  if (!hasWatcherSupport) {
    // Without watcher support, forward removeXHRBreakpoint to all threads.
    await forEachThread(thread => thread.removeXHRBreakpoint(path, method));
    return;
  }
  const breakpointsFront =
    await commands.targetCommand.watcherFront.getBreakpointListActor();
  await breakpointsFront.removeXHRBreakpoint(path, method);
}

export function toggleJavaScriptEnabled(enabled) {
  return commands.targetConfigurationCommand.updateConfiguration({
    javascriptEnabled: enabled,
  });
}

async function addWatchpoint(object, property, label, watchpointType) {
  if (!currentTarget().getTrait("watchpoints")) {
    return;
  }
  const objectFront = createObjectFront(object);
  await objectFront.addWatchpoint(property, label, watchpointType);
}

async function removeWatchpoint(object, property) {
  if (!currentTarget().getTrait("watchpoints")) {
    return;
  }
  const objectFront = createObjectFront(object);
  await objectFront.removeWatchpoint(property);
}

function hasBreakpoint(location) {
  return !!breakpoints[makeBreakpointServerLocationId(location)];
}

function getServerBreakpointsList() {
  return Object.values(breakpoints);
}

async function setBreakpoint(location, options) {
  const breakpoint = breakpoints[makeBreakpointServerLocationId(location)];
  if (
    breakpoint &&
    JSON.stringify(breakpoint.options) == JSON.stringify(options)
  ) {
    return null;
  }
  breakpoints[makeBreakpointServerLocationId(location)] = { location, options };

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
  const breakpointsFront =
    await commands.targetCommand.watcherFront.getBreakpointListActor();
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

    return Promise.resolve();
  });
}

async function removeBreakpoint(location) {
  delete breakpoints[makeBreakpointServerLocationId(location)];

  const hasWatcherSupport = commands.targetCommand.hasTargetWatcherSupport();
  if (!hasWatcherSupport) {
    // Without watcher support, unconditionally forward removeBreakpoint to all threads.
    return forEachThread(async thread => thread.removeBreakpoint(location));
  }
  const breakpointsFront =
    await commands.targetCommand.watcherFront.getBreakpointListActor();
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

    return Promise.resolve();
  });
}

async function evaluateExpressions(scripts, options) {
  return Promise.all(scripts.map(script => evaluate(script, options)));
}

async function evaluate(script, { frameId, threadId } = {}) {
  if (!currentTarget() || !script) {
    return { result: null };
  }

  const selectedTargetFront = threadId ? lookupTarget(threadId) : null;

  return commands.scriptCommand.execute(script, {
    frameActor: frameId,
    selectedTargetFront,
    disableBreaks: true,
  });
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

async function pauseOnDebuggerStatement(shouldPauseOnDebuggerStatement) {
  await commands.threadConfigurationCommand.updateConfiguration({
    shouldPauseOnDebuggerStatement,
  });
}

async function pauseOnExceptions(
  shouldPauseOnExceptions,
  shouldPauseOnCaughtExceptions
) {
  await commands.threadConfigurationCommand.updateConfiguration({
    pauseOnExceptions: shouldPauseOnExceptions,
    ignoreCaughtExceptions: !shouldPauseOnCaughtExceptions,
  });
}

async function blackBox(sourceActor, shouldBlackBox, ranges) {
  const hasWatcherSupport = commands.targetCommand.hasTargetWatcherSupport();
  if (hasWatcherSupport) {
    const blackboxingFront =
      await commands.targetCommand.watcherFront.getBlackboxingActor();
    if (shouldBlackBox) {
      await blackboxingFront.blackbox(sourceActor.url, ranges);
    } else {
      await blackboxingFront.unblackbox(sourceActor.url, ranges);
    }
  } else {
    const sourceFront = currentThreadFront().source({
      actor: sourceActor.actor,
    });
    // If there are no ranges, the whole source is being blackboxed
    if (!ranges.length) {
      await toggleBlackBoxSourceFront(sourceFront, shouldBlackBox);
      return;
    }
    // Blackbox the specific ranges
    for (const range of ranges) {
      await toggleBlackBoxSourceFront(sourceFront, shouldBlackBox, range);
    }
  }
}

async function toggleBlackBoxSourceFront(sourceFront, shouldBlackBox, range) {
  if (shouldBlackBox) {
    await sourceFront.blackBox(range);
  } else {
    await sourceFront.unblackBox(range);
  }
}

async function setSkipPausing(shouldSkip) {
  await commands.threadConfigurationCommand.updateConfiguration({
    skipBreakpoints: shouldSkip,
  });
}

async function setEventListenerBreakpoints(ids) {
  const hasWatcherSupport = commands.targetCommand.hasTargetWatcherSupport();
  if (!hasWatcherSupport) {
    await forEachThread(thread => thread.setActiveEventBreakpoints(ids));
    return;
  }
  const breakpointListFront =
    await commands.targetCommand.watcherFront.getBreakpointListActor();
  await breakpointListFront.setActiveEventBreakpoints(ids);
}

async function getEventListenerBreakpointTypes() {
  return currentThreadFront().getAvailableEventBreakpoints();
}

function pauseGrip(thread, func) {
  return lookupThreadFront(thread).pauseGrip(func);
}

async function toggleEventLogging(logEventBreakpoints) {
  await commands.threadConfigurationCommand.updateConfiguration({
    logEventBreakpoints,
  });
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
  let actorLines = [];
  try {
    const sourceThreadFront = lookupThreadFront(thread);
    const sourceFront = sourceThreadFront.source({ actor });
    actorLines = await sourceFront.getBreakableLines();
  } catch (e) {
    // Exceptions could be due to the target thread being shut down.
    console.warn(`getSourceActorBreakableLines failed: ${e}`);
  }

  return actorLines;
}

function getFrontByID(actorID) {
  return commands.client.getFrontByID(actorID);
}

function fetchAncestorFramePositions(index) {
  currentThreadFront().fetchAncestorFramePositions(index);
}

async function setOverride(url, path) {
  const hasWatcherSupport = commands.targetCommand.hasTargetWatcherSupport();
  if (hasWatcherSupport) {
    const networkFront =
      await commands.targetCommand.watcherFront.getNetworkParentActor();
    return networkFront.override(url, path);
  }
  return null;
}

async function removeOverride(url) {
  const hasWatcherSupport = commands.targetCommand.hasTargetWatcherSupport();
  if (hasWatcherSupport) {
    const networkFront =
      await commands.targetCommand.watcherFront.getNetworkParentActor();
    networkFront.removeOverride(url);
  }
}

const clientCommands = {
  autocomplete,
  blackBox,
  createObjectFront,
  loadObjectProperties,
  releaseActor,
  pauseGrip,
  toggleTracing,
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
  getServerBreakpointsList,
  setBreakpoint,
  setXHRBreakpoint,
  removeXHRBreakpoint,
  addWatchpoint,
  removeWatchpoint,
  removeBreakpoint,
  evaluate,
  evaluateExpressions,
  getProperties,
  getFrameScopes,
  getFrames,
  pauseOnDebuggerStatement,
  pauseOnExceptions,
  toggleEventLogging,
  getMainThread,
  setSkipPausing,
  setEventListenerBreakpoints,
  getEventListenerBreakpointTypes,
  getFrontByID,
  fetchAncestorFramePositions,
  toggleJavaScriptEnabled,
  setOverride,
  removeOverride,
};

export { setupCommands, clientCommands };
