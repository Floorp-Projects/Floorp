/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { Cr } = require("chrome");
const { ActorPool, GeneratedLocation } = require("devtools/server/actors/common");
const { createValueGrip } = require("devtools/server/actors/object/utils");
const { longStringGrip } = require("devtools/server/actors/object/long-string");
const { ActorClassWithSpec, Actor } = require("devtools/shared/protocol");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const flags = require("devtools/shared/flags");
const { assert, dumpn } = DevToolsUtils;
const { DevToolsWorker } = require("devtools/shared/worker/worker");
const { threadSpec } = require("devtools/shared/specs/script");

loader.lazyRequireGetter(this, "findCssSelector", "devtools/shared/inspector/css-logic", true);
loader.lazyRequireGetter(this, "BreakpointActor", "devtools/server/actors/breakpoint", true);
loader.lazyRequireGetter(this, "setBreakpointAtEntryPoints", "devtools/server/actors/breakpoint", true);
loader.lazyRequireGetter(this, "EnvironmentActor", "devtools/server/actors/environment", true);
loader.lazyRequireGetter(this, "SourceActorStore", "devtools/server/actors/utils/source-actor-store", true);
loader.lazyRequireGetter(this, "BreakpointActorMap", "devtools/server/actors/utils/breakpoint-actor-map", true);
loader.lazyRequireGetter(this, "PauseScopedObjectActor", "devtools/server/actors/pause-scoped", true);
loader.lazyRequireGetter(this, "EventLoopStack", "devtools/server/actors/utils/event-loop", true);
loader.lazyRequireGetter(this, "FrameActor", "devtools/server/actors/frame", true);
loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");

/**
 * JSD2 actors.
 */

/**
 * Creates a ThreadActor.
 *
 * ThreadActors manage a JSInspector object and manage execution/inspection
 * of debuggees.
 *
 * @param aParent object
 *        This |ThreadActor|'s parent actor. It must implement the following
 *        properties:
 *          - url: The URL string of the debuggee.
 *          - window: The global window object.
 *          - preNest: Function called before entering a nested event loop.
 *          - postNest: Function called after exiting a nested event loop.
 *          - makeDebugger: A function that takes no arguments and instantiates
 *            a Debugger that manages its globals on its own.
 * @param aGlobal object [optional]
 *        An optional (for content debugging only) reference to the content
 *        window.
 */
const ThreadActor = ActorClassWithSpec(threadSpec, {
  initialize: function(parent, global) {
    Actor.prototype.initialize.call(this, parent.conn);
    this._state = "detached";
    this._frameActors = [];
    this._parent = parent;
    this._dbg = null;
    this._gripDepth = 0;
    this._threadLifetimePool = null;
    this._parentClosed = false;
    this._scripts = null;
    this._pauseOnDOMEvents = null;

    this._options = {
      useSourceMaps: false,
      autoBlackBox: false
    };

    this.breakpointActorMap = new BreakpointActorMap();
    this.sourceActorStore = new SourceActorStore();

    this._debuggerSourcesSeen = null;

    // A map of actorID -> actor for breakpoints created and managed by the
    // server.
    this._hiddenBreakpoints = new Map();

    this.global = global;

    this._allEventsListener = this._allEventsListener.bind(this);
    this.onNewGlobal = this.onNewGlobal.bind(this);
    this.onNewSourceEvent = this.onNewSourceEvent.bind(this);
    this.onUpdatedSourceEvent = this.onUpdatedSourceEvent.bind(this);

    this.uncaughtExceptionHook = this.uncaughtExceptionHook.bind(this);
    this.onDebuggerStatement = this.onDebuggerStatement.bind(this);
    this.onNewScript = this.onNewScript.bind(this);
    this.objectGrip = this.objectGrip.bind(this);
    this.pauseObjectGrip = this.pauseObjectGrip.bind(this);
    this._onWindowReady = this._onWindowReady.bind(this);
    EventEmitter.on(this._parent, "window-ready", this._onWindowReady);
    // Set a wrappedJSObject property so |this| can be sent via the observer svc
    // for the xpcshell harness.
    this.wrappedJSObject = this;
  },

  // Used by the ObjectActor to keep track of the depth of grip() calls.
  _gripDepth: null,

  get dbg() {
    if (!this._dbg) {
      this._dbg = this._parent.makeDebugger();
      this._dbg.uncaughtExceptionHook = this.uncaughtExceptionHook;
      this._dbg.onDebuggerStatement = this.onDebuggerStatement;
      this._dbg.onNewScript = this.onNewScript;
      this._dbg.on("newGlobal", this.onNewGlobal);
      // Keep the debugger disabled until a client attaches.
      this._dbg.enabled = this._state != "detached";
    }
    return this._dbg;
  },

  get globalDebugObject() {
    if (!this._parent.window) {
      return null;
    }
    return this.dbg.makeGlobalObjectReference(this._parent.window);
  },

  get state() {
    return this._state;
  },

  get attached() {
    return this.state == "attached" ||
           this.state == "running" ||
           this.state == "paused";
  },

  get threadLifetimePool() {
    if (!this._threadLifetimePool) {
      this._threadLifetimePool = new ActorPool(this.conn);
      this.conn.addActorPool(this._threadLifetimePool);
      this._threadLifetimePool.objectActors = new WeakMap();
    }
    return this._threadLifetimePool;
  },

  get sources() {
    return this._parent.sources;
  },

  get youngestFrame() {
    if (this.state != "paused") {
      return null;
    }
    return this.dbg.getNewestFrame();
  },

  _prettyPrintWorker: null,
  get prettyPrintWorker() {
    if (!this._prettyPrintWorker) {
      this._prettyPrintWorker = new DevToolsWorker(
        "resource://devtools/server/actors/pretty-print-worker.js",
        { name: "pretty-print",
          verbose: flags.wantLogging }
      );
    }
    return this._prettyPrintWorker;
  },

  /**
   * Keep track of all of the nested event loops we use to pause the debuggee
   * when we hit a breakpoint/debugger statement/etc in one place so we can
   * resolve them when we get resume packets. We have more than one (and keep
   * them in a stack) because we can pause within client evals.
   */
  _threadPauseEventLoops: null,
  _pushThreadPause: function() {
    if (!this._threadPauseEventLoops) {
      this._threadPauseEventLoops = [];
    }
    const eventLoop = this._nestedEventLoops.push();
    this._threadPauseEventLoops.push(eventLoop);
    eventLoop.enter();
  },
  _popThreadPause: function() {
    const eventLoop = this._threadPauseEventLoops.pop();
    assert(eventLoop, "Should have an event loop.");
    eventLoop.resolve();
  },

  /**
   * Remove all debuggees and clear out the thread's sources.
   */
  clearDebuggees: function() {
    if (this._dbg) {
      this.dbg.removeAllDebuggees();
    }
    this._sources = null;
    this._scripts = null;
  },

  /**
   * Listener for our |Debugger|'s "newGlobal" event.
   */
  onNewGlobal: function(global) {
    // Notify the client.
    this.conn.send({
      from: this.actorID,
      type: "newGlobal",
      // TODO: after bug 801084 lands see if we need to JSONify this.
      hostAnnotations: global.hostAnnotations
    });
  },

  /**
   * Clean up listeners, debuggees and clear actor pools associated with
   * the lifetime of this actor. This does not destroy the thread actor,
   * it resets it. This is used in methods `onReleaseMany` `onDetatch` and
   * `exit`. The actor is truely destroyed in the `exit method`.
   */
  destroy: function() {
    dumpn("in ThreadActor.prototype.destroy");
    if (this._state == "paused") {
      this.onResume();
    }

    // Blow away our source actor ID store because those IDs are only
    // valid for this connection. This is ok because we never keep
    // things like breakpoints across connections.
    this._sourceActorStore = null;

    EventEmitter.off(this._parent, "window-ready", this._onWindowReady);
    this.sources.off("newSource", this.onNewSourceEvent);
    this.sources.off("updatedSource", this.onUpdatedSourceEvent);
    this.clearDebuggees();
    this.conn.removeActorPool(this._threadLifetimePool);
    this._threadLifetimePool = null;

    if (this._prettyPrintWorker) {
      this._prettyPrintWorker.destroy();
      this._prettyPrintWorker = null;
    }

    if (!this._dbg) {
      return;
    }
    this._dbg.enabled = false;
    this._dbg = null;
  },

  /**
   * destroy the debugger and put the actor in the exited state.
   */
  exit: function() {
    this.destroy();
    this._state = "exited";
    // This actor has a slightly different destroy behavior than other
    // actors using Protocol.js. The thread actor may detach but still
    // be in use, however detach calls the destroy method, even though it
    // expects the actor to still be alive. Therefore, we are calling
    // `Actor.prototype.destroy` in the `exit` method, after its state has
    // been set to "exited", where we are certain that the actor is no
    // longer in use.
    Actor.prototype.destroy.call(this);
  },

  // Request handlers
  onAttach: function(request) {
    if (this.state === "exited") {
      return { type: "exited" };
    }

    if (this.state !== "detached") {
      return { error: "wrongState",
               message: "Current state is " + this.state };
    }

    this._state = "attached";
    this._debuggerSourcesSeen = new WeakSet();

    Object.assign(this._options, request.options || {});
    this.sources.setOptions(this._options);
    this.sources.on("newSource", this.onNewSourceEvent);
    this.sources.on("updatedSource", this.onUpdatedSourceEvent);

    // Initialize an event loop stack. This can't be done in the constructor,
    // because this.conn is not yet initialized by the actor pool at that time.
    this._nestedEventLoops = new EventLoopStack({
      hooks: this._parent,
      connection: this.conn,
      thread: this
    });

    this.dbg.addDebuggees();
    this.dbg.enabled = true;
    try {
      // Put ourselves in the paused state.
      const packet = this._paused();
      if (!packet) {
        return { error: "notAttached" };
      }
      packet.why = { type: "attached" };

      // Send the response to the attach request now (rather than
      // returning it), because we're going to start a nested event loop
      // here.
      this.conn.send(packet);

      // Start a nested event loop.
      this._pushThreadPause();

      // We already sent a response to this request, don't send one
      // now.
      return null;
    } catch (e) {
      reportError(e);
      return { error: "notAttached", message: e.toString() };
    }
  },

  onDetach: function(request) {
    this.destroy();
    this._state = "detached";
    this._debuggerSourcesSeen = null;

    dumpn("ThreadActor.prototype.onDetach: returning 'detached' packet");
    return {
      type: "detached"
    };
  },

  onReconfigure: function(request) {
    if (this.state == "exited") {
      return { error: "wrongState" };
    }
    const options = request.options || {};

    if ("observeAsmJS" in options) {
      this.dbg.allowUnobservedAsmJS = !options.observeAsmJS;
    }
    if ("wasmBinarySource" in options) {
      this.dbg.allowWasmBinarySource = !!options.wasmBinarySource;
    }

    Object.assign(this._options, options);

    // Update the global source store
    this.sources.setOptions(options);

    return {};
  },

  /**
   * Pause the debuggee, by entering a nested event loop, and return a 'paused'
   * packet to the client.
   *
   * @param Debugger.Frame frame
   *        The newest debuggee frame in the stack.
   * @param object reason
   *        An object with a 'type' property containing the reason for the pause.
   * @param function onPacket
   *        Hook to modify the packet before it is sent. Feel free to return a
   *        promise.
   */
  _pauseAndRespond: async function(frame, reason, onPacket = k => k) {
    try {
      const packet = this._paused(frame);
      if (!packet) {
        return undefined;
      }
      packet.why = reason;

      const generatedLocation = this.sources.getFrameLocation(frame);
      this.sources.getOriginalLocation(generatedLocation).then((originalLocation) => {
        if (!originalLocation.originalSourceActor) {
          // The only time the source actor will be null is if there
          // was a sourcemap and it tried to look up the original
          // location but there was no original URL. This is a strange
          // scenario so we simply don't pause.
          DevToolsUtils.reportException(
            "ThreadActor",
            new Error("Attempted to pause in a script with a sourcemap but " +
                      "could not find original location.")
          );
          return undefined;
        }

        packet.frame.where = {
          source: originalLocation.originalSourceActor.form(),
          line: originalLocation.originalLine,
          column: originalLocation.originalColumn
        };

        Promise.resolve(onPacket(packet))
          .catch(error => {
            reportError(error);
            return {
              error: "unknownError",
              message: error.message + "\n" + error.stack
            };
          })
          .then(pkt => {
            this.conn.send(pkt);
          });

        return undefined;
      });

      this._pushThreadPause();
    } catch (e) {
      reportError(e, "Got an exception during TA__pauseAndRespond: ");
    }

    // If the parent actor has been closed, terminate the debuggee script
    // instead of continuing. Executing JS after the content window is gone is
    // a bad idea.
    return this._parentClosed ? null : undefined;
  },

  _makeOnEnterFrame: function({ pauseAndRespond }) {
    return frame => {
      const generatedLocation = this.sources.getFrameLocation(frame);
      const { originalSourceActor } = this.unsafeSynchronize(
        this.sources.getOriginalLocation(generatedLocation)
      );

      const url = originalSourceActor.url;

      if (this.sources.isBlackBoxed(url)) {
        return undefined;
      }

      return pauseAndRespond(frame);
    };
  },

  _makeOnPop: function({ thread, pauseAndRespond, createValueGrip: createValueGripHook,
                          startLocation }) {
    const result = function(completion) {
      // onPop is called with 'this' set to the current frame.
      const generatedLocation = thread.sources.getFrameLocation(this);
      const { originalSourceActor } = thread.unsafeSynchronize(
        thread.sources.getOriginalLocation(generatedLocation)
      );

      const url = originalSourceActor.url;

      if (thread.sources.isBlackBoxed(url)) {
        return undefined;
      }

      // Note that we're popping this frame; we need to watch for
      // subsequent step events on its caller.
      this.reportedPop = true;

      return pauseAndRespond(this, packet => {
        packet.why.frameFinished = {};
        if (!completion) {
          packet.why.frameFinished.terminated = true;
        } else if (completion.hasOwnProperty("return")) {
          packet.why.frameFinished.return = createValueGripHook(completion.return);
        } else if (completion.hasOwnProperty("yield")) {
          packet.why.frameFinished.return = createValueGripHook(completion.yield);
        } else {
          packet.why.frameFinished.throw = createValueGripHook(completion.throw);
        }
        return packet;
      });
    };

    // When stepping out, we don't want to stop at a breakpoint that
    // happened to be set exactly at the spot where we stepped out.
    // See bug 970469.  We record the original location here and check
    // it when a breakpoint is hit.  Furthermore we store this on the
    // function because, while we could store it directly on the
    // frame, if we did we'd also have to find the appropriate spot to
    // clear it.
    result.originalLocation = startLocation;

    return result;
  },

  _makeOnStep: function({ thread, pauseAndRespond, startFrame,
                          startLocation, steppingType }) {
    // Breaking in place: we should always pause.
    if (steppingType === "break") {
      return () => pauseAndRespond(this);
    }

    // Otherwise take what a "step" means into consideration.
    return function() {
      // onStep is called with 'this' set to the current frame.

      // Only allow stepping stops at entry points for the line, when
      // the stepping occurs in a single frame.  The "same frame"
      // check makes it so a sequence of steps can step out of a frame
      // and into subsequent calls in the outer frame.  E.g., if there
      // is a call "a(b())" and the user steps into b, then this
      // condition makes it possible to step out of b and into a.
      if (this === startFrame &&
          !this.script.getOffsetLocation(this.offset).isEntryPoint) {
        return undefined;
      }

      const generatedLocation = thread.sources.getFrameLocation(this);
      const newLocation = thread.unsafeSynchronize(
        thread.sources.getOriginalLocation(generatedLocation)
      );

      // Cases when we should pause because we have executed enough to consider
      // a "step" to have occured:
      //
      // 1.1. We change frames.
      // 1.2. We change URLs (can happen without changing frames thanks to
      //      source mapping).
      // 1.3. The source has pause points and we change locations.
      // 1.4  The source does not have pause points and We change lines.
      //
      // Cases when we should always continue execution, even if one of the
      // above cases is true:
      //
      // 2.1. We are in a source mapped region, but inside a null mapping
      //      (doesn't correlate to any region of original source)
      // 2.2. The source we are in is black boxed.

      // Cases 2.1 and 2.2
      if (newLocation.originalUrl == null
          || thread.sources.isBlackBoxed(newLocation.originalUrl)) {
        return undefined;
      }

      // Cases 1.1, 1.2
      if (this !== startFrame || startLocation.originalUrl !== newLocation.originalUrl) {
        return pauseAndRespond(this);
      }

      const pausePoints = newLocation.originalSourceActor.pausePoints;
      const lineChanged = startLocation.originalLine !== newLocation.originalLine;
      const columnChanged = startLocation.originalColumn !== newLocation.originalColumn;

      if (!pausePoints) {
        // Case 1.4
        if (lineChanged) {
          return pauseAndRespond(this);
        }

        return undefined;
      }

      // Case 1.3
      if (!lineChanged && !columnChanged) {
        return undefined;
      }

      // When pause points are specified for the source,
      // we should pause when we are at a stepOver pause point
      const pausePoint = findPausePointForLocation(pausePoints, newLocation);

      if (pausePoint) {
        if (pausePoint.step) {
          return pauseAndRespond(this);
        }
      } else if (lineChanged) {
        // NOTE: if we do not find a pause point we want to
        // fall back on the old behavior (1.3)
        return pauseAndRespond(this);
      }

      // Otherwise, let execution continue (we haven't executed enough code to
      // consider this a "step" yet).
      return undefined;
    };
  },

  /**
   * Define the JS hook functions for stepping.
   */
  _makeSteppingHooks: function(startLocation, steppingType) {
    // Bind these methods and state because some of the hooks are called
    // with 'this' set to the current frame. Rather than repeating the
    // binding in each _makeOnX method, just do it once here and pass it
    // in to each function.
    const steppingHookState = {
      pauseAndRespond: (frame, onPacket = k=>k) => this._pauseAndRespond(
        frame,
        { type: "resumeLimit" },
        onPacket
      ),
      createValueGrip: v => createValueGrip(v, this._pausePool, this.objectGrip),
      thread: this,
      startFrame: this.youngestFrame,
      startLocation: startLocation,
      steppingType: steppingType
    };

    return {
      onEnterFrame: this._makeOnEnterFrame(steppingHookState),
      onPop: this._makeOnPop(steppingHookState),
      onStep: this._makeOnStep(steppingHookState)
    };
  },

  /**
   * Handle attaching the various stepping hooks we need to attach when we
   * receive a resume request with a resumeLimit property.
   *
   * @param Object request
   *        The request packet received over the RDP.
   * @returns A promise that resolves to true once the hooks are attached, or is
   *          rejected with an error packet.
   */
  _handleResumeLimit: async function(request) {
    const steppingType = request.resumeLimit.type;
    if (!["break", "step", "next", "finish"].includes(steppingType)) {
      return Promise.reject({
        error: "badParameterType",
        message: "Unknown resumeLimit type"
      });
    }

    const generatedLocation = this.sources.getFrameLocation(this.youngestFrame);
    const originalLocation = await this.sources.getOriginalLocation(generatedLocation);
    const { onEnterFrame, onPop, onStep } = this._makeSteppingHooks(
      originalLocation,
      steppingType
    );

    // Make sure there is still a frame on the stack if we are to continue stepping.
    const stepFrame = this._getNextStepFrame(this.youngestFrame);
    if (stepFrame) {
      switch (steppingType) {
        case "step":
          this.dbg.onEnterFrame = onEnterFrame;
          // Fall through.
        case "break":
        case "next":
          if (stepFrame.script) {
            stepFrame.onStep = onStep;
          }
          stepFrame.onPop = onPop;
          break;
        case "finish":
          stepFrame.onPop = onPop;
      }
    }

    return true;
  },

  /**
   * Clear the onStep and onPop hooks from the given frame and all of the frames
   * below it.
   *
   * @param Debugger.Frame aFrame
   *        The frame we want to clear the stepping hooks from.
   */
  _clearSteppingHooks: function(frame) {
    if (frame && frame.live) {
      while (frame) {
        frame.onStep = undefined;
        frame.onPop = undefined;
        frame = frame.older;
      }
    }
  },

  /**
   * Listen to the debuggee's DOM events if we received a request to do so.
   *
   * @param Object request
   *        The resume request packet received over the RDP.
   */
  _maybeListenToEvents: function(request) {
    // Break-on-DOMEvents is only supported in content debugging.
    const events = request.pauseOnDOMEvents;
    if (this.global && events &&
        (events == "*" ||
        (Array.isArray(events) && events.length))) {
      this._pauseOnDOMEvents = events;
      Services.els.addListenerForAllEvents(this.global, this._allEventsListener, true);
    }
  },

  /**
   * If we are tasked with breaking on the load event, we have to add the
   * listener early enough.
   */
  _onWindowReady: function() {
    this._maybeListenToEvents({
      pauseOnDOMEvents: this._pauseOnDOMEvents
    });
  },

  /**
   * Handle a protocol request to resume execution of the debuggee.
   */
  onResume: function(request) {
    if (this._state !== "paused") {
      return {
        error: "wrongState",
        message: "Can't resume when debuggee isn't paused. Current state is '"
          + this._state + "'",
        state: this._state
      };
    }

    // In case of multiple nested event loops (due to multiple debuggers open in
    // different tabs or multiple debugger clients connected to the same tab)
    // only allow resumption in a LIFO order.
    if (this._nestedEventLoops.size && this._nestedEventLoops.lastPausedUrl
        && (this._nestedEventLoops.lastPausedUrl !== this._parent.url
            || this._nestedEventLoops.lastConnection !== this.conn)) {
      return {
        error: "wrongOrder",
        message: "trying to resume in the wrong order.",
        lastPausedUrl: this._nestedEventLoops.lastPausedUrl
      };
    }

    let resumeLimitHandled;
    if (request && request.resumeLimit) {
      resumeLimitHandled = this._handleResumeLimit(request);
    } else {
      this._clearSteppingHooks(this.youngestFrame);
      resumeLimitHandled = Promise.resolve(true);
    }

    return resumeLimitHandled.then(() => {
      if (request) {
        this._options.pauseOnExceptions = request.pauseOnExceptions;
        this._options.ignoreCaughtExceptions = request.ignoreCaughtExceptions;
        this.maybePauseOnExceptions();
        this._maybeListenToEvents(request);
      }

      const packet = this._resumed();
      this._popThreadPause();
      // Tell anyone who cares of the resume (as of now, that's the xpcshell harness and
      // devtools-startup.js when handling the --wait-for-jsdebugger flag)
      if (Services.obs) {
        Services.obs.notifyObservers(this, "devtools-thread-resumed");
      }
      return packet;
    }, error => {
      return error instanceof Error
        ? { error: "unknownError",
            message: DevToolsUtils.safeErrorString(error) }
        // It is a known error, and the promise was rejected with an error
        // packet.
        : error;
    });
  },

  /**
   * Spin up a nested event loop so we can synchronously resolve a promise.
   *
   * DON'T USE THIS UNLESS YOU ABSOLUTELY MUST! Nested event loops suck: the
   * world's state can change out from underneath your feet because JS is no
   * longer run-to-completion.
   *
   * @param p
   *        The promise we want to resolve.
   * @returns The promise's resolution.
   */
  unsafeSynchronize: function(p) {
    let needNest = true;
    let eventLoop;
    let returnVal;

    p.then((resolvedVal) => {
      needNest = false;
      returnVal = resolvedVal;
    })
    .catch((error) => {
      reportError(error, "Error inside unsafeSynchronize:");
    })
    .then(() => {
      if (eventLoop) {
        eventLoop.resolve();
      }
    });

    if (needNest) {
      eventLoop = this._nestedEventLoops.push();
      eventLoop.enter();
    }

    return returnVal;
  },

  /**
   * Set the debugging hook to pause on exceptions if configured to do so.
   */
  maybePauseOnExceptions: function() {
    if (this._options.pauseOnExceptions) {
      this.dbg.onExceptionUnwind = this.onExceptionUnwind.bind(this);
    }
  },

  /**
   * A listener that gets called for every event fired on the page, when a list
   * of interesting events was provided with the pauseOnDOMEvents property. It
   * is used to set server-managed breakpoints on any existing event listeners
   * for those events.
   *
   * @param Event event
   *        The event that was fired.
   */
  _allEventsListener: function(event) {
    if (this._pauseOnDOMEvents == "*" ||
        this._pauseOnDOMEvents.includes(event.type)) {
      for (const listener of this._getAllEventListeners(event.target)) {
        if (event.type == listener.type || this._pauseOnDOMEvents == "*") {
          this._breakOnEnter(listener.script);
        }
      }
    }
  },

  /**
   * Return an array containing all the event listeners attached to the
   * specified event target and its ancestors in the event target chain.
   *
   * @param EventTarget eventTarget
   *        The target the event was dispatched on.
   * @returns Array
   */
  _getAllEventListeners: function(eventTarget) {
    const targets = Services.els.getEventTargetChainFor(eventTarget, true);
    const listeners = [];

    for (const target of targets) {
      const handlers = Services.els.getListenerInfoFor(target);
      for (const handler of handlers) {
        // Null is returned for all-events handlers, and native event listeners
        // don't provide any listenerObject, which makes them not that useful to
        // a JS debugger.
        if (!handler || !handler.listenerObject || !handler.type) {
          continue;
        }
        // Create a listener-like object suitable for our purposes.
        const l = Object.create(null);
        l.type = handler.type;
        const listener = handler.listenerObject;
        let listenerDO = this.globalDebugObject.makeDebuggeeValue(listener);
        // If the listener is not callable, assume it is an event handler object.
        if (!listenerDO.callable) {
          // For some events we don't have permission to access the
          // 'handleEvent' property when running in content scope.
          if (!listenerDO.unwrap()) {
            continue;
          }
          let heDesc;
          while (!heDesc && listenerDO) {
            heDesc = listenerDO.getOwnPropertyDescriptor("handleEvent");
            listenerDO = listenerDO.proto;
          }
          if (heDesc && heDesc.value) {
            listenerDO = heDesc.value;
          }
        }
        // When the listener is a bound function, we are actually interested in
        // the target function.
        while (listenerDO.isBoundFunction) {
          listenerDO = listenerDO.boundTargetFunction;
        }
        l.script = listenerDO.script;
        // Chrome listeners won't be converted to debuggee values, since their
        // compartment is not added as a debuggee.
        if (!l.script) {
          continue;
        }
        listeners.push(l);
      }
    }
    return listeners;
  },

  /**
   * Set a breakpoint on the first line of the given script that has an entry
   * point.
   */
  _breakOnEnter: function(script) {
    const offsets = script.getAllOffsets();
    for (let line = 0, n = offsets.length; line < n; line++) {
      if (offsets[line]) {
        // N.B. Hidden breakpoints do not have an original location, and are not
        // stored in the breakpoint actor map.
        const actor = new BreakpointActor(this);
        this.threadLifetimePool.addActor(actor);

        const scripts = this.dbg.findScripts({ source: script.source, line: line });
        const entryPoints = findEntryPointsForLine(scripts, line);
        setBreakpointAtEntryPoints(actor, entryPoints);
        this._hiddenBreakpoints.set(actor.actorID, actor);
        break;
      }
    }
  },

  /**
   * Helper method that returns the next frame when stepping.
   */
  _getNextStepFrame: function(frame) {
    let stepFrame = frame.reportedPop ? frame.older : frame;
    if (!stepFrame || !stepFrame.script) {
      stepFrame = null;
    }
    return stepFrame;
  },

  onClientEvaluate: function(request) {
    if (this.state !== "paused") {
      return { error: "wrongState",
               message: "Debuggee must be paused to evaluate code." };
    }

    const frame = this._requestFrame(request.frame);
    if (!frame) {
      return { error: "unknownFrame",
               message: "Evaluation frame not found" };
    }

    if (!frame.environment) {
      return { error: "notDebuggee",
               message: "cannot access the environment of this frame." };
    }

    const youngest = this.youngestFrame;

    // Put ourselves back in the running state and inform the client.
    const resumedPacket = this._resumed();
    this.conn.send(resumedPacket);

    // Run the expression.
    // XXX: test syntax errors
    const completion = frame.eval(request.expression);

    // Put ourselves back in the pause state.
    const packet = this._paused(youngest);
    packet.why = { type: "clientEvaluated",
                   frameFinished: this.createProtocolCompletionValue(completion) };

    // Return back to our previous pause's event loop.
    return packet;
  },

  onFrames: function(request) {
    if (this.state !== "paused") {
      return { error: "wrongState",
               message: "Stack frames are only available while the debuggee is paused."};
    }

    const start = request.start ? request.start : 0;
    const count = request.count;

    // Find the starting frame...
    let frame = this.youngestFrame;
    let i = 0;
    while (frame && (i < start)) {
      frame = frame.older;
      i++;
    }

    // Return request.count frames, or all remaining
    // frames if count is not defined.
    const promises = [];
    for (; frame && (!count || i < (start + count)); i++, frame = frame.older) {
      const form = this._createFrameActor(frame).form();
      form.depth = i;

      const framePromise = this.sources.getOriginalLocation(new GeneratedLocation(
        this.sources.createNonSourceMappedActor(frame.script.source),
        form.where.line,
        form.where.column
      )).then((originalLocation) => {
        if (!originalLocation.originalSourceActor) {
          return null;
        }

        const sourceForm = originalLocation.originalSourceActor.form();
        form.where = {
          source: sourceForm,
          line: originalLocation.originalLine,
          column: originalLocation.originalColumn
        };
        form.source = sourceForm;
        return form;
      });
      promises.push(framePromise);
    }

    return Promise.all(promises).then(function(frames) {
      // Filter null values because sourcemapping may have failed.
      return { frames: frames.filter(x => !!x) };
    });
  },

  onReleaseMany: function(request) {
    if (!request.actors) {
      return { error: "missingParameter",
               message: "no actors were specified" };
    }

    let res;
    for (const actorID of request.actors) {
      const actor = this.threadLifetimePool.get(actorID);
      if (!actor) {
        if (!res) {
          res = { error: "notReleasable",
                  message: "Only thread-lifetime actors can be released." };
        }
        continue;
      }

      // We can still have old-style actors (e.g. object/long-string) in the pool, so we
      // need to check onRelease existence.
      if (actor.onRelease) {
        actor.onRelease();
      } else if (actor.destroy) {
        actor.destroy();
      }
    }
    return res ? res : {};
  },

  /**
   * Get the script and source lists from the debugger.
   */
  _discoverSources: function() {
    // Only get one script per Debugger.Source.
    const sourcesToScripts = new Map();
    const scripts = this.dbg.findScripts();

    for (let i = 0, len = scripts.length; i < len; i++) {
      const s = scripts[i];
      if (s.source) {
        sourcesToScripts.set(s.source, s);
      }
    }

    return Promise.all([...sourcesToScripts.values()].map(script => {
      return this.sources.createSourceActors(script.source);
    }));
  },

  onSources: function(request) {
    return this._discoverSources().then(() => {
      // No need to flush the new source packets here, as we are sending the
      // list of sources out immediately and we don't need to invoke the
      // overhead of an RDP packet for every source right now. Let the default
      // timeout flush the buffered packets.

      return {
        sources: this.sources.iter().map(s => s.form())
      };
    });
  },

  /**
   * Disassociate all breakpoint actors from their scripts and clear the
   * breakpoint handlers. This method can be used when the thread actor intends
   * to keep the breakpoint store, but needs to clear any actual breakpoints,
   * e.g. due to a page navigation. This way the breakpoint actors' script
   * caches won't hold on to the Debugger.Script objects leaking memory.
   */
  disableAllBreakpoints: function() {
    for (const bpActor of this.breakpointActorMap.findActors()) {
      bpActor.removeScripts();
    }
  },

  /**
   * Handle a protocol request to pause the debuggee.
   */
  onInterrupt: function(request) {
    if (this.state == "exited") {
      return { type: "exited" };
    } else if (this.state == "paused") {
      // TODO: return the actual reason for the existing pause.
      return { type: "paused", why: { type: "alreadyPaused" } };
    } else if (this.state != "running") {
      return { error: "wrongState",
               message: "Received interrupt request in " + this.state +
                        " state." };
    }

    try {
      // If execution should pause just before the next JavaScript bytecode is
      // executed, just set an onEnterFrame handler.
      if (request.when == "onNext") {
        const onEnterFrame = (frame) => {
          return this._pauseAndRespond(frame, { type: "interrupted", onNext: true });
        };
        this.dbg.onEnterFrame = onEnterFrame;

        return { type: "willInterrupt" };
      }

      // If execution should pause immediately, just put ourselves in the paused
      // state.
      const packet = this._paused();
      if (!packet) {
        return { error: "notInterrupted" };
      }
      packet.why = { type: "interrupted" };

      // Send the response to the interrupt request now (rather than
      // returning it), because we're going to start a nested event loop
      // here.
      this.conn.send(packet);

      // Start a nested event loop.
      this._pushThreadPause();

      // We already sent a response to this request, don't send one
      // now.
      return null;
    } catch (e) {
      reportError(e);
      return { error: "notInterrupted", message: e.toString() };
    }
  },

  /**
   * Handle a protocol request to retrieve all the event listeners on the page.
   */
  onEventListeners: function(request) {
    // This request is only supported in content debugging.
    if (!this.global) {
      return {
        error: "notImplemented",
        message: "eventListeners request is only supported in content debugging"
      };
    }

    let nodes = this.global.document.getElementsByTagName("*");
    nodes = [this.global].concat([].slice.call(nodes));
    const listeners = [];

    for (const node of nodes) {
      const handlers = Services.els.getListenerInfoFor(node);

      for (const handler of handlers) {
        // Create a form object for serializing the listener via the protocol.
        const listenerForm = Object.create(null);
        const listener = handler.listenerObject;
        // Native event listeners don't provide any listenerObject or type and
        // are not that useful to a JS debugger.
        if (!listener || !handler.type) {
          continue;
        }

        // There will be no tagName if the event listener is set on the window.
        const selector = node.tagName ? findCssSelector(node) : "window";
        const nodeDO = this.globalDebugObject.makeDebuggeeValue(node);
        listenerForm.node = {
          selector: selector,
          object: createValueGrip(nodeDO, this._pausePool, this.objectGrip)
        };
        listenerForm.type = handler.type;
        listenerForm.capturing = handler.capturing;
        listenerForm.allowsUntrusted = handler.allowsUntrusted;
        listenerForm.inSystemEventGroup = handler.inSystemEventGroup;
        const handlerName = "on" + listenerForm.type;
        listenerForm.isEventHandler = false;
        if (typeof node.hasAttribute !== "undefined") {
          listenerForm.isEventHandler = !!node.hasAttribute(handlerName);
        }
        if (node[handlerName]) {
          listenerForm.isEventHandler = !!node[handlerName];
        }
        // Get the Debugger.Object for the listener object.
        let listenerDO = this.globalDebugObject.makeDebuggeeValue(listener);
        // If the listener is an object with a 'handleEvent' method, use that.
        if (listenerDO.class === "Object" || /^XUL\w*Element$/.test(listenerDO.class)) {
          // For some events we don't have permission to access the
          // 'handleEvent' property when running in content scope.
          if (!listenerDO.unwrap()) {
            continue;
          }
          let heDesc;
          while (!heDesc && listenerDO) {
            heDesc = listenerDO.getOwnPropertyDescriptor("handleEvent");
            listenerDO = listenerDO.proto;
          }
          if (heDesc && heDesc.value) {
            listenerDO = heDesc.value;
          }
        }
        // When the listener is a bound function, we are actually interested in
        // the target function.
        while (listenerDO.isBoundFunction) {
          listenerDO = listenerDO.boundTargetFunction;
        }
        listenerForm.function = createValueGrip(listenerDO, this._pausePool,
          this.objectGrip);
        listeners.push(listenerForm);
      }
    }
    return { listeners: listeners };
  },

  /**
   * Return the Debug.Frame for a frame mentioned by the protocol.
   */
  _requestFrame: function(frameID) {
    if (!frameID) {
      return this.youngestFrame;
    }

    if (this._framePool.has(frameID)) {
      return this._framePool.get(frameID).frame;
    }

    return undefined;
  },

  _paused: function(frame) {
    // We don't handle nested pauses correctly.  Don't try - if we're
    // paused, just continue running whatever code triggered the pause.
    // We don't want to actually have nested pauses (although we
    // have nested event loops).  If code runs in the debuggee during
    // a pause, it should cause the actor to resume (dropping
    // pause-lifetime actors etc) and then repause when complete.

    if (this.state === "paused") {
      return undefined;
    }

    // Clear stepping hooks.
    this.dbg.onEnterFrame = undefined;
    this.dbg.onExceptionUnwind = undefined;
    if (frame) {
      frame.onStep = undefined;
      frame.onPop = undefined;
    }

    // Clear DOM event breakpoints.
    // XPCShell tests don't use actual DOM windows for globals and cause
    // removeListenerForAllEvents to throw.
    if (!isWorker && this.global && !this.global.toString().includes("Sandbox")) {
      Services.els.removeListenerForAllEvents(this.global, this._allEventsListener, true);
      for (const [, bp] of this._hiddenBreakpoints) {
        bp.delete();
      }
      this._hiddenBreakpoints.clear();
    }

    this._state = "paused";

    // Create the actor pool that will hold the pause actor and its
    // children.
    assert(!this._pausePool, "No pause pool should exist yet");
    this._pausePool = new ActorPool(this.conn);
    this.conn.addActorPool(this._pausePool);

    // Give children of the pause pool a quick link back to the
    // thread...
    this._pausePool.threadActor = this;

    // Create the pause actor itself...
    assert(!this._pauseActor, "No pause actor should exist yet");
    this._pauseActor = new PauseActor(this._pausePool);
    this._pausePool.addActor(this._pauseActor);

    // Update the list of frames.
    const poppedFrames = this._updateFrames();

    // Send off the paused packet and spin an event loop.
    const packet = { from: this.actorID,
                     type: "paused",
                     actor: this._pauseActor.actorID };
    if (frame) {
      packet.frame = this._createFrameActor(frame).form();
    }

    if (poppedFrames) {
      packet.poppedFrames = poppedFrames;
    }

    return packet;
  },

  _resumed: function() {
    this._state = "running";

    // Drop the actors in the pause actor pool.
    this.conn.removeActorPool(this._pausePool);

    this._pausePool = null;
    this._pauseActor = null;

    return { from: this.actorID, type: "resumed" };
  },

  /**
   * Expire frame actors for frames that have been popped.
   *
   * @returns A list of actor IDs whose frames have been popped.
   */
  _updateFrames: function() {
    const popped = [];

    // Create the actor pool that will hold the still-living frames.
    const framePool = new ActorPool(this.conn);
    const frameList = [];

    for (const frameActor of this._frameActors) {
      if (frameActor.frame.live) {
        framePool.addActor(frameActor);
        frameList.push(frameActor);
      } else {
        popped.push(frameActor.actorID);
      }
    }

    // Remove the old frame actor pool, this will expire
    // any actors that weren't added to the new pool.
    if (this._framePool) {
      this.conn.removeActorPool(this._framePool);
    }

    this._frameActors = frameList;
    this._framePool = framePool;
    this.conn.addActorPool(framePool);

    return popped;
  },

  _createFrameActor: function(frame) {
    if (frame.actor) {
      return frame.actor;
    }

    const actor = new FrameActor(frame, this);
    this._frameActors.push(actor);
    this._framePool.addActor(actor);
    frame.actor = actor;

    return actor;
  },

  /**
   * Create and return an environment actor that corresponds to the provided
   * Debugger.Environment.
   * @param Debugger.Environment environment
   *        The lexical environment we want to extract.
   * @param object pool
   *        The pool where the newly-created actor will be placed.
   * @return The EnvironmentActor for environment or undefined for host
   *         functions or functions scoped to a non-debuggee global.
   */
  createEnvironmentActor: function(environment, pool) {
    if (!environment) {
      return undefined;
    }

    if (environment.actor) {
      return environment.actor;
    }

    const actor = new EnvironmentActor(environment, this);
    pool.addActor(actor);
    environment.actor = actor;

    return actor;
  },

  /**
   * Return a protocol completion value representing the given
   * Debugger-provided completion value.
   */
  createProtocolCompletionValue: function(completion) {
    const protoValue = {};
    if (completion == null) {
      protoValue.terminated = true;
    } else if ("return" in completion) {
      protoValue.return = createValueGrip(
        completion.return,
        this._pausePool,
        this.objectGrip
      );
    } else if ("throw" in completion) {
      protoValue.throw = createValueGrip(
        completion.throw,
        this._pausePool,
        this.objectGrip
      );
    } else {
      protoValue.return = createValueGrip(
        completion.yield,
        this._pausePool,
        this.objectGrip
      );
    }
    return protoValue;
  },

  /**
   * Create a grip for the given debuggee object.
   *
   * @param value Debugger.Object
   *        The debuggee object value.
   * @param pool ActorPool
   *        The actor pool where the new object actor will be added.
   */
  objectGrip: function(value, pool) {
    if (!pool.objectActors) {
      pool.objectActors = new WeakMap();
    }

    if (pool.objectActors.has(value)) {
      return pool.objectActors.get(value).form();
    }

    if (this.threadLifetimePool.objectActors.has(value)) {
      return this.threadLifetimePool.objectActors.get(value).form();
    }

    const actor = new PauseScopedObjectActor(value, {
      getGripDepth: () => this._gripDepth,
      incrementGripDepth: () => this._gripDepth++,
      decrementGripDepth: () => this._gripDepth--,
      createValueGrip: v => createValueGrip(v, this._pausePool, this.pauseObjectGrip),
      sources: () => this.sources,
      createEnvironmentActor: (e, p) => this.createEnvironmentActor(e, p),
      promote: () => this.threadObjectGrip(actor),
      isThreadLifetimePool: () => actor.registeredPool !== this.threadLifetimePool,
      getGlobalDebugObject: () => this.globalDebugObject
    }, this.conn);
    pool.addActor(actor);
    pool.objectActors.set(value, actor);
    return actor.form();
  },

  /**
   * Create a grip for the given debuggee object with a pause lifetime.
   *
   * @param value Debugger.Object
   *        The debuggee object value.
   */
  pauseObjectGrip: function(value) {
    if (!this._pausePool) {
      throw new Error("Object grip requested while not paused.");
    }

    return this.objectGrip(value, this._pausePool);
  },

  /**
   * Extend the lifetime of the provided object actor to thread lifetime.
   *
   * @param actor object
   *        The object actor.
   */
  threadObjectGrip: function(actor) {
    // We want to reuse the existing actor ID, so we just remove it from the
    // current pool's weak map and then let pool.addActor do the rest.
    actor.registeredPool.objectActors.delete(actor.obj);
    this.threadLifetimePool.addActor(actor);
    this.threadLifetimePool.objectActors.set(actor.obj, actor);
  },

  /**
   * Handle a protocol request to promote multiple pause-lifetime grips to
   * thread-lifetime grips.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onThreadGrips: function(request) {
    if (this.state != "paused") {
      return { error: "wrongState" };
    }

    if (!request.actors) {
      return { error: "missingParameter",
               message: "no actors were specified" };
    }

    for (const actorID of request.actors) {
      const actor = this._pausePool.get(actorID);
      if (actor) {
        this.threadObjectGrip(actor);
      }
    }
    return {};
  },

  /**
   * Create a long string grip that is scoped to a pause.
   *
   * @param string String
   *        The string we are creating a grip for.
   */
  pauseLongStringGrip: function(string) {
    return longStringGrip(string, this._pausePool);
  },

  /**
   * Create a long string grip that is scoped to a thread.
   *
   * @param string String
   *        The string we are creating a grip for.
   */
  threadLongStringGrip: function(string) {
    return longStringGrip(string, this._threadLifetimePool);
  },

  // JS Debugger API hooks.

  /**
   * A function that the engine calls when a call to a debug event hook,
   * breakpoint handler, watchpoint handler, or similar function throws some
   * exception.
   *
   * @param exception exception
   *        The exception that was thrown in the debugger code.
   */
  uncaughtExceptionHook: function(exception) {
    dumpn("Got an exception: " + exception.message + "\n" + exception.stack);
  },

  /**
   * A function that the engine calls when a debugger statement has been
   * executed in the specified frame.
   *
   * @param frame Debugger.Frame
   *        The stack frame that contained the debugger statement.
   */
  onDebuggerStatement: function(frame) {
    // Don't pause if we are currently stepping (in or over) or the frame is
    // black-boxed.
    const generatedLocation = this.sources.getFrameLocation(frame);
    const { originalSourceActor } = this.unsafeSynchronize(
      this.sources.getOriginalLocation(generatedLocation));
    const url = originalSourceActor ? originalSourceActor.url : null;

    if (this.skipBreakpoints || this.sources.isBlackBoxed(url) || frame.onStep) {
      return undefined;
    }

    return this._pauseAndRespond(frame, { type: "debuggerStatement" });
  },

  onSkipBreakpoints: function({ skip }) {
    this.skipBreakpoints = skip;
    return { skip };
  },

  /**
   * A function that the engine calls when an exception has been thrown and has
   * propagated to the specified frame.
   *
   * @param youngestFrame Debugger.Frame
   *        The youngest remaining stack frame.
   * @param value object
   *        The exception that was thrown.
   */
  onExceptionUnwind: function(youngestFrame, value) {
    let willBeCaught = false;
    for (let frame = youngestFrame; frame != null; frame = frame.older) {
      if (frame.script.isInCatchScope(frame.offset)) {
        willBeCaught = true;
        break;
      }
    }

    if (willBeCaught && this._options.ignoreCaughtExceptions) {
      return undefined;
    }

    // NS_ERROR_NO_INTERFACE exceptions are a special case in browser code,
    // since they're almost always thrown by QueryInterface functions, and
    // handled cleanly by native code.
    if (value == Cr.NS_ERROR_NO_INTERFACE) {
      return undefined;
    }

    const generatedLocation = this.sources.getFrameLocation(youngestFrame);
    const { originalSourceActor } = this.unsafeSynchronize(
      this.sources.getOriginalLocation(generatedLocation));
    const url = originalSourceActor ? originalSourceActor.url : null;

    // We ignore sources without a url because we do not
    // want to pause at console evaluations or watch expressions.
    if (!url || this.skipBreakpoints || this.sources.isBlackBoxed(url)) {
      return undefined;
    }

    try {
      const packet = this._paused(youngestFrame);
      if (!packet) {
        return undefined;
      }

      packet.why = { type: "exception",
                     exception: createValueGrip(value, this._pausePool,
                                                this.objectGrip)
      };
      this.conn.send(packet);

      this._pushThreadPause();
    } catch (e) {
      reportError(e, "Got an exception during TA_onExceptionUnwind: ");
    }

    return undefined;
  },

  /**
   * A function that the engine calls when a new script has been loaded into the
   * scope of the specified debuggee global.
   *
   * @param script Debugger.Script
   *        The source script that has been loaded into a debuggee compartment.
   * @param global Debugger.Object
   *        A Debugger.Object instance whose referent is the global object.
   */
  onNewScript: function(script, global) {
    this._addSource(script.source);
  },

  /**
   * A function called when there's a new source from a thread actor's sources.
   * Emits `newSource` on the target actor.
   *
   * @param {SourceActor} source
   */
  onNewSourceEvent: function(source) {
    const type = "newSource";
    this.conn.send({
      from: this._parent.actorID,
      type,
      source: source.form()
    });

    // For compatibility and debugger still using `newSource` on the thread client,
    // still emit this event here. Clean up in bug 1247084
    this.conn.send({
      from: this.actorID,
      type,
      source: source.form()
    });
  },

  /**
   * A function called when there's an updated source from a thread actor' sources.
   * Emits `updatedSource` on the target actor.
   *
   * @param {SourceActor} source
   */
  onUpdatedSourceEvent: function(source) {
    this.conn.send({
      from: this._parent.actorID,
      type: "updatedSource",
      source: source.form()
    });
  },

  /**
   * Add the provided source to the server cache.
   *
   * @param aSource Debugger.Source
   *        The source that will be stored.
   * @returns true, if the source was added; false otherwise.
   */
  _addSource: function(source) {
    if (!this.sources.allowSource(source)) {
      return false;
    }

    // Preloaded WebExtension content scripts may be cached internally by
    // ExtensionContent.jsm and ThreadActor would ignore them on a page reload
    // because it finds them in the _debuggerSourcesSeen WeakSet,
    // and so we also need to be sure that there is still a source actor for the source.
    if (this._debuggerSourcesSeen.has(source) && this.sources.hasSourceActor(source)) {
      return false;
    }

    const sourceActor = this.sources.createNonSourceMappedActor(source);
    const bpActors = [...this.breakpointActorMap.findActors()];

    if (this._options.useSourceMaps) {
      const promises = [];

      // Go ahead and establish the source actors for this script, which
      // fetches sourcemaps if available and sends onNewSource
      // notifications.
      const sourceActorsCreated = this.sources._createSourceMappedActors(source);

      if (bpActors.length) {
        // We need to use unsafeSynchronize here because if the page is being reloaded,
        // this call will replace the previous set of source actors for this source
        // with a new one. If the source actors have not been replaced by the time
        // we try to reset the breakpoints below, their location objects will still
        // point to the old set of source actors, which point to different
        // scripts.
        this.unsafeSynchronize(sourceActorsCreated);
      }

      for (const actor of bpActors) {
        if (actor.isPending) {
          promises.push(actor.originalLocation.originalSourceActor._setBreakpoint(actor));
        } else {
          promises.push(
            this.sources.getAllGeneratedLocations(actor.originalLocation).then(
              (generatedLocations) => {
                if (generatedLocations.length > 0 &&
                    generatedLocations[0].generatedSourceActor
                                         .actorID === sourceActor.actorID) {
                  sourceActor._setBreakpointAtAllGeneratedLocations(
                    actor, generatedLocations);
                }
              }));
        }
      }

      if (promises.length > 0) {
        this.unsafeSynchronize(Promise.all(promises));
      }
    } else {
      // Bug 1225160: If addSource is called in response to a new script
      // notification, and this notification was triggered by loading a JSM from
      // chrome code, calling unsafeSynchronize could cause a debuggee timer to
      // fire. If this causes the JSM to be loaded a second time, the browser
      // will crash, because loading JSMS is not reentrant, and the first load
      // has not completed yet.
      //
      // The root of the problem is that unsafeSynchronize can cause debuggee
      // code to run. Unfortunately, fixing that is prohibitively difficult. The
      // best we can do at the moment is disable source maps for the browser
      // debugger, and carefully avoid the use of unsafeSynchronize in this
      // function when source maps are disabled.
      for (const actor of bpActors) {
        if (actor.isPending) {
          actor.originalLocation.originalSourceActor._setBreakpoint(actor);
        } else {
          actor.originalLocation.originalSourceActor._setBreakpointAtGeneratedLocation(
            actor, GeneratedLocation.fromOriginalLocation(actor.originalLocation)
          );
        }
      }
    }

    this._debuggerSourcesSeen.add(source);
    return true;
  },

  /**
   * Get prototypes and properties of multiple objects.
   */
  onPrototypesAndProperties: function(request) {
    const result = {};
    for (const actorID of request.actors) {
      // This code assumes that there are no lazily loaded actors returned
      // by this call.
      const actor = this.conn.getActor(actorID);
      if (!actor) {
        return { from: this.actorID,
                 error: "noSuchActor" };
      }
      const handler = actor.prototypeAndProperties;
      if (!handler) {
        return { from: this.actorID,
                 error: "unrecognizedPacketType",
                 message: ('Actor "' + actorID +
                           '" does not recognize the packet type ' +
                           '"prototypeAndProperties"') };
      }
      result[actorID] = handler.call(actor, {});
    }
    return { from: this.actorID,
             actors: result };
  }
});

Object.assign(ThreadActor.prototype.requestTypes, {
  "attach": ThreadActor.prototype.onAttach,
  "detach": ThreadActor.prototype.onDetach,
  "reconfigure": ThreadActor.prototype.onReconfigure,
  "resume": ThreadActor.prototype.onResume,
  "clientEvaluate": ThreadActor.prototype.onClientEvaluate,
  "frames": ThreadActor.prototype.onFrames,
  "interrupt": ThreadActor.prototype.onInterrupt,
  "eventListeners": ThreadActor.prototype.onEventListeners,
  "releaseMany": ThreadActor.prototype.onReleaseMany,
  "sources": ThreadActor.prototype.onSources,
  "threadGrips": ThreadActor.prototype.onThreadGrips,
  "prototypesAndProperties": ThreadActor.prototype.onPrototypesAndProperties,
  "skipBreakpoints": ThreadActor.prototype.onSkipBreakpoints
});

exports.ThreadActor = ThreadActor;

/**
 * Creates a PauseActor.
 *
 * PauseActors exist for the lifetime of a given debuggee pause.  Used to
 * scope pause-lifetime grips.
 *
 * @param ActorPool aPool
 *        The actor pool created for this pause.
 */
function PauseActor(pool) {
  this.pool = pool;
}

PauseActor.prototype = {
  actorPrefix: "pause"
};

/**
 * Creates an actor for handling chrome debugging. ChromeDebuggerActor is a
 * thin wrapper over ThreadActor, slightly changing some of its behavior.
 *
 * @param connection object
 *        The DebuggerServerConnection with which this ChromeDebuggerActor
 *        is associated. (Currently unused, but required to make this
 *        constructor usable with addGlobalActor.)
 *
 * @param parent object
 *        This actor's parent actor. See ThreadActor for a list of expected
 *        properties.
 */
function ChromeDebuggerActor(connection, parent) {
  ThreadActor.prototype.initialize.call(this, parent);
}

ChromeDebuggerActor.prototype = Object.create(ThreadActor.prototype);

Object.assign(ChromeDebuggerActor.prototype, {
  constructor: ChromeDebuggerActor,

  // A constant prefix that will be used to form the actor ID by the server.
  actorPrefix: "chromeDebugger"
});

exports.ChromeDebuggerActor = ChromeDebuggerActor;

/**
 * Creates an actor for handling add-on debugging. AddonThreadActor is
 * a thin wrapper over ThreadActor.
 *
 * @param connection object
 *        The DebuggerServerConnection with which this AddonThreadActor
 *        is associated. (Currently unused, but required to make this
 *        constructor usable with addGlobalActor.)
 *
 * @param parent object
 *        This actor's parent actor. See ThreadActor for a list of expected
 *        properties.
 */
function AddonThreadActor(connection, parent) {
  ThreadActor.prototype.initialize.call(this, parent);
}

AddonThreadActor.prototype = Object.create(ThreadActor.prototype);

Object.assign(AddonThreadActor.prototype, {
  constructor: AddonThreadActor,

  // A constant prefix that will be used to form the actor ID by the server.
  actorPrefix: "addonThread"
});

exports.AddonThreadActor = AddonThreadActor;

// Utility functions.

/**
 * Report the given error in the error console and to stdout.
 *
 * @param Error error
 *        The error object you wish to report.
 * @param String prefix
 *        An optional prefix for the reported error message.
 */
var oldReportError = reportError;
this.reportError = function(error, prefix = "") {
  assert(error instanceof Error, "Must pass Error objects to reportError");
  const msg = prefix + error.message + ":\n" + error.stack;
  oldReportError(msg);
  dumpn(msg);
};

/**
 * Find the scripts which contain offsets that are an entry point to the given
 * line.
 *
 * @param Array scripts
 *        The set of Debugger.Scripts to consider.
 * @param Number line
 *        The line we are searching for entry points into.
 * @returns Array of objects of the form { script, offsets } where:
 *          - script is a Debugger.Script
 *          - offsets is an array of offsets that are entry points into the
 *            given line.
 */
function findEntryPointsForLine(scripts, line) {
  const entryPoints = [];
  for (const script of scripts) {
    const offsets = script.getLineOffsets(line);
    if (offsets.length) {
      entryPoints.push({ script, offsets });
    }
  }
  return entryPoints;
}

function findPausePointForLocation(pausePoints, location) {
  const { originalLine: line, originalColumn: column } = location;
  return pausePoints[line] && pausePoints[line][column];
}

/**
 * Unwrap a global that is wrapped in a |Debugger.Object|, or if the global has
 * become a dead object, return |undefined|.
 *
 * @param Debugger.Object wrappedGlobal
 *        The |Debugger.Object| which wraps a global.
 *
 * @returns {Object|undefined}
 *          Returns the unwrapped global object or |undefined| if unwrapping
 *          failed.
 */
exports.unwrapDebuggerObjectGlobal = wrappedGlobal => {
  try {
    // Because of bug 991399 we sometimes get nuked window references here. We
    // just bail out in that case.
    //
    // Note that addon sandboxes have a DOMWindow as their prototype. So make
    // sure that we can touch the prototype too (whatever it is), in case _it_
    // is it a nuked window reference. We force stringification to make sure
    // that any dead object proxies make themselves known.
    const global = wrappedGlobal.unsafeDereference();
    Object.getPrototypeOf(global) + "";
    return global;
  } catch (e) {
    return undefined;
  }
};
