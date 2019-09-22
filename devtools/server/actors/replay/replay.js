/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy, consistent-return */

// This file defines the logic that runs in the record/replay devtools sandbox.
// This code is loaded into all recording/replaying processes, and responds to
// requests and other instructions from the middleman via the exported symbols
// defined at the end of this file.
//
// Like all other JavaScript in the recording/replaying process, this code's
// state is included in memory snapshots and reset when checkpoints are
// restored. In the process of handling the middleman's requests, however, its
// state may vary between recording and replaying, or between different
// replays. As a result, we have to be very careful about performing operations
// that might interact with the recording --- any time we enter the debuggee
// and evaluate code or perform other operations.
// The divergeFromRecording function should be used at any point where such
// interactions might occur.
// eslint-disable spaced-comment

"use strict";

const CC = Components.Constructor;

// Create a sandbox with the resources we need.
const sandbox = Cu.Sandbox(
  CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")(),
  {
    wantGlobalProperties: ["InspectorUtils", "CSSRule"],
  }
);
Cu.evalInSandbox(
  "Components.utils.import('resource://gre/modules/jsdebugger.jsm');" +
    "Components.utils.import('resource://gre/modules/Services.jsm');" +
    "Components.utils.import('resource://devtools/shared/execution-point-utils.js');" +
    "addDebuggerToGlobal(this);",
  sandbox
);
const {
  Debugger,
  RecordReplayControl,
  Services,
  InspectorUtils,
  CSSRule,
  pointPrecedes,
  pointEquals,
  pointArrayIncludes,
  findClosestPoint,
} = sandbox;

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const jsmScope = require("resource://devtools/shared/Loader.jsm");
const { DebuggerNotificationObserver } = Cu.getGlobalForObject(jsmScope);

const {
  eventBreakpointForNotification,
} = require("devtools/server/actors/utils/event-breakpoints");

const dbg = new Debugger();
const gFirstGlobal = dbg.makeGlobalObjectReference(sandbox);
const gAllGlobals = [];

// We are interested in debugging all globals in the process.
dbg.onNewGlobalObject = function(global) {
  try {
    dbg.addDebuggee(global);
    gAllGlobals.push(global);

    scanningOnNewGlobal(global);
    eventListenerOnNewGlobal(global);
  } catch (e) {
    // Ignore errors related to adding a same-compartment debuggee.
    // See bug 1523755.
    if (
      !/debugger and debuggee must be in different compartments/.test("" + e)
    ) {
      throw e;
    }
  }
};

///////////////////////////////////////////////////////////////////////////////
// Utilities
///////////////////////////////////////////////////////////////////////////////

const dump = str => {
  RecordReplayControl.dump(`[Child #${RecordReplayControl.childId()}]: ${str}`);
};

function assert(v) {
  if (!v) {
    dump(`Assertion Failed: ${Error().stack}\n`);
    throw new Error("Assertion Failed!");
  }
}

function throwError(v) {
  dump(`Error: ${v}\n`);
  throw new Error(v);
}

// Bidirectional map between objects and IDs.
function IdMap() {
  this._idToObject = [undefined];
  this._objectToId = new Map();
}

IdMap.prototype = {
  add(object) {
    assert(object && !this._objectToId.has(object));
    const id = this._idToObject.length;
    this._idToObject.push(object);
    this._objectToId.set(object, id);
    return id;
  },

  getId(object) {
    const id = this._objectToId.get(object);
    return id === undefined ? 0 : id;
  },

  getObject(id) {
    return this._idToObject[id];
  },

  forEach(callback) {
    for (let i = 1; i < this._idToObject.length; i++) {
      callback(i, this._idToObject[i]);
    }
  },
};

function countScriptFrames() {
  let count = 0;
  let frame = dbg.getNewestFrame();
  while (frame) {
    if (considerScript(frame.script)) {
      count++;
    }
    frame = frame.older;
  }
  return count;
}

function scriptFrameForIndex(index) {
  let indexFromTop = countScriptFrames() - 1 - index;
  let frame = dbg.getNewestFrame();
  while (true) {
    if (considerScript(frame.script)) {
      if (indexFromTop-- == 0) {
        break;
      }
    }
    frame = frame.older;
  }
  return frame;
}

function isNonNullObject(obj) {
  return obj && (typeof obj == "object" || typeof obj == "function");
}

function getMemoryUsage() {
  const memoryKinds = {
    Generic: [1],
    Snapshots: [2, 3, 4, 5, 6, 7],
    ScriptHits: [8],
  };

  const rv = {};
  for (const [name, kinds] of Object.entries(memoryKinds)) {
    let total = 0;
    kinds.forEach(kind => {
      total += RecordReplayControl.memoryUsage(kind);
    });
    rv[name] = total;
  }
  return rv;
}

///////////////////////////////////////////////////////////////////////////////
// Persistent Script State
///////////////////////////////////////////////////////////////////////////////

// Association between Debugger.Scripts and their IDs. The indices that this
// table assigns to scripts are stable across the entire recording, even though
// this table (like all JS state) is included in snapshots, rolled back when
// rewinding, and so forth.  In debuggee time, this table only grows (there is
// no way to remove entries). Scripts created for debugger activity (e.g. eval)
// are ignored, and off thread compilation is disabled, so this table acquires
// the same scripts in the same order as we roll back and run forward in the
// recording.
const gScripts = new IdMap();

// Any scripts added since the last checkpoint.
const gNewScripts = [];

function addScript(script) {
  const id = gScripts.add(script);
  script.setInstrumentationId(id);
  script.getChildScripts().forEach(addScript);
}

// Association between Debugger.ScriptSources and their IDs. As for gScripts,
// the indices assigned to a script source are consistent across all replays
// and rewinding.
const gScriptSources = new IdMap();

function addScriptSource(source) {
  // Tolerate redundant attempts to add the same source, as we might see
  // onNewScript calls for different scripts with the same source.
  if (!gScriptSources.getId(source)) {
    gScriptSources.add(source);
  }
}

function considerScript(script) {
  // The set of scripts which is exposed to the debugger server is the same as
  // the scripts for which the progress counter is updated.
  return RecordReplayControl.shouldUpdateProgressCounter(script.url);
}

function setEmptyInstrumentationId(script) {
  script.setInstrumentationId(0);
  script.getChildScripts().forEach(setEmptyInstrumentationId);
}

dbg.onNewScript = function(script) {
  if (RecordReplayControl.areThreadEventsDisallowed()) {
    // This script is part of an eval on behalf of the debugger.
    return;
  }

  if (!considerScript(script)) {
    setEmptyInstrumentationId(script);
    return;
  }

  addScript(script);
  addScriptSource(script.source);

  if (gManifest.kind == "resume") {
    gNewScripts.push(getScriptData(gScripts.getId(script)));
  }

  // Check in case any handlers we need to install are on the scripts just
  // created.
  installPendingHandlers();
};

// Listen to the content of all loaded HTML files, with similar logic to that
// in TabSources.
const gHtmlContent = new Map();

getWindow().docShell.watchedByDevtools = true;

Services.obs.addObserver(
  {
    observe(subject, topic, data) {
      assert(topic == "webnavigation-create");
      subject.QueryInterface(Ci.nsIDocShell);
      subject.watchedByDevtools = true;
    },
  },
  "webnavigation-create"
);

Services.obs.addObserver(
  {
    observe(subject, topic, data) {
      assert(topic == "devtools-html-content");
      const { uri, contents } = JSON.parse(data);
      if (gHtmlContent.has(uri)) {
        const existing = gHtmlContent.get(uri);
        existing.content = existing.content + contents;
      } else {
        gHtmlContent.set(uri, {
          content: contents,
          contentType: "text/html",
        });
      }
    },
  },
  "devtools-html-content"
);

///////////////////////////////////////////////////////////////////////////////
// Console Message State
///////////////////////////////////////////////////////////////////////////////

// All console messages that have been generated.
const gConsoleMessages = [];

// Any new console messages since the last checkpoint.
const gNewConsoleMessages = [];

function newConsoleMessage(contents) {
  gConsoleMessages.push(contents);

  if (gManifest.kind == "resume") {
    gNewConsoleMessages.push(contents);
  }
}

function convertStack(stack) {
  if (stack) {
    const { source, line, column, functionDisplayName } = stack;
    const parent = convertStack(stack.parent);
    return { source, line, column, functionDisplayName, parent };
  }
  return null;
}

// Map from warp target values attached to messages to the associated execution
// point.
const gWarpTargetPoints = [null];

// Listen to all console messages in the process.
Services.console.registerListener({
  QueryInterface: ChromeUtils.generateQI([Ci.nsIConsoleListener]),

  observe(message) {
    if (message instanceof Ci.nsIScriptError) {
      // If there is a warp target associated with the error, use that. This
      // will take users to the point where the error was originally generated,
      // rather than where it was reported to the console.
      let executionPoint = gWarpTargetPoints[message.timeWarpTarget];
      if (!executionPoint) {
        executionPoint = currentScriptedExecutionPoint();
      }

      const contents = JSON.parse(JSON.stringify(message));
      contents.stack = convertStack(message.stack);
      contents.executionPoint = executionPoint;
      contents.messageType = "PageError";
      newConsoleMessage(contents);
    }
  },
});

// Listen to all console API messages in the process.
Services.obs.addObserver(
  {
    QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),

    observe(message, topic, data) {
      const apiMessage = message.wrappedJSObject;

      const contents = { messageType: "ConsoleAPI" };
      for (const id in apiMessage) {
        if (id != "wrappedJSObject" && id != "arguments") {
          contents[id] = JSON.parse(JSON.stringify(apiMessage[id]));
        }
      }

      contents.executionPoint = currentScriptedExecutionPoint();

      // Message arguments are preserved as debuggee values.
      if (apiMessage.arguments) {
        contents.arguments = apiMessage.arguments.map(v => {
          return makeConvertedDebuggeeValue(v);
        });

        contents.argumentsData = new PreviewedObjects();
        contents.arguments.forEach(v =>
          contents.argumentsData.addValue(v, PropertyLevels.FULL)
        );
      }

      newConsoleMessage(contents);
    },
  },
  "console-api-log-event"
);

// eslint-disable-next-line no-unused-vars
function NewTimeWarpTarget() {
  // Create a counter which will be associated with the current scripted
  // location if we want to warp here later.
  gWarpTargetPoints.push(currentScriptedExecutionPoint());
  return gWarpTargetPoints.length - 1;
}

///////////////////////////////////////////////////////////////////////////////
// Event State
///////////////////////////////////////////////////////////////////////////////

const gNewEvents = [];

const gDebuggerNotificationObserver = new DebuggerNotificationObserver();
gDebuggerNotificationObserver.addListener(eventBreakpointListener);

function eventListenerOnNewGlobal(global) {
  try {
    gDebuggerNotificationObserver.connect(global.unsafeDereference());
  } catch (e) {}
}

function eventBreakpointListener(notification) {
  const event = eventBreakpointForNotification(dbg, notification);
  if (!event) {
    return;
  }

  // Advance the progress counter before and after each event, so that we can
  // determine later if any JS ran between the two points.
  RecordReplayControl.advanceProgressCounter();

  if (notification.phase == "pre") {
    const progress = RecordReplayControl.progressCounter();

    if (gManifest.kind == "resume") {
      gNewEvents.push({ event, checkpoint: gLastCheckpoint, progress });
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// Recording Scanning
///////////////////////////////////////////////////////////////////////////////

// The recording is scanned using the Debugger's instrumentation API. We need to
// accumulate the execution points at which every breakpoint site is hit, and
// use instrumentation both to invoke a callback at those breakpoint site hits
// and to efficiently update the frame depth when no generators/async frames
// or exception unwinds occur. In the latter case we fallback on the Debugger
// API to make sure we maintain the correct frame depth.
//
// The Debugger API can also straightforwardly provide this information,
// by setting EnterFrame/OnPop hooks and breakpoints on all appropriate sites
// in content scripts. Unfortunately, this is extremely slow: setting a single
// breakpoint in a script prevents it from being Ion compiled and causes it to
// run several times slower than the normal baseline code. If the page being
// debugged has much JS, scanning it will be extremely slow compared to the
// normal execution speed, and many replaying processes will be needed to keep
// scan data up to date.

function scanningOnNewGlobal(global) {
  global.setInstrumentation(
    global.makeDebuggeeNativeFunction(
      RecordReplayControl.instrumentationCallback
    ),
    ["main", "entry", "breakpoint", "exit"]
  );

  if (RecordReplayControl.isScanningScripts()) {
    global.setInstrumentationActive(true);
  }
}

// eslint-disable-next-line no-unused-vars
function ScriptResumeFrame(script) {
  // At frame resumption points, sync the frame depth. These won't be hit very
  // often, and handling them is tricky when e.g. catching exceptions thrown by
  // an await, which could be either a resumption or a continuation of an
  // existing frame.
  RecordReplayControl.setFrameDepth(countScriptFrames() - 1);
  RecordReplayControl.onResumeFrame("", script);
}

function startScanningAllScripts() {
  if (RecordReplayControl.isScanningScripts()) {
    return;
  }
  RecordReplayControl.setScanningScripts(true);

  for (const global of gAllGlobals) {
    global.setInstrumentationActive(true);
  }

  // The onExceptionUnwind hook gets called anytime an error needs to be handled
  // for a frame. If there are try/catch or try/finally blocks in the script
  // then the hook might be called multiple times, and the frame might finish
  // normally. To avoid dealing with this complexity we just add an onPop hook
  // to any frame that has had exceptions unwound in it, to make sure the frame
  // index is set correctly when it finally unwinds.
  dbg.onExceptionUnwind = frame => {
    if (considerScript(frame.script)) {
      frame.onPop = () => {
        const script = gScripts.getId(frame.script);
        RecordReplayControl.setFrameDepth(countScriptFrames());
        RecordReplayControl.onExitFrame("", script);
      };
    }
  };
}

function stopScanningAllScripts() {
  if (!RecordReplayControl.isScanningScripts()) {
    return;
  }
  RecordReplayControl.setScanningScripts(false);

  for (const global of gAllGlobals) {
    global.setInstrumentationActive(false);
  }

  dbg.onExceptionUnwind = undefined;
}

///////////////////////////////////////////////////////////////////////////////
// Scanning Queries
///////////////////////////////////////////////////////////////////////////////

function findScriptHits(position, startpoint, endpoint) {
  const { kind, script, offset, frameIndex: bpFrameIndex } = position;
  const hits = [];
  for (let checkpoint = startpoint; checkpoint < endpoint; checkpoint++) {
    const allHits = RecordReplayControl.findScriptHits(
      checkpoint,
      script,
      offset
    );
    for (const { progress, frameIndex } of allHits) {
      switch (kind) {
        case "OnStep":
          if (bpFrameIndex != frameIndex) {
            continue;
          }
        // FALLTHROUGH
        case "Break":
          hits.push({
            checkpoint,
            progress,
            position: { kind: "OnStep", script, offset, frameIndex },
          });
      }
    }
  }
  return hits;
}

function findAllScriptHits(script, frameIndex, offsets, startpoint, endpoint) {
  const allHits = [];
  for (const offset of offsets) {
    const position = {
      kind: "OnStep",
      script,
      offset,
      frameIndex,
    };

    const hits = findScriptHits(position, startpoint, endpoint);
    allHits.push(...hits);
  }
  return allHits;
}

function findChangeFrames(checkpoint, which, kind) {
  const hits = RecordReplayControl.findChangeFrames(checkpoint, which);
  return hits.map(({ script, progress, frameIndex }) => ({
    checkpoint,
    progress,
    position: { kind, script, frameIndex },
  }));
}

function findFrameSteps({ targetPoint, breakpointOffsets }) {
  const {
    checkpoint,
    position: { script: targetScript, frameIndex: targetIndex },
  } = targetPoint;

  const potentialStepsFilter = point => {
    const { frameIndex, script } = point.position;
    return frameIndex == targetIndex && script == targetScript;
  };

  // Find the entry point of the frame whose steps contain |targetPoint|.
  let entryPoint;
  if (targetPoint.position.kind == "EnterFrame") {
    entryPoint = targetPoint;
  } else {
    const entryHits = [
      ...findChangeFrames(checkpoint, 0, "EnterFrame"),
      ...findChangeFrames(checkpoint, 2, "EnterFrame"),
    ].filter(potentialStepsFilter);

    // Find the last frame entry or resume for the frame's script preceding the
    // target point. Since frames do not span checkpoints the hit must be in the
    // range we are searching.
    entryPoint = findClosestPoint(
      entryHits,
      targetPoint,
      /* before */ true,
      /* inclusive */ true
    );
    assert(entryPoint);
  }

  // Find the exit point of the frame.
  const exitHits = findChangeFrames(checkpoint, 1, "OnPop").filter(
    potentialStepsFilter
  );
  const exitPoint = findClosestPoint(
    exitHits,
    targetPoint,
    /* before */ false,
    /* inclusive */ true
  );

  // The steps in the frame are the hits in the script which have the right
  // frame index and happen between the entry and exit points. Any EnterFrame
  // points for immediate callees of the frame are also included.
  const breakpointHits = findAllScriptHits(
    targetScript,
    targetIndex,
    breakpointOffsets,
    checkpoint,
    checkpoint + 1
  );
  const enterFrameHits = findChangeFrames(checkpoint, 0, "EnterFrame").filter(
    point => point.position.frameIndex == targetIndex + 1
  );
  const steps = breakpointHits.concat(enterFrameHits).filter(point => {
    return pointPrecedes(entryPoint, point) && pointPrecedes(point, exitPoint);
  });
  steps.push(entryPoint, exitPoint);

  steps.sort((pointA, pointB) => {
    return pointPrecedes(pointB, pointA);
  });

  return steps;
}

function findParentFrameEntryPoint(point) {
  const hits = findChangeFrames(point.checkpoint, 0, "EnterFrame").filter(p => {
    return p.position.frameIndex == point.position.frameIndex - 1;
  });
  const parentPoint = findClosestPoint(
    hits,
    point,
    /* before */ true,
    /* inclusive */ false
  );
  return { parentPoint };
}

function findEventFrameEntry({ checkpoint, progress }) {
  return findChangeFrames(checkpoint, 0, "EnterFrame").filter(point => {
    return point.progress == progress + 1;
  })[0];
}

///////////////////////////////////////////////////////////////////////////////
// Position Handler State
///////////////////////////////////////////////////////////////////////////////

// Whether EnterFrame positions should be hit.
let gHasEnterFrameHandler = false;

// Handlers we tried to install but couldn't due to a script not existing.
// Breakpoints requested by the middleman --- which are preserved when
// restoring earlier checkpoints --- identify target scripts by their stable ID
// in gScripts. This array holds the breakpoints for scripts whose IDs we know
// but which have not been created yet.
const gPendingPcHandlers = [];

// Script/offset pairs where we have installed a breakpoint handler. We have to
// avoid installing duplicate handlers here because they will both be called.
const gInstalledPcHandlers = [];

// Callbacks to test whether a frame should have an OnPop handler.
const gOnPopFilters = [];

function clearPositionHandlers() {
  dbg.clearAllBreakpoints();
  dbg.onEnterFrame = undefined;
  dbg.onDebuggerStatement = undefined;

  gHasEnterFrameHandler = false;
  gPendingPcHandlers.length = 0;
  gInstalledPcHandlers.length = 0;
  gOnPopFilters.length = 0;
}

function installPendingHandlers() {
  const pending = gPendingPcHandlers.map(position => position);
  gPendingPcHandlers.length = 0;

  pending.forEach(ensurePositionHandler);
}

// The completion state of any frame that is being popped.
let gPopFrameResult = null;

function onPopFrame(completion) {
  gPopFrameResult = completion;
  positionHit({
    kind: "OnPop",
    script: gScripts.getId(this.script),
    frameIndex: countScriptFrames() - 1,
  });
  gPopFrameResult = null;
}

function onEnterFrame(frame) {
  if (considerScript(frame.script)) {
    if (gHasEnterFrameHandler) {
      ensurePositionHandler({
        kind: "OnStep",
        script: gScripts.getId(frame.script),
        frameIndex: countScriptFrames() - 1,
        offset: frame.script.mainOffset,
      });
    }

    gOnPopFilters.forEach(filter => {
      if (filter(frame)) {
        frame.onPop = onPopFrame;
      }
    });
  }
}

function addOnPopFilter(filter) {
  let frame = dbg.getNewestFrame();
  while (frame) {
    if (considerScript(frame.script) && filter(frame)) {
      frame.onPop = onPopFrame;
    }
    frame = frame.older;
  }

  gOnPopFilters.push(filter);
  dbg.onEnterFrame = onEnterFrame;
}

function ensurePositionHandler(position) {
  switch (position.kind) {
    case "Break":
    case "OnStep":
      let debugScript;
      if (position.script) {
        debugScript = gScripts.getObject(position.script);
        if (!debugScript) {
          // The script referred to in this position does not exist yet, so we
          // can't install a handler for it. Add a pending handler so that we
          // can install the handler once the script is created.
          gPendingPcHandlers.push(position);
          return;
        }

        // Make sure the script is delazified and has been instrumented before
        // we try to operate on it, so that we can compute the appropriate offsets
        // to use. Accessing mainOffset here is a hack but ensures the script is
        // not lazy.
        debugScript.mainOffset;
      }

      const match = function({ script, offset }) {
        return script == position.script && offset == position.offset;
      };
      if (gInstalledPcHandlers.some(match)) {
        return;
      }
      gInstalledPcHandlers.push({
        script: position.script,
        offset: position.offset,
      });

      debugScript.setBreakpoint(position.offset, {
        hit(frame) {
          if (position.offset == debugScript.mainOffset) {
            positionHit({
              kind: "EnterFrame",
              script: position.script,
              frameIndex: countScriptFrames() - 1,
            });
          }

          positionHit(
            {
              kind: "OnStep",
              script: position.script,
              offset: position.offset,
              frameIndex: countScriptFrames() - 1,
            },
            frame
          );
        },
      });
      break;
    case "OnPop":
      assert(position.script);
      addOnPopFilter(frame => gScripts.getId(frame.script) == position.script);
      break;
    case "EnterFrame":
      dbg.onEnterFrame = onEnterFrame;
      gHasEnterFrameHandler = true;
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////
// Paused State
///////////////////////////////////////////////////////////////////////////////

let gPausedObjects = new IdMap();
let gDereferencedObjects = new Map();

function getObjectId(obj) {
  const id = gPausedObjects.getId(obj);
  if (!id && obj) {
    assert(
      obj instanceof Debugger.Object || obj instanceof Debugger.Environment
    );

    // There can be multiple Debugger.Objects for the same dereferenced object.
    // gDereferencedObjects is used to make sure the IDs we send to the
    // middleman are canonical and are specific to their referent.
    if (obj instanceof Debugger.Object) {
      if (gDereferencedObjects.has(obj.unsafeDereference())) {
        const canonical = gDereferencedObjects.get(obj.unsafeDereference());
        return gPausedObjects.getId(canonical);
      }
      gDereferencedObjects.set(obj.unsafeDereference(), obj);
    }

    return gPausedObjects.add(obj);
  }
  return id;
}

// Convert a value for sending to the parent.
function convertValue(value) {
  if (value instanceof Debugger.Object) {
    return { object: getObjectId(value) };
  }
  if (
    value === undefined ||
    value == Infinity ||
    value == -Infinity ||
    Object.is(value, NaN) ||
    Object.is(value, -0)
  ) {
    return { special: "" + value };
  }
  return value;
}

function convertCompletionValue(value) {
  if ("return" in value) {
    return { return: convertValue(value.return) };
  }
  if ("throw" in value) {
    return {
      throw: convertValue(value.throw),
      stack: convertSavedFrameToPlainObject(value.stack),
    };
  }
  throwError("Unexpected completion value");
}

// Make sure that SavedFrame objects can be serialized to JSON.
function convertSavedFrameToPlainObject(frame) {
  if (!frame) {
    return null;
  }
  return {
    source: frame.source,
    sourceId: frame.sourceId,
    line: frame.line,
    column: frame.column,
    functionDisplayName: frame.functionDisplayName,
    asyncCause: frame.asyncCause,
    parent: convertSavedFrameToPlainObject(frame.parent),
    asyncParent: convertSavedFrameToPlainObject(frame.asyncParent),
  };
}

// Convert a value we received from the parent.
function convertValueFromParent(value) {
  if (isNonNullObject(value)) {
    if (value.object) {
      return gPausedObjects.getObject(value.object);
    }
    switch (value.special) {
      case "undefined":
        return undefined;
      case "Infinity":
        return Infinity;
      case "-Infinity":
        return -Infinity;
      case "NaN":
        return NaN;
      case "0":
        return -0;
    }
  }
  return value;
}

function makeDebuggeeValue(value) {
  if (isNonNullObject(value)) {
    assert(!(value instanceof Debugger.Object));
    try {
      const global = Cu.getGlobalForObject(value);
      const dbgGlobal = dbg.makeGlobalObjectReference(global);
      return dbgGlobal.makeDebuggeeValue(value);
    } catch (e) {
      // Sometimes the global which Cu.getGlobalForObject finds has
      // isInvisibleToDebugger set. Wrap the object into the first global we
      // found in this case.
      return gFirstGlobal.makeDebuggeeValue(value);
    }
  }
  return value;
}

function makeConvertedDebuggeeValue(value) {
  return convertValue(makeDebuggeeValue(value));
}

function getDebuggeeValue(value) {
  if (value && typeof value == "object") {
    assert(value instanceof Debugger.Object);
    return value.unsafeDereference();
  }
  return value;
}

///////////////////////////////////////////////////////////////////////////////
// Manifest Management
///////////////////////////////////////////////////////////////////////////////

// The manifest that is currently being processed.
let gManifest = { kind: "primordial" };

// When processing certain manifests this tracks the execution time when the
// manifest started executing.
let gManifestStartTime;

// Points of any debugger statements that need to be flushed to the middleman.
const gNewDebuggerStatements = [];

// Whether to pause on debugger statements when running forward.
let gPauseOnDebuggerStatement = false;

function ensureRunToPointPositionHandlers({ endpoint, snapshotPoints }) {
  if (gLastCheckpoint == endpoint.checkpoint) {
    assert(endpoint.position);
    ensurePositionHandler(endpoint.position);
  }
  snapshotPoints.forEach(snapshot => {
    if (gLastCheckpoint == snapshot.checkpoint && snapshot.position) {
      ensurePositionHandler(snapshot.position);
    }
  });
}

// Handlers that run when a manifest is first received. This must be specified
// for all manifests.
const gManifestStartHandlers = {
  resume({ breakpoints, pauseOnDebuggerStatement }) {
    RecordReplayControl.resumeExecution();
    breakpoints.forEach(ensurePositionHandler);

    gPauseOnDebuggerStatement = pauseOnDebuggerStatement;
    dbg.onDebuggerStatement = debuggerStatementHit;
  },

  restoreSnapshot({ numSnapshots }) {
    RecordReplayControl.restoreSnapshot(numSnapshots);
    throwError("Unreachable!");
  },

  runToPoint(manifest) {
    ensureRunToPointPositionHandlers(manifest);
    RecordReplayControl.resumeExecution();
  },

  scanRecording() {
    RecordReplayControl.resumeExecution();
  },

  findHits({ position, startpoint, endpoint }) {
    RecordReplayControl.manifestFinished(
      findScriptHits(position, startpoint, endpoint)
    );
  },

  findFrameSteps(info) {
    RecordReplayControl.manifestFinished(findFrameSteps(info));
  },

  findParentFrameEntryPoint({ point }) {
    RecordReplayControl.manifestFinished(findParentFrameEntryPoint(point));
  },

  findEventFrameEntry(progress) {
    RecordReplayControl.manifestFinished({ rv: findEventFrameEntry(progress) });
  },

  flushRecording() {
    RecordReplayControl.flushRecording();
    RecordReplayControl.manifestFinished();
  },

  setMainChild() {
    const endpoint = RecordReplayControl.setMainChild();
    RecordReplayControl.manifestFinished({ endpoint });
  },

  debuggerRequest({ request }) {
    const response = processRequest(request);
    RecordReplayControl.manifestFinished({
      response,
      divergedFromRecording: gDivergedFromRecording,
    });
  },

  batchDebuggerRequest({ requests }) {
    for (const request of requests) {
      processRequest(request);
    }
    RecordReplayControl.manifestFinished({
      divergedFromRecording: gDivergedFromRecording,
    });
  },

  getPauseData() {
    divergeFromRecording();
    const data = getPauseData();
    RecordReplayControl.manifestFinished(data);
  },

  hitLogpoint({ text, condition, skipPauseData }) {
    divergeFromRecording();

    const frame = scriptFrameForIndex(countScriptFrames() - 1);
    if (condition) {
      const crv = frame.eval(condition);
      if ("return" in crv && !crv.return) {
        RecordReplayControl.manifestFinished({ result: null });
        return;
      }
    }

    const displayName = formatDisplayName(frame);
    const rv = frame.evalWithBindings(`[${text}]`, { displayName });

    const pauseData = skipPauseData ? undefined : getPauseData();

    let result;
    if (rv.return) {
      result = getDebuggeeValue(rv.return);
    } else {
      result = [getDebuggeeValue(rv.throw)];
    }
    result = result.map(v => makeConvertedDebuggeeValue(v));

    const resultData = new PreviewedObjects();
    result.forEach(v => resultData.addValue(v, PropertyLevels.FULL));

    RecordReplayControl.manifestFinished({ result, resultData, pauseData });
  },
};

// eslint-disable-next-line no-unused-vars
function ManifestStart(manifest) {
  try {
    gManifest = manifest;
    gManifestStartTime = RecordReplayControl.currentExecutionTime();

    if (gManifestStartHandlers[manifest.kind]) {
      gManifestStartHandlers[manifest.kind](manifest);
    } else {
      dump(`Unknown manifest: ${JSON.stringify(manifest)}\n`);
    }
  } catch (e) {
    const msg = printError("ManifestStart", e);
    RecordReplayControl.manifestFinished({
      exception: `ManifestStart failed: ${msg}`,
    });
  }
}

const FirstCheckpointId = 1;

// The most recent encountered checkpoint.
let gLastCheckpoint;

function currentExecutionPoint(position) {
  const checkpoint = gLastCheckpoint;
  const progress = RecordReplayControl.progressCounter();
  return { checkpoint, progress, position };
}

function currentScriptedExecutionPoint() {
  const numFrames = countScriptFrames();
  if (!numFrames) {
    return null;
  }

  const index = numFrames - 1;
  const frame = scriptFrameForIndex(index);
  return currentExecutionPoint({
    kind: "OnStep",
    script: gScripts.getId(frame.script),
    offset: frame.offset,
    frameIndex: index,
  });
}

function finishResume(point) {
  RecordReplayControl.manifestFinished({
    point,
    duration: RecordReplayControl.currentExecutionTime() - gManifestStartTime,
    consoleMessages: gNewConsoleMessages,
    scripts: gNewScripts,
    debuggerStatements: gNewDebuggerStatements,
    events: gNewEvents,
  });
  gNewConsoleMessages.length = 0;
  gNewScripts.length = 0;
  gNewDebuggerStatements.length = 0;
  gNewEvents.length = 0;
}

// Handlers that run after a checkpoint is reached to see if the manifest has
// finished. This does not need to be specified for all manifests.
const gManifestFinishedAfterCheckpointHandlers = {
  primordial(_, point) {
    // The primordial manifest runs forward to the first checkpoint, saves it,
    // and then finishes.
    assert(point.checkpoint == FirstCheckpointId);
    if (!newSnapshot(point)) {
      return;
    }
    RecordReplayControl.manifestFinished({ point });
  },

  resume(_, point) {
    clearPositionHandlers();
    finishResume(point);
  },

  runToPoint({ endpoint, snapshotPoints }, point) {
    assert(endpoint.checkpoint >= point.checkpoint);
    if (pointArrayIncludes(snapshotPoints, point) && !newSnapshot(point)) {
      return;
    }
    if (!endpoint.position && point.checkpoint == endpoint.checkpoint) {
      RecordReplayControl.manifestFinished({ point });
    }
  },

  scanRecording({ endpoint, snapshotPoints }, point) {
    stopScanningAllScripts();
    if (pointArrayIncludes(snapshotPoints, point) && !newSnapshot(point)) {
      return;
    }
    if (point.checkpoint == endpoint.checkpoint) {
      const duration =
        RecordReplayControl.currentExecutionTime() - gManifestStartTime;
      RecordReplayControl.manifestFinished({
        point,
        duration,
        memoryUsage: getMemoryUsage(),
      });
    }
  },
};

// Handlers that run after a checkpoint is reached and before execution resumes.
// This does not need to be specified for all manifests. This is specified
// separately from gManifestFinishedAfterCheckpointHandlers to ensure that if
// we finish a manifest after the checkpoint and then start a new one, that new
// one is able to prepare to execute. These handlers must therefore not finish
// the current manifest.
const gManifestPrepareAfterCheckpointHandlers = {
  runToPoint: ensureRunToPointPositionHandlers,

  scanRecording({ endpoint }) {
    assert(!endpoint.position);
    startScanningAllScripts();
  },
};

function processManifestAfterCheckpoint(point, restoredSnapshot) {
  if (gManifestFinishedAfterCheckpointHandlers[gManifest.kind]) {
    gManifestFinishedAfterCheckpointHandlers[gManifest.kind](gManifest, point);
  }

  if (gManifestPrepareAfterCheckpointHandlers[gManifest.kind]) {
    gManifestPrepareAfterCheckpointHandlers[gManifest.kind](gManifest, point);
  }
}

// eslint-disable-next-line no-unused-vars
function HitCheckpoint(id) {
  gLastCheckpoint = id;
  const point = currentExecutionPoint();

  // Reset paused state at each checkpoint. In order to reach the checkpoint we
  // must have unpaused, and resetting the state allows these objects to be
  // collected by the GC.
  gPausedObjects = new IdMap();
  gDereferencedObjects = new Map();

  try {
    processManifestAfterCheckpoint(point);
  } catch (e) {
    const msg = printError("AfterCheckpoint", e);
    RecordReplayControl.manifestFinished({
      exception: `AfterCheckpoint failed: ${msg}`,
    });
  }
}

// Handlers that run after reaching a position watched by ensurePositionHandler.
// This must be specified for any manifest that uses ensurePositionHandler.
const gManifestPositionHandlers = {
  resume(manifest, point) {
    clearPositionHandlers();
    finishResume(point);
  },

  runToPoint({ endpoint, snapshotPoints }, point) {
    if (pointArrayIncludes(snapshotPoints, point)) {
      clearPositionHandlers();
      if (newSnapshot(point)) {
        ensureRunToPointPositionHandlers({ endpoint, snapshotPoints });
      }
    }
    if (pointEquals(point, endpoint)) {
      clearPositionHandlers();
      RecordReplayControl.manifestFinished({ point });
    }
  },
};

function positionHit(position, frame) {
  const point = currentExecutionPoint(position);

  if (gManifestPositionHandlers[gManifest.kind]) {
    gManifestPositionHandlers[gManifest.kind](gManifest, point);
  } else {
    throwError(`Unexpected manifest in positionHit: ${gManifest.kind}`);
  }
}

function debuggerStatementHit() {
  assert(gManifest.kind == "resume");
  const point = currentScriptedExecutionPoint();
  gNewDebuggerStatements.push(point);

  if (gPauseOnDebuggerStatement) {
    clearPositionHandlers();
    finishResume(point);
  }
}

function newSnapshot(point) {
  if (RecordReplayControl.newSnapshot()) {
    return true;
  }

  // After rewinding gManifest won't be correct, so we always mark the current
  // manifest as finished and rely on the middleman to give us a new one.
  RecordReplayControl.manifestFinished({ restoredSnapshot: true, point });

  return false;
}

///////////////////////////////////////////////////////////////////////////////
// Handler Helpers
///////////////////////////////////////////////////////////////////////////////

function getScriptData(id) {
  const script = gScripts.getObject(id);
  return {
    id,
    sourceId: gScriptSources.getId(script.source),
    startLine: script.startLine,
    lineCount: script.lineCount,
    sourceStart: script.sourceStart,
    sourceLength: script.sourceLength,
    displayName: script.displayName,
    url: script.url,
    format: script.format,
    mainOffset: script.mainOffset,
  };
}

function getSourceData(id) {
  const source = gScriptSources.getObject(id);
  const introductionScript = gScripts.getId(source.introductionScript);
  return {
    id: id,
    text: source.text,
    url: source.url,
    displayURL: source.displayURL,
    elementAttributeName: source.elementAttributeName,
    introductionScript,
    introductionOffset: introductionScript
      ? source.introductionOffset
      : undefined,
    introductionType: source.introductionType,
    sourceMapURL: source.sourceMapURL,
  };
}

function forwardToScript(name) {
  return request => gScripts.getObject(request.id)[name](request.value);
}

function getFrameData(index) {
  const frame = scriptFrameForIndex(index);

  let _arguments = null;
  if (frame.arguments) {
    _arguments = [];
    for (let i = 0; i < frame.arguments.length; i++) {
      _arguments.push(convertValue(frame.arguments[i]));
    }
  }

  const script = gScripts.getId(frame.script);
  return {
    index,
    type: frame.type,
    callee: getObjectId(frame.callee),
    environment: getObjectId(frame.environment),
    generator: frame.generator,
    constructing: frame.constructing,
    this: convertValue(frame.this),
    script,
    offset: frame.offset,
    arguments: _arguments,
  };
}

function unknownObjectProperties(why) {
  return [
    {
      name: "Unknown properties",
      desc: {
        value: why,
        enumerable: true,
      },
    },
  ];
}

function getObjectData(id) {
  const object = gPausedObjects.getObject(id);
  if (object instanceof Debugger.Object) {
    const rv = {
      id,
      kind: "Object",
      callable: object.callable,
      isBoundFunction: object.isBoundFunction,
      isArrowFunction: object.isArrowFunction,
      isGeneratorFunction: object.isGeneratorFunction,
      isAsyncFunction: object.isAsyncFunction,
      proto: getObjectId(object.proto),
      class: object.class,
      name: object.name,
      displayName: object.displayName,
      parameterNames: object.parameterNames,
      script: gScripts.getId(object.script),
      environment: getObjectId(object.environment),
      isProxy: object.isProxy,
      isExtensible: object.isExtensible(),
      isSealed: object.isSealed(),
      isFrozen: object.isFrozen(),
    };
    if (rv.isBoundFunction) {
      rv.boundTargetFunction = getObjectId(object.boundTargetFunction);
      rv.boundThis = convertValue(object.boundThis);
      rv.boundArguments = getObjectId(makeDebuggeeValue(object.boundArguments));
    }
    if (rv.isProxy) {
      rv.proxyUnwrapped = convertValue(object.unwrap());
      rv.proxyTarget = convertValue(object.proxyTarget);
      rv.proxyHandler = convertValue(object.proxyHandler);
    }
    try {
      if (object.errorMessageName) {
        rv.errorMessageName = object.errorMessageName;
      }
      if (object.errorNotes) {
        rv.errorNotes = object.errorNotes;
      }
      if (object.errorLineNumber) {
        rv.errorLineNumber = object.errorLineNumber;
      }
      if (object.errorColumnNumber) {
        rv.errorColumnNumber = object.errorColumnNumber;
      }
    } catch (e) {
      // Error getters can throw access denied errors.
    }
    if (CSSRule.isInstance(object.unsafeDereference())) {
      rv.isInstance = "CSSRule";
    } else if (Event.isInstance(object.unsafeDereference())) {
      rv.isInstance = "Event";
    }
    return rv;
  }
  if (object instanceof Debugger.Environment) {
    return {
      id,
      kind: "Environment",
      type: object.type,
      parent: getObjectId(object.parent),
      object: object.type == "declarative" ? 0 : getObjectId(object.object),
      callee: getObjectId(object.callee),
      optimizedOut: object.optimizedOut,
    };
  }
  throwError(`Unknown object kind: ${object}`);
}

// Return whether to avoid operating on an object due to the likelihood of a
// recording divergence or other bad behavior.
function isBlacklisted(object) {
  // Enumerate a Storage object's properties requires the content process to
  // synchronously communicate with the UI process, which it can't do.
  return object.class == "Storage";
}

function getObjectProperties(object) {
  const rv = Object.create(null);

  if (isBlacklisted(object)) {
    return rv;
  }

  let names;
  try {
    names = object.getOwnPropertyNames();
  } catch (e) {
    return unknownObjectProperties(e.toString());
  }

  names.forEach(name => {
    // Workaround this test-only getter not reporting exceptions properly.
    if (name == "SpecialPowers_wrappedObject") {
      return;
    }

    let desc;
    try {
      desc = object.getOwnPropertyDescriptor(name);
      if (!desc) {
        desc = {
          name,
          desc: {
            value: `Unexpected missing property ${name}`,
            enumerable: true,
          },
        };
      }
    } catch (e) {
      desc = { name, desc: { value: "Unknown: " + e, enumerable: true } };
    }
    if ("value" in desc) {
      desc.value = convertValue(desc.value);
    }
    if ("get" in desc) {
      desc.get = getObjectId(desc.get);
    }
    if ("set" in desc) {
      desc.set = getObjectId(desc.set);
    }
    rv[name] = desc;
  });
  return rv;
}

function getObjectContainerContents(object) {
  const raw = object.unsafeDereference();
  switch (object.class) {
    case "Set": {
      const iter = Cu.waiveXrays(Set.prototype.values.call(raw));
      return [...iter].map(v => makeConvertedDebuggeeValue(v));
    }
    case "Map": {
      const iter = Cu.waiveXrays(Map.prototype.entries.call(raw));
      return [...iter].map(([k, v]) => [
        makeConvertedDebuggeeValue(k),
        makeConvertedDebuggeeValue(v),
      ]);
    }
    case "WeakSet": {
      const keys = ChromeUtils.nondeterministicGetWeakSetKeys(raw);
      return keys.map(k => makeConvertedDebuggeeValue(Cu.waiveXrays(k)));
    }
    case "WeakMap": {
      const keys = ChromeUtils.nondeterministicGetWeakMapKeys(raw);
      return keys.map(k => [
        makeConvertedDebuggeeValue(k),
        makeConvertedDebuggeeValue(WeakMap.prototype.get.call(raw, k)),
      ]);
    }
    default:
      return null;
  }
}

function getEnvironmentNames(env) {
  try {
    const names = env.names();

    return names.map(name => {
      return { name, value: convertValue(env.getVariable(name)) };
    });
  } catch (e) {
    return [
      {
        name: "Unknown names",
        value: "Exception thrown in getEnvironmentNames",
      },
    ];
  }
}

function getWindow() {
  // Hopefully there is exactly one window in this enumerator.
  for (const window of Services.ww.getWindowEnumerator()) {
    return window;
  }
  return null;
}

// Maximum number of properties the server is interested in when previewing an
// object.
const OBJECT_PREVIEW_MAX_ITEMS = 10;

// Levels at which property information can be included in previews.
// If not specified, minimal properties are included.
const PropertyLevels = {
  BASIC: 1, // Include enough properties to show an inline preview.
  FULL: 2, // Include enough properties to allow the object to be expanded.
};

// A collection of objects which we can send up to the server, along with
// property information so that the server can show a preview for the object.
function PreviewedObjects() {
  this.objects = {};
  this.environments = {};
}

PreviewedObjects.prototype = {
  addValue(value, level) {
    if (value && typeof value == "object" && value.object) {
      this.addObject(value.object, level);
    }
  },

  // eslint-disable-next-line complexity
  addObject(id, level) {
    if (!id) {
      return;
    }

    const object = gPausedObjects.getObject(id);
    assert(object instanceof Debugger.Object);

    if (!this.objects[id]) {
      let ownPropertyNamesCount = 0;
      try {
        ownPropertyNamesCount = object.getOwnPropertyNames().length;
      } catch (e) {}

      this.objects[id] = {
        data: getObjectData(id),
        preview: { ownPropertyNamesCount, level },
      };
    } else {
      const preview = this.objects[id].preview;
      if ((preview.level | 0) >= (level | 0)) {
        return;
      }
      preview.level = level;
    }

    const { data, preview } = this.objects[id];

    // If this is a DOM object identified with isInstance, the previewer might
    // need additional properties.
    if (level == PropertyLevels.BASIC && data.isInstance) {
      preview.level = level = PropertyLevels.FULL;
    }

    // Add intrinsic properties that are always included.
    switch (object.class) {
      case "Array":
      case "Uint8Array":
      case "Uint8ClampedArray":
      case "Uint16Array":
      case "Uint32Array":
      case "Int8Array":
      case "Int16Array":
      case "Int32Array":
      case "Float32Array":
      case "Float64Array":
        this.addObjectPropertyValue(object, "length");
        break;
      case "Function":
        this.addObjectPropertyValue(object, "displayName");
        break;
    }

    if (!level) {
      return;
    }

    const properties = Object.entries(getObjectProperties(object));

    // For an inline preview the server is only interested in enumerable
    // properties, and at most OBJECT_PREVIEW_MAX_ITEMS of them. Limiting the
    // properties we send to only those the server needs avoids having to send
    // the contents of huge objects like Windows, most of which will not be
    // used. When doing a full property enumeration, include all properties.
    let enumerablePropertyCount = 0;
    for (const [name, desc] of properties) {
      if (level == PropertyLevels.FULL || desc.enumerable) {
        this.addObjectProperty(object, name, desc);
        if (level == PropertyLevels.BASIC) {
          if (++enumerablePropertyCount == OBJECT_PREVIEW_MAX_ITEMS) {
            break;
          }
        }
      }
    }

    // The server is interested in at most OBJECT_PREVIEW_MAX_ITEMS items in
    // set and map containers.
    let containerContents = getObjectContainerContents(object);
    if (containerContents) {
      if (level == PropertyLevels.BASIC) {
        containerContents = containerContents.slice(
          0,
          OBJECT_PREVIEW_MAX_ITEMS
        );
      }
      preview.containerContents = containerContents;
      preview.containerContents.forEach(v => this.addContainerValue(v));
    }

    switch (object.class) {
      case "RegExp":
        this.addObjectCall(object, "toString");
        break;
      case "Date":
        this.addObjectCall(object, "getTime");
        break;
      case "Set":
      case "Map":
        this.addObjectPropertyValue(object, "size");
        break;
      case "Error":
      case "EvalError":
      case "RangeError":
      case "ReferenceError":
      case "SyntaxError":
      case "TypeError":
      case "URIError":
        this.addObjectPropertyValue(object, "name");
        this.addObjectPropertyValue(object, "message");
        this.addObjectPropertyValue(object, "stack");
        this.addObjectPropertyValue(object, "fileName");
        this.addObjectPropertyValue(object, "lineNumber");
        this.addObjectPropertyValue(object, "columnNumber");
        break;
    }

    // Search the prototype chain for getter properties and fill in their values
    // if we are getting all properties of the object.
    if (level == PropertyLevels.FULL) {
      let { proto } = object;
      while (proto) {
        let names = [];
        try {
          names = proto.getOwnPropertyNames();
        } catch (e) {}

        for (const name of names) {
          let desc = null;
          try {
            desc = proto.getOwnPropertyDescriptor(name);
          } catch (e) {}

          if (desc && desc.get) {
            this.addObjectPropertyValue(object, name);
          }
        }

        proto = proto.proto;
      }
    }
  },

  addObjectPropertyValue(object, name) {
    try {
      const value = makeConvertedDebuggeeValue(
        object.unsafeDereference()[name]
      );

      this.addObjectProperty(object, name, { value, enumerable: true });
    } catch (e) {}
  },

  addObjectProperty(object, name, desc) {
    const id = gPausedObjects.getId(object);
    const preview = this.objects[id].preview;

    if (!preview.properties) {
      preview.properties = Object.create(null);
    }
    if (name in preview.properties) {
      return;
    }

    this.addPropertyDescriptor(desc);
    preview.properties[name] = desc;
  },

  addObjectCall(object, name) {
    const id = gPausedObjects.getId(object);
    const preview = this.objects[id].preview;

    if (!preview.callResults) {
      preview.callResults = Object.create(null);
    }
    if (name in preview.callResults) {
      return;
    }

    try {
      const value = makeConvertedDebuggeeValue(
        object.unsafeDereference()[name]()
      );

      this.addValue(value);
      preview.callResults[name] = value;
    } catch (e) {}
  },

  addPropertyDescriptor(desc, level) {
    if (desc.value) {
      this.addValue(desc.value, level);
    }
    if (desc.get) {
      this.addObject(desc.get, level);
    }
    if (desc.set) {
      this.addObject(desc.set, level);
    }
  },

  addContainerValue(value) {
    // Watch for [key, value] pairs in maps.
    if (value.length == 2) {
      value.forEach(v => this.addValue(v));
    } else {
      this.addValue(value);
    }
  },

  addEnvironment(id) {
    if (!id || this.environments[id]) {
      return;
    }

    const env = gPausedObjects.getObject(id);
    assert(env instanceof Debugger.Environment);

    const data = getObjectData(id);
    const names = getEnvironmentNames(env);
    this.environments[id] = { data, names };

    names.forEach(({ value }) => this.addValue(value, PropertyLevels.BASIC));

    if (data.type != "declarative") {
      this.addObject(data.object, PropertyLevels.BASIC);
    }

    this.addObject(data.callee);
    this.addEnvironment(data.parent);
  },

  addCompletionValue(v) {
    if (v.return) {
      this.addValue(v.return);
    } else if (v.throw) {
      this.addValue(v.throw);
    }
  },
};

// When the replaying process pauses, the server needs to inspect a lot of state
// around frames, objects, etc. in order to fill in all the information the
// client needs to update the UI for the pause location. Done naively, this
// inspection requires a lot of back and forth with the replaying process to
// get all this data. This is bad for performance, and especially so if the
// replaying process is on a different machine from the server. Instead, the
// debugger running in the server can request a pause data packet which includes
// everything the server will need.
//
// This should avoid overapproximation, so that we can quickly send pause data
// across a network connection, and especially should not underapproximate
// as the server will end up needing to make more requests before the client can
// finish pausing.
function getPauseData() {
  const paintData = RecordReplayControl.repaint();

  const numFrames = countScriptFrames();
  if (!numFrames) {
    return { paintData };
  }

  const rv = new PreviewedObjects();

  rv.paintData = paintData;
  rv.frames = [];
  rv.scripts = {};
  rv.offsetMetadata = [];

  // eslint-disable-next-line no-shadow
  function addScript(id) {
    if (!rv.scripts[id]) {
      rv.scripts[id] = getScriptData(id);
    }
  }

  for (let i = 0; i < numFrames; i++) {
    const dbgFrame = scriptFrameForIndex(i);
    const frame = getFrameData(i);
    const script = gScripts.getObject(frame.script);
    rv.frames.push(frame);
    rv.offsetMetadata.push({
      scriptId: frame.script,
      offset: frame.offset,
      metadata: script.getOffsetMetadata(dbgFrame.offset),
    });
    addScript(frame.script);
    rv.addValue(frame.this, PropertyLevels.BASIC);
    if (frame.arguments) {
      for (const arg of frame.arguments) {
        rv.addValue(arg, PropertyLevels.BASIC);
      }
    }
    rv.addObject(frame.callee, PropertyLevels.NONE);
    rv.addEnvironment(frame.environment, PropertyLevels.BASIC);
  }

  if (gPopFrameResult) {
    rv.popFrameResult = convertCompletionValue(gPopFrameResult);
    rv.addCompletionValue(gPopFrameResult);
  }

  return rv;
}

function formatDisplayName(frame) {
  if (frame.type === "call") {
    const callee = frame.callee;
    return callee.name || callee.userDisplayName || callee.displayName;
  }

  return `(${frame.type})`;
}

///////////////////////////////////////////////////////////////////////////////
// Handlers
///////////////////////////////////////////////////////////////////////////////

let gDivergedFromRecording = false;

function divergeFromRecording() {
  RecordReplayControl.divergeFromRecording();

  // This flag can only be unset when we rewind.
  gDivergedFromRecording = true;
}

const gRequestHandlers = {
  repaint() {
    divergeFromRecording();
    return RecordReplayControl.repaint();
  },

  /////////////////////////////////////////////////////////
  // Debugger Requests
  /////////////////////////////////////////////////////////

  findScripts(request) {
    const query = Object.assign({}, request.query);
    if ("global" in query) {
      query.global = gPausedObjects.getObject(query.global);
    }
    if ("source" in query) {
      query.source = gScriptSources.getObject(query.source);
      if (!query.source) {
        return [];
      }
    }
    const scripts = dbg.findScripts(query);
    const rv = [];
    scripts.forEach(script => {
      if (considerScript(script)) {
        rv.push(getScriptData(gScripts.getId(script)));
      }
    });
    return rv;
  },

  getScript(request) {
    return getScriptData(request.id);
  },

  getContent(request) {
    if (gHtmlContent.has(request.url)) {
      return gHtmlContent.get(request.url);
    }
    return RecordReplayControl.getContent(request.url);
  },

  findSources(request) {
    const sources = [];
    gScriptSources.forEach(id => {
      sources.push(getSourceData(id));
    });
    return sources;
  },

  getSource(request) {
    return getSourceData(request.id);
  },

  getObject(request) {
    return getObjectData(request.id);
  },

  getObjectProperties(request) {
    divergeFromRecording();
    const object = gPausedObjects.getObject(request.id);
    return { properties: getObjectProperties(object) };
  },

  getObjectContainerContents(request) {
    const object = gPausedObjects.getObject(request.id);
    return getObjectContainerContents(object);
  },

  objectApply(request) {
    divergeFromRecording();
    const obj = gPausedObjects.getObject(request.id);
    const thisv = convertValueFromParent(request.thisv);

    if (thisv instanceof Debugger.Object && isBlacklisted(thisv)) {
      return { return: "Can't call method on blacklisted object" };
    }

    const args = request.args.map(v => convertValueFromParent(v));
    const rv = obj.apply(thisv, args);
    return convertCompletionValue(rv);
  },

  getEnvironmentNames(request) {
    divergeFromRecording();
    const env = gPausedObjects.getObject(request.id);
    return getEnvironmentNames(env);
  },

  getFrame(request) {
    if (request.index == -1 /* NewestFrameIndex */) {
      const numFrames = countScriptFrames();

      if (!numFrames) {
        // Return an empty object when there are no frames.
        return {};
      }
      request.index = numFrames - 1;
    }

    return getFrameData(request.index);
  },

  pauseData(request) {
    divergeFromRecording();
    return getPauseData();
  },

  getLineOffsets: forwardToScript("getLineOffsets"),
  getOffsetLocation: forwardToScript("getOffsetLocation"),
  getSuccessorOffsets: forwardToScript("getSuccessorOffsets"),
  getPredecessorOffsets: forwardToScript("getPredecessorOffsets"),
  getAllColumnOffsets: forwardToScript("getAllColumnOffsets"),
  getOffsetMetadata: forwardToScript("getOffsetMetadata"),
  getPossibleBreakpoints: forwardToScript("getPossibleBreakpoints"),
  getPossibleBreakpointOffsets: forwardToScript("getPossibleBreakpointOffsets"),

  frameStepsInfo(request) {
    const script = gScripts.getObject(request.script);
    return {
      breakpointOffsets: script.getPossibleBreakpointOffsets(),
    };
  },

  frameEvaluate(request) {
    divergeFromRecording();
    const frame = scriptFrameForIndex(request.index);
    const rv = frame.eval(request.text, request.options);
    return convertCompletionValue(rv);
  },

  findConsoleMessages(request) {
    return gConsoleMessages;
  },

  /////////////////////////////////////////////////////////
  // Inspector Requests
  /////////////////////////////////////////////////////////

  getFixedObjects(request) {
    divergeFromRecording();
    const window = getWindow();
    return {
      window: getObjectId(makeDebuggeeValue(window)),
      document: getObjectId(makeDebuggeeValue(window.document)),
      Services: getObjectId(makeDebuggeeValue(Services)),
      InspectorUtils: getObjectId(makeDebuggeeValue(InspectorUtils)),
    };
  },

  newDeepTreeWalker(request) {
    divergeFromRecording();
    const walker = Cc[
      "@mozilla.org/inspector/deep-tree-walker;1"
    ].createInstance(Ci.inIDeepTreeWalker);
    return { id: getObjectId(makeDebuggeeValue(walker)) };
  },

  getObjectPropertyValue(request) {
    divergeFromRecording();
    const object = gPausedObjects.getObject(request.id);

    if (isBlacklisted(object)) {
      return { return: "Can't get blacklisted object property" };
    }

    try {
      const rv = object.unsafeDereference()[request.name];
      return { return: makeConvertedDebuggeeValue(rv) };
    } catch (e) {
      return { throw: "" + e };
    }
  },

  setObjectPropertyValue(request) {
    divergeFromRecording();
    const object = gPausedObjects.getObject(request.id);
    const value = getDebuggeeValue(convertValueFromParent(request.value));

    try {
      object.unsafeDereference()[request.name] = value;
      return { return: request.value };
    } catch (e) {
      return { throw: "" + e };
    }
  },

  createObject(request) {
    const global = dbg.getDebuggees()[0];
    const value = global.executeInGlobal("({})");
    return { id: getObjectId(value.return) };
  },

  findEventTarget(request) {
    const element = getWindow().document.elementFromPoint(
      request.clientX,
      request.clientY
    );
    if (!element) {
      return { id: 0 };
    }
    const obj = makeDebuggeeValue(element);
    return { id: getObjectId(obj) };
  },
};

function processRequest(request) {
  if (gRequestHandlers[request.type]) {
    return gRequestHandlers[request.type](request);
  }
  throwError(`"No handler for ${request.type}`);
}

function printError(why, e) {
  let msg;
  try {
    msg = "" + e + " line " + e.lineNumber;
  } catch (ee) {
    msg = "Unknown";
  }
  dump(`Record/Replay Error: ${why}: ${msg}\n`);
  return msg;
}

// eslint-disable-next-line no-unused-vars
var EXPORTED_SYMBOLS = [
  "ManifestStart",
  "HitCheckpoint",
  "NewTimeWarpTarget",
  "ScriptResumeFrame",
];
