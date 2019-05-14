/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy */

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
// The RecordReplayControl.maybeDivergeFromRecording function should be used at
// any point where such interactions might occur.
// eslint-disable spaced-comment

"use strict";

const CC = Components.Constructor;

// Create a sandbox with the resources we need. require() doesn't work here.
const sandbox = Cu.Sandbox(CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")(), {
  wantGlobalProperties: [
    "InspectorUtils",
    "CSSRule",
  ],
});
Cu.evalInSandbox(
  "Components.utils.import('resource://gre/modules/jsdebugger.jsm');" +
  "Components.utils.import('resource://gre/modules/Services.jsm');" +
  "addDebuggerToGlobal(this);",
  sandbox
);
const {
  Debugger,
  RecordReplayControl,
  Services,
  InspectorUtils,
  CSSRule,
} = sandbox;

const dbg = new Debugger();
const firstGlobal = dbg.makeGlobalObjectReference(sandbox);

// We are interested in debugging all globals in the process.
dbg.onNewGlobalObject = function(global) {
  try {
    dbg.addDebuggee(global);
  } catch (e) {
    // Ignore errors related to adding a same-compartment debuggee.
    // See bug 1523755.
    if (!/debugger and debuggee must be in different compartments/.test("" + e)) {
      throw e;
    }
  }
};

///////////////////////////////////////////////////////////////////////////////
// Utilities
///////////////////////////////////////////////////////////////////////////////

const dump = RecordReplayControl.dump;

function assert(v) {
  if (!v) {
    dump("Assertion Failed: " + (new Error()).stack + "\n");
    throw new Error("Assertion Failed!");
  }
}

// Bidirectional map between objects and IDs.
function IdMap() {
  this._idToObject = [ undefined ];
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
    return (id === undefined) ? 0 : id;
  },

  getObject(id) {
    return this._idToObject[id];
  },

  forEach(callback) {
    for (let i = 1; i < this._idToObject.length; i++) {
      callback(i, this._idToObject[i]);
    }
  },

  lastId() {
    return this._idToObject.length - 1;
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

function addScript(script) {
  gScripts.add(script);
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

dbg.onNewScript = function(script) {
  if (RecordReplayControl.areThreadEventsDisallowed()) {
    // This script is part of an eval on behalf of the debugger.
    return;
  }

  if (!considerScript(script)) {
    return;
  }

  addScript(script);
  addScriptSource(script.source);

  // Each onNewScript call advances the progress counter, to preserve the
  // ProgressCounter invariant when onNewScript is called multiple times
  // without executing any scripts.
  RecordReplayControl.advanceProgressCounter();

  hitGlobalHandler("NewScript");

  // Check in case any handlers we need to install are on the scripts just
  // created.
  installPendingHandlers();
};

///////////////////////////////////////////////////////////////////////////////
// Object Snapshots
///////////////////////////////////////////////////////////////////////////////

// Snapshots are generated for objects that might be inspected at times when we
// are not paused at the point where the snapshot was originally taken. The
// snapshot data is provided to the server, which can use it to provide limited
// answers to the client about the object's contents, without having to consult
// a child process.

function snapshotObjectProperty([ name, desc ]) {
  // Only capture primitive properties in object snapshots.
  if ("value" in desc && !convertedValueIsObject(desc.value)) {
    return { name, desc };
  }
  return { name, desc: { value: "<unavailable>" } };
}

function makeObjectSnapshot(object) {
  assert(object instanceof Debugger.Object);

  // Include properties that would be included in a normal object's data packet,
  // except do not allow inspection of any other referenced objects.
  // In particular, don't set the prototype so that the object inspector will
  // not attempt to crawl the object's prototype chain.
  return {
    kind: "Object",
    callable: object.callable,
    isBoundFunction: object.isBoundFunction,
    isArrowFunction: object.isArrowFunction,
    isGeneratorFunction: object.isGeneratorFunction,
    isAsyncFunction: object.isAsyncFunction,
    class: object.class,
    name: object.name,
    displayName: object.displayName,
    parameterNames: object.parameterNames,
    isProxy: object.isProxy,
    isExtensible: object.isExtensible(),
    isSealed: object.isSealed(),
    isFrozen: object.isFrozen(),
    properties: Object.entries(getObjectProperties(object)).map(snapshotObjectProperty),
  };
}

///////////////////////////////////////////////////////////////////////////////
// Console Message State
///////////////////////////////////////////////////////////////////////////////

const gConsoleMessages = [];

function newConsoleMessage(messageType, executionPoint, contents) {
  // Each new console message advances the progress counter, to make sure
  // that different messages have different progress values.
  RecordReplayControl.advanceProgressCounter();

  if (!executionPoint) {
    executionPoint =
      RecordReplayControl.currentExecutionPoint({ kind: "ConsoleMessage" });
  }

  contents.messageType = messageType;
  contents.executionPoint = executionPoint;
  gConsoleMessages.push(contents);

  hitGlobalHandler("ConsoleMessage");
}

function convertStack(stack) {
  if (stack) {
    const { source, line, column, functionDisplayName } = stack;
    const parent = convertStack(stack.parent);
    return { source, line, column, functionDisplayName, parent };
  }
  return null;
}

// Listen to all console messages in the process.
Services.console.registerListener({
  QueryInterface: ChromeUtils.generateQI([Ci.nsIConsoleListener]),

  observe(message) {
    if (message instanceof Ci.nsIScriptError) {
      // If there is a warp target associated with the execution point, use
      // that. This will take users to the point where the error was originally
      // generated, rather than where it was reported to the console.
      let executionPoint;
      if (message.timeWarpTarget) {
        executionPoint =
          RecordReplayControl.timeWarpTargetExecutionPoint(message.timeWarpTarget);
      }

      const contents = JSON.parse(JSON.stringify(message));
      contents.stack = convertStack(message.stack);
      newConsoleMessage("PageError", executionPoint, contents);
    }
  },
});

// Listen to all console API messages in the process.
Services.obs.addObserver({
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),

  observe(message, topic, data) {
    const apiMessage = message.wrappedJSObject;

    const contents = {};
    for (const id in apiMessage) {
      if (id != "wrappedJSObject" && id != "arguments") {
        contents[id] = JSON.parse(JSON.stringify(apiMessage[id]));
      }
    }

    // Message arguments are preserved as debuggee values.
    if (apiMessage.arguments) {
      contents.arguments = apiMessage.arguments.map(v => {
        return convertValue(makeDebuggeeValue(v), { snapshot: true });
      });
    }

    newConsoleMessage("ConsoleAPI", null, contents);
  },
}, "console-api-log-event");

///////////////////////////////////////////////////////////////////////////////
// Position Handler State
///////////////////////////////////////////////////////////////////////////////

// Position kinds we are expected to hit.
let gPositionHandlerKinds = Object.create(null);

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

// eslint-disable-next-line no-unused-vars
function ClearPositionHandlers() {
  dbg.clearAllBreakpoints();
  dbg.onEnterFrame = undefined;

  gPositionHandlerKinds = Object.create(null);
  gPendingPcHandlers.length = 0;
  gInstalledPcHandlers.length = 0;
  gOnPopFilters.length = 0;
}

function installPendingHandlers() {
  const pending = gPendingPcHandlers.map(position => position);
  gPendingPcHandlers.length = 0;

  pending.forEach(EnsurePositionHandler);
}

// Hit a position with the specified kind if we are expected to. This is for
// use with position kinds that have no script/offset/frameIndex information.
function hitGlobalHandler(kind) {
  if (gPositionHandlerKinds[kind]) {
    RecordReplayControl.positionHit({ kind });
  }
}

// The completion state of any frame that is being popped.
let gPopFrameResult = null;

function onPopFrame(completion) {
  gPopFrameResult = completion;
  RecordReplayControl.positionHit({
    kind: "OnPop",
    script: gScripts.getId(this.script),
    frameIndex: countScriptFrames() - 1,
  });
  gPopFrameResult = null;
}

function onEnterFrame(frame) {
  hitGlobalHandler("EnterFrame");

  if (considerScript(frame.script)) {
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

function EnsurePositionHandler(position) {
  gPositionHandlerKinds[position.kind] = true;

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
    }

    const match = function({script, offset}) {
      return script == position.script && offset == position.offset;
    };
    if (gInstalledPcHandlers.some(match)) {
      return;
    }
    gInstalledPcHandlers.push({ script: position.script, offset: position.offset });

    debugScript.setBreakpoint(position.offset, {
      hit() {
        RecordReplayControl.positionHit({
          kind: "OnStep",
          script: position.script,
          offset: position.offset,
          frameIndex: countScriptFrames() - 1,
        });
      },
    });
    break;
  case "OnPop":
    if (position.script) {
      addOnPopFilter(frame => gScripts.getId(frame.script) == position.script);
    } else {
      addOnPopFilter(frame => true);
    }
    break;
  case "EnterFrame":
    dbg.onEnterFrame = onEnterFrame;
    break;
  }
}

// eslint-disable-next-line no-unused-vars
function GetEntryPosition(position) {
  if (position.kind == "Break" || position.kind == "OnStep") {
    const script = gScripts.getObject(position.script);
    if (script) {
      return {
        kind: "Break",
        script: position.script,
        offset: script.mainOffset,
      };
    }
  }
  return null;
}

///////////////////////////////////////////////////////////////////////////////
// Paused State
///////////////////////////////////////////////////////////////////////////////

let gPausedObjects = new IdMap();
let gDereferencedObjects = new Map();

function getObjectId(obj) {
  const id = gPausedObjects.getId(obj);
  if (!id && obj) {
    assert((obj instanceof Debugger.Object) ||
           (obj instanceof Debugger.Environment));

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
function convertValue(value, options) {
  if (value instanceof Debugger.Object) {
    if (options && options.snapshot) {
      return { snapshot: makeObjectSnapshot(value) };
    }
    return { object: getObjectId(value) };
  }
  if (value === undefined ||
      value == Infinity ||
      value == -Infinity ||
      Object.is(value, NaN) ||
      Object.is(value, -0)) {
    return { special: "" + value };
  }
  return value;
}

function convertedValueIsObject(value) {
  return isNonNullObject(value) && "object" in value;
}

function convertCompletionValue(value, options) {
  if ("return" in value) {
    return { return: convertValue(value.return, options) };
  }
  if ("throw" in value) {
    return { throw: convertValue(value.throw, options) };
  }
  throw new Error("Unexpected completion value");
}

// Convert a value we received from the parent.
function convertValueFromParent(value) {
  if (isNonNullObject(value)) {
    if (value.object) {
      return gPausedObjects.getObject(value.object);
    }
    switch (value.special) {
    case "undefined": return undefined;
    case "Infinity": return Infinity;
    case "-Infinity": return -Infinity;
    case "NaN": return NaN;
    case "0": return -0;
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
      return firstGlobal.makeDebuggeeValue(value);
    }
  }
  return value;
}

function getDebuggeeValue(value) {
  if (value && typeof value == "object") {
    assert(value instanceof Debugger.Object);
    return value.unsafeDereference();
  }
  return value;
}

// eslint-disable-next-line no-unused-vars
function ClearPausedState() {
  gPausedObjects = new IdMap();
  gDereferencedObjects = new Map();
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
    introductionOffset: introductionScript ? source.introductionOffset : undefined,
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

  return {
    index,
    type: frame.type,
    callee: getObjectId(frame.callee),
    environment: getObjectId(frame.environment),
    generator: frame.generator,
    constructing: frame.constructing,
    this: convertValue(frame.this),
    script: gScripts.getId(frame.script),
    offset: frame.offset,
    arguments: _arguments,
  };
}

function unknownObjectProperties(why) {
  return [{
    name: "Unknown properties",
    desc: {
      value: why,
      enumerable: true,
    },
  }];
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
  throw new Error("Unknown object kind");
}

function getObjectProperties(object) {
  let names;
  try {
    names = object.getOwnPropertyNames();
  } catch (e) {
    return unknownObjectProperties(e.toString());
  }

  const rv = Object.create(null);
  names.forEach(name => {
    let desc;
    try {
      desc = object.getOwnPropertyDescriptor(name);
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

function getEnvironmentNames(env) {
  try {
    const names = env.names();

    return names.map(name => {
      return { name, value: convertValue(env.getVariable(name)) };
    });
  } catch (e) {
    return [{name: "Unknown names",
             value: "Exception thrown in getEnvironmentNames" }];
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
  const numFrames = countScriptFrames();
  if (!numFrames) {
    return {};
  }

  const rv = {
    frames: [],
    scripts: {},
    offsetMetadata: [],
    objects: {},
    environments: {},
  };

  function addValue(value, includeProperties) {
    if (value && typeof value == "object" && value.object) {
      addObject(value.object, includeProperties);
    }
  }

  function addObject(id, includeProperties) {
    if (!id) {
      return;
    }

    // If includeProperties is set then previewing the object requires knowledge
    // of its enumerable properties.
    const needObject = !rv.objects[id];
    const needProperties =
      includeProperties &&
      (needObject || !rv.objects[id].preview.enumerableOwnProperties);

    if (!needObject && !needProperties) {
      return;
    }

    const object = gPausedObjects.getObject(id);
    assert(object instanceof Debugger.Object);

    const properties = getObjectProperties(object);
    const propertyEntries = Object.entries(properties);

    if (needObject) {
      rv.objects[id] = {
        data: getObjectData(id),
        preview: {
          ownPropertyNamesCount: propertyEntries.length,
        },
      };

      const preview = rv.objects[id].preview;

      // Add some properties (if present) which the server might ask for
      // even when it isn't interested in the rest of the properties.
      if (properties.length) {
        preview.lengthProperty = properties.length;
      }
      if (properties.displayName) {
        preview.displayNameProperty = properties.displayName;
      }
    }

    if (needProperties) {
      const preview = rv.objects[id].preview;

      // The server is only interested in enumerable properties, and at most
      // OBJECT_PREVIEW_MAX_ITEMS of them. Limiting the properties we send to
      // only those the server needs avoids having to send the contents of huge
      // objects like Windows, most of which will not be used.
      const enumerableOwnProperties = Object.create(null);
      let enumerablePropertyCount = 0;
      for (const [ name, desc ] of propertyEntries) {
        if (desc.enumerable) {
          enumerableOwnProperties[name] = desc;
          addPropertyDescriptor(desc, false);
          if (++enumerablePropertyCount == OBJECT_PREVIEW_MAX_ITEMS) {
            break;
          }
        }
      }
      preview.enumerableOwnProperties = enumerableOwnProperties;
    }
  }

  function addPropertyDescriptor(desc, includeProperties) {
    if (desc.value) {
      addValue(desc.value, includeProperties);
    }
    if (desc.get) {
      addObject(desc.get, includeProperties);
    }
    if (desc.set) {
      addObject(desc.set, includeProperties);
    }
  }

  function addEnvironment(id) {
    if (!id || rv.environments[id]) {
      return;
    }

    const env = gPausedObjects.getObject(id);
    assert(env instanceof Debugger.Environment);

    const data = getObjectData(id);
    const names = getEnvironmentNames(env);
    rv.environments[id] = { data, names };

    addEnvironment(data.parent);
  }

  // eslint-disable-next-line no-shadow
  function addScript(id) {
    if (!rv.scripts[id]) {
      rv.scripts[id] = getScriptData(id);
    }
  }

  for (let i = 0; i < numFrames; i++) {
    const frame = getFrameData(i);
    const script = gScripts.getObject(frame.script);
    rv.frames.push(frame);
    rv.offsetMetadata.push({
      scriptId: frame.script,
      offset: frame.offset,
      metadata: script.getOffsetMetadata(frame.offset),
    });
    addScript(frame.script);
    addValue(frame.this, true);
    if (frame.arguments) {
      for (const arg of frame.arguments) {
        addValue(arg, true);
      }
    }
    addObject(frame.callee, false);
    addEnvironment(frame.environment, true);
  }

  return rv;
}

///////////////////////////////////////////////////////////////////////////////
// Handlers
///////////////////////////////////////////////////////////////////////////////

const gRequestHandlers = {

  repaint() {
    if (!RecordReplayControl.maybeDivergeFromRecording()) {
      return {};
    }
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

  getNewScript(request) {
    return getScriptData(gScripts.lastId());
  },

  getContent(request) {
    return RecordReplayControl.getContent(request.url);
  },

  findSources(request) {
    const sources = [];
    gScriptSources.forEach((id) => {
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
    if (!RecordReplayControl.maybeDivergeFromRecording()) {
      return unknownObjectProperties("Recording divergence in getObjectProperties");
    }

    const object = gPausedObjects.getObject(request.id);
    return getObjectProperties(object);
  },

  objectApply(request) {
    if (!RecordReplayControl.maybeDivergeFromRecording()) {
      return { throw: "Recording divergence in objectApply" };
    }
    const obj = gPausedObjects.getObject(request.id);
    const thisv = convertValueFromParent(request.thisv);
    const args = request.args.map(v => convertValueFromParent(v));
    const rv = obj.apply(thisv, args);
    return convertCompletionValue(rv);
  },

  getEnvironmentNames(request) {
    if (!RecordReplayControl.maybeDivergeFromRecording()) {
      return [{name: "Unknown names",
               value: "Recording divergence in getEnvironmentNames" }];
    }

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
    if (!RecordReplayControl.maybeDivergeFromRecording()) {
      return { error: "Recording divergence in pauseData" };
    }

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

  frameEvaluate(request) {
    if (!RecordReplayControl.maybeDivergeFromRecording()) {
      return { throw: "Recording divergence in frameEvaluate" };
    }

    const frame = scriptFrameForIndex(request.index);
    const rv = frame.eval(request.text, request.options);
    return convertCompletionValue(rv, request.convertOptions);
  },

  popFrameResult(request) {
    return gPopFrameResult ? convertCompletionValue(gPopFrameResult) : {};
  },

  findConsoleMessages(request) {
    return gConsoleMessages;
  },

  getNewConsoleMessage(request) {
    return gConsoleMessages[gConsoleMessages.length - 1];
  },

  currentExecutionPoint(request) {
    return RecordReplayControl.currentExecutionPoint();
  },

  recordingEndpoint(request) {
    return RecordReplayControl.recordingEndpoint();
  },

  /////////////////////////////////////////////////////////
  // Inspector Requests
  /////////////////////////////////////////////////////////

  getFixedObjects(request) {
    if (!RecordReplayControl.maybeDivergeFromRecording()) {
      return { throw: "Recording divergence in getWindow" };
    }

    const window = getWindow();
    return {
      window: getObjectId(makeDebuggeeValue(window)),
      document: getObjectId(makeDebuggeeValue(window.document)),
      Services: getObjectId(makeDebuggeeValue(Services)),
      InspectorUtils: getObjectId(makeDebuggeeValue(InspectorUtils)),
      CSSRule: getObjectId(makeDebuggeeValue(CSSRule)),
    };
  },

  newDeepTreeWalker(request) {
    if (!RecordReplayControl.maybeDivergeFromRecording()) {
      return { throw: "Recording divergence in newDeepTreeWalker" };
    }

    const walker = Cc["@mozilla.org/inspector/deep-tree-walker;1"]
      .createInstance(Ci.inIDeepTreeWalker);
    return { id: getObjectId(makeDebuggeeValue(walker)) };
  },

  getObjectPropertyValue(request) {
    if (!RecordReplayControl.maybeDivergeFromRecording()) {
      return { throw: "Recording divergence in getObjectPropertyValue" };
    }

    const object = gPausedObjects.getObject(request.id);

    try {
      const rv = object.unsafeDereference()[request.name];
      return { "return": convertValue(makeDebuggeeValue(rv)) };
    } catch (e) {
      return { "throw": "" + e };
    }
  },

  setObjectPropertyValue(request) {
    if (!RecordReplayControl.maybeDivergeFromRecording()) {
      return { throw: "Recording divergence in getObjectPropertyValue" };
    }

    const object = gPausedObjects.getObject(request.id);
    const value = getDebuggeeValue(convertValueFromParent(request.value));

    try {
      object.unsafeDereference()[request.name] = value;
      return { "return": request.value };
    } catch (e) {
      return { "throw": "" + e };
    }
  },

  createObject(request) {
    const global = dbg.getDebuggees()[0];
    const value = global.executeInGlobal("({})");
    return { id: getObjectId(value.return) };
  },

  findEventTarget(request) {
    const element =
      getWindow().document.elementFromPoint(request.clientX, request.clientY);
    if (!element) {
      return { id: 0 };
    }
    const obj = makeDebuggeeValue(element);
    return { id: getObjectId(obj) };
  },
};

// eslint-disable-next-line no-unused-vars
function ProcessRequest(request) {
  try {
    if (gRequestHandlers[request.type]) {
      return gRequestHandlers[request.type](request);
    }
    return { exception: "No handler for " + request.type };
  } catch (e) {
    let msg;
    try {
      msg = "" + e + " line " + e.lineNumber;
    } catch (ee) {
      msg = "Unknown";
    }
    dump("ReplayDebugger Record/Replay Error: " + msg + "\n");
    return { exception: msg };
  }
}

// eslint-disable-next-line no-unused-vars
var EXPORTED_SYMBOLS = [
  "EnsurePositionHandler",
  "ClearPositionHandlers",
  "ClearPausedState",
  "ProcessRequest",
  "GetEntryPosition",
];
