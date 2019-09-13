/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DebuggerNotificationObserver = require("DebuggerNotificationObserver");
const Services = require("Services");
const { Cr, Ci } = require("chrome");
const { ActorPool } = require("devtools/server/actors/common");
const { createValueGrip } = require("devtools/server/actors/object/utils");
const { ActorClassWithSpec, Actor } = require("devtools/shared/protocol");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { assert, dumpn } = DevToolsUtils;
const { threadSpec } = require("devtools/shared/specs/thread");
const {
  getAvailableEventBreakpoints,
  eventBreakpointForNotification,
  makeEventBreakpointMessage,
} = require("devtools/server/actors/utils/event-breakpoints");

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
loader.lazyRequireGetter(this, "throttle", "devtools/shared/throttle", true);
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

loader.lazyRequireGetter(
  this,
  "findStepOffsets",
  "devtools/server/actors/replay/utils/findStepOffsets",
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
    this._xhrBreakpoints = [];
    this._observingNetwork = false;
    this._activeEventBreakpoints = new Set();
    this._activeEventPause = null;
    this._pauseOverlay = null;
    this._priorPause = null;

    this._options = {
      autoBlackBox: false,
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

    this.uncaughtExceptionHook = this.uncaughtExceptionHook.bind(this);
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
      this._dbg = this._parent.makeDebugger();
      this._dbg.uncaughtExceptionHook = this.uncaughtExceptionHook;
      this._dbg.onDebuggerStatement = this.onDebuggerStatement;
      this._dbg.onNewScript = this.onNewScript;
      this._dbg.onNewDebuggee = this._onNewDebuggee;
      if (this._dbg.replaying) {
        this._dbg.replayingOnForcedPause = this.replayingOnForcedPause.bind(
          this
        );
        this._dbg.replayingOnPositionChange = this._makeReplayingOnPositionChange();
      }
      // Keep the debugger disabled until a client attaches.
      this._dbg.enabled = this._state != "detached";
    }
    return this._dbg;
  },

  get globalDebugObject() {
    if (!this._parent.window || this.dbg.replaying) {
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
      this._threadLifetimePool = new ActorPool(this.conn);
      this.conn.addActorPool(this._threadLifetimePool);
      this._threadLifetimePool.objectActors = new WeakMap();
    }
    return this._threadLifetimePool;
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
    return this._options.skipBreakpoints;
  },

  /**
   * Keep track of all of the nested event loops we use to pause the debuggee
   * when we hit a breakpoint/debugger statement/etc in one place so we can
   * resolve them when we get resume packets. We have more than one (and keep
   * them in a stack) because we can pause within client evals.
   */
  _threadPauseEventLoops: null,
  _pushThreadPause: function() {
    if (this.dbg.replaying) {
      this.dbg.replayPushThreadPause();
    }
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
    if (this.dbg.replaying) {
      this.dbg.replayPopThreadPause();
    }
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

    this._xhrBreakpoints = [];
    this._updateNetworkObserver();

    this._activeEventBreakpoints = new Set();
    this._debuggerNotificationObserver.removeListener(
      this._eventBreakpointListener
    );

    for (const global of this.dbg.getDebuggees()) {
      try {
        this._debuggerNotificationObserver.disconnect(global);
      } catch (e) {}
    }

    this._parent.off("window-ready", this._onWindowReady);
    this._parent.off("will-navigate", this._onWillNavigate);
    this._parent.off("navigate", this._onNavigate);

    this.sources.off("newSource", this.onNewSourceEvent);
    this.clearDebuggees();
    this.conn.removeActorPool(this._threadLifetimePool);
    this._threadLifetimePool = null;

    if (!this._dbg) {
      return;
    }
    this._dbg.disable();
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
  onAttach: function({ options }) {
    if (this.state === "exited") {
      return {
        error: "exited",
        message: "threadActor has exited",
      };
    }

    if (this.state !== "detached") {
      return {
        error: "wrongState",
        message: "Current state is " + this.state,
      };
    }

    this._state = "attached";
    this._debuggerSourcesSeen = new WeakSet();

    Object.assign(this._options, options || {});
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
        return {
          error: "notAttached",
          message: "cannot attach, could not create pause packet",
        };
      }
      packet.why = { type: "attached" };

      // Send the response to the attach request now (rather than
      // returning it), because we're going to start a nested event
      // loop here.
      this.conn.send({ from: this.actorID });
      this.conn.sendActorEvent(this.actorID, "paused", packet);

      // Start a nested event loop.
      this._pushThreadPause();

      // We already sent a response to this request, don't send one
      // now
      return null;
    } catch (e) {
      reportError(e);
      return {
        error: "notAttached",
        message: e.toString(),
      };
    }
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
      showOverlayStepButtons: this._options.showOverlayStepButtons,
      resume: () => this.onResume({ resumeLimit: null }),
      stepOver: () => this.onResume({ resumeLimit: { type: "next" } }),
    });
    this._pauseOverlay = highlighter;
    return highlighter;
  },

  showOverlay() {
    if (
      this.isPaused() &&
      this._parent.on &&
      this._parent.window.document &&
      !this._parent.window.isChromeWindow &&
      this.pauseOverlay
    ) {
      const reason = this._priorPause.why.type;

      // Do not show the pause overlay when scanning
      if (this.dbg.replaying) {
        return;
      }

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

  setBreakpoint: async function(location, options) {
    const actor = this.breakpointActorMap.getOrCreateBreakpointActor(location);
    actor.setOptions(options);

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
      this._debuggerNotificationObserver.connect(global);
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
    this.conn.sendActorEvent(this.actorID, "detached");
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

    Object.assign(this._options, options);

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
        const url = sourceActor.url;
        if (this.sources.isBlackBoxed(url)) {
          return;
        }

        this._pauseAndRespondEventBreakpoint(frame, eventBreakpoint);
      }
    }
  },

  _makeEventBreakpointEnterFrame(eventBreakpoint) {
    return frame => {
      const { sourceActor } = this.sources.getFrameLocation(frame);
      const url = sourceActor.url;
      if (this.sources.isBlackBoxed(url)) {
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
        this.suspendedFrame.waitingOnStep = false;
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
      this.conn.sendActorEvent(this.actorID, "paused", pkt);
      this.showOverlay();
    } catch (error) {
      reportError(error);
      this.conn.send({
        error: "unknownError",
        message: error.message + "\n" + error.stack,
      });
      return undefined;
    }

    try {
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
      // Continue forward until we get to a valid step target.
      const { onStep, onPop } = this._makeSteppingHooks({
        steppingType: "next",
        rewinding: false,
      });

      if (this.sources.isFrameBlackBoxed(frame)) {
        return undefined;
      }

      if (this.dbg.replaying) {
        const offsets = findStepOffsets(frame);
        frame.setReplayingOnStep(onStep, offsets);
      } else {
        frame.onStep = onStep;
      }

      frame.onPop = onPop;
      return undefined;
    };
  },

  _makeOnPop: function({ pauseAndRespond, steppingType, rewinding }) {
    const thread = this;
    return function(completion) {
      // onPop is called when we temporarily leave an async/generator
      if (completion.await || completion.yield) {
        thread.suspendedFrame = this;
        this.waitingOnStep = true;
        thread.dbg.onEnterFrame = undefined;
        return undefined;
      }

      if (thread.sources.isFrameBlackBoxed(this)) {
        return undefined;
      }

      // Note that we're popping this frame; we need to watch for
      // subsequent step events on its caller.
      this.reportedPop = true;

      if (steppingType != "finish") {
        return pauseAndRespond(this, packet =>
          thread.createCompletionGrip(packet, completion)
        );
      }

      const parentFrame = thread._getNextStepFrame(this);
      if (parentFrame && parentFrame.script) {
        const { onStep, onPop } = thread._makeSteppingHooks({
          steppingType: "next",
          rewinding,
          completion,
        });

        if (thread.dbg.replaying) {
          const offsets = findStepOffsets(parentFrame, rewinding);
          parentFrame.setReplayingOnStep(onStep, offsets);
        } else {
          parentFrame.onStep = onStep;
        }

        // We need the onPop alongside the onStep because it is possible that
        // the parent frame won't have any steppable offsets, and we want to
        // make sure that we always pause in the parent _somewhere_.
        parentFrame.onPop = onPop;
        return undefined;
      }

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
    rewinding,
  }) {
    const thread = this;
    return function() {
      // onStep is called with 'this' set to the current frame.
      // NOTE: we need to clear the stepping hooks when we are
      // no longer waiting for an async step to occur.
      if (this.hasOwnProperty("waitingOnStep") && !this.waitingOnStep) {
        delete this.waitingOnStep;
        this.onStep = undefined;
        this.onPop = undefined;
        return undefined;
      }

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
    // 2. the source is not blackboxed
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
  _makeSteppingHooks: function({ steppingType, rewinding, completion }) {
    // We can't use the completion value in stepping hooks if we're
    // replaying, as we can't use its contents after resuming.
    if (this.dbg.replaying) {
      completion = null;
    }

    // Bind these methods and state because some of the hooks are called
    // with 'this' set to the current frame. Rather than repeating the
    // binding in each _makeOnX method, just do it once here and pass it
    // in to each function.
    const steppingHookState = {
      pauseAndRespond: (frame, onPacket = k => k) =>
        this._pauseAndRespond(frame, { type: "resumeLimit" }, onPacket),
      startFrame: this.youngestFrame,
      steppingType,
      rewinding,
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
   * @param Object { rewind, resumeLimit }
   *        The values received over the RDP.
   * @returns A promise that resolves to true once the hooks are attached, or is
   *          rejected with an error packet.
   */
  _handleResumeLimit: async function({ rewind, resumeLimit }) {
    let steppingType = resumeLimit.type;
    const rewinding = rewind;
    if (!["break", "step", "next", "finish", "warp"].includes(steppingType)) {
      return Promise.reject({
        error: "badParameterType",
        message: "Unknown resumeLimit type",
      });
    }

    if (steppingType == "warp") {
      // Time warp resume limits are handled by the caller.
      return true;
    }

    // If we are stepping out of the onPop handler, we want to use "next" mode
    // so that the parent frame's handlers behave consistently.
    if (steppingType === "finish" && this.youngestFrame.reportedPop) {
      steppingType = "next";
    }

    const { onEnterFrame, onPop, onStep } = this._makeSteppingHooks({
      steppingType,
      rewinding,
    });

    // Make sure there is still a frame on the stack if we are to continue
    // stepping.
    const stepFrame = this._getNextStepFrame(this.youngestFrame, rewinding);
    if (stepFrame) {
      switch (steppingType) {
        case "step":
          assert(
            !rewinding,
            "'step' resume limit cannot be used while rewinding"
          );
          this.dbg.onEnterFrame = onEnterFrame;
        // Fall through.
        case "break":
        case "next":
          if (stepFrame.script) {
            if (this.dbg.replaying) {
              const offsets = findStepOffsets(stepFrame, rewinding);
              stepFrame.setReplayingOnStep(onStep, offsets);
            } else {
              stepFrame.waitingOnStep = true;
              stepFrame.onStep = onStep;
              stepFrame.onPop = onPop;
            }
          }
        // Fall through.
        case "finish":
          if (rewinding) {
            let olderFrame = stepFrame.older;
            while (olderFrame && !olderFrame.script) {
              olderFrame = olderFrame.older;
            }
            if (olderFrame) {
              // Set an onStep handler in the older frame to stop at the call site.
              // Make sure the offsets we use are valid breakpoint locations, as we
              // cannot stop at other offsets when replaying.
              const offsets = findStepOffsets(olderFrame, true);
              olderFrame.setReplayingOnStep(onStep, offsets);
            }
          } else {
            stepFrame.onPop = onPop;
          }
          break;
      }
    }

    return true;
  },

  /**
   * Clear the onStep and onPop hooks for all frames on the stack.
   */
  _clearSteppingHooks: function() {
    if (this.dbg.replaying) {
      this.dbg.replayClearSteppingHooks();
    } else {
      let frame = this.youngestFrame;
      if (frame && frame.live) {
        while (frame) {
          frame.onStep = undefined;
          frame.onPop = undefined;
          frame = frame.older;
        }
      }
    }
  },

  paint: function(point) {
    this.dbg.replayPaint(point);
  },

  paintCurrentPoint: function() {
    this.dbg.replayPaintCurrentPoint();
  },

  /**
   * Handle a protocol request to resume execution of the debuggee.
   */
  onResume: async function({ resumeLimit, rewind }) {
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
    // different tabs or multiple debugger clients connected to the same tab)
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

    if (rewind && !this.dbg.replaying) {
      return {
        error: "cantRewind",
        message: "Can't rewind a debuggee that is not replaying.",
      };
    }

    try {
      if (resumeLimit) {
        await this._handleResumeLimit({ resumeLimit, rewind });
      } else {
        this._clearSteppingHooks();
      }

      this.doResume({ resumeLimit, rewind });
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
  doResume({ resumeLimit, rewind } = {}) {
    // When replaying execution in a separate process we need to explicitly
    // notify that process when to resume execution.
    if (this.dbg.replaying) {
      if (resumeLimit && resumeLimit.type == "warp") {
        this.dbg.replayTimeWarp(resumeLimit.target);
      } else if (rewind) {
        this.dbg.replayResumeBackward();
      } else {
        this.dbg.replayResumeForward();
      }
    }

    this.maybePauseOnExceptions();
    this._state = "running";

    // Drop the actors in the pause actor pool.
    this.conn.removeActorPool(this._pausePool);

    this._pausePool = null;
    this._pauseActor = null;
    this._popThreadPause();
    // Tell anyone who cares of the resume (as of now, that's the xpcshell harness and
    // devtools-startup.js when handling the --wait-for-jsdebugger flag)
    this.conn.sendActorEvent(this.actorID, "resumed");
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
      .catch(error => {
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
    } else {
      this.dbg.onExceptionUnwind = undefined;
    }
  },

  /**
   * Helper method that returns the next frame when stepping.
   */
  _getNextStepFrame: function(frame, rewinding) {
    const endOfFrame = rewinding
      ? frame.offset == frame.script.mainOffset
      : frame.reportedPop;
    const stepFrame = endOfFrame ? frame.older : frame;
    if (!stepFrame || !stepFrame.script) {
      return null;
    }
    return stepFrame;
  },

  onFrames: function(request) {
    if (this.state !== "paused") {
      return {
        error: "wrongState",
        message:
          "Stack frames are only available while the debuggee is paused.",
      };
    }

    const start = request.start ? request.start : 0;
    const count = request.count;

    // Find the starting frame...
    let frame = this.youngestFrame;
    let i = 0;
    while (frame && i < start) {
      frame = frame.older;
      i++;
    }

    // Return request.count frames, or all remaining
    // frames if count is not defined.
    const frames = [];
    for (; frame && (!count || i < start + count); i++, frame = frame.older) {
      const form = this._createFrameActor(frame).form();
      form.depth = i;

      let frameItem = null;

      const frameSourceActor = this.sources.createSourceActor(
        frame.script.source
      );
      if (frameSourceActor) {
        form.where = {
          actor: frameSourceActor.actorID,
          line: form.where.line,
          column: form.where.column,
        };
        frameItem = form;
      }
      frames.push(frameItem);
    }

    // Filter null values because createSourceActor can be falsey
    return { frames: frames.filter(x => !!x) };
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

  /**
   * Handle a protocol request to pause the debuggee.
   */
  onInterrupt: function({ when }) {
    if (this.state == "exited") {
      return { type: "exited" };
    } else if (this.state == "paused") {
      // TODO: return the actual reason for the existing pause.
      this.conn.sendActorEvent(this.actorID, "paused", {
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
      if (when == "onNext" && !this.dbg.replaying) {
        const onEnterFrame = frame => {
          this._pauseAndRespond(frame, { type: "interrupted", onNext: true });
        };
        this.dbg.onEnterFrame = onEnterFrame;

        this.conn.sendActorEvent(this.actorID, "willInterrupt");
        return {};
      }
      if (this.dbg.replaying) {
        this.dbg.replayPause();
      }

      // If execution should pause immediately, just put ourselves in the paused
      // state.
      const packet = this._paused();
      if (!packet) {
        return { error: "notInterrupted" };
      }
      // onNext is set while replaying so that the client will treat us as paused
      // at a breakpoint. When replaying we may need to pause and interact with
      // the server even if there are no frames on the stack.
      packet.why = { type: "interrupted", onNext: this.dbg.replaying };

      // Send the response to the interrupt request now (rather than
      // returning it), because we're going to start a nested event loop
      // here.
      this.conn.send({ from: this.actorID, type: "interrupt" });
      this.conn.sendActorEvent(this.actorID, "paused", packet);

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
    const packet = {
      from: this.actorID,
      type: "paused",
      actor: this._pauseActor.actorID,
    };
    if (frame) {
      packet.frame = this._createFrameActor(frame).form();
    }

    if (this.dbg.replaying) {
      packet.executionPoint = this.dbg.replayCurrentExecutionPoint();
      packet.recordingEndpoint = this.dbg.replayRecordingEndpoint();
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
          actor.registeredPool !== this.threadLifetimePool,
        getGlobalDebugObject: () => this.globalDebugObject,
      },
      this.conn
    );
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

  _onWindowReady: function({ isTopLevel, isBFCache, window }) {
    if (isTopLevel && this.state != "detached") {
      this.sources.reset();
      this.clearDebuggees();
      this.dbg.enabled = true;
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
      this.dbg.enabled = false;
    }
    this.disableAllBreakpoints();
  },

  _onNavigate: function() {
    if (this.state == "running") {
      this.dbg.enabled = true;
    }
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

  pauseForMutationBreakpoint: function(mutationType) {
    if (
      !["subtreeModified", "nodeRemoved", "attributeModified"].includes(
        mutationType
      )
    ) {
      throw new Error("Unexpected mutation breakpoint type");
    }

    if (this.skipBreakpoints) {
      return;
    }

    const frame = this.dbg.getNewestFrame();
    if (frame) {
      this._pauseAndRespond(frame, {
        type: "mutationBreakpoint",
        mutationType,
        message: `DOM Mutation: '${mutationType}'`,
      });
    }
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
    Object.assign(this._options, { pauseOnExceptions, ignoreCaughtExceptions });
    this.maybePauseOnExceptions();
    return {};
  },

  /*
   * A function that the engine calls when a recording/replaying process has
   * changed its position: a checkpoint was reached or a switch between a
   * recording and replaying child process occurred.
   */
  _makeReplayingOnPositionChange() {
    return throttle(() => {
      if (this.attached) {
        const recording = this.dbg.replayIsRecording();
        const executionPoint = this.dbg.replayCurrentExecutionPoint();
        const unscannedRegions = this.dbg.replayUnscannedRegions();
        const cachedPoints = this.dbg.replayCachedPoints();
        this.conn.send({
          type: "progress",
          from: this.actorID,
          recording,
          executionPoint,
          unscannedRegions,
          cachedPoints,
        });
      }
    }, 100);
  },

  /**
   * A function that the engine calls when replay has hit a point where it will
   * pause, even if no breakpoint has been set. Such points include hitting the
   * beginning or end of the replay, or reaching the target of a time warp.
   *
   * @param frame Debugger.Frame
   *        The youngest stack frame, or null.
   */
  replayingOnForcedPause: function(frame) {
    if (frame) {
      this._pauseAndRespond(frame, { type: "replayForcedPause" });
    } else {
      const packet = this._paused(frame);
      if (!packet) {
        return;
      }
      packet.why = "replayForcedPause";

      this.conn.send(packet);
      this._pushThreadPause();
    }
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
    if (value == Cr.NS_ERROR_NO_INTERFACE) {
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
      this.conn.send({
        from: this.actorID,
        type: "newSource",
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
      skipBreakpoints: this.skipBreakpoints,
      breakpoints: this.breakpointActorMap.listKeys(),
    };
  },

  logLocation: function(prefix, frame) {
    const loc = this.sources.getFrameLocation(frame);
    dump(`${prefix} (${loc.line}, ${loc.column})\n`);
  },

  debuggerRequests() {
    return this.dbg.replayDebuggerRequests();
  },
});

Object.assign(ThreadActor.prototype.requestTypes, {
  attach: ThreadActor.prototype.onAttach,
  detach: ThreadActor.prototype.onDetach,
  reconfigure: ThreadActor.prototype.onReconfigure,
  resume: ThreadActor.prototype.onResume,
  frames: ThreadActor.prototype.onFrames,
  interrupt: ThreadActor.prototype.onInterrupt,
  sources: ThreadActor.prototype.onSources,
  skipBreakpoints: ThreadActor.prototype.onSkipBreakpoints,
  pauseOnExceptions: ThreadActor.prototype.onPauseOnExceptions,
  dumpThread: ThreadActor.prototype.onDump,
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
  actorPrefix: "pause",
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
  actorPrefix: "chromeDebugger",
});

exports.ChromeDebuggerActor = ChromeDebuggerActor;

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
  const message = error.message ? error.message : String(error);
  const msg = prefix + message + ":\n" + error.stack;
  oldReportError(msg);
  dumpn(msg);
};

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
