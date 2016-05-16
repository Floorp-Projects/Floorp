/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const DBG_STRINGS_URI = "chrome://devtools/locale/debugger.properties";
const NEW_SOURCE_IGNORED_URLS = ["debugger eval code", "XStringBundle"];
const NEW_SOURCE_DISPLAY_DELAY = 200; // ms
const FETCH_SOURCE_RESPONSE_DELAY = 200; // ms
const FRAME_STEP_CLEAR_DELAY = 100; // ms
const CALL_STACK_PAGE_SIZE = 25; // frames

// The panel's window global is an EventEmitter firing the following events:
const EVENTS = {
  // When the debugger's source editor instance finishes loading or unloading.
  EDITOR_LOADED: "Debugger:EditorLoaded",
  EDITOR_UNLOADED: "Debugger:EditorUnloaded",

  // When new sources are received from the debugger server.
  NEW_SOURCE: "Debugger:NewSource",
  SOURCES_ADDED: "Debugger:SourcesAdded",

  // When a source is shown in the source editor.
  SOURCE_SHOWN: "Debugger:EditorSourceShown",
  SOURCE_ERROR_SHOWN: "Debugger:EditorSourceErrorShown",

  // When the editor has shown a source and set the line / column position
  EDITOR_LOCATION_SET: "Debugger:EditorLocationSet",

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
  BREAKPOINT_CLICKED: "Debugger:BreakpointClicked",

  // When a breakpoint has been shown or hidden in the source editor
  // or the pane.
  BREAKPOINT_SHOWN_IN_EDITOR: "Debugger:BreakpointShownInEditor",
  BREAKPOINT_SHOWN_IN_PANE: "Debugger:BreakpointShownInPane",
  BREAKPOINT_HIDDEN_IN_EDITOR: "Debugger:BreakpointHiddenInEditor",
  BREAKPOINT_HIDDEN_IN_PANE: "Debugger:BreakpointHiddenInPane",

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

  // After the the StackFrames object has been filled with frames
  AFTER_FRAMES_REFILLED: "Debugger:AfterFramesRefilled",

  // After the stackframes are cleared and debugger won't pause anymore.
  AFTER_FRAMES_CLEARED: "Debugger:AfterFramesCleared",

  // When the options popup is showing or hiding.
  OPTIONS_POPUP_SHOWING: "Debugger:OptionsPopupShowing",
  OPTIONS_POPUP_HIDDEN: "Debugger:OptionsPopupHidden",

  // When the widgets layout has been changed.
  LAYOUT_CHANGED: "Debugger:LayoutChanged",

  // When a worker has been selected.
  WORKER_SELECTED: "Debugger::WorkerSelected"
};

// Descriptions for what a stack frame represents after the debugger pauses.
const FRAME_TYPE = {
  NORMAL: 0,
  CONDITIONAL_BREAKPOINT_EVAL: 1,
  WATCH_EXPRESSIONS_EVAL: 2,
  PUBLIC_CLIENT_EVAL: 3
};

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://devtools/shared/event-emitter.js");
Cu.import("resource://devtools/client/shared/widgets/SimpleListWidget.jsm");
Cu.import("resource://devtools/client/shared/widgets/BreadcrumbsWidget.jsm");
Cu.import("resource://devtools/client/shared/widgets/SideMenuWidget.jsm");
Cu.import("resource://devtools/client/shared/widgets/VariablesView.jsm");
Cu.import("resource://devtools/client/shared/widgets/VariablesViewController.jsm");

Cu.import("resource://devtools/client/shared/browser-loader.js");
const { require } = BrowserLoader({
  baseURI: "resource://devtools/client/debugger/",
  window,
});
XPCOMUtils.defineConstant(this, "require", require);
const { gDevTools } = require("devtools/client/framework/devtools");
const { ViewHelpers, Heritage, WidgetMethods, setNamedTimeout,
        clearNamedTimeout } = require("devtools/client/shared/widgets/view-helpers");

// React
const React = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

// Used to create the Redux store
const createStore = require("devtools/client/shared/redux/create-store")({
  getTargetClient: () => DebuggerController.client,
  log: false
});
const {
  makeStateBroadcaster,
  enhanceStoreWithBroadcaster,
  combineBroadcastingReducers
} = require("devtools/client/shared/redux/non-react-subscriber");
const { bindActionCreators } = require('devtools/client/shared/vendor/redux');
const reducers = require("./content/reducers/index");
const { onReducerEvents } = require("./content/utils");

const waitUntilService = require("devtools/client/shared/redux/middleware/wait-service");
var services = {
  WAIT_UNTIL: waitUntilService.NAME
};

var Services = require("Services");
var {TargetFactory} = require("devtools/client/framework/target");
var {Toolbox} = require("devtools/client/framework/toolbox");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var promise = require("devtools/shared/deprecated-sync-thenables");
var Editor = require("devtools/client/sourceeditor/editor");
var DebuggerEditor = require("devtools/client/sourceeditor/debugger");
var {Tooltip} = require("devtools/client/shared/widgets/Tooltip");
var FastListWidget = require("devtools/client/shared/widgets/FastListWidget");
var {LocalizationHelper} = require("devtools/client/shared/l10n");
var {PrefsHelper} = require("devtools/client/shared/prefs");

XPCOMUtils.defineConstant(this, "EVENTS", EVENTS);

XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Parser",
  "resource://devtools/shared/Parser.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ShortcutUtils",
  "resource://gre/modules/ShortcutUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "clipboardHelper",
  "@mozilla.org/widget/clipboardhelper;1", "nsIClipboardHelper");

Object.defineProperty(this, "NetworkHelper", {
  get: function() {
    return require("devtools/shared/webconsole/network-helper");
  },
  configurable: true,
  enumerable: true
});

/**
 * Localization convenience methods.
 */
var L10N = new LocalizationHelper(DBG_STRINGS_URI);

/**
 * Object defining the debugger controller components.
 */
var DebuggerController = {
  /**
   * Initializes the debugger controller.
   */
  initialize: function() {
    dumpn("Initializing the DebuggerController");

    this.startupDebugger = this.startupDebugger.bind(this);
    this.shutdownDebugger = this.shutdownDebugger.bind(this);
    this._onNavigate = this._onNavigate.bind(this);
    this._onWillNavigate = this._onWillNavigate.bind(this);
    this._onTabDetached = this._onTabDetached.bind(this);

    const broadcaster = makeStateBroadcaster(() => !!this.activeThread);
    const reducer = combineBroadcastingReducers(
      reducers,
      broadcaster.emitChange
    );
    // TODO: Bug 1228867, clean this up and probably abstract it out
    // better.
    //
    // We only want to process async event that are appropriate for
    // this page. The devtools are open across page reloads, so async
    // requests from the last page might bleed through if reloading
    // fast enough. We check to make sure the async action is part of
    // a current request, and ignore it if not.
    let store = createStore((state, action) => {
      if (action.seqId &&
         (action.status === 'done' || action.status === 'error') &&
         state && state.asyncRequests.indexOf(action.seqId) === -1) {
        return state;
      }
      return reducer(state, action);
    });
    store = enhanceStoreWithBroadcaster(store, broadcaster);

    // This controller right now acts as the store that's globally
    // available, so just copy the Redux API onto it.
    Object.keys(store).forEach(name => {
      this[name] = store[name];
    });
  },

  /**
   * Initializes the view.
   *
   * @return object
   *         A promise that is resolved when the debugger finishes startup.
   */
  startupDebugger: Task.async(function*() {
    if (this._startup) {
      return;
    }

    yield DebuggerView.initialize();
    this._startup = true;
  }),

  /**
   * Destroys the view and disconnects the debugger client from the server.
   *
   * @return object
   *         A promise that is resolved when the debugger finishes shutdown.
   */
  shutdownDebugger: Task.async(function*() {
    if (this._shutdown) {
      return;
    }

    DebuggerView.destroy();
    this.StackFrames.disconnect();
    this.ThreadState.disconnect();
    if (this._target.isTabActor) {
      this.Workers.disconnect();
    }

    this.disconnect();

    this._shutdown = true;
  }),

  /**
   * Initiates remote debugging based on the current target, wiring event
   * handlers as necessary.
   *
   * @return object
   *         A promise that is resolved when the debugger finishes connecting.
   */
  connect: Task.async(function*() {
    let target = this._target;

    let { client } = target;
    target.on("close", this._onTabDetached);
    target.on("navigate", this._onNavigate);
    target.on("will-navigate", this._onWillNavigate);
    this.client = client;
    this.activeThread = this._toolbox.threadClient;

    // Disable asm.js so that we can set breakpoints and other things
    // on asm.js scripts
    yield this.reconfigureThread({ observeAsmJS: true });
    yield this.connectThread();

    // We need to call this to sync the state of the resume
    // button in the toolbar with the state of the thread.
    this.ThreadState._update();

    this._hideUnsupportedFeatures();
  }),

  connectThread: function() {
    const { newSource, fetchEventListeners } = bindActionCreators(actions, this.dispatch);

    // TODO: bug 806775, update the globals list using aPacket.hostAnnotations
    // from bug 801084.
    // this.client.addListener("newGlobal", ...);

    this.activeThread.addListener("newSource", (event, packet) => {
      newSource(packet.source);

      // Make sure the events listeners are up to date.
      if (DebuggerView.instrumentsPaneTab == "events-tab") {
        fetchEventListeners();
      }
    });

    if (this._target.isTabActor) {
      this.Workers.connect();
    }
    this.ThreadState.connect();
    this.StackFrames.connect();

    // Load all of the sources. Note that the server will actually
    // emit individual `newSource` notifications, which trigger
    // separate actions, so this won't do anything other than force
    // the server to traverse sources.
    this.dispatch(actions.loadSources()).then(() => {
      // If the engine is already paused, update the UI to represent the
      // paused state
      if (this.activeThread) {
        const pausedPacket = this.activeThread.getLastPausePacket();
        DebuggerView.Toolbar.toggleResumeButtonState(
          this.activeThread.state,
          !!pausedPacket
        );
        if (pausedPacket) {
          this.StackFrames._onPaused("paused", pausedPacket);
        }
      }
    });
  },

  /**
   * Disconnects the debugger client and removes event handlers as necessary.
   */
  disconnect: function() {
    // Return early if the client didn't even have a chance to instantiate.
    if (!this.client) {
      return;
    }

    this.client.removeListener("newGlobal");
    this.activeThread.removeListener("newSource");
    this.activeThread.removeListener("blackboxchange");

    this._connected = false;
    this.client = null;
    this.activeThread = null;
  },

  _hideUnsupportedFeatures: function() {
    if (this.client.mainRoot.traits.noPrettyPrinting) {
      DebuggerView.Sources.hidePrettyPrinting();
    }

    if (this.client.mainRoot.traits.noBlackBoxing) {
      DebuggerView.Sources.hideBlackBoxing();
    }
  },

  _onWillNavigate: function(opts={}) {
    // Reset UI.
    DebuggerView.handleTabNavigation();
    if (!opts.noUnload) {
      this.dispatch(actions.unload());
    }

    // Discard all the cached parsed sources *before* the target
    // starts navigating. Sources may be fetched during navigation, in
    // which case we don't want to hang on to the old source contents.
    DebuggerController.Parser.clearCache();
    SourceUtils.clearCache();

    // Prevent performing any actions that were scheduled before
    // navigation.
    clearNamedTimeout("new-source");
    clearNamedTimeout("event-breakpoints-update");
    clearNamedTimeout("event-listeners-fetch");
  },

  _onNavigate: function() {
    this.ThreadState.handleTabNavigation();
    this.StackFrames.handleTabNavigation();
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
   * Detach and reattach to the thread actor with useSourceMaps true, blow
   * away old sources and get them again.
   */
  reconfigureThread: function(opts) {
    const deferred = promise.defer();
    this.activeThread.reconfigure(
      opts,
      aResponse => {
        if (aResponse.error) {
          deferred.reject(aResponse.error);
          return;
        }

        if (('useSourceMaps' in opts) || ('autoBlackBox' in opts)) {
          // Reset the view and fetch all the sources again.
          DebuggerView.handleTabNavigation();
          this.dispatch(actions.unload());
          this.dispatch(actions.loadSources());

          // Update the stack frame list.
          if (this.activeThread.paused) {
            this.activeThread._clearFrames();
            this.activeThread.fillFrames(CALL_STACK_PAGE_SIZE);
          }
        }

        deferred.resolve();
      }
    );
    return deferred.promise;
  },

  waitForSourcesLoaded: function() {
    const deferred = promise.defer();
    this.dispatch({
      type: services.WAIT_UNTIL,
      predicate: action => (action.type === constants.LOAD_SOURCES &&
                            action.status === "done"),
      run: deferred.resolve
    });
    return deferred.promise;
  },

  waitForSourceShown: function(name) {
    const deferred = promise.defer();
    window.on(EVENTS.SOURCE_SHOWN, function onShown(_, source) {
      if (source.url.includes(name)) {
        window.off(EVENTS.SOURCE_SHOWN, onShown);
        deferred.resolve();
      }
    });
    return deferred.promise;
  },

  _startup: false,
  _shutdown: false,
  _connected: false,
  client: null,
  activeThread: null
};

function Workers() {
  this._workerForms = Object.create(null);
  this._onWorkerListChanged = this._onWorkerListChanged.bind(this);
  this._onWorkerSelect = this._onWorkerSelect.bind(this);
}

Workers.prototype = {
  get _tabClient() {
    return DebuggerController._target.activeTab;
  },

  connect: function () {
    if (!Prefs.workersEnabled) {
      return;
    }

    this._updateWorkerList();
    this._tabClient.addListener("workerListChanged", this._onWorkerListChanged);
  },

  disconnect: function () {
    this._tabClient.removeListener("workerListChanged", this._onWorkerListChanged);
  },

  _updateWorkerList: function () {
    if (!this._tabClient.listWorkers) {
      return;
    }

    this._tabClient.listWorkers((response) => {
      let workerForms = Object.create(null);
      for (let worker of response.workers) {
        workerForms[worker.actor] = worker;
      }

      for (let workerActor in this._workerForms) {
        if (!(workerActor in workerForms)) {
          DebuggerView.Workers.removeWorker(this._workerForms[workerActor]);
          delete this._workerForms[workerActor];
        }
      }

      for (let workerActor in workerForms) {
        if (!(workerActor in this._workerForms)) {
          let workerForm = workerForms[workerActor];
          this._workerForms[workerActor] = workerForm;
          DebuggerView.Workers.addWorker(workerForm);
        }
      }
    });
  },

  _onWorkerListChanged: function () {
    this._updateWorkerList();
  },

  _onWorkerSelect: function (workerForm) {
    DebuggerController.client.attachWorker(workerForm.actor, (response, workerClient) => {
      let toolbox = gDevTools.showToolbox(TargetFactory.forWorker(workerClient),
                                          "jsdebugger", Toolbox.HostType.WINDOW);
      window.emit(EVENTS.WORKER_SELECTED, toolbox);
    });
  }
};

/**
 * ThreadState keeps the UI up to date with the state of the
 * thread (paused/attached/etc.).
 */
function ThreadState() {
  this._update = this._update.bind(this);
  this.interruptedByResumeButton = false;
}

ThreadState.prototype = {
  get activeThread() {
    return DebuggerController.activeThread;
  },

  /**
   * Connect to the current thread client.
   */
  connect: function() {
    dumpn("ThreadState is connecting...");
    this.activeThread.addListener("paused", this._update);
    this.activeThread.addListener("resumed", this._update);
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
  _update: function(aEvent, aPacket) {
    if (aEvent == "paused") {
      if (aPacket.why.type == "interrupted" &&
          this.interruptedByResumeButton) {
        // Interrupt requests suppressed by default, but if this is an
        // explicit interrupt by the pause button we want to emit it.
        gTarget.emit("thread-paused", aPacket);
      } else if (aPacket.why.type == "breakpointConditionThrown" && aPacket.why.message) {
        let where = aPacket.frame.where;
        let aLocation = {
          line: where.line,
          column: where.column,
          actor: where.source ? where.source.actor : null
        };
        DebuggerView.Sources.showBreakpointConditionThrownMessage(
          aLocation,
          aPacket.why.message
        );
      }
    }

    this.interruptedByResumeButton = false;
    DebuggerView.Toolbar.toggleResumeButtonState(
      this.activeThread.state,
      aPacket ? aPacket.frame : false
    );
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
  get activeThread() {
    return DebuggerController.activeThread;
  },

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
      case "breakpointConditionThrown":
        this._currentBreakpointLocation = aPacket.frame.where;
        this._conditionThrowMessage = aPacket.why.message;
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
      // If paused by an explicit interrupt, which are generated by the slow
      // script dialog and internal events such as setting breakpoints, ignore
      // the event to avoid UI flicker.
      case "interrupted":
        if (!aPacket.why.onNext) {
          return;
        }
        break;
    }

    this.activeThread.fillFrames(CALL_STACK_PAGE_SIZE);
    // Focus the editor, but don't steal focus from the split console.
    if (!DebuggerController._toolbox.isSplitConsoleFocused()) {
      DebuggerView.editor.focus();
    }
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
  _onFrames: Task.async(function*() {
    // Ignore useless notifications.
    if (!this.activeThread || !this.activeThread.cachedFrames.length) {
      return;
    }
    if (this._currentFrameDescription != FRAME_TYPE.NORMAL &&
        this._currentFrameDescription != FRAME_TYPE.PUBLIC_CLIENT_EVAL) {
      return;
    }

    // TODO: remove all of this deprecated code: Bug 990137.
    yield this._handleConditionalBreakpoint();

    // TODO: handle all of this server-side: Bug 832470, comment 14.
    yield this._handleWatchExpressions();

    // Make sure the debugger view panes are visible, then refill the frames.
    DebuggerView.showInstrumentsPane();
    this._refillFrames();

    // No additional processing is necessary for this stack frame.
    if (this._currentFrameDescription != FRAME_TYPE.NORMAL) {
      this._currentFrameDescription = FRAME_TYPE.NORMAL;
    }
  }),

  /**
   * Fill the StackFrames view with the frames we have in the cache, compressing
   * frames which have black boxed sources into single frames.
   */
  _refillFrames: function() {
    // Make sure all the previous stackframes are removed before re-adding them.
    DebuggerView.StackFrames.empty();

    for (let frame of this.activeThread.cachedFrames) {
      let { depth, source, where: { line, column } } = frame;

      let isBlackBoxed = source ? this.activeThread.source(source).isBlackBoxed : false;
      DebuggerView.StackFrames.addFrame(frame, line, column, depth, isBlackBoxed);
    }

    DebuggerView.StackFrames.selectedDepth = Math.max(this.currentFrameDepth, 0);
    DebuggerView.StackFrames.dirty = this.activeThread.moreFrames;

    DebuggerView.StackFrames.addCopyContextMenu();

    window.emit(EVENTS.AFTER_FRAMES_REFILLED);
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
    if (this.activeThread.state != "paused") {
      return;
    }
    // Makes sure the selected source remains selected
    // after the fillFrames is called.
    const source = DebuggerView.Sources.selectedValue;

    this.activeThread.fillFrames(CALL_STACK_PAGE_SIZE, () => {
      DebuggerView.Sources.selectedValue = source;
    });
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
    let { environment, where, source } = frame;
    if (!environment) {
      return;
    }

    // Don't change the editor's location if the execution was paused by a
    // public client evaluation. This is useful for adding overlays on
    // top of the editor, like a variable inspection popup.
    let isClientEval = this._currentFrameDescription == FRAME_TYPE.PUBLIC_CLIENT_EVAL;
    let isPopupShown = DebuggerView.VariableBubble.contentsShown();
    if (!isClientEval && !isPopupShown) {
      // Move the editor's caret to the proper url and line.
      DebuggerView.setEditorLocation(source.actor, where.line);
    } else {
      // Highlight the line where the execution is paused in the editor.
      DebuggerView.setEditorLocation(source.actor, where.line, { noCaret: true });
    }

    // Highlight the breakpoint at the line and column if it exists.
    DebuggerView.Sources.highlightBreakpointAtCursor();

    // Don't display the watch expressions textbox inputs in the pane.
    DebuggerView.WatchExpressions.toggleContents(false);

    // Start recording any added variables or properties in any scope and
    // clear existing scopes to create each one dynamically.
    DebuggerView.Variables.empty();

    // If watch expressions evaluation results are available, create a scope
    // to contain all the values.
    if (this._syncedWatchExpressions && aDepth == 0) {
      let label = L10N.getStr("watchExpressionsScopeLabel");
      let scope = DebuggerView.Variables.addScope(label);

      // Customize the scope for holding watch expressions evaluations.
      scope.descriptorTooltip = false;
      scope.contextMenuId = "debuggerWatchExpressionsContextMenu";
      scope.separatorStr = L10N.getStr("watchExpressionsSeparatorLabel2");
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
      // be inspected. The previously expanded scopes are also reexpanded here.
      if (innermost || DebuggerView.Variables.wasExpanded(scope)) {
        scope.expand();
      }
    } while ((environment = environment.parent));

    // Signal that scope environments have been shown.
    window.emit(EVENTS.FETCHED_SCOPES);
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
        deferred.resolve(frameFinished);
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
      let excRef = aScope.addItem("<exception>", { value: this._currentException },
                                  { internalItem: true });
      DebuggerView.Variables.controller.addExpander(excRef, this._currentException);
    }
    // Add any returned value.
    if (this._currentReturnedValue) {
      let retRef = aScope.addItem("<return>",
                                  { value: this._currentReturnedValue },
                                  { internalItem: true });
      DebuggerView.Variables.controller.addExpander(retRef, this._currentReturnedValue);
    }
    // Add "this".
    if (aFrame.this) {
      let thisRef = aScope.addItem("this", { value: aFrame.this });
      DebuggerView.Variables.controller.addExpander(thisRef, aFrame.this);
    }
  },

  /**
   * Handles conditional breakpoints when the debugger pauses and the
   * stackframes are received.
   *
   * We moved conditional breakpoint handling to the server, but
   * need to support it in the client for a while until most of the
   * server code in production is updated with it.
   * TODO: remove all of this deprecated code: Bug 990137.
   *
   * @return object
   *         A promise that is resolved after a potential breakpoint's
   *         conditional expression is evaluated. If there's no breakpoint
   *         where the debugger is paused, the promise is resolved immediately.
   */
  _handleConditionalBreakpoint: Task.async(function*() {
    if (gClient.mainRoot.traits.conditionalBreakpoints) {
      return;
    }
    let breakLocation = this._currentBreakpointLocation;
    if (!breakLocation) {
      return;
    }

    let bp = queries.getBreakpoint(DebuggerController.getState(), {
      actor: breakLocation.source.actor,
      line: breakLocation.line
    });
    let conditionalExpression = bp.condition;
    if (!conditionalExpression) {
      return;
    }

    // Evaluating the current breakpoint's conditional expression will
    // cause the stack frames to be cleared and active thread to pause,
    // sending a 'clientEvaluated' packed and adding the frames again.
    let evaluationOptions = { depth: 0, meta: FRAME_TYPE.CONDITIONAL_BREAKPOINT_EVAL };
    yield this.evaluate(conditionalExpression, evaluationOptions);
    this._currentFrameDescription = FRAME_TYPE.NORMAL;

    // If the breakpoint's conditional expression evaluation is falsy
    // and there is no exception, automatically resume execution.
    if (!this._currentEvaluation.throw &&
        VariablesView.isFalsy({ value: this._currentEvaluation.return })) {
      this.activeThread.resume(DebuggerController._ensureResumptionOrder);
    }
  }),

  /**
   * Handles watch expressions when the debugger pauses and the stackframes
   * are received.
   *
   * @return object
   *         A promise that is resolved after the potential watch expressions
   *         are evaluated. If there are no watch expressions where the debugger
   *         is paused, the promise is resolved immediately.
   */
  _handleWatchExpressions: Task.async(function*() {
    // Ignore useless notifications.
    if (!this.activeThread || !this.activeThread.cachedFrames.length) {
      return;
    }

    let watchExpressions = this._currentWatchExpressions;
    if (!watchExpressions) {
      return;
    }

    // Evaluation causes the stack frames to be cleared and active thread to
    // pause, sending a 'clientEvaluated' packet and adding the frames again.
    let evaluationOptions = { depth: 0, meta: FRAME_TYPE.WATCH_EXPRESSIONS_EVAL };
    yield this.evaluate(watchExpressions, evaluationOptions);
    this._currentFrameDescription = FRAME_TYPE.NORMAL;

    // If an error was thrown during the evaluation of the watch expressions
    // or the evaluation was terminated from the slow script dialog, then at
    // least one expression evaluation could not be performed. So remove the
    // most recent watch expression and try again.
    if (this._currentEvaluation.throw || this._currentEvaluation.terminated) {
      DebuggerView.WatchExpressions.removeAt(0);
      yield DebuggerController.StackFrames.syncWatchExpressions();
    }
  }),

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

      // Signal that watch expressions have been fetched.
      window.emit(EVENTS.FETCHED_WATCH_EXPRESSIONS);
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

    if (!sanitizedExpressions.length) {
      this._currentWatchExpressions = null;
      this._syncedWatchExpressions = null;
    } else {
      this._syncedWatchExpressions =
      this._currentWatchExpressions = "[" +
        sanitizedExpressions.map(aString =>
          "eval(\"" +
            "try {" +
              // Make sure all quotes are escaped in the expression's syntax,
              // and add a newline after the statement to avoid comments
              // breaking the code integrity inside the eval block.
              aString.replace(/\\/g, "\\\\").replace(/"/g, "\\$&") + "\" + " + "'\\n'" + " + \"" +
            "} catch (e) {" +
              "e.name + ': ' + e.message;" + // TODO: Bug 812765, 812764.
            "}" +
          "\")"
        ).join(",") +
      "]";
    }

    this.currentFrameDepth = -1;
    return this._onFrames();
  }
};

/**
 * Shortcuts for accessing various debugger preferences.
 */
var Prefs = new PrefsHelper("devtools", {
  workersAndSourcesWidth: ["Int", "debugger.ui.panes-workers-and-sources-width"],
  instrumentsWidth: ["Int", "debugger.ui.panes-instruments-width"],
  panesVisibleOnStartup: ["Bool", "debugger.ui.panes-visible-on-startup"],
  variablesSortingEnabled: ["Bool", "debugger.ui.variables-sorting-enabled"],
  variablesOnlyEnumVisible: ["Bool", "debugger.ui.variables-only-enum-visible"],
  variablesSearchboxVisible: ["Bool", "debugger.ui.variables-searchbox-visible"],
  pauseOnExceptions: ["Bool", "debugger.pause-on-exceptions"],
  ignoreCaughtExceptions: ["Bool", "debugger.ignore-caught-exceptions"],
  sourceMapsEnabled: ["Bool", "debugger.source-maps-enabled"],
  prettyPrintEnabled: ["Bool", "debugger.pretty-print-enabled"],
  autoPrettyPrint: ["Bool", "debugger.auto-pretty-print"],
  workersEnabled: ["Bool", "debugger.workers"],
  editorTabSize: ["Int", "editor.tabsize"],
  autoBlackBox: ["Bool", "debugger.auto-black-box"],
  promiseDebuggerEnabled: ["Bool", "debugger.promise"]
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
DebuggerController.Workers = new Workers();
DebuggerController.ThreadState = new ThreadState();
DebuggerController.StackFrames = new StackFrames();

/**
 * Export some properties to the global scope for easier access.
 */
Object.defineProperties(window, {
  "gTarget": {
    get: function() {
      return DebuggerController._target;
    },
    configurable: true
  },
  "gHostType": {
    get: function() {
      return DebuggerView._hostType;
    },
    configurable: true
  },
  "gClient": {
    get: function() {
      return DebuggerController.client;
    },
    configurable: true
  },
  "gThreadClient": {
    get: function() {
      return DebuggerController.activeThread;
    },
    configurable: true
  },
  "gCallStackPageSize": {
    get: function() {
      return CALL_STACK_PAGE_SIZE;
    },
    configurable: true
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

var wantLogging = Services.prefs.getBoolPref("devtools.debugger.log");
