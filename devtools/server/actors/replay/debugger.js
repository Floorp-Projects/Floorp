/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy */

// When recording/replaying an execution with Web Replay, Devtools server code
// runs in the middleman process instead of the recording/replaying process the
// code is interested in.
//
// This file defines replay objects analogous to those constructed by the
// C++ Debugger (Debugger, Debugger.Object, etc.), which implement similar
// methods and properties to those C++ objects. These replay objects are
// created in the middleman process, and describe things that exist in the
// recording/replaying process, inspecting them via the interface provided by
// control.js.

"use strict";

const RecordReplayControl = !isWorker && require("RecordReplayControl");
const Services = require("Services");
const ChromeUtils = require("ChromeUtils");

ChromeUtils.defineModuleGetter(
  this,
  "positionSubsumes",
  "resource://devtools/shared/execution-point-utils.js"
);

///////////////////////////////////////////////////////////////////////////////
// ReplayDebugger
///////////////////////////////////////////////////////////////////////////////

// Possible preferred directions of travel.
const Direction = {
  FORWARD: "FORWARD",
  BACKWARD: "BACKWARD",
  NONE: "NONE",
};

function ReplayDebugger() {
  const existing = RecordReplayControl.registerReplayDebugger(this);
  if (existing) {
    // There is already a ReplayDebugger in existence, use that. There can only
    // be one ReplayDebugger in the process.
    return existing;
  }

  // We should have been connected to control.js by the call above.
  assert(this._control);

  // Preferred direction of travel when not explicitly resumed.
  this._direction = Direction.NONE;

  // All breakpoint positions and handlers installed by this debugger.
  this._breakpoints = [];

  // All ReplayDebuggerFramees that have been created while paused at the
  // current position, indexed by their index (zero is the oldest frame, with
  // the index increasing for newer frames). These are invalidated when
  // unpausing.
  this._frames = [];

  // All ReplayDebuggerObjects and ReplayDebuggerEnvironments that have been
  // created while paused at the current position, indexed by their id. These
  // are invalidated when unpausing.
  this._objects = [];

  // All ReplayDebuggerScripts and ReplayDebuggerScriptSources that have been
  // created, indexed by their id. These stay valid even after unpausing.
  this._scripts = [];
  this._scriptSources = [];

  // How many nested thread-wide paused have been entered.
  this._threadPauseCount = 0;

  // Flag set if the dispatched _performPause() call can be ignored because the
  // server entered a thread-wide pause first.
  this._cancelPerformPause = false;

  // After we are done pausing, callback describing how to resume.
  this._resumeCallback = null;

  // Handler called when hitting the beginning/end of the recording, or when
  // a time warp target has been reached.
  this.replayingOnForcedPause = null;

  // Handler called when the child pauses for any reason.
  this.replayingOnPositionChange = null;
}

// Frame index used to refer to the newest frame in the child process.
const NewestFrameIndex = -1;

ReplayDebugger.prototype = {
  /////////////////////////////////////////////////////////
  // General methods
  /////////////////////////////////////////////////////////

  replaying: true,

  canRewind: RecordReplayControl.canRewind,

  replayCurrentExecutionPoint() {
    return this._control.lastPausePoint();
  },

  replayRecordingEndpoint() {
    return this._control.recordingEndpoint();
  },

  replayIsRecording() {
    return this._control.childIsRecording();
  },

  replayUnscannedRegions() {
    return this._control.unscannedRegions();
  },

  replayCachedPoints() {
    return this._control.cachedPoints();
  },

  addDebuggee() {},
  removeAllDebuggees() {},

  replayingContent(url) {
    return this._sendRequestMainChild({ type: "getContent", url });
  },

  _processResponse(request, response, divergeResponse) {
    dumpv(`SendRequest: ${stringify(request)} -> ${stringify(response)}`);
    if (response.exception) {
      ThrowError(response.exception);
    }
    if (response.unhandledDivergence) {
      if (divergeResponse) {
        return divergeResponse;
      }
      ThrowError(`Unhandled recording divergence in ${request.type}`);
    }
    return response;
  },

  // Send a request object to the child process, and synchronously wait for it
  // to respond. divergeResponse must be specified for requests that can diverge
  // from the recording and which we want to recover gracefully.
  _sendRequest(request, divergeResponse) {
    const response = this._control.sendRequest(request);
    return this._processResponse(request, response, divergeResponse);
  },

  // Send a request that requires the child process to perform actions that
  // diverge from the recording. In such cases we want to be interacting with a
  // replaying process (if there is one), as recording child processes won't
  // provide useful responses to such requests.
  _sendRequestAllowDiverge(request, divergeResponse) {
    this._control.maybeSwitchToReplayingChild();
    return this._sendRequest(request, divergeResponse);
  },

  _sendRequestMainChild(request) {
    const response = this._control.sendRequestMainChild(request);
    return this._processResponse(request, response);
  },

  getDebuggees() {
    return [];
  },

  /////////////////////////////////////////////////////////
  // Paused/running state
  /////////////////////////////////////////////////////////

  // Paused State Management
  //
  // The single ReplayDebugger is exclusively responsible for controlling the
  // position of the child process by keeping track of when it pauses and
  // sending it commands to resume.
  //
  // The general goal of controlling this position is to make the child process
  // execute at predictable times, similar to how it would execute if the
  // debuggee was in the same process as this one (as is the case when not
  // replaying), as described below:
  //
  // - After the child pauses, the it will only resume executing when an event
  //   loop is running that is *not* associated with the thread actor's nested
  //   pauses. As long as the thread actor has pushed a pause, the child will
  //   remain paused.
  //
  // - After the child resumes, installed breakpoint handlers will only execute
  //   when an event loop is running (which, because of the above point, cannot
  //   be associated with a thread actor's nested pause).

  get _paused() {
    return !!this._control.pausePoint();
  },

  replayResumeBackward() {
    this._resume(/* forward = */ false);
  },
  replayResumeForward() {
    this._resume(/* forward = */ true);
  },

  _resume(forward) {
    this._ensurePaused();
    this._setResume(() => {
      this._direction = forward ? Direction.FORWARD : Direction.BACKWARD;
      dumpv("Resuming " + this._direction);
      this._control.resume(forward);
      if (this._paused) {
        // If we resume and immediately pause, we are at an endpoint of the
        // recording. Force the thread to pause.
        this._capturePauseData();
        this.replayingOnForcedPause(this.getNewestFrame());
      }
    });
  },

  replayTimeWarp(target) {
    this._ensurePaused();
    this._setResume(() => {
      this._direction = Direction.NONE;
      dumpv("Warping " + JSON.stringify(target));
      this._control.timeWarp(target);

      // timeWarp() doesn't return until the child has reached the target of
      // the warp, after which we force the thread to pause.
      assert(this._paused);
      this._capturePauseData();
      this.replayingOnForcedPause(this.getNewestFrame());
    });
  },

  replayPause() {
    this._ensurePaused();

    // Cancel any pending resume.
    this._resumeCallback = null;
  },

  _ensurePaused() {
    if (!this._paused) {
      this._control.waitUntilPaused();
      assert(this._paused);
    }
  },

  // This hook is called whenever the child has paused, which can happen
  // within a control method (resume, timeWarp, waitUntilPaused) or be
  // delivered via the event loop.
  _onPause() {
    // The position change handler is always called on pause notifications.
    if (this.replayingOnPositionChange) {
      this.replayingOnPositionChange();
    }

    // Call _performPause() soon via the event loop to check for breakpoint
    // handlers at this point.
    this._cancelPerformPause = false;
    Services.tm.dispatchToMainThread(this._performPause.bind(this));
  },

  _performPause() {
    // The child paused at some time in the past and any breakpoint handlers
    // may still need to be called. If we've entered a thread-wide pause or
    // have already told the child to resume, don't call handlers.
    if (!this._paused || this._cancelPerformPause || this._resumeCallback) {
      return;
    }

    const point = this.replayCurrentExecutionPoint();
    dumpv("PerformPause " + JSON.stringify(point));

    if (!point.position) {
      // We paused at a checkpoint, and there are no handlers to call.
    } else {
      // Call any handlers for this point, unless one resumes execution.
      for (const { handler, position } of this._breakpoints) {
        if (positionSubsumes(position, point.position)) {
          handler();
          assert(!this._threadPauseCount);
          if (this._resumeCallback) {
            break;
          }
        }
      }

      if (
        this._control.isPausedAtDebuggerStatement() &&
        this.onDebuggerStatement
      ) {
        this._capturePauseData();
        this.onDebuggerStatement(this.getNewestFrame());
      }
    }

    // If no handlers entered a thread-wide pause (resetting this._direction)
    // or gave an explicit resume, continue traveling in the same direction
    // we were going when we paused.
    assert(!this._threadPauseCount);
    if (!this._resumeCallback) {
      switch (this._direction) {
        case Direction.FORWARD:
          this.replayResumeForward();
          break;
        case Direction.BACKWARD:
          this.replayResumeBackward();
          break;
      }
    }
  },

  // This hook is called whenever control state changes which affects something
  // the position change handler listens to (more than just position changes,
  // alas).
  _callOnPositionChange() {
    if (this.replayingOnPositionChange) {
      this.replayingOnPositionChange();
    }
  },

  replayPushThreadPause() {
    // The thread has paused so that the user can interact with it. The child
    // will stay paused until this thread-wide pause has been popped.
    this._ensurePaused();
    assert(!this._resumeCallback);
    if (++this._threadPauseCount == 1) {
      // There is no preferred direction of travel after an explicit pause.
      this._direction = Direction.NONE;

      // Update graphics according to the current state of the child.
      this._control.repaint();

      // If breakpoint handlers for the pause haven't been called yet, don't
      // call them at all.
      this._cancelPerformPause = true;
    }
    const point = this.replayCurrentExecutionPoint();
    dumpv("PushPause " + JSON.stringify(point));
  },

  replayPopThreadPause() {
    dumpv("PopPause");

    // After popping the last thread-wide pause, the child can resume.
    if (--this._threadPauseCount == 0 && this._resumeCallback) {
      Services.tm.dispatchToMainThread(this._performResume.bind(this));
    }
  },

  _setResume(callback) {
    assert(this._paused);

    // Overwrite any existing resume direction.
    this._resumeCallback = callback;

    // The child can resume immediately if there is no thread-wide pause.
    if (!this._threadPauseCount) {
      Services.tm.dispatchToMainThread(this._performResume.bind(this));
    }
  },

  _performResume() {
    this._ensurePaused();
    if (this._resumeCallback && !this._threadPauseCount) {
      const callback = this._resumeCallback;
      this._invalidateAfterUnpause();
      this._resumeCallback = null;
      callback();
    }
  },

  // Clear out all data that becomes invalid when the child unpauses.
  _invalidateAfterUnpause() {
    this._frames.forEach(frame => frame._invalidate());
    this._frames.length = 0;

    this._objects.forEach(obj => obj._invalidate());
    this._objects.length = 0;
  },

  // Fill in the debugger with (hopefully) all data the client/server need to
  // pause at the current location.
  _capturePauseData() {
    if (this._frames.length) {
      return;
    }

    const pauseData = this._control.getPauseData();
    if (!pauseData.frames) {
      return;
    }

    for (const data of Object.values(pauseData.scripts)) {
      this._addScript(data);
    }

    for (const { scriptId, offset, metadata } of pauseData.offsetMetadata) {
      if (this._scripts[scriptId]) {
        const script = this._getScript(scriptId);
        script._addOffsetMetadata(offset, metadata);
      }
    }

    for (const { data, preview } of Object.values(pauseData.objects)) {
      if (!this._objects[data.id]) {
        this._addObject(data);
      }
      this._getObject(data.id)._preview = {
        ...preview,
        enumerableOwnProperties: mapify(preview.enumerableOwnProperties),
      };
    }

    for (const { data, names } of Object.values(pauseData.environments)) {
      if (!this._objects[data.id]) {
        this._addObject(data);
      }
      this._getObject(data.id)._setNames(names);
    }

    for (const frame of pauseData.frames) {
      this._frames[frame.index] = new ReplayDebuggerFrame(this, frame);
    }
  },

  _virtualConsoleLog(position, text, condition, callback) {
    dumpv(`AddLogpoint ${JSON.stringify(position)} ${text} ${condition}`);
    this._control.addLogpoint({ position, text, condition, callback });
  },

  /////////////////////////////////////////////////////////
  // Breakpoint management
  /////////////////////////////////////////////////////////

  _setBreakpoint(handler, position, data) {
    dumpv(`AddBreakpoint ${JSON.stringify(position)}`);
    this._control.addBreakpoint(position);
    this._breakpoints.push({ handler, position, data });
  },

  _clearMatchingBreakpoints(callback) {
    const newBreakpoints = this._breakpoints.filter(bp => !callback(bp));
    if (newBreakpoints.length != this._breakpoints.length) {
      dumpv("ClearBreakpoints");
      this._control.clearBreakpoints();
      for (const { position } of newBreakpoints) {
        dumpv("AddBreakpoint " + JSON.stringify(position));
        this._control.addBreakpoint(position);
      }
    }
    this._breakpoints = newBreakpoints;
  },

  _searchBreakpoints(callback) {
    for (const breakpoint of this._breakpoints) {
      const v = callback(breakpoint);
      if (v) {
        return v;
      }
    }
    return undefined;
  },

  // Getter for a breakpoint kind that has no script/offset/frameIndex.
  _breakpointKindGetter(kind) {
    return this._searchBreakpoints(({ position, data }) => {
      return position.kind == kind ? data : null;
    });
  },

  // Setter for a breakpoint kind that has no script/offset/frameIndex.
  _breakpointKindSetter(kind, handler, callback) {
    if (handler) {
      this._setBreakpoint(callback, { kind }, handler);
    } else {
      this._clearMatchingBreakpoints(({ position }) => position.kind == kind);
    }
  },

  // Clear OnStep and OnPop hooks for all frames.
  replayClearSteppingHooks() {
    this._clearMatchingBreakpoints(
      ({ position }) => position.kind == "OnStep" || position.kind == "OnPop"
    );
  },

  /////////////////////////////////////////////////////////
  // Script methods
  /////////////////////////////////////////////////////////

  _getScript(id) {
    if (!id) {
      return undefined;
    }
    const rv = this._scripts[id];
    if (rv) {
      return rv;
    }
    return this._addScript(this._sendRequest({ type: "getScript", id }));
  },

  _addScript(data) {
    if (!this._scripts[data.id]) {
      this._scripts[data.id] = new ReplayDebuggerScript(this, data);
    }
    return this._scripts[data.id];
  },

  _convertScriptQuery(query) {
    // Make a copy of the query, converting properties referring to debugger
    // things into their associated ids.
    const rv = Object.assign({}, query);
    if ("global" in query) {
      // Script queries might be sent to a different process from the one which
      // is paused at the current point and which we are interacting with.
      NYI();
    }
    if ("source" in query) {
      rv.source = query.source._data.id;
    }
    return rv;
  },

  findScripts(query) {
    const data = this._sendRequestMainChild({
      type: "findScripts",
      query: this._convertScriptQuery(query),
    });
    return data.map(script => this._addScript(script));
  },

  _onNewScript(data) {
    if (this.onNewScript) {
      const script = this._addScript(data);
      this.onNewScript(script);
    }
  },

  /////////////////////////////////////////////////////////
  // ScriptSource methods
  /////////////////////////////////////////////////////////

  _getSource(id) {
    const source = this._scriptSources[id];
    if (source) {
      return source;
    }
    return this._addSource(
      this._sendRequestMainChild({ type: "getSource", id })
    );
  },

  _addSource(data) {
    if (!this._scriptSources[data.id]) {
      this._scriptSources[data.id] = new ReplayDebuggerScriptSource(this, data);
    }
    return this._scriptSources[data.id];
  },

  findSources() {
    const data = this._sendRequestMainChild({ type: "findSources" });
    return data.map(source => this._addSource(source));
  },

  findSourceURLs() {
    return this.findSources().map(source => source.url);
  },

  adoptSource(source) {
    assert(source._dbg == this);
    return source;
  },

  /////////////////////////////////////////////////////////
  // Object methods
  /////////////////////////////////////////////////////////

  _getObject(id) {
    if (id && !this._objects[id]) {
      const data = this._sendRequest({ type: "getObject", id });
      this._addObject(data);
    }
    return this._objects[id];
  },

  _addObject(data) {
    switch (data.kind) {
      case "Object":
        this._objects[data.id] = new ReplayDebuggerObject(this, data);
        break;
      case "Environment":
        this._objects[data.id] = new ReplayDebuggerEnvironment(this, data);
        break;
      default:
        ThrowError("Unknown object kind");
    }
  },

  // Convert a value we received from the child.
  _convertValue(value) {
    if (isNonNullObject(value)) {
      if (value.object) {
        return this._getObject(value.object);
      }
      if (value.snapshot) {
        return new ReplayDebuggerObjectSnapshot(this, value.snapshot);
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
  },

  _convertCompletionValue(value) {
    if ("return" in value) {
      return { return: this._convertValue(value.return) };
    }
    if ("throw" in value) {
      return {
        throw: this._convertValue(value.throw),
        stack: value.stack,
      };
    }
    ThrowError("Unexpected completion value");
    return null; // For eslint
  },

  // Convert a value for sending to the child.
  _convertValueForChild(value) {
    if (isNonNullObject(value)) {
      assert(value instanceof ReplayDebuggerObject);
      return { object: value._data.id };
    } else if (
      value === undefined ||
      value == Infinity ||
      value == -Infinity ||
      Object.is(value, NaN) ||
      Object.is(value, -0)
    ) {
      return { special: "" + value };
    }
    return value;
  },

  /////////////////////////////////////////////////////////
  // Frame methods
  /////////////////////////////////////////////////////////

  _getFrame(index) {
    if (index == NewestFrameIndex) {
      if (this._frames.length) {
        return this._frames[this._frames.length - 1];
      }
    } else {
      assert(index < this._frames.length);
      if (this._frames[index]) {
        return this._frames[index];
      }
    }

    const data = this._sendRequest({ type: "getFrame", index });

    if (index == NewestFrameIndex) {
      if ("index" in data) {
        index = data.index;
      } else {
        // There are no frames on the stack.
        return null;
      }
    }

    this._frames[index] = new ReplayDebuggerFrame(this, data);
    return this._frames[index];
  },

  getNewestFrame() {
    return this._getFrame(NewestFrameIndex);
  },

  /////////////////////////////////////////////////////////
  // Console Message methods
  /////////////////////////////////////////////////////////

  _convertConsoleMessage(message) {
    // Console API message arguments need conversion to debuggee values, but
    // other contents of the message can be left alone.
    if (message.messageType == "ConsoleAPI" && message.arguments) {
      for (let i = 0; i < message.arguments.length; i++) {
        message.arguments[i] = this._convertValue(message.arguments[i]);
      }
    }
    return message;
  },

  findAllConsoleMessages() {
    const messages = this._sendRequestMainChild({
      type: "findConsoleMessages",
    });
    return messages.map(this._convertConsoleMessage.bind(this));
  },

  /////////////////////////////////////////////////////////
  // Handlers
  /////////////////////////////////////////////////////////

  get onEnterFrame() {
    return this._breakpointKindGetter("EnterFrame");
  },
  set onEnterFrame(handler) {
    this._breakpointKindSetter("EnterFrame", handler, () => {
      this._capturePauseData();
      handler.call(this, this.getNewestFrame());
    });
  },

  clearAllBreakpoints: NYI,
}; // ReplayDebugger.prototype

///////////////////////////////////////////////////////////////////////////////
// ReplayDebuggerScript
///////////////////////////////////////////////////////////////////////////////

function ReplayDebuggerScript(dbg, data) {
  this._dbg = dbg;
  this._data = data;
  this._offsetMetadata = [];
}

ReplayDebuggerScript.prototype = {
  get displayName() {
    return this._data.displayName;
  },
  get url() {
    return this._data.url;
  },
  get startLine() {
    return this._data.startLine;
  },
  get lineCount() {
    return this._data.lineCount;
  },
  get source() {
    return this._dbg._getSource(this._data.sourceId);
  },
  get sourceStart() {
    return this._data.sourceStart;
  },
  get sourceLength() {
    return this._data.sourceLength;
  },
  get format() {
    return this._data.format;
  },

  _forward(type, value) {
    return this._dbg._sendRequestMainChild({ type, id: this._data.id, value });
  },

  getLineOffsets(line) {
    return this._forward("getLineOffsets", line);
  },
  getOffsetLocation(pc) {
    return this._forward("getOffsetLocation", pc);
  },
  getSuccessorOffsets(pc) {
    return this._forward("getSuccessorOffsets", pc);
  },
  getPredecessorOffsets(pc) {
    return this._forward("getPredecessorOffsets", pc);
  },
  getAllColumnOffsets() {
    return this._forward("getAllColumnOffsets");
  },
  getPossibleBreakpoints(query) {
    return this._forward("getPossibleBreakpoints", query);
  },
  getPossibleBreakpointOffsets(query) {
    return this._forward("getPossibleBreakpointOffsets", query);
  },

  getOffsetMetadata(pc) {
    if (!this._offsetMetadata[pc]) {
      this._addOffsetMetadata(pc, this._forward("getOffsetMetadata", pc));
    }
    return this._offsetMetadata[pc];
  },

  _addOffsetMetadata(pc, metadata) {
    this._offsetMetadata[pc] = metadata;
  },

  setBreakpoint(offset, handler) {
    this._dbg._setBreakpoint(
      () => {
        this._dbg._capturePauseData();
        handler.hit(this._dbg.getNewestFrame());
      },
      { kind: "Break", script: this._data.id, offset },
      handler
    );
  },

  clearBreakpoint(handler) {
    this._dbg._clearMatchingBreakpoints(({ position, data }) => {
      return position.script == this._data.id && handler == data;
    });
  },

  replayVirtualConsoleLog(offset, text, condition, callback) {
    this._dbg._virtualConsoleLog(
      { kind: "Break", script: this._data.id, offset },
      text,
      condition,
      callback
    );
  },

  get isGeneratorFunction() {
    NYI();
  },
  get isAsyncFunction() {
    NYI();
  },
  getChildScripts: NYI,
  getAllOffsets: NYI,
  getBreakpoints: NYI,
  clearAllBreakpoints: NYI,
  isInCatchScope: NYI,
};

///////////////////////////////////////////////////////////////////////////////
// ReplayDebuggerScriptSource
///////////////////////////////////////////////////////////////////////////////

function ReplayDebuggerScriptSource(dbg, data) {
  this._dbg = dbg;
  this._data = data;
}

ReplayDebuggerScriptSource.prototype = {
  get text() {
    return this._data.text;
  },
  get url() {
    return this._data.url;
  },
  get displayURL() {
    return this._data.displayURL;
  },
  get elementAttributeName() {
    return this._data.elementAttributeName;
  },
  get introductionOffset() {
    return this._data.introductionOffset;
  },
  get introductionType() {
    return this._data.introductionType;
  },
  get sourceMapURL() {
    return this._data.sourceMapURL;
  },
  get element() {
    return null;
  },

  get introductionScript() {
    return this._dbg._getScript(this._data.introductionScript);
  },

  get binary() {
    NYI();
  },
};

///////////////////////////////////////////////////////////////////////////////
// ReplayDebuggerFrame
///////////////////////////////////////////////////////////////////////////////

function ReplayDebuggerFrame(dbg, data) {
  this._dbg = dbg;
  this._data = data;
  if (this._data.arguments) {
    this._arguments = this._data.arguments.map(a => this._dbg._convertValue(a));
  }
}

ReplayDebuggerFrame.prototype = {
  _invalidate() {
    this._data = null;
  },

  get type() {
    return this._data.type;
  },
  get callee() {
    return this._dbg._getObject(this._data.callee);
  },
  get environment() {
    return this._dbg._getObject(this._data.environment);
  },
  get generator() {
    return this._data.generator;
  },
  get constructing() {
    return this._data.constructing;
  },
  get this() {
    return this._dbg._convertValue(this._data.this);
  },
  get script() {
    return this._dbg._getScript(this._data.script);
  },
  get offset() {
    return this._data.offset;
  },
  get arguments() {
    assert(this._data);
    return this._arguments;
  },
  get live() {
    return true;
  },

  eval(text, options) {
    const rv = this._dbg._sendRequestAllowDiverge(
      {
        type: "frameEvaluate",
        index: this._data.index,
        text,
        options,
      },
      { throw: "Recording divergence in frameEvaluate" }
    );
    return this._dbg._convertCompletionValue(rv);
  },

  _positionMatches(position, kind) {
    return (
      position.kind == kind &&
      position.script == this._data.script &&
      position.frameIndex == this._data.index
    );
  },

  get onStep() {
    return this._dbg._searchBreakpoints(({ position, data }) => {
      return this._positionMatches(position, "OnStep") ? data : null;
    });
  },

  set onStep(handler) {
    // Use setReplayingOnStep or replayClearSteppingHooks instead.
    NotAllowed();
  },

  setReplayingOnStep(handler, offsets) {
    offsets.forEach(offset => {
      this._dbg._setBreakpoint(
        () => {
          this._dbg._capturePauseData();
          handler.call(this._dbg.getNewestFrame());
        },
        {
          kind: "OnStep",
          script: this._data.script,
          offset,
          frameIndex: this._data.index,
        },
        handler
      );
    });
  },

  get onPop() {
    return this._dbg._searchBreakpoints(({ position, data }) => {
      return this._positionMatches(position, "OnPop") ? data : null;
    });
  },

  set onPop(handler) {
    if (handler) {
      this._dbg._setBreakpoint(
        () => {
          this._dbg._capturePauseData();
          const result = this._dbg._sendRequest({ type: "popFrameResult" });
          handler.call(
            this._dbg.getNewestFrame(),
            this._dbg._convertCompletionValue(result)
          );
        },
        {
          kind: "OnPop",
          script: this._data.script,
          frameIndex: this._data.index,
        },
        handler
      );
    } else {
      // Use replayClearSteppingHooks instead.
      NotAllowed();
    }
  },

  get older() {
    if (this._data.index == 0) {
      // This is the oldest frame.
      return null;
    }
    return this._dbg._getFrame(this._data.index - 1);
  },

  get implementation() {
    NYI();
  },
  evalWithBindings: NYI,
};

///////////////////////////////////////////////////////////////////////////////
// ReplayDebuggerObject
///////////////////////////////////////////////////////////////////////////////

function ReplayDebuggerObject(dbg, data) {
  this._dbg = dbg;
  this._data = data;
  this._preview = null;
  this._properties = null;
}

ReplayDebuggerObject.prototype = {
  _invalidate() {
    this._data = null;
    this._preview = null;
    this._properties = null;
  },

  toString() {
    const id = this._data ? this._data.id : "INVALID";
    return `ReplayDebugger.Object #${id}`;
  },

  get callable() {
    return this._data.callable;
  },
  get isBoundFunction() {
    return this._data.isBoundFunction;
  },
  get isArrowFunction() {
    return this._data.isArrowFunction;
  },
  get isGeneratorFunction() {
    return this._data.isGeneratorFunction;
  },
  get isAsyncFunction() {
    return this._data.isAsyncFunction;
  },
  get class() {
    return this._data.class;
  },
  get name() {
    return this._data.name;
  },
  get displayName() {
    return this._data.displayName;
  },
  get parameterNames() {
    return this._data.parameterNames;
  },
  get script() {
    return this._dbg._getScript(this._data.script);
  },
  get environment() {
    return this._dbg._getObject(this._data.environment);
  },
  get isProxy() {
    return this._data.isProxy;
  },
  get proto() {
    return this._dbg._getObject(this._data.proto);
  },

  isExtensible() {
    return this._data.isExtensible;
  },
  isSealed() {
    return this._data.isSealed;
  },
  isFrozen() {
    return this._data.isFrozen;
  },

  unsafeDereference() {
    // Direct access to the referent is not currently available.
    return null;
  },

  getOwnPropertyNames() {
    this._ensureProperties();
    return [...this._properties.keys()];
  },

  getEnumerableOwnPropertyNamesForPreview() {
    if (this._preview && this._preview.enumerableOwnProperties) {
      return [...this._preview.enumerableOwnProperties.keys()];
    }
    return this.getOwnPropertyNames();
  },

  getOwnPropertyNamesCount() {
    if (this._preview) {
      return this._preview.ownPropertyNamesCount;
    }
    return this.getOwnPropertyNames().length;
  },

  getOwnPropertySymbols() {
    // Symbol properties are not handled yet.
    return [];
  },

  getOwnPropertyDescriptor(name) {
    if (this._preview) {
      if (this._preview.enumerableOwnProperties) {
        const desc = this._preview.enumerableOwnProperties.get(name);
        if (desc) {
          return this._convertPropertyDescriptor(desc);
        }
      }
      if (name == "length") {
        return this._convertPropertyDescriptor(this._preview.lengthProperty);
      }
      if (name == "displayName") {
        return this._convertPropertyDescriptor(
          this._preview.displayNameProperty
        );
      }
    }
    this._ensureProperties();
    return this._convertPropertyDescriptor(this._properties.get(name));
  },

  _ensureProperties() {
    if (!this._properties) {
      const id = this._data.id;
      const properties = this._dbg._sendRequestAllowDiverge(
        { type: "getObjectProperties", id },
        []
      );
      this._properties = mapify(properties);
    }
  },

  _convertPropertyDescriptor(desc) {
    if (!desc) {
      return undefined;
    }
    const rv = Object.assign({}, desc);
    if ("value" in desc) {
      rv.value = this._dbg._convertValue(desc.value);
    }
    if ("get" in desc) {
      rv.get = this._dbg._getObject(desc.get);
    }
    if ("set" in desc) {
      rv.set = this._dbg._getObject(desc.set);
    }
    return rv;
  },

  unwrap() {
    if (!this.isProxy) {
      return this;
    }
    return this._dbg._convertValue(this._data.proxyUnwrapped);
  },

  get proxyTarget() {
    return this._dbg._convertValue(this._data.proxyTarget);
  },

  get proxyHandler() {
    return this._dbg._convertValue(this._data.proxyHandler);
  },

  get boundTargetFunction() {
    if (this.isBoundFunction) {
      return this._dbg._getObject(this._data.boundTargetFunction);
    }
    return undefined;
  },

  get boundThis() {
    if (this.isBoundFunction) {
      return this._dbg._convertValue(this._data.boundThis);
    }
    return undefined;
  },

  get boundArguments() {
    if (this.isBoundFunction) {
      return this._dbg._getObject(this._data.boundArguments);
    }
    return undefined;
  },

  call(thisv, ...args) {
    return this.apply(thisv, args);
  },

  apply(thisv, args) {
    thisv = this._dbg._convertValueForChild(thisv);
    args = (args || []).map(v => this._dbg._convertValueForChild(v));

    const rv = this._dbg._sendRequestAllowDiverge(
      {
        type: "objectApply",
        id: this._data.id,
        thisv,
        args,
      },
      { throw: "Recording divergence in objectApply" }
    );
    return this._dbg._convertCompletionValue(rv);
  },

  get allocationSite() {
    NYI();
  },
  get errorMessageName() {
    return this._data.errorMessageName;
  },
  get errorNotes() {
    return this._data.errorNotes;
  },
  get errorLineNumber() {
    return this._data.errorLineNumber;
  },
  get errorColumnNumber() {
    return this._data.errorColumnNumber;
  },
  get isPromise() {
    NYI();
  },
  asEnvironment: NYI,
  executeInGlobal: NYI,
  executeInGlobalWithBindings: NYI,

  makeDebuggeeValue: NotAllowed,
  preventExtensions: NotAllowed,
  seal: NotAllowed,
  freeze: NotAllowed,
  defineProperty: NotAllowed,
  defineProperties: NotAllowed,
  deleteProperty: NotAllowed,
  forceLexicalInitializationByName: NotAllowed,
};

ReplayDebugger.Object = ReplayDebuggerObject;

///////////////////////////////////////////////////////////////////////////////
// ReplayDebuggerObjectSnapshot
///////////////////////////////////////////////////////////////////////////////

// Create an object based on snapshot data which can be consulted without
// communicating with the child process. This uses data provided by the child
// process in the same format as for normal ReplayDebuggerObjects, except that
// it does not contain references to any other objects.
function ReplayDebuggerObjectSnapshot(dbg, data) {
  this._dbg = dbg;
  this._data = data;
  this._properties = new Map();
  data.properties.forEach(({ name, desc }) => {
    this._properties.set(name, desc);
  });
}

ReplayDebuggerObjectSnapshot.prototype = ReplayDebuggerObject.prototype;

///////////////////////////////////////////////////////////////////////////////
// ReplayDebuggerEnvironment
///////////////////////////////////////////////////////////////////////////////

function ReplayDebuggerEnvironment(dbg, data) {
  this._dbg = dbg;
  this._data = data;
  this._names = null;
}

ReplayDebuggerEnvironment.prototype = {
  _invalidate() {
    this._data = null;
    this._names = null;
  },

  get type() {
    return this._data.type;
  },
  get parent() {
    return this._dbg._getObject(this._data.parent);
  },
  get object() {
    return this._dbg._getObject(this._data.object);
  },
  get callee() {
    return this._dbg._getObject(this._data.callee);
  },
  get optimizedOut() {
    return this._data.optimizedOut;
  },

  _setNames(names) {
    this._names = {};
    names.forEach(({ name, value }) => {
      this._names[name] = this._dbg._convertValue(value);
    });
  },

  _ensureNames() {
    if (!this._names) {
      const names = this._dbg._sendRequestAllowDiverge(
        {
          type: "getEnvironmentNames",
          id: this._data.id,
        },
        []
      );
      this._setNames(names);
    }
  },

  names() {
    this._ensureNames();
    return Object.keys(this._names);
  },

  getVariable(name) {
    this._ensureNames();
    return this._names[name];
  },

  get inspectable() {
    // All ReplayDebugger environments are inspectable, as all compartments in
    // the replayed process are considered to be debuggees.
    return true;
  },

  find: NYI,
  setVariable: NotAllowed,
};

///////////////////////////////////////////////////////////////////////////////
// Utilities
///////////////////////////////////////////////////////////////////////////////

function dumpv(str) {
  //dump("[ReplayDebugger] " + str + "\n");
}

function NYI() {
  ThrowError("Not yet implemented");
}

function NotAllowed() {
  ThrowError("Not allowed");
}

function ThrowError(msg) {
  const error = new Error(msg);
  dump("ReplayDebugger Server Error: " + msg + " Stack: " + error.stack + "\n");
  throw error;
}

function assert(v) {
  if (!v) {
    ThrowError("Assertion Failed!");
  }
}

function isNonNullObject(obj) {
  return obj && (typeof obj == "object" || typeof obj == "function");
}

function stringify(object) {
  const str = JSON.stringify(object);
  if (str.length >= 4096) {
    return `${str.substr(0, 4096)} TRIMMED ${str.length}`;
  }
  return str;
}

function mapify(object) {
  if (!object) {
    return undefined;
  }
  const map = new Map();
  for (const key of Object.keys(object)) {
    map.set(key, object[key]);
  }
  return map;
}

module.exports = ReplayDebugger;
