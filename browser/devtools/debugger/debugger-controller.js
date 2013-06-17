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
const FETCH_SOURCE_RESPONSE_DELAY = 50; // ms
const FRAME_STEP_CLEAR_DELAY = 100; // ms
const CALL_STACK_PAGE_SIZE = 25; // frames

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/devtools/dbg-server.jsm");
Cu.import("resource://gre/modules/devtools/dbg-client.jsm");
Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js");
Cu.import("resource:///modules/source-editor.jsm");
Cu.import("resource:///modules/devtools/LayoutHelpers.jsm");
Cu.import("resource:///modules/devtools/BreadcrumbsWidget.jsm");
Cu.import("resource:///modules/devtools/SideMenuWidget.jsm");
Cu.import("resource:///modules/devtools/VariablesView.jsm");
Cu.import("resource:///modules/devtools/VariablesViewController.jsm");
Cu.import("resource:///modules/devtools/ViewHelpers.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Parser",
  "resource:///modules/devtools/Parser.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetworkHelper",
  "resource://gre/modules/devtools/NetworkHelper.jsm");

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
    if (this._isInitialized) {
      return this._startup.promise;
    }
    this._isInitialized = true;
    window.removeEventListener("DOMContentLoaded", this.startupDebugger, true);

    let deferred = this._startup = Promise.defer();

    DebuggerView.initialize(() => {
      DebuggerView._isInitialized = true;

      VariablesViewController.attach(DebuggerView.Variables, {
        getGripClient: aObject => {
          return this.activeThread.pauseGrip(aObject);
        }
      });

      // Relay events from the VariablesView.
      DebuggerView.Variables.on("fetched", (aEvent, aType) => {
        switch (aType) {
          case "variables":
            window.dispatchEvent(document, "Debugger:FetchedVariables");
            break;
          case "properties":
            window.dispatchEvent(document, "Debugger:FetchedProperties");
            break;
        }
      });

      // Chrome debugging needs to initiate the connection by itself.
      if (window._isChromeDebugger) {
        this.connect().then(deferred.resolve);
      } else {
        deferred.resolve();
      }
    });

    return deferred.promise;
  },

  /**
   * Destroys the view and disconnects the debugger client from the server.
   *
   * @return object
   *         A promise that is resolved when the debugger finishes shutdown.
   */
  shutdownDebugger: function() {
    if (this._isDestroyed) {
      return this._shutdown.promise;
    }
    this._isDestroyed = true;
    this._startup = null;
    window.removeEventListener("unload", this.shutdownDebugger, true);

    let deferred = this._shutdown = Promise.defer();

    DebuggerView.destroy(() => {
      DebuggerView._isDestroyed = true;
      this.SourceScripts.disconnect();
      this.StackFrames.disconnect();
      this.ThreadState.disconnect();

      this.disconnect();
      deferred.resolve();

      // Chrome debugging needs to close its parent process on shutdown.
      window._isChromeDebugger && this._quitApp();
    });

    return deferred.promise;
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
      return this._connection.promise;
    }

    let deferred = this._connection = Promise.defer();

    if (!window._isChromeDebugger) {
      let target = this._target;
      let { client, form, threadActor } = target;
      target.on("close", this._onTabDetached);
      target.on("navigate", this._onTabNavigated);
      target.on("will-navigate", this._onTabNavigated);

      if (target.chrome) {
        this._startChromeDebugging(client, form.chromeDebugger, deferred.resolve);
      } else {
        this._startDebuggingTab(client, threadActor, deferred.resolve);
      }

      return deferred.promise;
    }

    // Chrome debugging needs to make the connection to the debuggee.
    let transport = debuggerSocketConnect(Prefs.chromeDebuggingHost,
                                          Prefs.chromeDebuggingPort);

    let client = new DebuggerClient(transport);
    client.addListener("tabNavigated", this._onTabNavigated);
    client.addListener("tabDetached", this._onTabDetached);

    client.connect((aType, aTraits) => {
      client.listTabs((aResponse) => {
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
    // the RemoteTarget.
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
    if (aType == "will-navigate") {
      DebuggerView._handleTabNavigation();

      // Discard all the old sources.
      DebuggerController.SourceScripts.clearCache();
      DebuggerController.Parser.clearCache();
      SourceUtils.clearCache();
      return;
    }

    this.ThreadState._handleTabNavigation();
    this.StackFrames._handleTabNavigation();
    this.SourceScripts._handleTabNavigation();
  },

  /**
   * Called when the debugged tab is closed.
   */
  _onTabDetached: function() {
    this.shutdownDebugger();
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
   * Warn if resuming execution produced a wrongOrder error.
   */
  _ensureResumptionOrder: function(aResponse) {
    if (aResponse.error == "wrongOrder") {
      DebuggerView.Toolbar.showResumeWarning(aResponse.lastPausedUrl);
    }
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
   * away old scripts and get sources again.
   */
  reconfigureThread: function(aUseSourceMaps) {
    this.client.reconfigureThread(aUseSourceMaps, (aResponse) => {
      if (aResponse.error) {
        let msg = "Couldn't reconfigure thread: " + aResponse.message;
        Cu.reportError(msg);
        dumpn(msg);
        return;
      }

      // Update the source list widget.
      DebuggerView.Sources.empty();
      SourceUtils.clearCache();
      this.SourceScripts._handleTabNavigation();
      // Update the stack frame list.
      this.activeThread._clearFrames();
      this.activeThread.fillFrames(CALL_STACK_PAGE_SIZE);
    });
  },

  /**
   * Attempts to quit the current process if allowed.
   */
  _quitApp: function() {
    let canceled = Cc["@mozilla.org/supports-PRBool;1"]
      .createInstance(Ci.nsISupportsPRBool);

    Services.obs.notifyObservers(canceled, "quit-application-requested", null);

    // Somebody canceled our quit request.
    if (canceled.data) {
      return;
    }
    Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit);
  },

  _isInitialized: false,
  _isDestroyed: false,
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
    this.activeThread.pauseOnExceptions(Prefs.pauseOnExceptions);
    this._handleTabNavigation();
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
  _handleTabNavigation: function() {
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

    if (DebuggerController._target && (aEvent == "paused" || aEvent == "resumed")) {
      DebuggerController._target.emit("thread-" + aEvent);
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
  this._afterFramesCleared = this._afterFramesCleared.bind(this);
  this.evaluate = this.evaluate.bind(this);
}

StackFrames.prototype = {
  get activeThread() DebuggerController.activeThread,
  autoScopeExpand: false,
  currentFrame: null,
  syncedWatchExpressions: null,
  currentWatchExpressions: null,
  currentBreakpointLocation: null,
  currentEvaluation: null,
  currentException: null,
  currentReturnedValue: null,

  /**
   * Connect to the current thread client.
   */
  connect: function() {
    dumpn("StackFrames is connecting...");
    this.activeThread.addListener("paused", this._onPaused);
    this.activeThread.addListener("resumed", this._onResumed);
    this.activeThread.addListener("framesadded", this._onFrames);
    this.activeThread.addListener("framescleared", this._onFramesCleared);
    this._handleTabNavigation();
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
  },

  /**
   * Handles any initialization on a tab navigation event issued by the client.
   */
  _handleTabNavigation: function() {
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
        this.currentBreakpointLocation = aPacket.frame.where;
        break;
      // If paused by a client evaluation, store the evaluated value.
      case "clientEvaluated":
        this.currentEvaluation = aPacket.why.frameFinished;
        break;
      // If paused by an exception, store the exception value.
      case "exception":
        this.currentException = aPacket.why.exception;
        break;
      // If paused while stepping out of a frame, store the returned value or
      // thrown exception.
      case "resumeLimit":
        if (!aPacket.why.frameFinished) {
          break;
        } else if (aPacket.why.frameFinished.throw) {
          this.currentException = aPacket.why.frameFinished.throw;
        } else if (aPacket.why.frameFinished.return) {
          this.currentReturnedValue = aPacket.why.frameFinished.return;
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
    DebuggerView.editor.setDebugLocation(-1);

    // Prepare the watch expression evaluation string for the next pause.
    if (!this._isWatchExpressionsEvaluation) {
      this.currentWatchExpressions = this.syncedWatchExpressions;
    }
  },

  /**
   * Handler for the thread client's framesadded notification.
   */
  _onFrames: function() {
    // Ignore useless notifications.
    if (!this.activeThread.cachedFrames.length) {
      return;
    }

    // Conditional breakpoints are { breakpoint, expression } tuples. The
    // boolean evaluation of the expression decides if the active thread
    // automatically resumes execution or not.
    if (this.currentBreakpointLocation) {
      let { url, line } = this.currentBreakpointLocation;
      let breakpointClient = DebuggerController.Breakpoints.getBreakpoint(url, line);
      if (breakpointClient) {
        // Make sure a breakpoint actually exists at the specified url and line.
        let conditionalExpression = breakpointClient.conditionalExpression;
        if (conditionalExpression) {
          // Evaluating the current breakpoint's conditional expression will
          // cause the stack frames to be cleared and active thread to pause,
          // sending a 'clientEvaluated' packed and adding the frames again.
          this.evaluate(conditionalExpression, 0);
          this._isConditionalBreakpointEvaluation = true;
          return;
        }
      }
    }
    // Got our evaluation of the current breakpoint's conditional expression.
    if (this._isConditionalBreakpointEvaluation) {
      this._isConditionalBreakpointEvaluation = false;
      // If the breakpoint's conditional expression evaluation is falsy,
      // automatically resume execution.
      if (VariablesView.isFalsy({ value: this.currentEvaluation.return })) {
        this.activeThread.resume(DebuggerController._ensureResumptionOrder);
        return;
      }
    }


    // Watch expressions are evaluated in the context of the topmost frame,
    // and the results are displayed in the variables view.
    if (this.currentWatchExpressions) {
      // Evaluation causes the stack frames to be cleared and active thread to
      // pause, sending a 'clientEvaluated' packed and adding the frames again.
      this.evaluate(this.currentWatchExpressions, 0);
      this._isWatchExpressionsEvaluation = true;
      return;
    }
    // Got our evaluation of the current watch expressions.
    if (this._isWatchExpressionsEvaluation) {
      this._isWatchExpressionsEvaluation = false;
      // If an error was thrown during the evaluation of the watch expressions,
      // then at least one expression evaluation could not be performed.
      if (this.currentEvaluation.throw) {
        DebuggerView.WatchExpressions.removeExpressionAt(0);
        DebuggerController.StackFrames.syncWatchExpressions();
        return;
      }
      // If the watch expressions were evaluated successfully, attach
      // the results to the topmost frame.
      let topmostFrame = this.activeThread.cachedFrames[0];
      topmostFrame.watchExpressionsEvaluation = this.currentEvaluation.return;
    }


    // Make sure the debugger view panes are visible.
    DebuggerView.showInstrumentsPane();

    // Make sure all the previous stackframes are removed before re-adding them.
    DebuggerView.StackFrames.empty();

    for (let frame of this.activeThread.cachedFrames) {
      let depth = frame.depth;
      let { url, line } = frame.where;
      let frameLocation = NetworkHelper.convertToUnicode(unescape(url));
      let frameTitle = StackFrameUtils.getFrameTitle(frame);

      DebuggerView.StackFrames.addFrame(frameTitle, frameLocation, line, depth);
    }
    if (this.currentFrame == null) {
      DebuggerView.StackFrames.selectedDepth = 0;
    }
    if (this.activeThread.moreFrames) {
      DebuggerView.StackFrames.dirty = true;
    }
  },

  /**
   * Handler for the thread client's framescleared notification.
   */
  _onFramesCleared: function() {
    this.currentFrame = null;
    this.currentWatchExpressions = null;
    this.currentBreakpointLocation = null;
    this.currentEvaluation = null;
    this.currentException = null;
    this.currentReturnedValue = null;
    // After each frame step (in, over, out), framescleared is fired, which
    // forces the UI to be emptied and rebuilt on framesadded. Most of the times
    // this is not necessary, and will result in a brief redraw flicker.
    // To avoid it, invalidate the UI only after a short time if necessary.
    window.setTimeout(this._afterFramesCleared, FRAME_STEP_CLEAR_DELAY);
  },

  /**
   * Called soon after the thread client's framescleared notification.
   */
  _afterFramesCleared: function() {
    // Ignore useless notifications.
    if (this.activeThread.cachedFrames.length) {
      return;
    }
    DebuggerView.StackFrames.empty();
    DebuggerView.Sources.unhighlightBreakpoint();
    DebuggerView.WatchExpressions.toggleContents(true);
    DebuggerView.Variables.empty(0);
    window.dispatchEvent(document, "Debugger:AfterFramesCleared");
  },

  /**
   * Marks the stack frame at the specified depth as selected and updates the
   * properties view with the stack frame's data.
   *
   * @param number aDepth
   *        The depth of the frame in the stack.
   */
  selectFrame: function(aDepth) {
    let frame = this.activeThread.cachedFrames[this.currentFrame = aDepth];
    if (!frame) {
      return;
    }
    let { environment, watchExpressionsEvaluation } = frame;
    let { url, line } = frame.where;

    // Check if the frame does not represent the evaluation of debuggee code.
    if (!environment) {
      return;
    }

    // Move the editor's caret to the proper url and line.
    DebuggerView.updateEditor(url, line);
    // Highlight the breakpoint at the specified url and line if it exists.
    DebuggerView.Sources.highlightBreakpoint(url, line);
    // Don't display the watch expressions textbox inputs in the pane.
    DebuggerView.WatchExpressions.toggleContents(false);
    // Start recording any added variables or properties in any scope.
    DebuggerView.Variables.createHierarchy();
    // Clear existing scopes and create each one dynamically.
    DebuggerView.Variables.empty();


    // If watch expressions evaluation results are available, create a scope
    // to contain all the values.
    if (this.syncedWatchExpressions && watchExpressionsEvaluation) {
      let label = L10N.getStr("watchExpressionsScopeLabel");
      let scope = DebuggerView.Variables.addScope(label);

      // Customize the scope for holding watch expressions evaluations.
      scope.descriptorTooltip = false;
      scope.contextMenuId = "debuggerWatchExpressionsContextMenu";
      scope.separatorStr = L10N.getStr("watchExpressionsSeparatorLabel");
      scope.switch = DebuggerView.WatchExpressions.switchExpression;
      scope.delete = DebuggerView.WatchExpressions.deleteExpression;

      // The evaluation hasn't thrown, so display the returned results and
      // always expand the watch expressions scope by default.
      this._fetchWatchExpressions(scope, watchExpressionsEvaluation);
      scope.expand();
    }

    do {
      // Create a scope to contain all the inspected variables.
      let label = StackFrameUtils.getScopeLabel(environment);
      let scope = DebuggerView.Variables.addScope(label);
      let innermost = environment == frame.environment;

      // Handle special additions to the innermost scope.
      if (innermost) {
        this._insertScopeFrameReferences(scope, frame);
      }

      DebuggerView.Variables.controller.addExpander(scope, environment);

      // The innermost scope is always automatically expanded, because it
      // contains the variables in the current stack frame which are likely to
      // be inspected.
      if (innermost || this.autoScopeExpand) {
        scope.expand();
      }
    } while ((environment = environment.parent));

    // Signal that variables have been fetched.
    window.dispatchEvent(document, "Debugger:FetchedVariables");
    DebuggerView.Variables.commitHierarchy();
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
    this.activeThread.pauseGrip(aExp).getPrototypeAndProperties((aResponse) => {
      let ownProperties = aResponse.ownProperties;
      let totalExpressions = DebuggerView.WatchExpressions.itemCount;

      for (let i = 0; i < totalExpressions; i++) {
        let name = DebuggerView.WatchExpressions.getExpression(i);
        let expVal = ownProperties[i].value;
        let expRef = aScope.addItem(name, ownProperties[i]);
        DebuggerView.Variables.controller.addExpander(expRef, expVal);

        // Revert some of the custom watch expressions scope presentation flags.
        expRef.switch = null;
        expRef.delete = null;
        expRef.descriptorTooltip = true;
        expRef.separatorStr = L10N.getStr("variablesSeparatorLabel");
      }

      // Signal that watch expressions have been fetched.
      window.dispatchEvent(document, "Debugger:FetchedWatchExpressions");
      DebuggerView.Variables.commitHierarchy();
    });
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
    if (this.currentException) {
      let excRef = aScope.addItem("<exception>", { value: this.currentException });
      DebuggerView.Variables.controller.addExpander(excRef, this.currentException);
    }
    // Add any returned value.
    if (this.currentReturnedValue) {
      let retRef = aScope.addItem("<return>", { value: this.currentReturnedValue });
      DebuggerView.Variables.controller.addExpander(retRef, this.currentReturnedValue);
    }
    // Add "this".
    if (aFrame.this) {
      let thisRef = aScope.addItem("this", { value: aFrame.this });
      DebuggerView.Variables.controller.addExpander(thisRef, aFrame.this);
    }
  },

  /**
   * Loads more stack frames from the debugger server cache.
   */
  addMoreFrames: function() {
    this.activeThread.fillFrames(
      this.activeThread.cachedFrames.length + CALL_STACK_PAGE_SIZE);
  },

  /**
   * Updates a list of watch expressions to evaluate on each pause.
   */
  syncWatchExpressions: function() {
    let list = DebuggerView.WatchExpressions.getExpressions();

    // Sanity check all watch expressions before syncing them. To avoid
    // having the whole watch expressions array throw because of a single
    // faulty expression, simply convert it to a string describing the error.
    // There's no other information necessary to be offered in such cases.
    let sanitizedExpressions = list.map(function(str) {
      // Reflect.parse throws when it encounters a syntax error.
      try {
        Parser.reflectionAPI.parse(str);
        return str; // Watch expression can be executed safely.
      } catch (e) {
        return "\"" + e.name + ": " + e.message + "\""; // Syntax error.
      }
    });

    if (sanitizedExpressions.length) {
      this.syncedWatchExpressions =
        this.currentWatchExpressions =
          "[" +
            sanitizedExpressions.map(function(str)
              "eval(\"" +
                "try {" +
                  // Make sure all quotes are escaped in the expression's syntax,
                  // and add a newline after the statement to avoid comments
                  // breaking the code integrity inside the eval block.
                  str.replace(/"/g, "\\$&") + "\" + " + "'\\n'" + " + \"" +
                "} catch (e) {" +
                  "e.name + ': ' + e.message;" + // FIXME: bug 812765, 812764
                "}" +
              "\")"
            ).join(",") +
          "]";
    } else {
      this.syncedWatchExpressions =
        this.currentWatchExpressions = null;
    }
    this.currentFrame = null;
    this._onFrames();
  },

  /**
   * Evaluate an expression in the context of the selected frame. This is used
   * for modifying the value of variables or properties in scope.
   *
   * @param string aExpression
   *        The expression to evaluate.
   * @param number aFrame [optional]
   *        The frame depth used for evaluation.
   */
  evaluate: function(aExpression, aFrame = this.currentFrame || 0) {
    let frame = this.activeThread.cachedFrames[aFrame];
    this.activeThread.eval(frame.actor, aExpression);
  }
};

/**
 * Keeps the source script list up-to-date, using the thread client's
 * source script cache.
 */
function SourceScripts() {
  this._cache = new Map(); // Can't use a WeakMap because keys are strings.
  this._onNewSource = this._onNewSource.bind(this);
  this._onNewGlobal = this._onNewGlobal.bind(this);
  this._onSourcesAdded = this._onSourcesAdded.bind(this);
  this._onFetch = this._onFetch.bind(this);
  this._onTimeout = this._onTimeout.bind(this);
  this._onFinished = this._onFinished.bind(this);
}

SourceScripts.prototype = {
  get activeThread() DebuggerController.activeThread,
  get debuggerClient() DebuggerController.client,
  _newSourceTimeout: null,

  /**
   * Connect to the current thread client.
   */
  connect: function() {
    dumpn("SourceScripts is connecting...");
    this.debuggerClient.addListener("newGlobal", this._onNewGlobal);
    this.debuggerClient.addListener("newSource", this._onNewSource);
    this._handleTabNavigation();
  },

  /**
   * Disconnect from the client.
   */
  disconnect: function() {
    if (!this.activeThread) {
      return;
    }
    dumpn("SourceScripts is disconnecting...");
    window.clearTimeout(this._newSourceTimeout);
    this.debuggerClient.removeListener("newGlobal", this._onNewGlobal);
    this.debuggerClient.removeListener("newSource", this._onNewSource);
  },

  /**
   * Handles any initialization on a tab navigation event issued by the client.
   */
  _handleTabNavigation: function() {
    if (!this.activeThread) {
      return;
    }
    dumpn("Handling tab navigation in the SourceScripts");
    window.clearTimeout(this._newSourceTimeout);

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

    let container = DebuggerView.Sources;
    let preferredValue = container.preferredValue;

    // Select this source if it's the preferred one.
    if (aPacket.source.url == preferredValue) {
      container.selectedValue = preferredValue;
    }
    // ..or the first entry if there's none selected yet after a while
    else {
      window.clearTimeout(this._newSourceTimeout);
      this._newSourceTimeout = window.setTimeout(function() {
        // If after a certain delay the preferred source still wasn't received,
        // just give up on waiting and display the first entry.
        if (!container.selectedValue) {
          container.selectedIndex = 0;
        }
      }, NEW_SOURCE_DISPLAY_DELAY);
    }

    // If there are any stored breakpoints for this source, display them again,
    // both in the editor and the breakpoints pane.
    DebuggerController.Breakpoints.updateEditorBreakpoints();
    DebuggerController.Breakpoints.updatePaneBreakpoints();

    // Signal that a new script has been added.
    window.dispatchEvent(document, "Debugger:AfterNewSource");
  },

  /**
   * Callback for the debugger's active thread getSources() method.
   */
  _onSourcesAdded: function(aResponse) {
    if (aResponse.error) {
      Cu.reportError(new Error("Error getting sources: " + aResponse.message));
      return;
    }

    // Add all the sources in the debugger view sources container.
    for (let source of aResponse.sources) {
      // Ignore bogus scripts, e.g. generated from 'clientEvaluate' packets.
      if (NEW_SOURCE_IGNORED_URLS.indexOf(source.url) != -1) {
        continue;
      }
      DebuggerView.Sources.addSource(source, { staged: true });
    }

    let container = DebuggerView.Sources;
    let preferredValue = container.preferredValue;

    // Flushes all the prepared sources into the sources container.
    container.commit({ sorted: true });

    // Select the preferred source if it exists and was part of the response.
    if (container.containsValue(preferredValue)) {
      container.selectedValue = preferredValue;
    }
    // ..or the first entry if there's no one selected yet.
    else if (!container.selectedValue) {
      container.selectedIndex = 0;
    }

    // If there are any stored breakpoints for the sources, display them again,
    // both in the editor and the breakpoints pane.
    DebuggerController.Breakpoints.updateEditorBreakpoints();
    DebuggerController.Breakpoints.updatePaneBreakpoints();

    // Signal that scripts have been added.
    window.dispatchEvent(document, "Debugger:AfterSourcesAdded");
  },

  /**
   * Gets a specified source's text.
   *
   * @param object aSource
   *        The source object coming from the active thread.
   * @param function aCallback
   *        Function called after the source text has been loaded.
   * @param function aTimeout
   *        Function called when the source text takes too long to fetch.
   */
  getText: function(aSource, aCallback, aTimeout) {
    // If already loaded, return the source text immediately.
    if (aSource.loaded) {
      aCallback(aSource);
      return;
    }

    // If the source text takes too long to fetch, invoke a timeout to
    // avoid blocking any operations.
    if (aTimeout) {
      var fetchTimeout = window.setTimeout(() => {
        aSource._fetchingTimedOut = true;
        aTimeout(aSource);
      }, FETCH_SOURCE_RESPONSE_DELAY);
    }

    // Get the source text from the active thread.
    this.activeThread.source(aSource).source((aResponse) => {
      if (aTimeout) {
        window.clearTimeout(fetchTimeout);
      }
      if (aResponse.error) {
        Cu.reportError("Error loading: " + aSource.url + "\n" + aResponse.message);
        return void aCallback(aSource);
      }
      aSource.loaded = true;
      aSource.text = aResponse.source;
      aCallback(aSource);
    });
  },

  /**
   * Gets all the fetched sources.
   *
   * @return array
   *         An array containing [url, text] entries for the fetched sources.
   */
  getCache: function() {
    let sources = [];
    for (let source of this._cache) {
      sources.push(source);
    }
    return sources.sort(([first], [second]) => first > second);
  },

  /**
   * Clears all the fetched sources from cache.
   */
  clearCache: function() {
    this._cache.clear();
  },

  /**
   * Starts fetching all the sources, silently.
   *
   * @param array aUrls
   *        The urls for the sources to fetch.
   * @param object aCallbacks [optional]
   *        An object containing the callback functions to invoke:
   *          - onFetch: optional, called after each source is fetched
   *          - onTimeout: optional, called when a source takes too long to fetch
   *          - onFinished: called when all the sources are fetched
   */
  fetchSources: function(aUrls, aCallbacks = {}) {
    this._fetchQueue = new Set();
    this._fetchCallbacks = aCallbacks;

    // Add each new source which needs to be fetched in a queue.
    for (let url of aUrls) {
      if (!this._cache.has(url)) {
        this._fetchQueue.add(url);
      }
    }

    // If all the sources were already fetched, don't do anything special.
    if (this._fetchQueue.size == 0) {
      this._onFinished();
      return;
    }

    // Start fetching each new source.
    for (let url of this._fetchQueue) {
      let sourceItem = DebuggerView.Sources.getItemByValue(url);
      let sourceObject = sourceItem.attachment.source;
      this.getText(sourceObject, this._onFetch, this._onTimeout);
    }
  },

  /**
   * Called when a source has been fetched via fetchSources().
   *
   * @param object aSource
   *        The source object coming from the active thread.
   */
  _onFetch: function(aSource) {
    // Remember the source in a cache so we don't have to fetch it again.
    this._cache.set(aSource.url, aSource.text);

    // Fetch completed before timeout, remove the source from the fetch queue.
    this._fetchQueue.delete(aSource.url);

    // If this fetch was eventually completed at some point after a timeout,
    // don't call any subsequent event listeners.
    if (aSource._fetchingTimedOut) {
      return;
    }

    // Invoke the source fetch callback if provided via fetchSources();
    if (this._fetchCallbacks.onFetch) {
      this._fetchCallbacks.onFetch(aSource);
    }

    // Check if all sources were fetched and stored in the cache.
    if (this._fetchQueue.size == 0) {
      this._onFinished();
    }
  },

  /**
   * Called when a source's text takes too long to fetch via fetchSources().
   *
   * @param object aSource
   *        The source object coming from the active thread.
   */
  _onTimeout: function(aSource) {
    // Remove the source from the fetch queue.
    this._fetchQueue.delete(aSource.url);

    // Invoke the source timeout callback if provided via fetchSources();
    if (this._fetchCallbacks.onTimeout) {
      this._fetchCallbacks.onTimeout(aSource);
    }

    // Check if the remaining sources were fetched and stored in the cache.
    if (this._fetchQueue.size == 0) {
      this._onFinished();
    }
  },

  /**
   * Called when all the sources have been fetched.
   */
  _onFinished: function() {
    // Invoke the finish callback if provided via fetchSources();
    if (this._fetchCallbacks.onFinished) {
      this._fetchCallbacks.onFinished();
    }
  },

  _cache: null,
  _fetchQueue: null,
  _fetchCallbacks: null
};

/**
 * Handles all the breakpoints in the current debugger.
 */
function Breakpoints() {
  this._onEditorBreakpointChange = this._onEditorBreakpointChange.bind(this);
  this._onEditorBreakpointAdd = this._onEditorBreakpointAdd.bind(this);
  this._onEditorBreakpointRemove = this._onEditorBreakpointRemove.bind(this);
  this.addBreakpoint = this.addBreakpoint.bind(this);
  this.removeBreakpoint = this.removeBreakpoint.bind(this);
  this.getBreakpoint = this.getBreakpoint.bind(this);
}

Breakpoints.prototype = {
  get activeThread() DebuggerController.ThreadState.activeThread,
  get editor() DebuggerView.editor,

  /**
   * The list of breakpoints in the debugger as tracked by the current
   * debugger instance. This is an object where the values are BreakpointActor
   * objects received from the client, while the keys are actor names, for
   * example "conn0.breakpoint3".
   */
  store: {},

  /**
   * Skip editor breakpoint change events.
   *
   * This property tells the source editor event handler to skip handling of
   * the BREAKPOINT_CHANGE events. This is used when the debugger adds/removes
   * breakpoints from the editor. Typically, the BREAKPOINT_CHANGE event handler
   * adds/removes events from the debugger, but when breakpoints are added from
   * the public debugger API, we need to do things in reverse.
   *
   * This implementation relies on the fact that the source editor fires the
   * BREAKPOINT_CHANGE events synchronously.
   */
  _skipEditorBreakpointCallbacks: false,

  /**
   * Adds the source editor breakpoint handlers.
   */
  initialize: function() {
    this.editor.addEventListener(
      SourceEditor.EVENTS.BREAKPOINT_CHANGE, this._onEditorBreakpointChange);
  },

  /**
   * Removes the source editor breakpoint handlers & all the added breakpoints.
   */
  destroy: function() {
    this.editor.removeEventListener(
      SourceEditor.EVENTS.BREAKPOINT_CHANGE, this._onEditorBreakpointChange);

    for each (let breakpointClient in this.store) {
      this.removeBreakpoint(breakpointClient);
    }
  },

  /**
   * Event handler for breakpoint changes that happen in the editor. This
   * function syncs the breakpoints in the editor to those in the debugger.
   *
   * @param object aEvent
   *        The SourceEditor.EVENTS.BREAKPOINT_CHANGE event object.
   */
  _onEditorBreakpointChange: function(aEvent) {
    if (this._skipEditorBreakpointCallbacks) {
      return;
    }
    this._skipEditorBreakpointCallbacks = true;
    aEvent.added.forEach(this._onEditorBreakpointAdd, this);
    aEvent.removed.forEach(this._onEditorBreakpointRemove, this);
    this._skipEditorBreakpointCallbacks = false;
  },

  /**
   * Event handler for new breakpoints that come from the editor.
   *
   * @param object aEditorBreakpoint
   *        The breakpoint object coming from the editor.
   */
  _onEditorBreakpointAdd: function(aEditorBreakpoint) {
    let url = DebuggerView.Sources.selectedValue;
    let line = aEditorBreakpoint.line + 1;

    this.addBreakpoint({ url: url, line: line }, (aBreakpointClient) => {
      // If the breakpoint client has an "actualLocation" attached, then
      // the original requested placement for the breakpoint wasn't accepted.
      // In this case, we need to update the editor with the new location.
      if (aBreakpointClient.actualLocation) {
        this.editor.removeBreakpoint(line - 1);
        this.editor.addBreakpoint(aBreakpointClient.actualLocation.line - 1);
      }
    });
  },

  /**
   * Event handler for breakpoints that are removed from the editor.
   *
   * @param object aEditorBreakpoint
   *        The breakpoint object that was removed from the editor.
   */
  _onEditorBreakpointRemove: function(aEditorBreakpoint) {
    let url = DebuggerView.Sources.selectedValue;
    let line = aEditorBreakpoint.line + 1;

    this.removeBreakpoint(this.getBreakpoint(url, line));
  },

  /**
   * Update the breakpoints in the editor view. This function takes the list of
   * breakpoints in the debugger and adds them back into the editor view.
   * This is invoked when the selected script is changed.
   */
  updateEditorBreakpoints: function() {
    for each (let breakpointClient in this.store) {
      if (DebuggerView.Sources.selectedValue == breakpointClient.location.url) {
        this._showBreakpoint(breakpointClient, {
          noPaneUpdate: true,
          noPaneHighlight: true
        });
      }
    }
  },

  /**
   * Update the breakpoints in the pane view. This function takes the list of
   * breakpoints in the debugger and adds them back into the breakpoints pane.
   * This is invoked when scripts are added.
   */
  updatePaneBreakpoints: function() {
    for each (let breakpointClient in this.store) {
      if (DebuggerView.Sources.containsValue(breakpointClient.location.url)) {
        this._showBreakpoint(breakpointClient, {
          noEditorUpdate: true,
          noPaneHighlight: true
        });
      }
    }
  },

  /**
   * Add a breakpoint.
   *
   * @param object aLocation
   *        The location where you want the breakpoint. This object must have
   *        two properties:
   *          - url: the url of the source.
   *          - line: the line number (starting from 1).
   * @param function aCallback [optional]
   *        Optional function to invoke once the breakpoint is added. The
   *        callback is invoked with two arguments:
   *          - aBreakpointClient: the BreakpointActor client object
   *          - aResponseError: if there was any error
   * @param object aFlags [optional]
   *        An object containing some of the following boolean properties:
   *          - conditionalExpression: tells this breakpoint's conditional expression
   *          - openPopup: tells if the expression popup should be shown
   *          - noEditorUpdate: tells if you want to skip editor updates
   *          - noPaneUpdate: tells if you want to skip breakpoint pane updates
   *          - noPaneHighlight: tells if you don't want to highlight the breakpoint
   */
  addBreakpoint: function(aLocation, aCallback, aFlags = {}) {
    let breakpointClient = this.getBreakpoint(aLocation.url, aLocation.line);

    // If the breakpoint was already added, callback immediately.
    if (breakpointClient) {
      aCallback && aCallback(breakpointClient);
      return;
    }

    this.activeThread.setBreakpoint(aLocation, (aResponse, aBreakpointClient) => {
      let { url, line } = aResponse.actualLocation || aLocation;

      // If the response contains a breakpoint that exists in the cache, prevent
      // it from being shown in the source editor at an incorrect position.
      if (this.getBreakpoint(url, line)) {
        this._hideBreakpoint(aBreakpointClient);
        return;
      }

      // If the breakpoint response has an "actualLocation" attached, then
      // the original requested placement for the breakpoint wasn't accepted.
      if (aResponse.actualLocation) {
        // Store the originally requested location in case it's ever needed.
        aBreakpointClient.requestedLocation = {
          url: aBreakpointClient.location.url,
          line: aBreakpointClient.location.line
        };
        // Store the response actual location to be used.
        aBreakpointClient.actualLocation = aResponse.actualLocation;
        // Update the breakpoint client with the actual location.
        aBreakpointClient.location.url = aResponse.actualLocation.url;
        aBreakpointClient.location.line = aResponse.actualLocation.line;
      }

      // Remember the breakpoint client in the store.
      this.store[aBreakpointClient.actor] = aBreakpointClient;

      // Attach any specified conditional expression to the breakpoint client.
      aBreakpointClient.conditionalExpression = aFlags.conditionalExpression;

      // Preserve information about the breakpoint's line text, to display it in
      // the sources pane without requiring fetching the source.
      aBreakpointClient.lineText = DebuggerView.getEditorLine(line - 1).trim();

      // Show the breakpoint in the editor and breakpoints pane.
      this._showBreakpoint(aBreakpointClient, aFlags);

      // We're done here.
      aCallback && aCallback(aBreakpointClient, aResponse.error);
    });
  },

  /**
   * Remove a breakpoint.
   *
   * @param object aBreakpointClient
   *        The BreakpointActor client object to remove.
   * @param function aCallback [optional]
   *        Optional function to invoke once the breakpoint is removed. The
   *        callback is invoked with one argument
   *          - aBreakpointClient: the breakpoint location (url and line)
   * @param object aFlags [optional]
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  removeBreakpoint: function(aBreakpointClient, aCallback, aFlags = {}) {
    let breakpointActor = (aBreakpointClient || {}).actor;

    // If the breakpoint was already removed, callback immediately.
    if (!this.store[breakpointActor]) {
      aCallback && aCallback(aBreakpointClient.location);
      return;
    }

    aBreakpointClient.remove(() => {
      // Delete the breakpoint client from the store.
      delete this.store[breakpointActor];

      // Hide the breakpoint from the editor and breakpoints pane.
      this._hideBreakpoint(aBreakpointClient, aFlags);

      // We're done here.
      aCallback && aCallback(aBreakpointClient.location);
    });
  },

  /**
   * Update the editor and breakpoints pane to show a specified breakpoint.
   *
   * @param object aBreakpointClient
   *        The BreakpointActor client object to show.
   * @param object aFlags [optional]
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  _showBreakpoint: function(aBreakpointClient, aFlags = {}) {
    let currentSourceUrl = DebuggerView.Sources.selectedValue;
    let { url, line } = aBreakpointClient.location;

    // Update the editor if required.
    if (!aFlags.noEditorUpdate) {
      if (url == currentSourceUrl) {
        this._skipEditorBreakpointCallbacks = true;
        this.editor.addBreakpoint(line - 1);
        this._skipEditorBreakpointCallbacks = false;
      }
    }
    // Update the breakpoints pane if required.
    if (!aFlags.noPaneUpdate) {
      DebuggerView.Sources.addBreakpoint({
        sourceLocation: url,
        lineNumber: line,
        lineText: aBreakpointClient.lineText,
        actor: aBreakpointClient.actor,
        openPopupFlag: aFlags.openPopup
      });
    }
    // Highlight the breakpoint in the pane if required.
    if (!aFlags.noPaneHighlight) {
      DebuggerView.Sources.highlightBreakpoint(url, line, aFlags);
    }
  },

  /**
   * Update the editor and breakpoints pane to hide a specified breakpoint.
   *
   * @param object aBreakpointClient
   *        The BreakpointActor client object to hide.
   * @param object aFlags [optional]
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  _hideBreakpoint: function(aBreakpointClient, aFlags = {}) {
    let currentSourceUrl = DebuggerView.Sources.selectedValue;
    let { url, line } = aBreakpointClient.location;

    // Update the editor if required.
    if (!aFlags.noEditorUpdate) {
      if (url == currentSourceUrl) {
        this._skipEditorBreakpointCallbacks = true;
        this.editor.removeBreakpoint(line - 1);
        this._skipEditorBreakpointCallbacks = false;
      }
    }
    // Update the breakpoints pane if required.
    if (!aFlags.noPaneUpdate) {
      DebuggerView.Sources.removeBreakpoint(url, line);
    }
  },

  /**
   * Get the breakpoint object at the given location.
   *
   * @param string aUrl
   *        The URL of where the breakpoint is.
   * @param number aLine
   *        The line number where the breakpoint is.
   * @return object
   *         The BreakpointActor object.
   */
  getBreakpoint: function(aUrl, aLine) {
    for each (let breakpointClient in this.store) {
      if (breakpointClient.location.url == aUrl &&
          breakpointClient.location.line == aLine) {
        return breakpointClient;
      }
    }
    return null;
  }
};

/**
 * Localization convenience methods.
 */
let L10N = new ViewHelpers.L10N(DBG_STRINGS_URI);

/**
 * Shortcuts for accessing various debugger preferences.
 */
let Prefs = new ViewHelpers.Prefs("devtools.debugger", {
  chromeDebuggingHost: ["Char", "chrome-debugging-host"],
  chromeDebuggingPort: ["Int", "chrome-debugging-port"],
  windowX: ["Int", "ui.win-x"],
  windowY: ["Int", "ui.win-y"],
  windowWidth: ["Int", "ui.win-width"],
  windowHeight: ["Int", "ui.win-height"],
  sourcesWidth: ["Int", "ui.panes-sources-width"],
  instrumentsWidth: ["Int", "ui.panes-instruments-width"],
  pauseOnExceptions: ["Bool", "ui.pause-on-exceptions"],
  panesVisibleOnStartup: ["Bool", "ui.panes-visible-on-startup"],
  variablesSortingEnabled: ["Bool", "ui.variables-sorting-enabled"],
  variablesOnlyEnumVisible: ["Bool", "ui.variables-only-enum-visible"],
  variablesSearchboxVisible: ["Bool", "ui.variables-searchbox-visible"],
  sourceMapsEnabled: ["Bool", "source-maps-enabled"],
  remoteHost: ["Char", "remote-host"],
  remotePort: ["Int", "remote-port"],
  remoteAutoConnect: ["Bool", "remote-autoconnect"],
  remoteConnectionRetries: ["Int", "remote-connection-retries"],
  remoteTimeout: ["Int", "remote-timeout"]
});

/**
 * Returns true if this is a remote debugger instance.
 * @return boolean
 */
XPCOMUtils.defineLazyGetter(window, "_isRemoteDebugger", function() {
  // We're inside a single top level XUL window, not an iframe container.
  return !(window.frameElement instanceof XULElement) &&
         !!window._remoteFlag;
});

/**
 * Returns true if this is a chrome debugger instance.
 * @return boolean
 */
XPCOMUtils.defineLazyGetter(window, "_isChromeDebugger", function() {
  // We're inside a single top level XUL window, but not a remote debugger.
  return !(window.frameElement instanceof XULElement) &&
         !window._remoteFlag;
});

/**
 * Preliminary setup for the DebuggerController object.
 */
DebuggerController.initialize();
DebuggerController.Parser = new Parser();
DebuggerController.ThreadState = new ThreadState();
DebuggerController.StackFrames = new StackFrames();
DebuggerController.SourceScripts = new SourceScripts();
DebuggerController.Breakpoints = new Breakpoints();

/**
 * Export some properties to the global scope for easier access.
 */
Object.defineProperties(window, {
  "create": {
    get: function() ViewHelpers.create,
  },
  "dispatchEvent": {
    get: function() ViewHelpers.dispatchEvent,
  },
  "editor": {
    get: function() DebuggerView.editor
  },
  "gClient": {
    get: function() DebuggerController.client
  },
  "gThreadClient": {
    get: function() DebuggerController.activeThread
  },
  "gThreadState": {
    get: function() DebuggerController.ThreadState
  },
  "gStackFrames": {
    get: function() DebuggerController.StackFrames
  },
  "gSourceScripts": {
    get: function() DebuggerController.SourceScripts
  },
  "gBreakpoints": {
    get: function() DebuggerController.Breakpoints
  },
  "gCallStackPageSize": {
    get: function() CALL_STACK_PAGE_SIZE,
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
