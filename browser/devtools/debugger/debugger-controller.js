/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const DBG_STRINGS_URI = "chrome://browser/locale/devtools/debugger.properties";
const NEW_SOURCE_IGNORED_URLS = ["debugger eval code", "self-hosted", "XStringBundle"];
const NEW_SOURCE_DISPLAY_DELAY = 200; // ms
const FETCH_SOURCE_RESPONSE_DELAY = 200; // ms
const FETCH_EVENT_LISTENERS_DELAY = 200; // ms
const FRAME_STEP_CLEAR_DELAY = 100; // ms
const CALL_STACK_PAGE_SIZE = 25; // frames

// The panel's window global is an EventEmitter firing the following events:
const EVENTS = {
  // When the debugger's source editor instance finishes loading or unloading.
  EDITOR_LOADED: "Debugger:EditorLoaded",
  EDITOR_UNLOADED: "Debugger:EditorUnoaded",

  // When new sources are received from the debugger server.
  NEW_SOURCE: "Debugger:NewSource",
  SOURCES_ADDED: "Debugger:SourcesAdded",

  // When a source is shown in the source editor.
  SOURCE_SHOWN: "Debugger:EditorSourceShown",
  SOURCE_ERROR_SHOWN: "Debugger:EditorSourceErrorShown",

  // When scopes, variables, properties and watch expressions are fetched and
  // displayed in the variables view.
  FETCHED_SCOPES: "Debugger:FetchedScopes",
  FETCHED_VARIABLES: "Debugger:FetchedVariables",
  FETCHED_PROPERTIES: "Debugger:FetchedProperties",
  FETCHED_BUBBLE_PROPERTIES: "Debugger:FetchedBubbleProperties",
  FETCHED_WATCH_EXPRESSIONS: "Debugger:FetchedWatchExpressions",

  // When a breakpoint has been added or removed on the debugger server.
  BREAKPOINT_ADDED: "Debugger:BreakpointAdded",
  BREAKPOINT_REMOVED: "Debugger:BreakpointRemoved",

  // When a breakpoint has been shown or hidden in the source editor.
  BREAKPOINT_SHOWN: "Debugger:BreakpointShown",
  BREAKPOINT_HIDDEN: "Debugger:BreakpointHidden",

  // When a conditional breakpoint's popup is showing or hiding.
  CONDITIONAL_BREAKPOINT_POPUP_SHOWING: "Debugger:ConditionalBreakpointPopupShowing",
  CONDITIONAL_BREAKPOINT_POPUP_HIDING: "Debugger:ConditionalBreakpointPopupHiding",

  // When event listeners are fetched or event breakpoints are updated.
  EVENT_LISTENERS_FETCHED: "Debugger:EventListenersFetched",
  EVENT_BREAKPOINTS_UPDATED: "Debugger:EventBreakpointsUpdated",

  // When a file search was performed.
  FILE_SEARCH_MATCH_FOUND: "Debugger:FileSearch:MatchFound",
  FILE_SEARCH_MATCH_NOT_FOUND: "Debugger:FileSearch:MatchNotFound",

  // When a function search was performed.
  FUNCTION_SEARCH_MATCH_FOUND: "Debugger:FunctionSearch:MatchFound",
  FUNCTION_SEARCH_MATCH_NOT_FOUND: "Debugger:FunctionSearch:MatchNotFound",

  // When a global text search was performed.
  GLOBAL_SEARCH_MATCH_FOUND: "Debugger:GlobalSearch:MatchFound",
  GLOBAL_SEARCH_MATCH_NOT_FOUND: "Debugger:GlobalSearch:MatchNotFound",

  // After the stackframes are cleared and debugger won't pause anymore.
  AFTER_FRAMES_CLEARED: "Debugger:AfterFramesCleared",

  // When the options popup is showing or hiding.
  OPTIONS_POPUP_SHOWING: "Debugger:OptionsPopupShowing",
  OPTIONS_POPUP_HIDDEN: "Debugger:OptionsPopupHidden",

  // When the widgets layout has been changed.
  LAYOUT_CHANGED: "Debugger:LayoutChanged"
};

// Descriptions for what a stack frame represents after the debugger pauses.
const FRAME_TYPE = {
  NORMAL: 0,
  CONDITIONAL_BREAKPOINT_EVAL: 1,
  WATCH_EXPRESSIONS_EVAL: 2,
  PUBLIC_CLIENT_EVAL: 3
};

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/devtools/dbg-client.jsm");
Cu.import("resource:///modules/devtools/shared/event-emitter.js");
Cu.import("resource:///modules/devtools/BreadcrumbsWidget.jsm");
Cu.import("resource:///modules/devtools/SideMenuWidget.jsm");
Cu.import("resource:///modules/devtools/VariablesView.jsm");
Cu.import("resource:///modules/devtools/VariablesViewController.jsm");
Cu.import("resource:///modules/devtools/ViewHelpers.jsm");

const require = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools.require;
const promise = require("sdk/core/promise");
const Editor = require("devtools/sourceeditor/editor");
const DebuggerEditor = require("devtools/sourceeditor/debugger.js");
const {Tooltip} = require("devtools/shared/widgets/Tooltip");

XPCOMUtils.defineLazyModuleGetter(this, "Parser",
  "resource:///modules/devtools/Parser.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "devtools",
  "resource://gre/modules/devtools/Loader.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DevToolsUtils",
  "resource://gre/modules/devtools/DevToolsUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ShortcutUtils",
  "resource://gre/modules/ShortcutUtils.jsm");

Object.defineProperty(this, "NetworkHelper", {
  get: function() {
    return devtools.require("devtools/toolkit/webconsole/network-helper");
  },
  configurable: true,
  enumerable: true
});

/**
 * Object defining the debugger controller components.
 */
let DebuggerController = {
  /**
   * Initializes the debugger controller.
   */
  initialize: function() {
    dumpn("Initializing the DebuggerController");

    this.startupDebugger = this.startupDebugger.bind(this);
    this.shutdownDebugger = this.shutdownDebugger.bind(this);
    this._onTabNavigated = this._onTabNavigated.bind(this);
    this._onTabDetached = this._onTabDetached.bind(this);

    // Chrome debugging lives in a different process and needs to handle
    // debugger startup and shutdown by itself.
    if (window._isChromeDebugger) {
      window.addEventListener("DOMContentLoaded", this.startupDebugger, true);
      window.addEventListener("unload", this.shutdownDebugger, true);
    }
  },

  /**
   * Initializes the view.
   *
   * @return object
   *         A promise that is resolved when the debugger finishes startup.
   */
  startupDebugger: function() {
    if (this._startup) {
      return this._startup;
    }

    // Chrome debugging lives in a different process and needs to handle
    // debugger startup by itself.
    if (window._isChromeDebugger) {
      window.removeEventListener("DOMContentLoaded", this.startupDebugger, true);
    }

    return this._startup = DebuggerView.initialize().then(() => {
      // Chrome debugging needs to initiate the connection by itself.
      if (window._isChromeDebugger) {
        return this.connect();
      } else {
        return promise.resolve(null); // Done.
      }
    });
  },

  /**
   * Destroys the view and disconnects the debugger client from the server.
   *
   * @return object
   *         A promise that is resolved when the debugger finishes shutdown.
   */
  shutdownDebugger: function() {
    if (this._shutdown) {
      return this._shutdown;
    }

    // Chrome debugging lives in a different process and needs to handle
    // debugger shutdown by itself.
    if (window._isChromeDebugger) {
      window.removeEventListener("unload", this.shutdownDebugger, true);
    }

    return this._shutdown = DebuggerView.destroy().then(() => {
      DebuggerView.destroy();
      this.SourceScripts.disconnect();
      this.StackFrames.disconnect();
      this.ThreadState.disconnect();
      this.disconnect();

      // Chrome debugging needs to close its parent process on shutdown.
      if (window._isChromeDebugger) {
        return this._quitApp();
      } else {
        return promise.resolve(null); // Done.
      }
    });
  },

  /**
   * Initiates remote or chrome debugging based on the current target,
   * wiring event handlers as necessary.
   *
   * In case of a chrome debugger living in a different process, a socket
   * connection pipe is opened as well.
   *
   * @return object
   *         A promise that is resolved when the debugger finishes connecting.
   */
  connect: function() {
    if (this._connection) {
      return this._connection;
    }

    let deferred = promise.defer();
    this._connection = deferred.promise;

    if (!window._isChromeDebugger) {
      let target = this._target;
      let { client, form: { chromeDebugger }, threadActor } = target;
      target.on("close", this._onTabDetached);
      target.on("navigate", this._onTabNavigated);
      target.on("will-navigate", this._onTabNavigated);

      if (target.chrome) {
        this._startChromeDebugging(client, chromeDebugger, deferred.resolve);
      } else {
        this._startDebuggingTab(client, threadActor, deferred.resolve);
      }

      return deferred.promise;
    }

    // Chrome debugging needs to make its own connection to the debuggee.
    let transport = debuggerSocketConnect(
      Prefs.chromeDebuggingHost, Prefs.chromeDebuggingPort);

    let client = new DebuggerClient(transport);
    client.addListener("tabNavigated", this._onTabNavigated);
    client.addListener("tabDetached", this._onTabDetached);
    client.connect(() => {
      client.listTabs(aResponse => {
        this._startChromeDebugging(client, aResponse.chromeDebugger, deferred.resolve);
      });
    });

    return deferred.promise;
  },

  /**
   * Disconnects the debugger client and removes event handlers as necessary.
   */
  disconnect: function() {
    // Return early if the client didn't even have a chance to instantiate.
    if (!this.client) {
      return;
    }

    // When debugging local or a remote instance, the connection is closed by
    // the RemoteTarget. Chrome debugging needs to specifically close its own
    // connection to the debuggee.
    if (window._isChromeDebugger) {
      this.client.removeListener("tabNavigated", this._onTabNavigated);
      this.client.removeListener("tabDetached", this._onTabDetached);
      this.client.close();
    }

    this._connection = null;
    this.client = null;
    this.activeThread = null;
  },

  /**
   * Called for each location change in the debugged tab.
   *
   * @param string aType
   *        Packet type.
   * @param object aPacket
   *        Packet received from the server.
   */
  _onTabNavigated: function(aType, aPacket) {
    switch (aType) {
      case "will-navigate": {
        // Reset UI.
        DebuggerView.handleTabNavigation();

        // Discard all the cached sources *before* the target starts navigating.
        // Sources may be fetched during navigation, in which case we don't
        // want to hang on to the old source contents.
        DebuggerController.SourceScripts.clearCache();
        DebuggerController.Parser.clearCache();
        SourceUtils.clearCache();

        // Prevent performing any actions that were scheduled before navigation.
        clearNamedTimeout("new-source");
        clearNamedTimeout("event-breakpoints-update");
        clearNamedTimeout("event-listeners-fetch");
        break;
      }
      case "navigate": {
        this.ThreadState.handleTabNavigation();
        this.StackFrames.handleTabNavigation();
        this.SourceScripts.handleTabNavigation();
        break;
      }
    }
  },

  /**
   * Called when the debugged tab is closed.
   */
  _onTabDetached: function() {
    this.shutdownDebugger();
  },

  /**
   * Warn if resuming execution produced a wrongOrder error.
   */
  _ensureResumptionOrder: function(aResponse) {
    if (aResponse.error == "wrongOrder") {
      DebuggerView.Toolbar.showResumeWarning(aResponse.lastPausedUrl);
    }
  },

  /**
   * Sets up a debugging session.
   *
   * @param DebuggerClient aClient
   *        The debugger client.
   * @param string aThreadActor
   *        The remote protocol grip of the tab.
   * @param function aCallback
   *        A function to invoke once the client attached to the active thread.
   */
  _startDebuggingTab: function(aClient, aThreadActor, aCallback) {
    if (!aClient) {
      Cu.reportError("No client found!");
      return;
    }
    this.client = aClient;

    aClient.attachThread(aThreadActor, (aResponse, aThreadClient) => {
      if (!aThreadClient) {
        Cu.reportError("Couldn't attach to thread: " + aResponse.error);
        return;
      }
      this.activeThread = aThreadClient;

      this.ThreadState.connect();
      this.StackFrames.connect();
      this.SourceScripts.connect();
      aThreadClient.resume(this._ensureResumptionOrder);

      if (aCallback) {
        aCallback();
      }
    }, { useSourceMaps: Prefs.sourceMapsEnabled });
  },

  /**
   * Sets up a chrome debugging session.
   *
   * @param DebuggerClient aClient
   *        The debugger client.
   * @param object aChromeDebugger
   *        The remote protocol grip of the chrome debugger.
   * @param function aCallback
   *        A function to invoke once the client attached to the active thread.
   */
  _startChromeDebugging: function(aClient, aChromeDebugger, aCallback) {
    if (!aClient) {
      Cu.reportError("No client found!");
      return;
    }
    this.client = aClient;

    aClient.attachThread(aChromeDebugger, (aResponse, aThreadClient) => {
      if (!aThreadClient) {
        Cu.reportError("Couldn't attach to thread: " + aResponse.error);
        return;
      }
      this.activeThread = aThreadClient;

      this.ThreadState.connect();
      this.StackFrames.connect();
      this.SourceScripts.connect();
      aThreadClient.resume(this._ensureResumptionOrder);

      if (aCallback) {
        aCallback();
      }
    }, { useSourceMaps: Prefs.sourceMapsEnabled });
  },

  /**
   * Detach and reattach to the thread actor with useSourceMaps true, blow
   * away old sources and get them again.
   */
  reconfigureThread: function(aUseSourceMaps) {
    this.client.reconfigureThread({ useSourceMaps: aUseSourceMaps }, aResponse => {
      if (aResponse.error) {
        let msg = "Couldn't reconfigure thread: " + aResponse.message;
        Cu.reportError(msg);
        dumpn(msg);
        return;
      }

      // Reset the view and fetch all the sources again.
      DebuggerView.handleTabNavigation();
      this.SourceScripts.handleTabNavigation();

      // Update the stack frame list.
      this.activeThread._clearFrames();
      this.activeThread.fillFrames(CALL_STACK_PAGE_SIZE);
    });
  },

  /**
   * Attempts to quit the current process if allowed.
   *
   * @return object
   *         A promise that is resolved if the app will quit successfully.
   */
  _quitApp: function() {
    let deferred = promise.defer();

    // Quitting the app is synchronous. Give the returned promise consumers
    // a chance to do their thing before killing the process.
    Services.tm.currentThread.dispatch({ run: () => {
      let quit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
      Services.obs.notifyObservers(quit, "quit-application-requested", null);

      // Somebody canceled our quit request.
      if (quit.data) {
        deferred.reject(quit.data);
      } else {
        deferred.resolve(quit.data);
        Services.startup.quit(Ci.nsIAppStartup.eForceQuit);
      }
    }}, 0);

    return deferred.promise;
  },

  _startup: null,
  _shutdown: null,
  _connection: null,
  client: null,
  activeThread: null
};

/**
 * ThreadState keeps the UI up to date with the state of the
 * thread (paused/attached/etc.).
 */
function ThreadState() {
  this._update = this._update.bind(this);
}

ThreadState.prototype = {
  get activeThread() DebuggerController.activeThread,

  /**
   * Connect to the current thread client.
   */
  connect: function() {
    dumpn("ThreadState is connecting...");
    this.activeThread.addListener("paused", this._update);
    this.activeThread.addListener("resumed", this._update);
    this.activeThread.pauseOnExceptions(Prefs.pauseOnExceptions,
                                        Prefs.ignoreCaughtExceptions);
    this.handleTabNavigation();
  },

  /**
   * Disconnect from the client.
   */
  disconnect: function() {
    if (!this.activeThread) {
      return;
    }
    dumpn("ThreadState is disconnecting...");
    this.activeThread.removeListener("paused", this._update);
    this.activeThread.removeListener("resumed", this._update);
  },

  /**
   * Handles any initialization on a tab navigation event issued by the client.
   */
  handleTabNavigation: function() {
    if (!this.activeThread) {
      return;
    }
    dumpn("Handling tab navigation in the ThreadState");
    this._update();
  },

  /**
   * Update the UI after a thread state change.
   */
  _update: function(aEvent) {
    DebuggerView.Toolbar.toggleResumeButtonState(this.activeThread.state);

    if (gTarget && (aEvent == "paused" || aEvent == "resumed")) {
      gTarget.emit("thread-" + aEvent);
    }
  }
};

/**
 * Keeps the stack frame list up-to-date, using the thread client's
 * stack frame cache.
 */
function StackFrames() {
  this._onPaused = this._onPaused.bind(this);
  this._onResumed = this._onResumed.bind(this);
  this._onFrames = this._onFrames.bind(this);
  this._onFramesCleared = this._onFramesCleared.bind(this);
  this._onBlackBoxChange = this._onBlackBoxChange.bind(this);
  this._onPrettyPrintChange = this._onPrettyPrintChange.bind(this);
  this._afterFramesCleared = this._afterFramesCleared.bind(this);
  this.evaluate = this.evaluate.bind(this);
}

StackFrames.prototype = {
  get activeThread() DebuggerController.activeThread,
  currentFrameDepth: -1,
  _currentFrameDescription: FRAME_TYPE.NORMAL,
  _syncedWatchExpressions: null,
  _currentWatchExpressions: null,
  _currentBreakpointLocation: null,
  _currentEvaluation: null,
  _currentException: null,
  _currentReturnedValue: null,

  /**
   * Connect to the current thread client.
   */
  connect: function() {
    dumpn("StackFrames is connecting...");
    this.activeThread.addListener("paused", this._onPaused);
    this.activeThread.addListener("resumed", this._onResumed);
    this.activeThread.addListener("framesadded", this._onFrames);
    this.activeThread.addListener("framescleared", this._onFramesCleared);
    this.activeThread.addListener("blackboxchange", this._onBlackBoxChange);
    this.activeThread.addListener("prettyprintchange", this._onPrettyPrintChange);
    this.handleTabNavigation();
  },

  /**
   * Disconnect from the client.
   */
  disconnect: function() {
    if (!this.activeThread) {
      return;
    }
    dumpn("StackFrames is disconnecting...");
    this.activeThread.removeListener("paused", this._onPaused);
    this.activeThread.removeListener("resumed", this._onResumed);
    this.activeThread.removeListener("framesadded", this._onFrames);
    this.activeThread.removeListener("framescleared", this._onFramesCleared);
    this.activeThread.removeListener("blackboxchange", this._onBlackBoxChange);
    this.activeThread.removeListener("prettyprintchange", this._onPrettyPrintChange);
    clearNamedTimeout("frames-cleared");
  },

  /**
   * Handles any initialization on a tab navigation event issued by the client.
   */
  handleTabNavigation: function() {
    dumpn("Handling tab navigation in the StackFrames");
    // Nothing to do here yet.
  },

  /**
   * Handler for the thread client's paused notification.
   *
   * @param string aEvent
   *        The name of the notification ("paused" in this case).
   * @param object aPacket
   *        The response packet.
   */
  _onPaused: function(aEvent, aPacket) {
    switch (aPacket.why.type) {
      // If paused by a breakpoint, store the breakpoint location.
      case "breakpoint":
        this._currentBreakpointLocation = aPacket.frame.where;
        break;
      // If paused by a client evaluation, store the evaluated value.
      case "clientEvaluated":
        this._currentEvaluation = aPacket.why.frameFinished;
        break;
      // If paused by an exception, store the exception value.
      case "exception":
        this._currentException = aPacket.why.exception;
        break;
      // If paused while stepping out of a frame, store the returned value or
      // thrown exception.
      case "resumeLimit":
        if (!aPacket.why.frameFinished) {
          break;
        } else if (aPacket.why.frameFinished.throw) {
          this._currentException = aPacket.why.frameFinished.throw;
        } else if (aPacket.why.frameFinished.return) {
          this._currentReturnedValue = aPacket.why.frameFinished.return;
        }
        break;
    }

    this.activeThread.fillFrames(CALL_STACK_PAGE_SIZE);
    DebuggerView.editor.focus();
  },

  /**
   * Handler for the thread client's resumed notification.
   */
  _onResumed: function() {
    // Prepare the watch expression evaluation string for the next pause.
    if (this._currentFrameDescription != FRAME_TYPE.WATCH_EXPRESSIONS_EVAL) {
      this._currentWatchExpressions = this._syncedWatchExpressions;
    }
  },

  /**
   * Handler for the thread client's framesadded notification.
   */
  _onFrames: function() {
    // Ignore useless notifications.
    if (!this.activeThread || !this.activeThread.cachedFrames.length) {
      return;
    }

    let waitForNextPause = false;
    let breakLocation = this._currentBreakpointLocation;
    let watchExpressions = this._currentWatchExpressions;

    // Conditional breakpoints are { breakpoint, expression } tuples. The
    // boolean evaluation of the expression decides if the active thread
    // automatically resumes execution or not.
    // TODO: handle all of this server-side: Bug 812172.
    if (breakLocation) {
      // Make sure a breakpoint actually exists at the specified url and line.
      let breakpointPromise = DebuggerController.Breakpoints._getAdded(breakLocation);
      if (breakpointPromise) {
        breakpointPromise.then(({ conditionalExpression: e }) => { if (e) {
          // Evaluating the current breakpoint's conditional expression will
          // cause the stack frames to be cleared and active thread to pause,
          // sending a 'clientEvaluated' packed and adding the frames again.
          this.evaluate(e, { depth: 0, meta: FRAME_TYPE.CONDITIONAL_BREAKPOINT_EVAL });
          waitForNextPause = true;
        }});
      }
    }
    // We'll get our evaluation of the current breakpoint's conditional
    // expression the next time the thread client pauses...
    if (waitForNextPause) {
      return;
    }
    if (this._currentFrameDescription == FRAME_TYPE.CONDITIONAL_BREAKPOINT_EVAL) {
      this._currentFrameDescription = FRAME_TYPE.NORMAL;
      // If the breakpoint's conditional expression evaluation is falsy,
      // automatically resume execution.
      if (VariablesView.isFalsy({ value: this._currentEvaluation.return })) {
        this.activeThread.resume(DebuggerController._ensureResumptionOrder);
        return;
      }
    }

    // Watch expressions are evaluated in the context of the topmost frame,
    // and the results are displayed in the variables view.
    // TODO: handle all of this server-side: Bug 832470, comment 14.
    if (watchExpressions) {
      // Evaluation causes the stack frames to be cleared and active thread to
      // pause, sending a 'clientEvaluated' packet and adding the frames again.
      this.evaluate(watchExpressions, { depth: 0, meta: FRAME_TYPE.WATCH_EXPRESSIONS_EVAL });
      waitForNextPause = true;
    }
    // We'll get our evaluation of the current watch expressions the next time
    // the thread client pauses...
    if (waitForNextPause) {
      return;
    }
    if (this._currentFrameDescription == FRAME_TYPE.WATCH_EXPRESSIONS_EVAL) {
      this._currentFrameDescription = FRAME_TYPE.NORMAL;
      // If an error was thrown during the evaluation of the watch expressions,
      // then at least one expression evaluation could not be performed. So
      // remove the most recent watch expression and try again.
      if (this._currentEvaluation.throw) {
        DebuggerView.WatchExpressions.removeAt(0);
        DebuggerController.StackFrames.syncWatchExpressions();
        return;
      }
    }

    // Make sure the debugger view panes are visible, then refill the frames.
    DebuggerView.showInstrumentsPane();
    this._refillFrames();

    // No additional processing is necessary for this stack frame.
    if (this._currentFrameDescription != FRAME_TYPE.NORMAL) {
      this._currentFrameDescription = FRAME_TYPE.NORMAL;
    }
  },

  /**
   * Fill the StackFrames view with the frames we have in the cache, compressing
   * frames which have black boxed sources into single frames.
   */
  _refillFrames: function() {
    // Make sure all the previous stackframes are removed before re-adding them.
    DebuggerView.StackFrames.empty();

    for (let frame of this.activeThread.cachedFrames) {
      let { depth, where: { url, line }, source } = frame;
      let isBlackBoxed = source ? this.activeThread.source(source).isBlackBoxed : false;
      let location = NetworkHelper.convertToUnicode(unescape(url));
      let title = StackFrameUtils.getFrameTitle(frame);
      DebuggerView.StackFrames.addFrame(title, location, line, depth, isBlackBoxed);
    }

    DebuggerView.StackFrames.selectedDepth = Math.max(this.currentFrameDepth, 0);
    DebuggerView.StackFrames.dirty = this.activeThread.moreFrames;
  },

  /**
   * Handler for the thread client's framescleared notification.
   */
  _onFramesCleared: function() {
    switch (this._currentFrameDescription) {
      case FRAME_TYPE.NORMAL:
        this._currentEvaluation = null;
        this._currentException = null;
        this._currentReturnedValue = null;
        break;
      case FRAME_TYPE.CONDITIONAL_BREAKPOINT_EVAL:
        this._currentBreakpointLocation = null;
        break;
      case FRAME_TYPE.WATCH_EXPRESSIONS_EVAL:
        this._currentWatchExpressions = null;
        break;
    }

    // After each frame step (in, over, out), framescleared is fired, which
    // forces the UI to be emptied and rebuilt on framesadded. Most of the times
    // this is not necessary, and will result in a brief redraw flicker.
    // To avoid it, invalidate the UI only after a short time if necessary.
    setNamedTimeout("frames-cleared", FRAME_STEP_CLEAR_DELAY, this._afterFramesCleared);
  },

  /**
   * Handler for the debugger's blackboxchange notification.
   */
  _onBlackBoxChange: function() {
    if (this.activeThread.state == "paused") {
      // Hack to avoid selecting the topmost frame after blackboxing a source.
      this.currentFrameDepth = NaN;
      this._refillFrames();
    }
  },

  /**
   * Handler for the debugger's prettyprintchange notification.
   */
  _onPrettyPrintChange: function() {
    if (this.activeThread.state == "paused") {
      this.activeThread.fillFrames(CALL_STACK_PAGE_SIZE);
    }
  },

  /**
   * Called soon after the thread client's framescleared notification.
   */
  _afterFramesCleared: function() {
    // Ignore useless notifications.
    if (this.activeThread.cachedFrames.length) {
      return;
    }
    DebuggerView.editor.clearDebugLocation();
    DebuggerView.StackFrames.empty();
    DebuggerView.Sources.unhighlightBreakpoint();
    DebuggerView.WatchExpressions.toggleContents(true);
    DebuggerView.Variables.empty(0);

    window.emit(EVENTS.AFTER_FRAMES_CLEARED);
  },

  /**
   * Marks the stack frame at the specified depth as selected and updates the
   * properties view with the stack frame's data.
   *
   * @param number aDepth
   *        The depth of the frame in the stack.
   */
  selectFrame: function(aDepth) {
    // Make sure the frame at the specified depth exists first.
    let frame = this.activeThread.cachedFrames[this.currentFrameDepth = aDepth];
    if (!frame) {
      return;
    }

    // Check if the frame does not represent the evaluation of debuggee code.
    let { environment, where } = frame;
    if (!environment) {
      return;
    }

    // Don't change the editor's location if the execution was paused by a
    // public client evaluation. This is useful for adding overlays on
    // top of the editor, like a variable inspection popup.
    if (this._currentFrameDescription != FRAME_TYPE.PUBLIC_CLIENT_EVAL) {
      // Move the editor's caret to the proper url and line.
      DebuggerView.setEditorLocation(where.url, where.line);
      // Highlight the breakpoint at the specified url and line if it exists.
      DebuggerView.Sources.highlightBreakpoint(where, { noEditorUpdate: true });
    }

    // Don't display the watch expressions textbox inputs in the pane.
    DebuggerView.WatchExpressions.toggleContents(false);

    // Start recording any added variables or properties in any scope and
    // clear existing scopes to create each one dynamically.
    DebuggerView.Variables.createHierarchy();
    DebuggerView.Variables.empty();

    // If watch expressions evaluation results are available, create a scope
    // to contain all the values.
    if (this._syncedWatchExpressions && aDepth == 0) {
      let label = L10N.getStr("watchExpressionsScopeLabel");
      let scope = DebuggerView.Variables.addScope(label);

      // Customize the scope for holding watch expressions evaluations.
      scope.descriptorTooltip = false;
      scope.contextMenuId = "debuggerWatchExpressionsContextMenu";
      scope.separatorStr = L10N.getStr("watchExpressionsSeparatorLabel");
      scope.switch = DebuggerView.WatchExpressions.switchExpression;
      scope.delete = DebuggerView.WatchExpressions.deleteExpression;

      // The evaluation hasn't thrown, so fetch and add the returned results.
      this._fetchWatchExpressions(scope, this._currentEvaluation.return);

      // The watch expressions scope is always automatically expanded.
      scope.expand();
    }

    do {
      // Create a scope to contain all the inspected variables in the
      // current environment.
      let label = StackFrameUtils.getScopeLabel(environment);
      let scope = DebuggerView.Variables.addScope(label);
      let innermost = environment == frame.environment;

      // Handle special additions to the innermost scope.
      if (innermost) {
        this._insertScopeFrameReferences(scope, frame);
      }

      // Handle the expansion of the scope, lazily populating it with the
      // variables in the current environment.
      DebuggerView.Variables.controller.addExpander(scope, environment);

      // The innermost scope is always automatically expanded, because it
      // contains the variables in the current stack frame which are likely to
      // be inspected.
      if (innermost) {
        scope.expand();
      }
    } while ((environment = environment.parent));

    // Signal that scope environments have been shown and commit the current
    // variables view hierarchy to briefly flash items that changed between the
    // previous and current scope/variables/properties.
    window.emit(EVENTS.FETCHED_SCOPES);
    DebuggerView.Variables.commitHierarchy();
  },

  /**
   * Loads more stack frames from the debugger server cache.
   */
  addMoreFrames: function() {
    this.activeThread.fillFrames(
      this.activeThread.cachedFrames.length + CALL_STACK_PAGE_SIZE);
  },

  /**
   * Evaluate an expression in the context of the selected frame.
   *
   * @param string aExpression
   *        The expression to evaluate.
   * @param object aOptions [optional]
   *        Additional options for this client evaluation:
   *          - depth: the frame depth used for evaluation, 0 being the topmost.
   *          - meta: some meta-description for what this evaluation represents.
   * @return object
   *         A promise that is resolved when the evaluation finishes,
   *         or rejected if there was no stack frame available or some
   *         other error occurred.
   */
  evaluate: function(aExpression, aOptions = {}) {
    let depth = "depth" in aOptions ? aOptions.depth : this.currentFrameDepth;
    let frame = this.activeThread.cachedFrames[depth];
    if (frame == null) {
      return promise.reject(new Error("No stack frame available."));
    }

    let deferred = promise.defer();

    this.activeThread.addOneTimeListener("paused", (aEvent, aPacket) => {
      let { type, frameFinished } = aPacket.why;
      if (type == "clientEvaluated") {
        if (!("terminated" in frameFinished)) {
          deferred.resolve(frameFinished);
        } else {
          deferred.reject(new Error("The execution was abruptly terminated."));
        }
      } else {
        deferred.reject(new Error("Active thread paused unexpectedly."));
      }
    });

    let meta = "meta" in aOptions ? aOptions.meta : FRAME_TYPE.PUBLIC_CLIENT_EVAL;
    this._currentFrameDescription = meta;
    this.activeThread.eval(frame.actor, aExpression);

    return deferred.promise;
  },

  /**
   * Add nodes for special frame references in the innermost scope.
   *
   * @param Scope aScope
   *        The scope where the references will be placed into.
   * @param object aFrame
   *        The frame to get some references from.
   */
  _insertScopeFrameReferences: function(aScope, aFrame) {
    // Add any thrown exception.
    if (this._currentException) {
      let excRef = aScope.addItem("<exception>", { value: this._currentException });
      DebuggerView.Variables.controller.addExpander(excRef, this._currentException);
    }
    // Add any returned value.
    if (this._currentReturnedValue) {
      let retRef = aScope.addItem("<return>", { value: this._currentReturnedValue });
      DebuggerView.Variables.controller.addExpander(retRef, this._currentReturnedValue);
    }
    // Add "this".
    if (aFrame.this) {
      let thisRef = aScope.addItem("this", { value: aFrame.this });
      DebuggerView.Variables.controller.addExpander(thisRef, aFrame.this);
    }
  },

  /**
   * Adds the watch expressions evaluation results to a scope in the view.
   *
   * @param Scope aScope
   *        The scope where the watch expressions will be placed into.
   * @param object aExp
   *        The grip of the evaluation results.
   */
  _fetchWatchExpressions: function(aScope, aExp) {
    // Fetch the expressions only once.
    if (aScope._fetched) {
      return;
    }
    aScope._fetched = true;

    // Add nodes for every watch expression in scope.
    this.activeThread.pauseGrip(aExp).getPrototypeAndProperties(aResponse => {
      let ownProperties = aResponse.ownProperties;
      let totalExpressions = DebuggerView.WatchExpressions.itemCount;

      for (let i = 0; i < totalExpressions; i++) {
        let name = DebuggerView.WatchExpressions.getString(i);
        let expVal = ownProperties[i].value;
        let expRef = aScope.addItem(name, ownProperties[i]);
        DebuggerView.Variables.controller.addExpander(expRef, expVal);

        // Revert some of the custom watch expressions scope presentation flags,
        // so that they don't propagate to child items.
        expRef.switch = null;
        expRef.delete = null;
        expRef.descriptorTooltip = true;
        expRef.separatorStr = L10N.getStr("variablesSeparatorLabel");
      }

      // Signal that watch expressions have been fetched and commit the
      // current variables view hierarchy to briefly flash items that changed
      // between the previous and current scope/variables/properties.
      window.emit(EVENTS.FETCHED_WATCH_EXPRESSIONS);
      DebuggerView.Variables.commitHierarchy();
    });
  },

  /**
   * Updates a list of watch expressions to evaluate on each pause.
   * TODO: handle all of this server-side: Bug 832470, comment 14.
   */
  syncWatchExpressions: function() {
    let list = DebuggerView.WatchExpressions.getAllStrings();

    // Sanity check all watch expressions before syncing them. To avoid
    // having the whole watch expressions array throw because of a single
    // faulty expression, simply convert it to a string describing the error.
    // There's no other information necessary to be offered in such cases.
    let sanitizedExpressions = list.map(aString => {
      // Reflect.parse throws when it encounters a syntax error.
      try {
        Parser.reflectionAPI.parse(aString);
        return aString; // Watch expression can be executed safely.
      } catch (e) {
        return "\"" + e.name + ": " + e.message + "\""; // Syntax error.
      }
    });

    if (sanitizedExpressions.length) {
      this._syncedWatchExpressions =
        this._currentWatchExpressions =
          "[" +
            sanitizedExpressions.map(aString =>
              "eval(\"" +
                "try {" +
                  // Make sure all quotes are escaped in the expression's syntax,
                  // and add a newline after the statement to avoid comments
                  // breaking the code integrity inside the eval block.
                  aString.replace(/"/g, "\\$&") + "\" + " + "'\\n'" + " + \"" +
                "} catch (e) {" +
                  "e.name + ': ' + e.message;" + // TODO: Bug 812765, 812764.
                "}" +
              "\")"
            ).join(",") +
          "]";
    } else {
      this._syncedWatchExpressions =
        this._currentWatchExpressions = null;
    }

    this.currentFrameDepth = -1;
    this._onFrames();
  }
};

/**
 * Keeps the source script list up-to-date, using the thread client's
 * source script cache.
 */
function SourceScripts() {
  this._onNewGlobal = this._onNewGlobal.bind(this);
  this._onNewSource = this._onNewSource.bind(this);
  this._onSourcesAdded = this._onSourcesAdded.bind(this);
  this._onBlackBoxChange = this._onBlackBoxChange.bind(this);
  this._onPrettyPrintChange = this._onPrettyPrintChange.bind(this);
}

SourceScripts.prototype = {
  get activeThread() DebuggerController.activeThread,
  get debuggerClient() DebuggerController.client,
  _cache: new Map(),

  /**
   * Connect to the current thread client.
   */
  connect: function() {
    dumpn("SourceScripts is connecting...");
    this.debuggerClient.addListener("newGlobal", this._onNewGlobal);
    this.debuggerClient.addListener("newSource", this._onNewSource);
    this.activeThread.addListener("blackboxchange", this._onBlackBoxChange);
    this.activeThread.addListener("prettyprintchange", this._onPrettyPrintChange);
    this.handleTabNavigation();
  },

  /**
   * Disconnect from the client.
   */
  disconnect: function() {
    if (!this.activeThread) {
      return;
    }
    dumpn("SourceScripts is disconnecting...");
    this.debuggerClient.removeListener("newGlobal", this._onNewGlobal);
    this.debuggerClient.removeListener("newSource", this._onNewSource);
    this.activeThread.removeListener("blackboxchange", this._onBlackBoxChange);
    this.activeThread.addListener("prettyprintchange", this._onPrettyPrintChange);
  },

  /**
   * Clears all the cached source contents.
   */
  clearCache: function() {
    this._cache.clear();
  },

  /**
   * Handles any initialization on a tab navigation event issued by the client.
   */
  handleTabNavigation: function() {
    if (!this.activeThread) {
      return;
    }
    dumpn("Handling tab navigation in the SourceScripts");

    // Retrieve the list of script sources known to the server from before
    // the client was ready to handle "newSource" notifications.
    this.activeThread.getSources(this._onSourcesAdded);
  },

  /**
   * Handler for the debugger client's unsolicited newGlobal notification.
   */
  _onNewGlobal: function(aNotification, aPacket) {
    // TODO: bug 806775, update the globals list using aPacket.hostAnnotations
    // from bug 801084.
  },

  /**
   * Handler for the debugger client's unsolicited newSource notification.
   */
  _onNewSource: function(aNotification, aPacket) {
    // Ignore bogus scripts, e.g. generated from 'clientEvaluate' packets.
    if (NEW_SOURCE_IGNORED_URLS.indexOf(aPacket.source.url) != -1) {
      return;
    }

    // Add the source in the debugger view sources container.
    DebuggerView.Sources.addSource(aPacket.source, { staged: false });

    // Select this source if it's the preferred one.
    let preferredValue = DebuggerView.Sources.preferredValue;
    if (aPacket.source.url == preferredValue) {
      DebuggerView.Sources.selectedValue = preferredValue;
    }
    // ..or the first entry if there's none selected yet after a while
    else {
      setNamedTimeout("new-source", NEW_SOURCE_DISPLAY_DELAY, () => {
        // If after a certain delay the preferred source still wasn't received,
        // just give up on waiting and display the first entry.
        if (!DebuggerView.Sources.selectedValue) {
          DebuggerView.Sources.selectedIndex = 0;
        }
      });
    }

    // If there are any stored breakpoints for this source, display them again,
    // both in the editor and the breakpoints pane.
    DebuggerController.Breakpoints.updateEditorBreakpoints();
    DebuggerController.Breakpoints.updatePaneBreakpoints();

    // Make sure the events listeners are up to date.
    if (DebuggerView.instrumentsPaneTab == "events-tab") {
      DebuggerController.Breakpoints.DOM.scheduleEventListenersFetch();
    }

    // Signal that a new source has been added.
    window.emit(EVENTS.NEW_SOURCE);
  },

  /**
   * Callback for the debugger's active thread getSources() method.
   */
  _onSourcesAdded: function(aResponse) {
    if (aResponse.error) {
      let msg = "Error getting sources: " + aResponse.message;
      Cu.reportError(msg);
      dumpn(msg);
      return;
    }

    // Add all the sources in the debugger view sources container.
    for (let source of aResponse.sources) {
      // Ignore bogus scripts, e.g. generated from 'clientEvaluate' packets.
      if (NEW_SOURCE_IGNORED_URLS.indexOf(source.url) == -1) {
        DebuggerView.Sources.addSource(source, { staged: true });
      }
    }

    // Flushes all the prepared sources into the sources container.
    DebuggerView.Sources.commit({ sorted: true });

    // Select the preferred source if it exists and was part of the response.
    let preferredValue = DebuggerView.Sources.preferredValue;
    if (DebuggerView.Sources.containsValue(preferredValue)) {
      DebuggerView.Sources.selectedValue = preferredValue;
    }
    // ..or the first entry if there's no one selected yet.
    else if (!DebuggerView.Sources.selectedValue) {
      DebuggerView.Sources.selectedIndex = 0;
    }

    // If there are any stored breakpoints for the sources, display them again,
    // both in the editor and the breakpoints pane.
    DebuggerController.Breakpoints.updateEditorBreakpoints();
    DebuggerController.Breakpoints.updatePaneBreakpoints();

    // Signal that sources have been added.
    window.emit(EVENTS.SOURCES_ADDED);
  },

  /**
   * Handler for the debugger client's 'blackboxchange' notification.
   */
  _onBlackBoxChange: function (aEvent, { url, isBlackBoxed }) {
    const item = DebuggerView.Sources.getItemByValue(url);
    if (item) {
      if (isBlackBoxed) {
        item.target.classList.add("black-boxed");
      } else {
        item.target.classList.remove("black-boxed");
      }
    }
    DebuggerView.Sources.updateToolbarButtonsState();
    DebuggerView.maybeShowBlackBoxMessage();
  },

  /**
   * Set the black boxed status of the given source.
   *
   * @param Object aSource
   *        The source form.
   * @param bool aBlackBoxFlag
   *        True to black box the source, false to un-black box it.
   * @returns Promise
   *          A promize that resolves to [aSource, isBlackBoxed] or rejects to
   *          [aSource, error].
   */
  setBlackBoxing: function(aSource, aBlackBoxFlag) {
    const sourceClient = this.activeThread.source(aSource);
    const deferred = promise.defer();

    sourceClient[aBlackBoxFlag ? "blackBox" : "unblackBox"](aPacket => {
      const { error, message } = aPacket;
      if (error) {
        let msg = "Couldn't toggle black boxing for " + aSource.url + ": " + message;
        dumpn(msg);
        Cu.reportError(msg);
        deferred.reject([aSource, msg]);
      } else {
        deferred.resolve([aSource, sourceClient.isBlackBoxed]);
      }
    });

    return deferred.promise;
  },

  /**
   * Toggle the pretty printing of a source's text. All subsequent calls to
   * |getText| will return the pretty-toggled text. Nothing will happen for
   * non-javascript files.
   *
   * @param Object aSource
   *        The source form from the RDP.
   * @returns Promise
   *          A promise that resolves to [aSource, prettyText] or rejects to
   *          [aSource, error].
   */
  togglePrettyPrint: function(aSource) {
    // Only attempt to pretty print JavaScript sources.
    if (!SourceUtils.isJavaScript(aSource.url, aSource.contentType)) {
      return promise.reject([aSource, "Can't prettify non-javascript files."]);
    }

    const sourceClient = this.activeThread.source(aSource);
    const wantPretty = !sourceClient.isPrettyPrinted;

    // Only use the existing promise if it is pretty printed.
    let textPromise = this._cache.get(aSource.url);
    if (textPromise && textPromise.pretty === wantPretty) {
      return textPromise;
    }

    const deferred = promise.defer();
    deferred.promise.pretty = wantPretty;
    this._cache.set(aSource.url, deferred.promise);

    const afterToggle = ({ error, message, source: text }) => {
      if (error) {
        // Revert the rejected promise from the cache, so that the original
        // source's text may be shown when the source is selected.
        this._cache.set(aSource.url, textPromise);
        deferred.reject([aSource, message || error]);
        return;
      }
      deferred.resolve([aSource, text]);
    };

    if (wantPretty) {
      sourceClient.prettyPrint(Prefs.editorTabSize, afterToggle);
    } else {
      sourceClient.disablePrettyPrint(afterToggle);
    }

    return deferred.promise;
  },

  /**
   * Handler for the debugger's prettyprintchange notification.
   */
  _onPrettyPrintChange: function(aEvent, { url }) {
    // Remove the cached source AST from the Parser, to avoid getting
    // wrong locations when searching for functions.
    DebuggerController.Parser.clearSource(url);
  },

  /**
   * Gets a specified source's text.
   *
   * @param object aSource
   *        The source object coming from the active thread.
   * @param function aOnTimeout [optional]
   *        Function called when the source text takes a long time to fetch,
   *        but not necessarily failing. Long fetch times don't cause the
   *        rejection of the returned promise.
   * @param number aDelay [optional]
   *        The amount of time it takes to consider a source slow to fetch.
   *        If unspecified, it defaults to a predefined value.
   * @return object
   *         A promise that is resolved after the source text has been fetched.
   */
  getText: function(aSource, aOnTimeout, aDelay = FETCH_SOURCE_RESPONSE_DELAY) {
    // Fetch the source text only once.
    let textPromise = this._cache.get(aSource.url);
    if (textPromise) {
      return textPromise;
    }

    let deferred = promise.defer();
    this._cache.set(aSource.url, deferred.promise);

    // If the source text takes a long time to fetch, invoke a callback.
    if (aOnTimeout) {
      var fetchTimeout = window.setTimeout(() => aOnTimeout(aSource), aDelay);
    }

    // Get the source text from the active thread.
    this.activeThread.source(aSource).source(({ error, message, source: text }) => {
      if (aOnTimeout) {
        window.clearTimeout(fetchTimeout);
      }
      if (error) {
        deferred.reject([aSource, message || error]);
      } else {
        deferred.resolve([aSource, text]);
      }
    });

    return deferred.promise;
  },

  /**
   * Starts fetching all the sources, silently.
   *
   * @param array aUrls
   *        The urls for the sources to fetch. If fetching a source's text
   *        takes too long, it will be discarded.
   * @return object
   *         A promise that is resolved after source texts have been fetched.
   */
  getTextForSources: function(aUrls) {
    let deferred = promise.defer();
    let pending = new Set(aUrls);
    let fetched = [];

    // Can't use promise.all, because if one fetch operation is rejected, then
    // everything is considered rejected, thus no other subsequent source will
    // be getting fetched. We don't want that. Something like Q's allSettled
    // would work like a charm here.

    // Try to fetch as many sources as possible.
    for (let url of aUrls) {
      let sourceItem = DebuggerView.Sources.getItemByValue(url);
      let sourceForm = sourceItem.attachment.source;
      this.getText(sourceForm, onTimeout).then(onFetch, onError);
    }

    /* Called if fetching a source takes too long. */
    function onTimeout(aSource) {
      onError([aSource]);
    }

    /* Called if fetching a source finishes successfully. */
    function onFetch([aSource, aText]) {
      // If fetching the source has previously timed out, discard it this time.
      if (!pending.has(aSource.url)) {
        return;
      }
      pending.delete(aSource.url);
      fetched.push([aSource.url, aText]);
      maybeFinish();
    }

    /* Called if fetching a source failed because of an error. */
    function onError([aSource, aError]) {
      pending.delete(aSource.url);
      maybeFinish();
    }

    /* Called every time something interesting happens while fetching sources. */
    function maybeFinish() {
      if (pending.size == 0) {
        // Sort the fetched sources alphabetically by their url.
        deferred.resolve(fetched.sort(([aFirst], [aSecond]) => aFirst > aSecond));
      }
    }

    return deferred.promise;
  }
};

/**
 * Handles breaking on event listeners in the currently debugged target.
 */
function EventListeners() {
  this._onEventListeners = this._onEventListeners.bind(this);
}

EventListeners.prototype = {
  /**
   * A list of event names on which the debuggee will automatically pause
   * when invoked.
   */
  activeEventNames: [],

  /**
   * Updates the list of events types with listeners that, when invoked,
   * will automatically pause the debuggee. The respective events are
   * retrieved from the UI.
   */
  scheduleEventBreakpointsUpdate: function() {
    // Make sure we're not sending a batch of closely repeated requests.
    // This can easily happen when toggling all events of a certain type.
    setNamedTimeout("event-breakpoints-update", 0, () => {
      this.activeEventNames = DebuggerView.EventListeners.getCheckedEvents();
      gThreadClient.pauseOnDOMEvents(this.activeEventNames);

      // Notify that event breakpoints were added/removed on the server.
      window.emit(EVENTS.EVENT_BREAKPOINTS_UPDATED);
    });
  },

  /**
   * Fetches the currently attached event listeners from the debugee.
   */
  scheduleEventListenersFetch: function() {
    let getListeners = aCallback => gThreadClient.eventListeners(aResponse => {
      if (aResponse.error) {
        let msg = "Error getting event listeners: " + aResponse.message;
        DevToolsUtils.reportException("scheduleEventListenersFetch", msg);
        return;
      }

      let outstandingListenersDefinitionSite = aResponse.listeners.map(aListener => {
        const deferred = promise.defer();

        gThreadClient.pauseGrip(aListener.function).getDefinitionSite(aResponse => {
          if (aResponse.error) {
            const msg = "Error getting function definition site: " + aResponse.message;
            DevToolsUtils.reportException("scheduleEventListenersFetch", msg);
            deferred.reject(msg);
            return;
          }

          aListener.function.url = aResponse.url;
          deferred.resolve(aListener);
        });

        return deferred.promise;
      });

      promise.all(outstandingListenersDefinitionSite).then(aListeners => {
        this._onEventListeners(aListeners);

        // Notify that event listeners were fetched and shown in the view,
        // and callback to resume the active thread if necessary.
        window.emit(EVENTS.EVENT_LISTENERS_FETCHED);
        aCallback && aCallback();
      });
    });

    // Make sure we're not sending a batch of closely repeated requests.
    // This can easily happen whenever new sources are fetched.
    setNamedTimeout("event-listeners-fetch", FETCH_EVENT_LISTENERS_DELAY, () => {
      if (gThreadClient.state != "paused") {
        gThreadClient.interrupt(() => getListeners(() => gThreadClient.resume()));
      } else {
        getListeners();
      }
    });
  },

  /**
   * Callback for a debugger's successful active thread eventListeners() call.
   */
  _onEventListeners: function(aListeners) {
    // Add all the listeners in the debugger view event linsteners container.
    for (let listener of aListeners) {
      DebuggerView.EventListeners.addListener(listener, { staged: true });
    }

    // Flushes all the prepared events into the event listeners container.
    DebuggerView.EventListeners.commit();
  }
};

/**
 * Handles all the breakpoints in the current debugger.
 */
function Breakpoints() {
  this._onEditorBreakpointAdd = this._onEditorBreakpointAdd.bind(this);
  this._onEditorBreakpointRemove = this._onEditorBreakpointRemove.bind(this);
  this.addBreakpoint = this.addBreakpoint.bind(this);
  this.removeBreakpoint = this.removeBreakpoint.bind(this);
}

Breakpoints.prototype = {
  /**
   * A map of breakpoint promises as tracked by the debugger frontend.
   * The keys consist of a string representation of the breakpoint location.
   */
  _added: new Map(),
  _removing: new Map(),
  _disabled: new Map(),

  /**
   * Adds the source editor breakpoint handlers.
   *
   * @return object
   *         A promise that is resolved when the breakpoints finishes initializing.
   */
  initialize: function() {
    DebuggerView.editor.on("breakpointAdded", this._onEditorBreakpointAdd);
    DebuggerView.editor.on("breakpointRemoved", this._onEditorBreakpointRemove);

    // Initialization is synchronous, for now.
    return promise.resolve(null);
  },

  /**
   * Removes the source editor breakpoint handlers & all the added breakpoints.
   *
   * @return object
   *         A promise that is resolved when the breakpoints finishes destroying.
   */
  destroy: function() {
    DebuggerView.editor.off("breakpointAdded", this._onEditorBreakpointAdd);
    DebuggerView.editor.off("breakpointRemoved", this._onEditorBreakpointRemove);

    return this.removeAllBreakpoints();
  },

  /**
   * Event handler for new breakpoints that come from the editor.
   *
   * @param number aLine
   *        Line number where breakpoint was set.
   */
  _onEditorBreakpointAdd: function(_, aLine) {
    let url = DebuggerView.Sources.selectedValue;
    let location = { url: url, line: aLine + 1 };

    // Initialize the breakpoint, but don't update the editor, since this
    // callback is invoked because a breakpoint was added in the editor itself.
    this.addBreakpoint(location, { noEditorUpdate: true }).then(aBreakpointClient => {
      // If the breakpoint client has an "requestedLocation" attached, then
      // the original requested placement for the breakpoint wasn't accepted.
      // In this case, we need to update the editor with the new location.
      if (aBreakpointClient.requestedLocation) {
        DebuggerView.editor.removeBreakpoint(aBreakpointClient.requestedLocation.line - 1);
        DebuggerView.editor.addBreakpoint(aBreakpointClient.location.line - 1);
      }
      // Notify that we've shown a breakpoint in the source editor.
      window.emit(EVENTS.BREAKPOINT_SHOWN);
    });
  },

  /**
   * Event handler for breakpoints that are removed from the editor.
   *
   * @param number aLine
   *        Line number where breakpoint was removed.
   */
  _onEditorBreakpointRemove: function(_, aLine) {
    let url = DebuggerView.Sources.selectedValue;
    let location = { url: url, line: aLine + 1 };

    // Destroy the breakpoint, but don't update the editor, since this callback
    // is invoked because a breakpoint was removed from the editor itself.
    this.removeBreakpoint(location, { noEditorUpdate: true }).then(() => {
      // Notify that we've hidden a breakpoint in the source editor.
      window.emit(EVENTS.BREAKPOINT_HIDDEN);
    });
  },

  /**
   * Update the breakpoints in the editor view. This function takes the list of
   * breakpoints in the debugger and adds them back into the editor view.
   * This is invoked when the selected script is changed, or when new sources
   * are received via the _onNewSource and _onSourcesAdded event listeners.
   */
  updateEditorBreakpoints: function() {
    for (let breakpointPromise of this._addedOrDisabled) {
      breakpointPromise.then(aBreakpointClient => {
        let currentSourceUrl = DebuggerView.Sources.selectedValue;
        let breakpointUrl = aBreakpointClient.location.url;

        // Update the view only if the breakpoint is in the currently shown source.
        if (currentSourceUrl == breakpointUrl) {
          this._showBreakpoint(aBreakpointClient, { noPaneUpdate: true });
        }
      });
    }
  },

  /**
   * Update the breakpoints in the pane view. This function takes the list of
   * breakpoints in the debugger and adds them back into the breakpoints pane.
   * This is invoked when new sources are received via the _onNewSource and
   * _onSourcesAdded event listeners.
   */
  updatePaneBreakpoints: function() {
    for (let breakpointPromise of this._addedOrDisabled) {
      breakpointPromise.then(aBreakpointClient => {
        let container = DebuggerView.Sources;
        let breakpointUrl = aBreakpointClient.location.url;

        // Update the view only if the breakpoint exists in a known source.
        if (container.containsValue(breakpointUrl)) {
          this._showBreakpoint(aBreakpointClient, { noEditorUpdate: true });
        }
      });
    }
  },

  /**
   * Add a breakpoint.
   *
   * @param object aLocation
   *        The location where you want the breakpoint.
   *        This object must have two properties:
   *          - url: the breakpoint's source location.
   *          - line: the breakpoint's line number.
   * @param object aOptions [optional]
   *        Additional options or flags supported by this operation:
   *          - openPopup: tells if the expression popup should be shown.
   *          - noEditorUpdate: tells if you want to skip editor updates.
   *          - noPaneUpdate: tells if you want to skip breakpoint pane updates.
   * @return object
   *         A promise that is resolved after the breakpoint is added, or
   *         rejected if there was an error.
   */
  addBreakpoint: function(aLocation, aOptions = {}) {
    // Make sure a proper location is available.
    if (!aLocation) {
      return promise.reject(new Error("Invalid breakpoint location."));
    }

    // If the breakpoint was already added, or is currently being added at the
    // specified location, then return that promise immediately.
    let addedPromise = this._getAdded(aLocation);
    if (addedPromise) {
      return addedPromise;
    }

    // If the breakpoint is currently being removed from the specified location,
    // then wait for that to finish and retry afterwards.
    let removingPromise = this._getRemoving(aLocation);
    if (removingPromise) {
      return removingPromise.then(() => this.addBreakpoint(aLocation, aOptions));
    }

    let deferred = promise.defer();

    // Remember the breakpoint initialization promise in the store.
    let identifier = this.getIdentifier(aLocation);
    this._added.set(identifier, deferred.promise);

    // Try adding the breakpoint.
    gThreadClient.setBreakpoint(aLocation, (aResponse, aBreakpointClient) => {
      // If the breakpoint response has an "actualLocation" attached, then
      // the original requested placement for the breakpoint wasn't accepted.
      if (aResponse.actualLocation) {
        // Remember the initialization promise for the new location instead.
        let oldIdentifier = identifier;
        let newIdentifier = identifier = this.getIdentifier(aResponse.actualLocation);
        this._added.delete(oldIdentifier);
        this._added.set(newIdentifier, deferred.promise);

        // Store the originally requested location in case it's ever needed
        // and update the breakpoint client with the actual location.
        aBreakpointClient.requestedLocation = aLocation;
        aBreakpointClient.location = aResponse.actualLocation;
      }

      // By default, new breakpoints are always enabled. Disabled breakpoints
      // are, in fact, removed from the server but preserved in the frontend,
      // so that they may not be forgotten across target navigations.
      let disabledPromise = this._disabled.get(identifier);
      if (disabledPromise) {
        disabledPromise.then(({ conditionalExpression: previousValue }) => {
          aBreakpointClient.conditionalExpression = previousValue;
        });
        this._disabled.delete(identifier);
      }

      // Preserve information about the breakpoint's line text, to display it
      // in the sources pane without requiring fetching the source (for example,
      // after the target navigated). Note that this will get out of sync
      // if the source text contents change.
      let line = aBreakpointClient.location.line - 1;
      aBreakpointClient.text = DebuggerView.editor.getText(line).trim();

      // Show the breakpoint in the editor and breakpoints pane, and resolve.
      this._showBreakpoint(aBreakpointClient, aOptions);

      // Notify that we've added a breakpoint.
      window.emit(EVENTS.BREAKPOINT_ADDED, aBreakpointClient);
      deferred.resolve(aBreakpointClient);
    });

    return deferred.promise;
  },

  /**
   * Remove a breakpoint.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   * @param object aOptions [optional]
   *        @see DebuggerController.Breakpoints.addBreakpoint
   * @return object
   *         A promise that is resolved after the breakpoint is removed, or
   *         rejected if there was an error.
   */
  removeBreakpoint: function(aLocation, aOptions = {}) {
    // Make sure a proper location is available.
    if (!aLocation) {
      return promise.reject(new Error("Invalid breakpoint location."));
    }

    // If the breakpoint was already removed, or has never even been added,
    // then return a resolved promise immediately.
    let addedPromise = this._getAdded(aLocation);
    if (!addedPromise) {
      return promise.resolve(aLocation);
    }

    // If the breakpoint is currently being removed from the specified location,
    // then return that promise immediately.
    let removingPromise = this._getRemoving(aLocation);
    if (removingPromise) {
      return removingPromise;
    }

    let deferred = promise.defer();

    // Remember the breakpoint removal promise in the store.
    let identifier = this.getIdentifier(aLocation);
    this._removing.set(identifier, deferred.promise);

    // Retrieve the corresponding breakpoint client first.
    addedPromise.then(aBreakpointClient => {
      // Try removing the breakpoint.
      aBreakpointClient.remove(aResponse => {
        // If there was an error removing the breakpoint, reject the promise
        // and forget about it that the breakpoint may be re-removed later.
        if (aResponse.error) {
          deferred.reject(aResponse);
          return void this._removing.delete(identifier);
        }

        // When a breakpoint is removed, the frontend may wish to preserve some
        // details about it, so that it can be easily re-added later. In such
        // cases, breakpoints are marked and stored as disabled, so that they
        // may not be forgotten across target navigations.
        if (aOptions.rememberDisabled) {
          aBreakpointClient.disabled = true;
          this._disabled.set(identifier, promise.resolve(aBreakpointClient));
        }

        // Forget both the initialization and removal promises from the store.
        this._added.delete(identifier);
        this._removing.delete(identifier);

        // Hide the breakpoint from the editor and breakpoints pane, and resolve.
        this._hideBreakpoint(aLocation, aOptions);

        // Notify that we've removed a breakpoint.
        window.emit(EVENTS.BREAKPOINT_REMOVED, aLocation);
        deferred.resolve(aLocation);
      });
    });

    return deferred.promise;
  },

  /**
   * Removes all the currently enabled breakpoints.
   *
   * @return object
   *         A promise that is resolved after all breakpoints are removed, or
   *         rejected if there was an error.
   */
  removeAllBreakpoints: function() {
    /* Gets an array of all the existing breakpoints promises. */
    let getActiveBreakpoints = (aPromises, aStore = []) => {
      for (let [, breakpointPromise] of aPromises) {
        aStore.push(breakpointPromise);
      }
      return aStore;
    }

    /* Gets an array of all the removed breakpoints promises. */
    let getRemovedBreakpoints = (aClients, aStore = []) => {
      for (let breakpointClient of aClients) {
        aStore.push(this.removeBreakpoint(breakpointClient.location));
      }
      return aStore;
    }

    // First, populate an array of all the currently added breakpoints promises.
    // Then, once all the breakpoints clients are retrieved, populate an array
    // of all the removed breakpoints promises and wait for their fulfillment.
    return promise.all(getActiveBreakpoints(this._added)).then(aBreakpointClients => {
      return promise.all(getRemovedBreakpoints(aBreakpointClients));
    });
  },

  /**
   * Update the editor and breakpoints pane to show a specified breakpoint.
   *
   * @param object aBreakpointData
   *        Information about the breakpoint to be shown.
   *        This object must have the following properties:
   *          - location: the breakpoint's source location and line number
   *          - disabled: the breakpoint's disabled state, boolean
   *          - text: the breakpoint's line text to be displayed
   * @param object aOptions [optional]
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  _showBreakpoint: function(aBreakpointData, aOptions = {}) {
    let currentSourceUrl = DebuggerView.Sources.selectedValue;
    let location = aBreakpointData.location;

    // Update the editor if required.
    if (!aOptions.noEditorUpdate && !aBreakpointData.disabled) {
      if (location.url == currentSourceUrl) {
        DebuggerView.editor.addBreakpoint(location.line - 1);
      }
    }

    // Update the breakpoints pane if required.
    if (!aOptions.noPaneUpdate) {
      DebuggerView.Sources.addBreakpoint(aBreakpointData, aOptions);
    }
  },

  /**
   * Update the editor and breakpoints pane to hide a specified breakpoint.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   * @param object aOptions [optional]
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  _hideBreakpoint: function(aLocation, aOptions = {}) {
    let currentSourceUrl = DebuggerView.Sources.selectedValue;

    // Update the editor if required.
    if (!aOptions.noEditorUpdate) {
      if (aLocation.url == currentSourceUrl) {
        DebuggerView.editor.removeBreakpoint(aLocation.line - 1);
      }
    }

    // Update the breakpoints pane if required.
    if (!aOptions.noPaneUpdate) {
      DebuggerView.Sources.removeBreakpoint(aLocation);
    }
  },

  /**
   * Gets all Promises for the BreakpointActor client objects that are
   * either enabled (added to the server) or disabled (removed from the server,
   * but for which some details are preserved).
   */
  get _addedOrDisabled() {
    for (let [, value] of this._added) yield value;
    for (let [, value] of this._disabled) yield value;
  },

  /**
   * Get a Promise for the BreakpointActor client object which is already added
   * or currently being added at the given location.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   * @return object | null
   *         A promise that is resolved after the breakpoint is added, or
   *         null if no breakpoint was found.
   */
  _getAdded: function(aLocation) {
    return this._added.get(this.getIdentifier(aLocation));
  },

  /**
   * Get a Promise for the BreakpointActor client object which is currently
   * being removed from the given location.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   * @return object | null
   *         A promise that is resolved after the breakpoint is removed, or
   *         null if no breakpoint was found.
   */
  _getRemoving: function(aLocation) {
    return this._removing.get(this.getIdentifier(aLocation));
  },

  /**
   * Get an identifier string for a given location. Breakpoint promises are
   * identified in the store by a string representation of their location.
   *
   * @param object aLocation
   *        The location to serialize to a string.
   * @return string
   *         The identifier string.
   */
  getIdentifier: function(aLocation) {
    return aLocation.url + ":" + aLocation.line;
  }
};

/**
 * Localization convenience methods.
 */
let L10N = new ViewHelpers.L10N(DBG_STRINGS_URI);

/**
 * Shortcuts for accessing various debugger preferences.
 */
let Prefs = new ViewHelpers.Prefs("devtools", {
  chromeDebuggingHost: ["Char", "debugger.chrome-debugging-host"],
  chromeDebuggingPort: ["Int", "debugger.chrome-debugging-port"],
  sourcesWidth: ["Int", "debugger.ui.panes-sources-width"],
  instrumentsWidth: ["Int", "debugger.ui.panes-instruments-width"],
  panesVisibleOnStartup: ["Bool", "debugger.ui.panes-visible-on-startup"],
  variablesSortingEnabled: ["Bool", "debugger.ui.variables-sorting-enabled"],
  variablesOnlyEnumVisible: ["Bool", "debugger.ui.variables-only-enum-visible"],
  variablesSearchboxVisible: ["Bool", "debugger.ui.variables-searchbox-visible"],
  pauseOnExceptions: ["Bool", "debugger.pause-on-exceptions"],
  ignoreCaughtExceptions: ["Bool", "debugger.ignore-caught-exceptions"],
  sourceMapsEnabled: ["Bool", "debugger.source-maps-enabled"],
  prettyPrintEnabled: ["Bool", "debugger.pretty-print-enabled"],
  editorTabSize: ["Int", "editor.tabsize"]
});

/**
 * Returns true if this is a chrome debugger instance.
 * @return boolean
 */
XPCOMUtils.defineLazyGetter(window, "_isChromeDebugger", function() {
  // We're inside a single top level XUL window in a different process.
  return !(window.frameElement instanceof XULElement);
});

/**
 * Convenient way of emitting events from the panel window.
 */
EventEmitter.decorate(this);

/**
 * Preliminary setup for the DebuggerController object.
 */
DebuggerController.initialize();
DebuggerController.Parser = new Parser();
DebuggerController.ThreadState = new ThreadState();
DebuggerController.StackFrames = new StackFrames();
DebuggerController.SourceScripts = new SourceScripts();
DebuggerController.Breakpoints = new Breakpoints();
DebuggerController.Breakpoints.DOM = new EventListeners();

/**
 * Export some properties to the global scope for easier access.
 */
Object.defineProperties(window, {
  "gTarget": {
    get: function() DebuggerController._target
  },
  "gHostType": {
    get: function() DebuggerView._hostType
  },
  "gClient": {
    get: function() DebuggerController.client
  },
  "gThreadClient": {
    get: function() DebuggerController.activeThread
  },
  "gCallStackPageSize": {
    get: function() CALL_STACK_PAGE_SIZE
  }
});

/**
 * Helper method for debugging.
 * @param string
 */
function dumpn(str) {
  if (wantLogging) {
    dump("DBG-FRONTEND: " + str + "\n");
  }
}

let wantLogging = Services.prefs.getBoolPref("devtools.debugger.log");
