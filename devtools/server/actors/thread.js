/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// protocol.js uses objects as exceptions in order to define
// error packets.
/* eslint-disable no-throw-literal */

const DebuggerNotificationObserver = require("DebuggerNotificationObserver");
const Services = require("Services");
const { Cr, Ci } = require("chrome");
const { Pool } = require("devtools/shared/protocol/Pool");
const { createValueGrip } = require("devtools/server/actors/object/utils");
const { ActorClassWithSpec, Actor } = require("devtools/shared/protocol");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const Debugger = require("Debugger");
const { assert, dumpn, reportException } = DevToolsUtils;
const { threadSpec } = require("devtools/shared/specs/thread");
const {
  getAvailableEventBreakpoints,
  eventBreakpointForNotification,
  makeEventBreakpointMessage,
} = require("devtools/server/actors/utils/event-breakpoints");
const {
  WatchpointMap,
} = require("devtools/server/actors/utils/watchpoint-map");

const { logEvent } = require("devtools/server/actors/utils/logEvent");

loader.lazyRequireGetter(
  this,
  "EnvironmentActor",
  "devtools/server/actors/environment",
  true
);
loader.lazyRequireGetter(
  this,
  "BreakpointActorMap",
  "devtools/server/actors/utils/breakpoint-actor-map",
  true
);
loader.lazyRequireGetter(
  this,
  "PauseScopedObjectActor",
  "devtools/server/actors/pause-scoped",
  true
);
loader.lazyRequireGetter(
  this,
  "EventLoopStack",
  "devtools/server/actors/utils/event-loop",
  true
);
loader.lazyRequireGetter(
  this,
  "FrameActor",
  "devtools/server/actors/frame",
  true
);
loader.lazyRequireGetter(
  this,
  "getSavedFrameParent",
  "devtools/server/actors/frame",
  true
);
loader.lazyRequireGetter(
  this,
  "isValidSavedFrame",
  "devtools/server/actors/frame",
  true
);
loader.lazyRequireGetter(
  this,
  "HighlighterEnvironment",
  "devtools/server/actors/highlighters",
  true
);
loader.lazyRequireGetter(
  this,
  "PausedDebuggerOverlay",
  "devtools/server/actors/highlighters/paused-debugger",
  true
);

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
 *          - dbg: a Debugger instance that manages its globals on its own.
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
    this._xhrBreakpoints = [];
    this._observingNetwork = false;
    this._activeEventBreakpoints = new Set();
    this._activeEventPause = null;
    this._pauseOverlay = null;
    this._priorPause = null;
    this._frameActorMap = new WeakMap();

    this._watchpointsMap = new WatchpointMap(this);

    this._options = {
      autoBlackBox: false,
      skipBreakpoints: false,
    };

    this.breakpointActorMap = new BreakpointActorMap(this);

    this._debuggerSourcesSeen = null;

    // A Set of URLs string to watch for when new sources are found by
    // the debugger instance.
    this._onLoadBreakpointURLs = new Set();

    // A WeakMap from Debugger.Frame to an exception value which will be ignored
    // when deciding to pause if the value is thrown by the frame. When we are
    // pausing on exceptions then we only want to pause when the youngest frame
    // throws a particular exception, instead of for all older frames as well.
    this._handledFrameExceptions = new WeakMap();

    this.global = global;

    this.onNewSourceEvent = this.onNewSourceEvent.bind(this);

    this.createCompletionGrip = this.createCompletionGrip.bind(this);
    this.onDebuggerStatement = this.onDebuggerStatement.bind(this);
    this.onNewScript = this.onNewScript.bind(this);
    this.objectGrip = this.objectGrip.bind(this);
    this.pauseObjectGrip = this.pauseObjectGrip.bind(this);
    this._onOpeningRequest = this._onOpeningRequest.bind(this);
    this._onNewDebuggee = this._onNewDebuggee.bind(this);
    this._eventBreakpointListener = this._eventBreakpointListener.bind(this);
    this._onWindowReady = this._onWindowReady.bind(this);
    this._onWillNavigate = this._onWillNavigate.bind(this);
    this._onNavigate = this._onNavigate.bind(this);

    this._parent.on("window-ready", this._onWindowReady);
    this._parent.on("will-navigate", this._onWillNavigate);
    this._parent.on("navigate", this._onNavigate);

    this._debuggerNotificationObserver = new DebuggerNotificationObserver();

    if (Services.obs) {
      // Set a wrappedJSObject property so |this| can be sent via the observer svc
      // for the xpcshell harness.
      this.wrappedJSObject = this;
      Services.obs.notifyObservers(this, "devtools-thread-instantiated");
    }
  },

  // Used by the ObjectActor to keep track of the depth of grip() calls.
  _gripDepth: null,

  get dbg() {
    if (!this._dbg) {
      this._dbg = this._parent.dbg;
      // Keep the debugger disabled until a client attaches.
      if (this._state === "detached") {
        this._dbg.disable();
      } else {
        this._dbg.enable();
      }
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
    return (
      this.state == "attached" ||
      this.state == "running" ||
      this.state == "paused"
    );
  },

  get threadLifetimePool() {
    if (!this._threadLifetimePool) {
      this._threadLifetimePool = new Pool(this.conn, "thread");
      this._threadLifetimePool.objectActors = new WeakMap();
    }
    return this._threadLifetimePool;
  },

  getThreadLifetimeObject(raw) {
    return this.threadLifetimePool.objectActors.get(raw);
  },

  createValueGrip(value) {
    return createValueGrip(value, this.threadLifetimePool, this.objectGrip);
  },

  get sources() {
    return this._parent.sources;
  },

  get breakpoints() {
    return this._parent.breakpoints;
  },

  get youngestFrame() {
    if (this.state != "paused") {
      return null;
    }
    return this.dbg.getNewestFrame();
  },

  get skipBreakpoints() {
    return (
      this._options.skipBreakpoints ||
      (this.insideClientEvaluation && this.insideClientEvaluation.eager)
    );
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

  isPaused() {
    return this._state === "paused";
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
   * Clean up listeners, debuggees and clear actor pools associated with
   * the lifetime of this actor. This does not destroy the thread actor,
   * it resets it. This is used in methods `onDetatch` and
   * `exit`. The actor is truely destroyed in the `exit method`.
   */
  destroy: function() {
    dumpn("in ThreadActor.prototype.destroy");
    if (this._state == "paused") {
      this.doResume();
    }

    this.removeAllWatchpoints();
    this._xhrBreakpoints = [];
    this._updateNetworkObserver();

    this._activeEventBreakpoints = new Set();
    this._debuggerNotificationObserver.removeListener(
      this._eventBreakpointListener
    );

    for (const global of this.dbg.getDebuggees()) {
      try {
        this._debuggerNotificationObserver.disconnect(
          global.unsafeDereference()
        );
      } catch (e) {}
    }

    this._parent.off("window-ready", this._onWindowReady);
    this._parent.off("will-navigate", this._onWillNavigate);
    this._parent.off("navigate", this._onNavigate);

    this.sources.off("newSource", this.onNewSourceEvent);
    this.clearDebuggees();
    this._threadLifetimePool.destroy();
    this._threadLifetimePool = null;
    this._dbg = null;
    // Note: here we don't call Actor.prototype.destroy.call(this)
    // because the real "destroy" method is `exit()`, where
    // Actor.prototype.destroy.call(this) will be called.
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
  onAttach: function({ options }) {
    if (this.state === "exited") {
      throw {
        error: "exited",
        message: "threadActor has exited",
      };
    }

    if (this.state !== "detached") {
      throw {
        error: "wrongState",
        message: "Current state is " + this.state,
      };
    }

    this._state = "attached";
    this.dbg.onDebuggerStatement = this.onDebuggerStatement;
    this.dbg.onNewScript = this.onNewScript;
    this.dbg.onNewDebuggee = this._onNewDebuggee;

    this._debuggerSourcesSeen = new WeakSet();

    this._options = { ...this._options, ...options };
    this.sources.setOptions(this._options);
    this.sources.on("newSource", this.onNewSourceEvent);

    // Initialize an event loop stack. This can't be done in the constructor,
    // because this.conn is not yet initialized by the actor pool at that time.
    this._nestedEventLoops = new EventLoopStack({
      thread: this,
    });

    if (options.breakpoints) {
      this._setBreakpointsOnAttach(options.breakpoints);
    }
    if (options.eventBreakpoints) {
      this.setActiveEventBreakpoints(options.eventBreakpoints);
    }

    this.dbg.enable();

    if ("observeAsmJS" in this._options) {
      this.dbg.allowUnobservedAsmJS = !this._options.observeAsmJS;
    }

    // Notify the parent that we've finished attaching. If this is a worker
    // thread which was paused until attaching, this will allow content to
    // begin executing.
    if (this._parent.onThreadAttached) {
      this._parent.onThreadAttached();
    }

    try {
      // Put ourselves in the paused state.
      const packet = this._paused();
      if (!packet) {
        throw {
          error: "notAttached",
          message: "cannot attach, could not create pause packet",
        };
      }
      packet.why = { type: "attached" };

      // Send the response to the attach request now (rather than
      // returning it), because we're going to start a nested event
      // loop here.
      this.conn.send({ from: this.actorID });
      this.emit("paused", packet);

      // Start a nested event loop.
      this._pushThreadPause();

      // We already sent a response to this request via this.conn.send(),
      // don't send one now. But protocol.js probably still emits a second
      // empty packet.
    } catch (e) {
      reportException("DBG-SERVER", e);
      throw {
        error: "notAttached",
        message: e.toString(),
      };
    }
  },

  toggleEventLogging(logEventBreakpoints) {
    this._options.logEventBreakpoints = logEventBreakpoints;
    return this._options.logEventBreakpoints;
  },

  _setBreakpointsOnAttach(breakpoints) {
    for (const { location, options } of Object.values(breakpoints)) {
      this.setBreakpoint(location, options);
    }
  },

  get pauseOverlay() {
    if (this._pauseOverlay) {
      return this._pauseOverlay;
    }

    const env = new HighlighterEnvironment();
    env.initFromTargetActor(this._parent);
    const highlighter = new PausedDebuggerOverlay(env, {
      resume: () => this.onResume({ resumeLimit: null }),
      stepOver: () => this.onResume({ resumeLimit: { type: "next" } }),
    });
    this._pauseOverlay = highlighter;
    return highlighter;
  },

  showOverlay() {
    if (
      this._options.shouldShowOverlay &&
      this.isPaused() &&
      this._parent.on &&
      this._parent.window.document &&
      !this._parent.window.isChromeWindow &&
      this.pauseOverlay
    ) {
      const reason = this._priorPause.why.type;
      this.pauseOverlay.show(null, { reason });
    }
  },

  hideOverlay(msg) {
    if (
      this._parent.window.document &&
      this._parent.on &&
      !this._parent.window.isChromeWindow
    ) {
      this.pauseOverlay.hide();
    }
  },

  /**
   * Tell the thread to automatically add a breakpoint on the first line of
   * a given file, when it is first loaded.
   *
   * This is currently only used by the xpcshell test harness, and unless
   * we decide to expand the scope of this feature, we should keep it that way.
   */
  setBreakpointOnLoad(urls) {
    this._onLoadBreakpointURLs = new Set(urls);
  },

  _findXHRBreakpointIndex(p, m) {
    return this._xhrBreakpoints.findIndex(
      ({ path, method }) => path === p && method === m
    );
  },

  // We clear the priorPause field when a breakpoint is added or removed
  // at the same location because we are no longer worried about pausing twice
  // at that location (e.g. debugger statement, stepping).
  _maybeClearPriorPause(location) {
    if (!this._priorPause) {
      return;
    }

    const { where } = this._priorPause.frame;
    if (where.line === location.line && where.column === location.column) {
      this._priorPause = null;
    }
  },

  setBreakpoint: async function(location, options) {
    const actor = this.breakpointActorMap.getOrCreateBreakpointActor(location);
    actor.setOptions(options);
    this._maybeClearPriorPause(location);

    if (location.sourceUrl) {
      // There can be multiple source actors for a URL if there are multiple
      // inline sources on an HTML page.
      const sourceActors = this.sources.getSourceActorsByURL(
        location.sourceUrl
      );
      for (const sourceActor of sourceActors) {
        await sourceActor.applyBreakpoint(actor);
      }
    } else {
      const sourceActor = this.sources.getSourceActorById(location.sourceId);
      if (sourceActor) {
        await sourceActor.applyBreakpoint(actor);
      }
    }
  },

  removeBreakpoint(location) {
    const actor = this.breakpointActorMap.getOrCreateBreakpointActor(location);
    this._maybeClearPriorPause(location);
    actor.delete();
  },

  removeXHRBreakpoint: function(path, method) {
    const index = this._findXHRBreakpointIndex(path, method);

    if (index >= 0) {
      this._xhrBreakpoints.splice(index, 1);
    }
    return this._updateNetworkObserver();
  },

  setXHRBreakpoint: function(path, method) {
    // request.path is a string,
    // If requested url contains the path, then we pause.
    const index = this._findXHRBreakpointIndex(path, method);

    if (index === -1) {
      this._xhrBreakpoints.push({ path, method });
    }
    return this._updateNetworkObserver();
  },

  getAvailableEventBreakpoints: function() {
    return getAvailableEventBreakpoints();
  },
  getActiveEventBreakpoints: function() {
    return Array.from(this._activeEventBreakpoints);
  },
  setActiveEventBreakpoints: function(ids) {
    this._activeEventBreakpoints = new Set(ids);

    if (this._activeEventBreakpoints.size === 0) {
      this._debuggerNotificationObserver.removeListener(
        this._eventBreakpointListener
      );
    } else {
      this._debuggerNotificationObserver.addListener(
        this._eventBreakpointListener
      );
    }
  },

  _onNewDebuggee(global) {
    try {
      this._debuggerNotificationObserver.connect(global.unsafeDereference());
    } catch (e) {}
  },

  _updateNetworkObserver() {
    // Workers don't have access to `Services` and even if they did, network
    // requests are all dispatched to the main thread, so there would be
    // nothing here to listen for. We'll need to revisit implementing
    // XHR breakpoints for workers.
    if (isWorker) {
      return false;
    }

    if (this._xhrBreakpoints.length > 0 && !this._observingNetwork) {
      this._observingNetwork = true;
      Services.obs.addObserver(
        this._onOpeningRequest,
        "http-on-opening-request"
      );
    } else if (this._xhrBreakpoints.length === 0 && this._observingNetwork) {
      this._observingNetwork = false;
      Services.obs.removeObserver(
        this._onOpeningRequest,
        "http-on-opening-request"
      );
    }

    return true;
  },

  _onOpeningRequest: function(subject) {
    if (this.skipBreakpoints) {
      return;
    }

    const channel = subject.QueryInterface(Ci.nsIHttpChannel);
    const url = channel.URI.asciiSpec;
    const requestMethod = channel.requestMethod;

    let causeType = Ci.nsIContentPolicy.TYPE_OTHER;
    if (channel.loadInfo) {
      causeType = channel.loadInfo.externalContentPolicyType;
    }

    const isXHR =
      causeType === Ci.nsIContentPolicy.TYPE_XMLHTTPREQUEST ||
      causeType === Ci.nsIContentPolicy.TYPE_FETCH;

    if (!isXHR) {
      // We currently break only if the request is either fetch or xhr
      return;
    }

    let shouldPause = false;
    for (const { path, method } of this._xhrBreakpoints) {
      if (method !== "ANY" && method !== requestMethod) {
        continue;
      }
      if (url.includes(path)) {
        shouldPause = true;
        break;
      }
    }

    if (shouldPause) {
      const frame = this.dbg.getNewestFrame();

      // If there is no frame, this request was dispatched by logic that isn't
      // primarily JS, so pausing the event loop wouldn't make sense.
      // This covers background requests like loading the initial page document,
      // or loading favicons. This also includes requests dispatched indirectly
      // from workers. We'll need to handle them separately in the future.
      if (frame) {
        this._pauseAndRespond(frame, { type: "XHR" });
      }
    }
  },

  onDetach: function(request) {
    this.destroy();
    this._state = "detached";
    this._debuggerSourcesSeen = null;

    dumpn("ThreadActor.prototype.onDetach: returning 'detached' packet");
    this.emit("detached");
    return {};
  },

  onReconfigure: function(request) {
    if (this.state == "exited") {
      return { error: "wrongState" };
    }
    const options = request.options || {};

    if ("observeAsmJS" in options) {
      this.dbg.allowUnobservedAsmJS = !options.observeAsmJS;
    }

    if ("pauseWorkersUntilAttach" in options) {
      if (this._parent.pauseWorkersUntilAttach) {
        this._parent.pauseWorkersUntilAttach(options.pauseWorkersUntilAttach);
      }
    }

    this._options = { ...this._options, ...options };

    // Update the global source store
    this.sources.setOptions(options);

    return {};
  },

  _eventBreakpointListener(notification) {
    if (this._state === "paused" || this._state === "detached") {
      return;
    }

    const eventBreakpoint = eventBreakpointForNotification(
      this.dbg,
      notification
    );

    if (!this._activeEventBreakpoints.has(eventBreakpoint)) {
      return;
    }

    if (notification.phase === "pre" && !this._activeEventPause) {
      this._activeEventPause = this._captureDebuggerHooks();

      this.dbg.onEnterFrame = this._makeEventBreakpointEnterFrame(
        eventBreakpoint
      );
    } else if (notification.phase === "post" && this._activeEventPause) {
      this._restoreDebuggerHooks(this._activeEventPause);
      this._activeEventPause = null;
    } else if (!notification.phase && !this._activeEventPause) {
      const frame = this.dbg.getNewestFrame();
      if (frame) {
        const { sourceActor } = this.sources.getFrameLocation(frame);
        const { url } = sourceActor;
        if (this.sources.isBlackBoxed(url)) {
          return;
        }

        this._pauseAndRespondEventBreakpoint(frame, eventBreakpoint);
      }
    }
  },

  _makeEventBreakpointEnterFrame(eventBreakpoint) {
    return frame => {
      const location = this.sources.getFrameLocation(frame);
      const { sourceActor, line, column } = location;
      const { url } = sourceActor;

      if (this.sources.isBlackBoxed(url, line, column)) {
        return undefined;
      }

      this._restoreDebuggerHooks(this._activeEventPause);
      this._activeEventPause = null;

      return this._pauseAndRespondEventBreakpoint(frame, eventBreakpoint);
    };
  },

  _pauseAndRespondEventBreakpoint(frame, eventBreakpoint) {
    if (this.skipBreakpoints) {
      return undefined;
    }

    if (this._options.logEventBreakpoints) {
      return logEvent({
        threadActor: this,
        frame,
        level: "logPoint",
        expression: `[_event]`,
        bindings: { _event: frame.arguments[0] },
      });
    }

    return this._pauseAndRespond(frame, {
      type: "eventBreakpoint",
      breakpoint: eventBreakpoint,
      message: makeEventBreakpointMessage(eventBreakpoint),
    });
  },

  _captureDebuggerHooks() {
    return {
      onEnterFrame: this.dbg.onEnterFrame,
      onStep: this.dbg.onStep,
      onPop: this.dbg.onPop,
    };
  },

  _restoreDebuggerHooks(hooks) {
    this.dbg.onEnterFrame = hooks.onEnterFrame;
    this.dbg.onStep = hooks.onStep;
    this.dbg.onPop = hooks.onPop;
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
  _pauseAndRespond: function(frame, reason, onPacket = k => k) {
    try {
      const packet = this._paused(frame);
      if (!packet) {
        return undefined;
      }

      const { sourceActor, line, column } = this.sources.getFrameLocation(
        frame
      );

      packet.why = reason;

      if (this.suspendedFrame) {
        this.suspendedFrame.onStep = undefined;
        this.suspendedFrame.onPop = undefined;
        this.suspendedFrame = undefined;
      }

      if (!sourceActor) {
        // If the frame location is in a source that not pass the 'allowSource'
        // check and thus has no actor, we do not bother pausing.
        return undefined;
      }

      packet.frame.where = {
        actor: sourceActor.actorID,
        line: line,
        column: column,
      };
      const pkt = onPacket(packet);

      this._priorPause = pkt;
      this.emit("paused", pkt);
      this.showOverlay();
    } catch (error) {
      reportException("DBG-SERVER", error);
      this.conn.send({
        error: "unknownError",
        message: error.message + "\n" + error.stack,
      });
      return undefined;
    }

    try {
      this._pushThreadPause();
    } catch (e) {
      reportException("TA__pauseAndRespond", e);
    }

    // If the parent actor has been closed, terminate the debuggee script
    // instead of continuing. Executing JS after the content window is gone is
    // a bad idea.
    return this._parentClosed ? null : undefined;
  },

  _makeOnEnterFrame: function({ pauseAndRespond }) {
    return frame => {
      // Continue forward until we get to a valid step target.
      const { onStep, onPop } = this._makeSteppingHooks({
        steppingType: "next",
      });

      if (this.sources.isFrameBlackBoxed(frame)) {
        return undefined;
      }

      frame.onStep = onStep;
      frame.onPop = onPop;
      return undefined;
    };
  },

  _makeOnPop: function({ pauseAndRespond, steppingType }) {
    const thread = this;
    return function(completion) {
      // onPop is called when we temporarily leave an async/generator
      if (completion.await || completion.yield) {
        thread.suspendedFrame = this;
        thread.dbg.onEnterFrame = undefined;
        return undefined;
      }

      // Note that we're popping this frame; we need to watch for
      // subsequent step events on its caller.
      this.reportedPop = true;

      if (steppingType != "finish" && !thread.sources.isFrameBlackBoxed(this)) {
        return pauseAndRespond(this, packet =>
          thread.createCompletionGrip(packet, completion)
        );
      }

      thread._attachSteppingHooks(this, "next", completion);
      return undefined;
    };
  },

  hasMoved: function(frame, newType) {
    const newLocation = this.sources.getFrameLocation(frame);

    if (!this._priorPause) {
      return true;
    }

    // Recursion/Loops makes it okay to resume and land at
    // the same breakpoint or debugger statement.
    // It is not okay to transition from a breakpoint to debugger statement
    // or a step to a debugger statement.
    const { type } = this._priorPause.why;

    if (type == newType) {
      return true;
    }

    const { line, column } = this._priorPause.frame.where;
    return line !== newLocation.line || column !== newLocation.column;
  },

  _makeOnStep: function({
    pauseAndRespond,
    startFrame,
    steppingType,
    completion,
  }) {
    const thread = this;
    return function() {
      if (thread._validFrameStepOffset(this, startFrame, this.offset)) {
        return pauseAndRespond(this, packet =>
          thread.createCompletionGrip(packet, completion)
        );
      }

      return undefined;
    };
  },

  _validFrameStepOffset: function(frame, startFrame, offset) {
    const meta = frame.script.getOffsetMetadata(offset);

    // Continue if:
    // 1. the location is not a valid breakpoint position
    // 2. the source is blackboxed
    // 3. we have not moved since the last pause
    if (
      !meta.isBreakpoint ||
      this.sources.isFrameBlackBoxed(frame) ||
      !this.hasMoved(frame)
    ) {
      return false;
    }

    // Pause if:
    // 1. the frame has changed
    // 2. the location is a step position.
    return frame !== startFrame || meta.isStepStart;
  },

  atBreakpointLocation(frame) {
    const location = this.sources.getFrameLocation(frame);
    return !!this.breakpointActorMap.get(location);
  },

  createCompletionGrip: function(packet, completion) {
    if (!completion) {
      return packet;
    }

    const createGrip = value =>
      createValueGrip(value, this._pausePool, this.objectGrip);
    packet.why.frameFinished = {};

    if (completion.hasOwnProperty("return")) {
      packet.why.frameFinished.return = createGrip(completion.return);
    } else if (completion.hasOwnProperty("yield")) {
      packet.why.frameFinished.return = createGrip(completion.yield);
    } else if (completion.hasOwnProperty("throw")) {
      packet.why.frameFinished.throw = createGrip(completion.throw);
    }

    return packet;
  },

  /**
   * Define the JS hook functions for stepping.
   */
  _makeSteppingHooks: function({ steppingType, startFrame, completion }) {
    // Bind these methods and state because some of the hooks are called
    // with 'this' set to the current frame. Rather than repeating the
    // binding in each _makeOnX method, just do it once here and pass it
    // in to each function.
    const steppingHookState = {
      pauseAndRespond: (frame, onPacket = k => k) =>
        this._pauseAndRespond(frame, { type: "resumeLimit" }, onPacket),
      startFrame: startFrame || this.youngestFrame,
      steppingType,
      completion,
    };

    return {
      onEnterFrame: this._makeOnEnterFrame(steppingHookState),
      onPop: this._makeOnPop(steppingHookState),
      onStep: this._makeOnStep(steppingHookState),
    };
  },

  /**
   * Handle attaching the various stepping hooks we need to attach when we
   * receive a resume request with a resumeLimit property.
   *
   * @param Object { resumeLimit }
   *        The values received over the RDP.
   * @returns A promise that resolves to true once the hooks are attached, or is
   *          rejected with an error packet.
   */
  _handleResumeLimit: async function({ resumeLimit, frameActorID }) {
    const steppingType = resumeLimit.type;
    if (!["break", "step", "next", "finish"].includes(steppingType)) {
      return Promise.reject({
        error: "badParameterType",
        message: "Unknown resumeLimit type",
      });
    }

    let frame = this.youngestFrame;

    if (frameActorID) {
      frame = this._framesPool.get(frameActorID).frame;
      if (!frame) {
        throw new Error("Frame should exist in the frames pool.");
      }
    }

    return this._attachSteppingHooks(frame, steppingType, undefined);
  },

  _attachSteppingHooks: function(frame, steppingType, completion) {
    // If we are stepping out of the onPop handler, we want to use "next" mode
    // so that the parent frame's handlers behave consistently.
    if (steppingType === "finish" && frame.reportedPop) {
      steppingType = "next";
    }

    // If there are no more frames on the stack, use "step" mode so that we will
    // pause on the next script to execute.
    const stepFrame = this._getNextStepFrame(frame);
    if (!stepFrame) {
      steppingType = "step";
    }

    const { onEnterFrame, onPop, onStep } = this._makeSteppingHooks({
      steppingType,
      completion,
      startFrame: frame,
    });

    if (steppingType === "step") {
      this.dbg.onEnterFrame = onEnterFrame;
    }

    if (stepFrame) {
      switch (steppingType) {
        case "step":
        case "break":
        case "next":
          if (stepFrame.script) {
            if (!this.sources.isFrameBlackBoxed(stepFrame)) {
              stepFrame.onStep = onStep;
            }
          }
        // Fall through.
        case "finish":
          stepFrame.onPop = onPop;
          break;
      }
    }

    return true;
  },

  /**
   * Clear the onStep and onPop hooks for all frames on the stack.
   */
  _clearSteppingHooks: function() {
    let frame = this.youngestFrame;
    if (frame?.onStack) {
      while (frame) {
        frame.onStep = undefined;
        frame.onPop = undefined;
        frame = frame.older;
      }
    }
  },

  /**
   * Handle a protocol request to resume execution of the debuggee.
   */
  onResume: async function({ resumeLimit, frameActorID }) {
    if (this._state !== "paused") {
      return {
        error: "wrongState",
        message:
          "Can't resume when debuggee isn't paused. Current state is '" +
          this._state +
          "'",
        state: this._state,
      };
    }

    // In case of multiple nested event loops (due to multiple debuggers open in
    // different tabs or multiple devtools clients connected to the same tab)
    // only allow resumption in a LIFO order.
    if (
      this._nestedEventLoops.size &&
      this._nestedEventLoops.lastPausedThreadActor &&
      this._nestedEventLoops.lastPausedThreadActor !== this
    ) {
      return {
        error: "wrongOrder",
        message: "trying to resume in the wrong order.",
      };
    }

    try {
      if (resumeLimit) {
        await this._handleResumeLimit({ resumeLimit, frameActorID });
      } else {
        this._clearSteppingHooks();
      }

      this.doResume({ resumeLimit });
      return {};
    } catch (error) {
      return error instanceof Error
        ? {
            error: "unknownError",
            message: DevToolsUtils.safeErrorString(error),
          }
        : // It is a known error, and the promise was rejected with an error
          // packet.
          error;
    }
  },

  /**
   * Only resume and notify necessary observers. This should be used in cases
   * when we do not want to notify the front end of a resume, for example when
   * we are shutting down.
   */
  doResume({ resumeLimit } = {}) {
    this.maybePauseOnExceptions();
    this._state = "running";

    // Drop the actors in the pause actor pool.
    this._pausePool.destroy();

    this._pausePool = null;
    this._pauseActor = null;
    this._popThreadPause();
    // Tell anyone who cares of the resume (as of now, that's the xpcshell harness and
    // devtools-startup.js when handling the --wait-for-jsdebugger flag)
    this.emit("resumed");
    this.hideOverlay();

    if (Services.obs) {
      Services.obs.notifyObservers(this, "devtools-thread-resumed");
    }
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

    p.then(resolvedVal => {
      needNest = false;
      returnVal = resolvedVal;
    })
      .catch(e => reportException("unsafeSynchronize", e))
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
    } else {
      this.dbg.onExceptionUnwind = undefined;
    }
  },

  /**
   * Helper method that returns the next frame when stepping.
   */
  _getNextStepFrame: function(frame) {
    const endOfFrame = frame.reportedPop;
    const stepFrame = endOfFrame ? frame.older : frame;
    if (!stepFrame || !stepFrame.script) {
      return null;
    }
    return stepFrame;
  },

  frames: function(start, count) {
    if (this.state !== "paused") {
      return {
        error: "wrongState",
        message:
          "Stack frames are only available while the debuggee is paused.",
      };
    }

    // Find the starting frame...
    let frame = this.youngestFrame;

    const walkToParentFrame = () => {
      if (!frame) {
        return;
      }

      const currentFrame = frame;
      frame = null;

      if (!(currentFrame instanceof Debugger.Frame)) {
        frame = getSavedFrameParent(this, currentFrame);
      } else if (currentFrame.older) {
        frame = currentFrame.older;
      } else if (
        this._options.shouldIncludeSavedFrames &&
        currentFrame.olderSavedFrame
      ) {
        frame = currentFrame.olderSavedFrame;
        if (frame && !isValidSavedFrame(this, frame)) {
          frame = null;
        }
      } else if (
        this._options.shouldIncludeAsyncLiveFrames &&
        currentFrame.asyncPromise
      ) {
        // We support returning Frame actors for frames that are suspended
        // at an 'await', and here we want to walk upward to look for the first
        // frame that will be resumed when the current frame's promise resolves.
        let reactions = currentFrame.asyncPromise.getPromiseReactions();
        while (true) {
          // We loop here because we may have code like:
          //
          //   async function inner(){ debugger; }
          //
          //   async function outer() {
          //     await Promise.resolve().then(() => inner());
          //   }
          //
          // where we can see that when `inner` resolves, we will resume from
          // `outer`, even though there is a layer of promises between, and
          // that layer could be any number of promises deep.
          if (!(reactions[0] instanceof Debugger.Object)) {
            break;
          }

          reactions = reactions[0].getPromiseReactions();
        }

        if (reactions[0] instanceof Debugger.Frame) {
          frame = reactions[0];
        }
      }
    };

    let i = 0;
    while (frame && i < start) {
      walkToParentFrame();
      i++;
    }

    // Return count frames, or all remaining frames if count is not defined.
    const frames = [];
    for (; frame && (!count || i < start + count); i++, walkToParentFrame()) {
      // SavedFrame instances don't have direct Debugger.Source object. If
      // there is an active Debugger.Source that represents the SaveFrame's
      // source, it will have already been created in the server.
      if (frame instanceof Debugger.Frame) {
        const sourceActor = this.sources.createSourceActor(frame.script.source);
        if (!sourceActor) {
          continue;
        }
      }
      const frameActor = this._createFrameActor(frame, i);
      frames.push(frameActor);
    }

    return { frames };
  },

  addAllSources() {
    // Compare the sources we find with the source URLs which have been loaded
    // in debuggee realms. Count the number of sources associated with each
    // URL so that we can detect if an HTML file has had some inline sources
    // collected but not all.
    const urlMap = {};
    for (const url of this.dbg.findSourceURLs()) {
      if (url !== "self-hosted") {
        urlMap[url] = 1 + (urlMap[url] || 0);
      }
    }

    const sources = this.dbg.findSources();

    for (const source of sources) {
      this._addSource(source);

      if (!source.introductionScript) {
        urlMap[source.url]--;
      }
    }

    // Resurrect any URLs for which not all sources are accounted for.
    for (const [url, count] of Object.entries(urlMap)) {
      if (count > 0) {
        this._resurrectSource(url);
      }
    }
  },

  onSources: function(request) {
    this.addAllSources();

    // No need to flush the new source packets here, as we are sending the
    // list of sources out immediately and we don't need to invoke the
    // overhead of an RDP packet for every source right now. Let the default
    // timeout flush the buffered packets.

    return {
      sources: this.sources.iter().map(s => s.form()),
    };
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

  removeAllWatchpoints: function() {
    for (const actor of this.threadLifetimePool.poolChildren()) {
      if (actor.typeName == "obj") {
        actor.removeWatchpoints();
      }
    }
  },

  addWatchpoint(objActor, data) {
    this._watchpointsMap.add(objActor, data);
  },

  removeWatchpoint(objActor, property) {
    this._watchpointsMap.remove(objActor, property);
  },

  getWatchpoint(obj, property) {
    return this._watchpointsMap.get(obj, property);
  },

  /**
   * Handle a protocol request to pause the debuggee.
   */
  onInterrupt: function({ when }) {
    if (this.state == "exited") {
      return { type: "exited" };
    } else if (this.state == "paused") {
      // TODO: return the actual reason for the existing pause.
      this.emit("paused", {
        why: { type: "alreadyPaused" },
      });
      return {};
    } else if (this.state != "running") {
      return {
        error: "wrongState",
        message: "Received interrupt request in " + this.state + " state.",
      };
    }
    try {
      // If execution should pause just before the next JavaScript bytecode is
      // executed, just set an onEnterFrame handler.
      if (when == "onNext") {
        const onEnterFrame = frame => {
          this._pauseAndRespond(frame, { type: "interrupted", onNext: true });
        };
        this.dbg.onEnterFrame = onEnterFrame;

        this.emit("willInterrupt");
        return {};
      }

      // If execution should pause immediately, just put ourselves in the paused
      // state.
      const packet = this._paused();
      if (!packet) {
        return { error: "notInterrupted" };
      }
      packet.why = { type: "interrupted", onNext: false };

      // Send the response to the interrupt request now (rather than
      // returning it), because we're going to start a nested event loop
      // here.
      this.conn.send({ from: this.actorID, type: "interrupt" });
      this.emit("paused", packet);

      // Start a nested event loop.
      this._pushThreadPause();

      // We already sent a response to this request, don't send one
      // now.
      return null;
    } catch (e) {
      reportException("DBG-SERVER", e);
      return { error: "notInterrupted", message: e.toString() };
    }
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

    this._state = "paused";

    // Clear stepping hooks.
    this.dbg.onEnterFrame = undefined;
    this.dbg.onExceptionUnwind = undefined;
    this._clearSteppingHooks();

    // Create the actor pool that will hold the pause actor and its
    // children.
    assert(!this._pausePool, "No pause pool should exist yet");
    this._pausePool = new Pool(this.conn, "pause");

    // Give children of the pause pool a quick link back to the
    // thread...
    this._pausePool.threadActor = this;

    // Create the pause actor itself...
    assert(!this._pauseActor, "No pause actor should exist yet");
    this._pauseActor = new PauseActor(this._pausePool);
    this._pausePool.manage(this._pauseActor);

    // Update the list of frames.
    const poppedFrames = this._updateFrames();

    // Send off the paused packet and spin an event loop.
    const packet = {
      actor: this._pauseActor.actorID,
    };

    if (frame) {
      packet.frame = this._createFrameActor(frame);
    }

    if (poppedFrames) {
      packet.poppedFrames = poppedFrames;
    }

    return packet;
  },

  /**
   * Expire frame actors for frames that have been popped.
   *
   * @returns A list of actor IDs whose frames have been popped.
   */
  _updateFrames: function() {
    const popped = [];

    // Create the actor pool that will hold the still-living frames.
    const framesPool = new Pool(this.conn, "frames");
    const frameList = [];

    for (const frameActor of this._frameActors) {
      if (frameActor.frame.onStack) {
        framesPool.manage(frameActor);
        frameList.push(frameActor);
      } else {
        popped.push(frameActor.actorID);
      }
    }

    // Remove the old frame actor pool, this will expire
    // any actors that weren't added to the new pool.
    if (this._framesPool) {
      this._framesPool.destroy();
    }

    this._frameActors = frameList;
    this._framesPool = framesPool;

    return popped;
  },

  _createFrameActor: function(frame, depth) {
    let actor = this._frameActorMap.get(frame);
    if (!actor) {
      actor = new FrameActor(frame, this, depth);
      this._frameActors.push(actor);
      this._framesPool.manage(actor);

      this._frameActorMap.set(frame, actor);
    }
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
    pool.manage(actor);
    environment.actor = actor;

    return actor;
  },

  /**
   * Create a grip for the given debuggee object.
   *
   * @param value Debugger.Object
   *        The debuggee object value.
   * @param pool Pool
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

    const actor = new PauseScopedObjectActor(
      value,
      {
        thread: this,
        getGripDepth: () => this._gripDepth,
        incrementGripDepth: () => this._gripDepth++,
        decrementGripDepth: () => this._gripDepth--,
        createValueGrip: v => {
          if (this._pausePool) {
            return createValueGrip(v, this._pausePool, this.pauseObjectGrip);
          }

          return createValueGrip(v, this.threadLifetimePool, this.objectGrip);
        },
        sources: () => this.sources,
        createEnvironmentActor: (e, p) => this.createEnvironmentActor(e, p),
        promote: () => this.threadObjectGrip(actor),
        isThreadLifetimePool: () =>
          actor.getParent() !== this.threadLifetimePool,
      },
      this.conn
    );
    pool.manage(actor);
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
    // Save the reference for when we need to demote the object
    // back to its original registered pool
    actor.originalRegisteredPool = actor.getParent();

    this.threadLifetimePool.manage(actor);
    this.threadLifetimePool.objectActors.set(actor.obj, actor);
  },

  _onWindowReady: function({ isTopLevel, isBFCache, window }) {
    if (isTopLevel && this.state != "detached") {
      this.sources.reset();
      this.clearDebuggees();
      this.dbg.enable();
      this.maybePauseOnExceptions();
      // Update the global no matter if the debugger is on or off,
      // otherwise the global will be wrong when enabled later.
      this.global = window;
    }

    // Refresh the debuggee list when a new window object appears (top window or
    // iframe).
    if (this.attached) {
      this.dbg.addDebuggees();
    }

    // BFCache navigations reuse old sources, so send existing sources to the
    // client instead of waiting for onNewScript debugger notifications.
    if (isBFCache) {
      this.addAllSources();
    }
  },

  _onWillNavigate: function({ isTopLevel }) {
    if (!isTopLevel) {
      return;
    }

    // Proceed normally only if the debuggee is not paused.
    if (this.state == "paused") {
      this.unsafeSynchronize(Promise.resolve(this.doResume()));
      this.dbg.disable();
    }

    this.removeAllWatchpoints();
    this.disableAllBreakpoints();
    this.dbg.onEnterFrame = undefined;
    this.dbg.onExceptionUnwind = undefined;
  },

  _onNavigate: function() {
    if (this.state == "running") {
      this.dbg.enable();
    }
  },

  // JS Debugger API hooks.
  pauseForMutationBreakpoint: function(
    mutationType,
    targetNode,
    ancestorNode,
    action = "" // "add" or "remove"
  ) {
    if (
      !["subtreeModified", "nodeRemoved", "attributeModified"].includes(
        mutationType
      )
    ) {
      throw new Error("Unexpected mutation breakpoint type");
    }

    const frame = this.dbg.getNewestFrame();
    if (!frame) {
      return undefined;
    }

    const location = this.sources.getFrameLocation(frame);

    if (this.skipBreakpoints || this.sources.isBlackBoxed(location.sourceUrl)) {
      return undefined;
    }

    const global = (targetNode.ownerDocument || targetNode).defaultView;
    assert(global && this.dbg.hasDebuggee(global));

    const targetObj = this.dbg
      .makeGlobalObjectReference(global)
      .makeDebuggeeValue(targetNode);

    let ancestorObj = null;
    if (ancestorNode) {
      ancestorObj = this.dbg
        .makeGlobalObjectReference(global)
        .makeDebuggeeValue(ancestorNode);
    }

    return this._pauseAndRespond(
      frame,
      {
        type: "mutationBreakpoint",
        mutationType,
        message: `DOM Mutation: '${mutationType}'`,
      },
      pkt => {
        // We have to add this here because `_pausePool` is `null` beforehand.
        pkt.why.nodeGrip = this.objectGrip(targetObj, this._pausePool);
        pkt.why.ancestorGrip = ancestorObj
          ? this.objectGrip(ancestorObj, this._pausePool)
          : null;
        pkt.why.action = action;
        return pkt;
      }
    );
  },

  /**
   * A function that the engine calls when a debugger statement has been
   * executed in the specified frame.
   *
   * @param frame Debugger.Frame
   *        The stack frame that contained the debugger statement.
   */
  onDebuggerStatement: function(frame) {
    const location = this.sources.getFrameLocation(frame);

    // Don't pause if
    // 1. we have not moved since the last pause
    // 2. breakpoints are disabled
    // 3. the source is blackboxed
    // 4. there is a breakpoint at the same location
    if (
      !this.hasMoved(frame, "debuggerStatement") ||
      this.skipBreakpoints ||
      this.sources.isBlackBoxed(location.sourceUrl) ||
      this.atBreakpointLocation(frame)
    ) {
      return undefined;
    }

    return this._pauseAndRespond(frame, { type: "debuggerStatement" });
  },

  onSkipBreakpoints: function({ skip }) {
    this._options.skipBreakpoints = skip;
    return { skip };
  },

  onPauseOnExceptions: function({ pauseOnExceptions, ignoreCaughtExceptions }) {
    this._options = {
      ...this._options,
      pauseOnExceptions,
      ignoreCaughtExceptions,
    };
    this.maybePauseOnExceptions();
    return {};
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

    if (
      this._handledFrameExceptions.has(youngestFrame) &&
      this._handledFrameExceptions.get(youngestFrame) === value
    ) {
      return undefined;
    }

    // NS_ERROR_NO_INTERFACE exceptions are a special case in browser code,
    // since they're almost always thrown by QueryInterface functions, and
    // handled cleanly by native code.
    if (!isWorker && value == Cr.NS_ERROR_NO_INTERFACE) {
      return undefined;
    }

    const { sourceActor } = this.sources.getFrameLocation(youngestFrame);
    const url = sourceActor ? sourceActor.url : null;

    // Don't pause on exceptions thrown while inside an evaluation being done on
    // behalf of the client.
    if (this.insideClientEvaluation) {
      return undefined;
    }

    if (this.skipBreakpoints || this.sources.isBlackBoxed(url)) {
      return undefined;
    }

    // Now that we've decided to pause, ignore this exception if it's thrown by
    // any older frames.
    for (let frame = youngestFrame.older; frame != null; frame = frame.older) {
      this._handledFrameExceptions.set(frame, value);
    }

    try {
      const packet = this._paused(youngestFrame);
      if (!packet) {
        return undefined;
      }

      packet.why = {
        type: "exception",
        exception: createValueGrip(value, this._pausePool, this.objectGrip),
      };
      this.emit("paused", packet);

      this._pushThreadPause();
    } catch (e) {
      reportException("TA_onExceptionUnwind", e);
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
   * Emits `newSource` on the thread actor.
   *
   * @param {SourceActor} source
   */
  onNewSourceEvent: function(source) {
    // Bug 1516197: New sources are likely detected due to either user
    // interaction on the page, or devtools requests sent to the server.
    // We use executeSoon because we don't want to block those operations
    // by sending packets in the middle of them.
    DevToolsUtils.executeSoon(() => {
      this.emit("newSource", {
        source: source.form(),
      });
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
    let sourceActor;
    if (
      this._debuggerSourcesSeen.has(source) &&
      this.sources.hasSourceActor(source)
    ) {
      sourceActor = this.sources.getSourceActor(source);
      sourceActor.resetDebuggeeScripts();
    } else {
      sourceActor = this.sources.createSourceActor(source);
    }

    const sourceUrl = sourceActor.url;
    if (this._onLoadBreakpointURLs.has(sourceUrl)) {
      this.setBreakpoint({ sourceUrl, line: 1 }, {});
    }

    const bpActors = this.breakpointActorMap
      .findActors()
      .filter(
        actor =>
          actor.location.sourceUrl && actor.location.sourceUrl == sourceUrl
      );

    for (const actor of bpActors) {
      sourceActor.applyBreakpoint(actor);
    }

    this._debuggerSourcesSeen.add(source);
    return true;
  },

  /**
   * Create a new source by refetching the specified URL and instantiating all
   * sources that were found in the result.
   *
   * @param url The URL string to fetch.
   */
  _resurrectSource: async function(url) {
    let { content, contentType, sourceMapURL } = await this.sources.urlContents(
      url,
      /* partial */ false,
      /* canUseCache */ true
    );

    // Newlines in all sources should be normalized. Do this with HTML content
    // to simplify the comparisons below.
    content = content.replace(/\r\n?|\u2028|\u2029/g, "\n");

    if (contentType == "text/html") {
      // HTML files can contain any number of inline sources. We have to find
      // all the inline sources and their start line without running any of the
      // scripts on the page. The approach used here is approximate.
      if (!this._parent.window) {
        return;
      }

      // Find the offsets in the HTML at which inline scripts might start.
      const scriptTagMatches = content.matchAll(/<script[^>]*>/gi);
      const scriptStartOffsets = [...scriptTagMatches].map(
        rv => rv.index + rv[0].length
      );

      // Find the script tags in this HTML page by parsing a new document from
      // the contentand looking for its script elements.
      const document = new DOMParser().parseFromString(content, "text/html");

      // For each inline source found, see if there is a start offset for what
      // appears to be a script tag, whose contents match the inline source.
      const scripts = document.querySelectorAll("script");
      for (const script of scripts) {
        if (script.src) {
          continue;
        }

        const text = script.innerText;
        for (const offset of scriptStartOffsets) {
          if (content.substring(offset, offset + text.length) == text) {
            const allLineBreaks = content.substring(0, offset).matchAll("\n");
            const startLine = 1 + [...allLineBreaks].length;
            try {
              const global = this.dbg.getDebuggees()[0];
              this._addSource(
                global.createSource({
                  text,
                  url,
                  startLine,
                  isScriptElement: true,
                })
              );
            } catch (e) {
              //  Ignore parse errors.
            }
            break;
          }
        }
      }

      // If no scripts were found, we might have an inaccurate content type and
      // the file is actually JavaScript. Fall through and add the entire file
      // as the source.
      if (scripts.length) {
        return;
      }
    }

    // Other files should only contain javascript, so add the file contents as
    // the source itself.
    try {
      const global = this.dbg.getDebuggees()[0];
      this._addSource(
        global.createSource({
          text: content,
          url,
          startLine: 1,
          sourceMapURL,
        })
      );
    } catch (e) {
      // Ignore parse errors.
    }
  },

  onDump: function() {
    return {
      pauseOnExceptions: this._options.pauseOnExceptions,
      ignoreCaughtExceptions: this._options.ignoreCaughtExceptions,
      logEventBreakpoints: this._options.logEventBreakpoints,
      skipBreakpoints: this.skipBreakpoints,
      breakpoints: this.breakpointActorMap.listKeys(),
    };
  },

  // NOTE: dumpPools is defined in the Thread actor to avoid
  // adding it to multiple target specs and actors.
  onDumpPools() {
    return this.conn.dumpPools();
  },

  logLocation: function(prefix, frame) {
    const loc = this.sources.getFrameLocation(frame);
    dump(`${prefix} (${loc.line}, ${loc.column})\n`);
  },
});

Object.assign(ThreadActor.prototype.requestTypes, {
  attach: ThreadActor.prototype.onAttach,
  detach: ThreadActor.prototype.onDetach,
  reconfigure: ThreadActor.prototype.onReconfigure,
  resume: ThreadActor.prototype.onResume,
  interrupt: ThreadActor.prototype.onInterrupt,
  sources: ThreadActor.prototype.onSources,
  skipBreakpoints: ThreadActor.prototype.onSkipBreakpoints,
  pauseOnExceptions: ThreadActor.prototype.onPauseOnExceptions,
  dumpThread: ThreadActor.prototype.onDump,
  dumpPools: ThreadActor.prototype.onDumpPools,
});

exports.ThreadActor = ThreadActor;

/**
 * Creates a PauseActor.
 *
 * PauseActors exist for the lifetime of a given debuggee pause.  Used to
 * scope pause-lifetime grips.
 *
 * @param {Pool} pool: The actor pool created for this pause.
 */
function PauseActor(pool) {
  this.pool = pool;
}

PauseActor.prototype = {
  typeName: "pause",
};

// Utility functions.

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
