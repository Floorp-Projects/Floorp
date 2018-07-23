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
const sandbox = Cu.Sandbox(CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")());
Cu.evalInSandbox(
  "Components.utils.import('resource://gre/modules/jsdebugger.jsm');" +
  "addDebuggerToGlobal(this);",
  sandbox
);
const Debugger = sandbox.Debugger;
const RecordReplayControl = sandbox.RecordReplayControl;

const dbg = new Debugger();

// We are interested in debugging all globals in the process.
dbg.onNewGlobalObject = function(global) {
  dbg.addDebuggee(global);
};

///////////////////////////////////////////////////////////////////////////////
// Utilities
///////////////////////////////////////////////////////////////////////////////

function assert(v) {
  if (!v) {
    RecordReplayControl.dump("Assertion Failed: " + (new Error()).stack + "\n");
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

///////////////////////////////////////////////////////////////////////////////
// Persistent State
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
  gScriptSources.add(source);
}

function considerScript(script) {
  return script.url
      && !script.url.startsWith("resource:")
      && !script.url.startsWith("chrome:");
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

  if (gHasNewScriptHandler) {
    RecordReplayControl.positionHit({ kind: "NewScript" });
  }

  // Check in case any handlers we need to install are on the scripts just
  // created.
  installPendingHandlers();
};

///////////////////////////////////////////////////////////////////////////////
// Position Handler State
///////////////////////////////////////////////////////////////////////////////

// Whether there is a position handler for NewScript.
let gHasNewScriptHandler = false;

// Whether there is a position handler for EnterFrame.
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

// eslint-disable-next-line no-unused-vars
function ClearPositionHandlers() {
  dbg.clearAllBreakpoints();
  dbg.onEnterFrame = undefined;

  gHasNewScriptHandler = false;
  gHasEnterFrameHandler = false;
  gPendingPcHandlers.length = 0;
  gInstalledPcHandlers.length = 0;
  gOnPopFilters.length = 0;
}

function installPendingHandlers() {
  const pending = gPendingPcHandlers.map(position => position);
  gPendingPcHandlers.length = 0;

  pending.forEach(EnsurePositionHandler);
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
  if (gHasEnterFrameHandler) {
    RecordReplayControl.positionHit({ kind: "EnterFrame" });
  }

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
      }
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
    gHasEnterFrameHandler = true;
    dbg.onEnterFrame = onEnterFrame;
    break;
  case "NewScript":
    gHasNewScriptHandler = true;
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

function getObjectId(obj) {
  const id = gPausedObjects.getId(obj);
  if (!id && obj) {
    assert((obj instanceof Debugger.Object) ||
           (obj instanceof Debugger.Environment));
    return gPausedObjects.add(obj);
  }
  return id;
}

function convertValue(value) {
  if (value instanceof Debugger.Object) {
    return { object: getObjectId(value) };
  }
  if (value === undefined) {
    return { special: "undefined" };
  }
  if (value !== value) { // eslint-disable-line no-self-compare
    return { special: "NaN" };
  }
  if (value == Infinity) {
    return { special: "Infinity" };
  }
  if (value == -Infinity) {
    return { special: "-Infinity" };
  }
  return value;
}

function convertCompletionValue(value) {
  if ("return" in value) {
    return { return: convertValue(value.return) };
  }
  if ("throw" in value) {
    return { throw: convertValue(value.throw) };
  }
  throw new Error("Unexpected completion value");
}

// eslint-disable-next-line no-unused-vars
function ClearPausedState() {
  gPausedObjects = new IdMap();
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
  };
}

function forwardToScript(name) {
  return request => gScripts.getObject(request.id)[name](request.value);
}

///////////////////////////////////////////////////////////////////////////////
// Handlers
///////////////////////////////////////////////////////////////////////////////

const gRequestHandlers = {

  findScripts(request) {
    const rv = [];
    gScripts.forEach((id) => {
      rv.push(getScriptData(id));
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

  getSource(request) {
    const source = gScriptSources.getObject(request.id);
    const introductionScript = gScripts.getId(source.introductionScript);
    return {
      id: request.id,
      text: source.text,
      url: source.url,
      displayURL: source.displayURL,
      elementAttributeName: source.elementAttributeName,
      introductionScript,
      introductionOffset: introductionScript ? source.introductionOffset : undefined,
      introductionType: source.introductionType,
      sourceMapURL: source.sourceMapURL,
    };
  },

  getObject(request) {
    const object = gPausedObjects.getObject(request.id);
    if (object instanceof Debugger.Object) {
      return {
        id: request.id,
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
        global: getObjectId(object.global),
        isProxy: object.isProxy,
        isExtensible: object.isExtensible(),
        isSealed: object.isSealed(),
        isFrozen: object.isFrozen(),
      };
    }
    if (object instanceof Debugger.Environment) {
      return {
        id: request.id,
        kind: "Environment",
        type: object.type,
        parent: getObjectId(object.parent),
        object: object.type == "declarative" ? 0 : getObjectId(object.object),
        callee: getObjectId(object.callee),
        optimizedOut: object.optimizedOut,
      };
    }
    throw new Error("Unknown object kind");
  },

  getObjectProperties(request) {
    if (!RecordReplayControl.maybeDivergeFromRecording()) {
      return [{
        name: "Unknown properties",
        desc: {
          value: "Recording divergence in getObjectProperties",
          enumerable: true
        },
      }];
    }

    const object = gPausedObjects.getObject(request.id);
    const names = object.getOwnPropertyNames();

    return names.map(name => {
      const desc = object.getOwnPropertyDescriptor(name);
      if ("value" in desc) {
        desc.value = convertValue(desc.value);
      }
      if ("get" in desc) {
        desc.get = getObjectId(desc.get);
      }
      if ("set" in desc) {
        desc.set = getObjectId(desc.set);
      }
      return { name, desc };
    });
  },

  getEnvironmentNames(request) {
    if (!RecordReplayControl.maybeDivergeFromRecording()) {
      return [{name: "Unknown names",
               value: "Recording divergence in getEnvironmentNames" }];
    }

    const env = gPausedObjects.getObject(request.id);
    const names = env.names();

    return names.map(name => {
      return { name, value: convertValue(env.getVariable(name)) };
    });
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

    const frame = scriptFrameForIndex(request.index);

    let _arguments = null;
    if (frame.arguments) {
      _arguments = [];
      for (let i = 0; i < frame.arguments.length; i++) {
        _arguments.push(convertValue(frame.arguments[i]));
      }
    }

    return {
      index: request.index,
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
  },

  getLineOffsets: forwardToScript("getLineOffsets"),
  getOffsetLocation: forwardToScript("getOffsetLocation"),
  getSuccessorOffsets: forwardToScript("getSuccessorOffsets"),
  getPredecessorOffsets: forwardToScript("getPredecessorOffsets"),

  frameEvaluate(request) {
    if (!RecordReplayControl.maybeDivergeFromRecording()) {
      return { throw: "Recording divergence in frameEvaluate" };
    }

    const frame = scriptFrameForIndex(request.index);
    const rv = frame.eval(request.text, request.options);
    return convertCompletionValue(rv);
  },

  popFrameResult(request) {
    return gPopFrameResult ? convertCompletionValue(gPopFrameResult) : {};
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
    RecordReplayControl.dump("ReplayDebugger Record/Replay Error: " + e + "\n");
    return { exception: "" + e };
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
