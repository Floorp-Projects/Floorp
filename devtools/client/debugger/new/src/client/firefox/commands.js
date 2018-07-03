"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.clientCommands = exports.setupCommands = undefined;

var _breakpoint = require("../../utils/breakpoint/index");

var _create = require("./create");

var _frontsDevice = require("devtools/shared/fronts/device");

var _devtoolsModules = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-modules"];

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
let bpClients;
let threadClient;
let tabTarget;
let debuggerClient;
let supportsWasm;

function setupCommands(dependencies) {
  threadClient = dependencies.threadClient;
  tabTarget = dependencies.tabTarget;
  debuggerClient = dependencies.debuggerClient;
  supportsWasm = dependencies.supportsWasm;
  bpClients = {};
  return {
    bpClients
  };
}

function sendPacket(packet, callback = r => r) {
  return debuggerClient.request(packet).then(callback);
}

function resume() {
  return new Promise(resolve => {
    threadClient.resume(resolve);
  });
}

function stepIn() {
  return new Promise(resolve => {
    threadClient.stepIn(resolve);
  });
}

function stepOver() {
  return new Promise(resolve => {
    threadClient.stepOver(resolve);
  });
}

function stepOut() {
  return new Promise(resolve => {
    threadClient.stepOut(resolve);
  });
}

function rewind() {
  return new Promise(resolve => {
    threadClient.rewind(resolve);
  });
}

function reverseStepIn() {
  return new Promise(resolve => {
    threadClient.reverseStepIn(resolve);
  });
}

function reverseStepOver() {
  return new Promise(resolve => {
    threadClient.reverseStepOver(resolve);
  });
}

function reverseStepOut() {
  return new Promise(resolve => {
    threadClient.reverseStepOut(resolve);
  });
}

function breakOnNext() {
  return threadClient.breakOnNext();
}

function sourceContents(sourceId) {
  const sourceClient = threadClient.source({
    actor: sourceId
  });
  return sourceClient.source();
}

function getBreakpointByLocation(location) {
  const id = (0, _breakpoint.makePendingLocationId)(location);
  const bpClient = bpClients[id];

  if (bpClient) {
    const {
      actor,
      url,
      line,
      column,
      condition
    } = bpClient.location;
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

function setBreakpoint(location, condition, noSliding) {
  const sourceClient = threadClient.source({
    actor: location.sourceId
  });
  return sourceClient.setBreakpoint({
    line: location.line,
    column: location.column,
    condition,
    noSliding
  }).then(([{
    actualLocation
  }, bpClient]) => {
    actualLocation = (0, _create.createBreakpointLocation)(location, actualLocation);
    const id = (0, _breakpoint.makePendingLocationId)(actualLocation);
    bpClients[id] = bpClient;
    bpClient.location.line = actualLocation.line;
    bpClient.location.column = actualLocation.column;
    bpClient.location.url = actualLocation.sourceUrl || "";
    return {
      id,
      actualLocation
    };
  });
}

function removeBreakpoint(generatedLocation) {
  try {
    const id = (0, _breakpoint.makePendingLocationId)(generatedLocation);
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

function setBreakpointCondition(breakpointId, location, condition, noSliding) {
  const bpClient = bpClients[breakpointId];
  delete bpClients[breakpointId];
  return bpClient.setCondition(threadClient, condition, noSliding).then(_bpClient => {
    bpClients[breakpointId] = _bpClient;
    return {
      id: breakpointId
    };
  });
}

async function evaluateInFrame(script, frameId) {
  return evaluate(script, {
    frameId
  });
}

async function evaluateExpressions(scripts, frameId) {
  return Promise.all(scripts.map(script => evaluate(script, {
    frameId
  })));
}

function evaluate(script, {
  frameId
} = {}) {
  const params = frameId ? {
    frameActor: frameId
  } : {};

  if (!tabTarget || !tabTarget.activeConsole || !script) {
    return Promise.resolve({});
  }

  return new Promise(resolve => {
    tabTarget.activeConsole.evaluateJSAsync(script, result => resolve(result), params);
  });
}

function autocomplete(input, cursor, frameId) {
  if (!tabTarget || !tabTarget.activeConsole || !input) {
    return Promise.resolve({});
  }

  return new Promise(resolve => {
    tabTarget.activeConsole.autocomplete(input, cursor, result => resolve(result), frameId);
  });
}

function debuggeeCommand(script) {
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

function navigate(url) {
  return tabTarget.activeTab.navigateTo(url);
}

function reload() {
  return tabTarget.activeTab.reload();
}

function getProperties(grip) {
  const objClient = threadClient.pauseGrip(grip);
  return objClient.getPrototypeAndProperties().then(resp => {
    const {
      ownProperties,
      safeGetterValues
    } = resp;

    for (const name in safeGetterValues) {
      const {
        enumerable,
        writable,
        getterValue
      } = safeGetterValues[name];
      ownProperties[name] = {
        enumerable,
        writable,
        value: getterValue
      };
    }

    return resp;
  });
}

async function getFrameScopes(frame) {
  if (frame.scope) {
    return frame.scope;
  }

  return threadClient.getEnvironment(frame.id);
}

function pauseOnExceptions(shouldPauseOnExceptions, shouldPauseOnCaughtExceptions) {
  return threadClient.pauseOnExceptions(shouldPauseOnExceptions, // Providing opposite value because server
  // uses "shouldIgnoreCaughtExceptions"
  !shouldPauseOnCaughtExceptions);
}

function prettyPrint(sourceId, indentSize) {
  const sourceClient = threadClient.source({
    actor: sourceId
  });
  return sourceClient.prettyPrint(indentSize);
}

async function blackBox(sourceId, isBlackBoxed) {
  const sourceClient = threadClient.source({
    actor: sourceId
  });

  if (isBlackBoxed) {
    await sourceClient.unblackBox();
  } else {
    await sourceClient.blackBox();
  }

  return {
    isBlackBoxed: !isBlackBoxed
  };
}

function disablePrettyPrint(sourceId) {
  const sourceClient = threadClient.source({
    actor: sourceId
  });
  return sourceClient.disablePrettyPrint();
}

async function setPausePoints(sourceId, pausePoints) {
  return sendPacket({
    to: sourceId,
    type: "setPausePoints",
    pausePoints
  });
}

async function setSkipPausing(shouldSkip) {
  return threadClient.request({
    skip: shouldSkip,
    to: threadClient.actor,
    type: "skipBreakpoints"
  });
}

function interrupt() {
  return threadClient.interrupt();
}

function eventListeners() {
  return threadClient.eventListeners();
}

function pauseGrip(func) {
  return threadClient.pauseGrip(func);
}

async function fetchSources() {
  const {
    sources
  } = await threadClient.getSources();
  return sources.map(source => (0, _create.createSource)(source, {
    supportsWasm
  }));
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
  const root = await tabTarget.root; // root is not available on all debug targets.

  if (!root) {
    return false;
  }

  const deviceFront = await (0, _frontsDevice.getDeviceFront)(debuggerClient, root);
  const description = await deviceFront.getDescription();
  const isFennec = description.apptype === "mobile/android";

  if (!isFennec) {
    // Explicitly return true early to avoid calling Services.vs.compare.
    // This would force us to extent the Services shim provided by
    // devtools-modules, used when this code runs in a tab.
    return true;
  } // We are only interested in Fennec release versions here.
  // We assume that the server fix for Bug 1443550 will land in FF61.


  const version = description.platformversion;
  return _devtoolsModules.Services.vc.compare(version, "61.0") >= 0;
}

async function fetchWorkers() {
  // Temporary workaround for Bug 1443550
  // XXX: Remove when FF60 for Android is no longer used or available.
  const supportsListWorkers = await checkServerSupportsListWorkers(); // NOTE: The Worker and Browser Content toolboxes do not have a parent
  // with a listWorkers function
  // TODO: there is a listWorkers property, but it is not a function on the
  // parent. Investigate what it is

  if (!threadClient._parent || typeof threadClient._parent.listWorkers != "function" || !supportsListWorkers) {
    return Promise.resolve({
      workers: []
    });
  }

  return threadClient._parent.listWorkers();
}

const clientCommands = {
  autocomplete,
  blackBox,
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
exports.setupCommands = setupCommands;
exports.clientCommands = clientCommands;