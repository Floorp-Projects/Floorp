/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const DBG_STRINGS_URI = "chrome://browser/locale/devtools/debugger.properties";
const NEW_SCRIPT_DISPLAY_DELAY = 200; // ms
const FRAME_STEP_CLEAR_DELAY = 100; // ms
const CALL_STACK_PAGE_SIZE = 25; // frames

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/devtools/dbg-server.jsm");
Cu.import("resource://gre/modules/devtools/dbg-client.jsm");
Cu.import("resource:///modules/source-editor.jsm");
Cu.import("resource:///modules/devtools/LayoutHelpers.jsm");
Cu.import("resource:///modules/devtools/VariablesView.jsm");

/**
 * Object defining the debugger controller components.
 */
let DebuggerController = {
  /**
   * Initializes the debugger controller.
   */
  initialize: function DC_initialize() {
    dumpn("Initializing the DebuggerController");
    this._startupDebugger = this._startupDebugger.bind(this);
    this._shutdownDebugger = this._shutdownDebugger.bind(this);
    this._onTabNavigated = this._onTabNavigated.bind(this);
    this._onTabDetached = this._onTabDetached.bind(this);

    window.addEventListener("load", this._startupDebugger, true);
    window.addEventListener("unload", this._shutdownDebugger, true);
  },

  /**
   * Initializes the view and connects a debugger client to the server.
   */
  _startupDebugger: function DC__startupDebugger() {
    if (this._isInitialized) {
      return;
    }
    this._isInitialized = true;

    window.removeEventListener("load", this._startupDebugger, true);

    DebuggerView.initialize(function() {
      DebuggerView._isInitialized = true;

      window.dispatchEvent("Debugger:Loaded");
      this._connect();
    }.bind(this));
  },

  /**
   * Destroys the view and disconnects the debugger client from the server.
   */
  _shutdownDebugger: function DC__shutdownDebugger() {
    if (this._isDestroyed || !DebuggerView._isInitialized) {
      return;
    }
    this._isDestroyed = true;
    window.removeEventListener("unload", this._shutdownDebugger, true);

    DebuggerView.destroy(function() {
      DebuggerView._isDestroyed = true;
      this.SourceScripts.disconnect();
      this.StackFrames.disconnect();
      this.ThreadState.disconnect();

      this._disconnect();
      window.dispatchEvent("Debugger:Unloaded");
      window._isChromeDebugger && this._quitApp();
    }.bind(this));
  },

  /**
   * Prepares the hostname and port number for a remote debugger connection
   * and handles connection retries and timeouts.
   *
   * @return boolean
   *         True if connection should proceed normally, false otherwise.
   */
  _prepareConnection: function DC__prepareConnection() {
    // If we exceeded the total number of connection retries, bail.
    if (this._remoteConnectionTry === Prefs.remoteConnectionRetries) {
      Services.prompt.alert(null,
        L10N.getStr("remoteDebuggerPromptTitle"),
        L10N.getStr("remoteDebuggerConnectionFailedMessage"));

      // If the connection was not established before a certain number of
      // retries, close the remote debugger.
      this._shutdownDebugger();
      return false;
    }

    // TODO: This is ugly, need to rethink the design for the UI in #751677.
    if (!Prefs.remoteAutoConnect) {
      let prompt = new RemoteDebuggerPrompt();
      let result = prompt.show(!!this._remoteConnectionTimeout);

      // If the connection was not established before the user canceled the
      // prompt, close the remote debugger.
      if (!result) {
        this._shutdownDebugger();
        return false;
      }

      Prefs.remoteHost = prompt.remote.host;
      Prefs.remotePort = prompt.remote.port;
      Prefs.remoteAutoConnect = prompt.remote.auto;
    }

    // If this debugger is connecting remotely to a server, we need to check
    // after a while if the connection actually succeeded.
    this._remoteConnectionTry = ++this._remoteConnectionTry || 1;
    this._remoteConnectionTimeout = window.setTimeout(function() {
      // If we couldn't connect to any server yet, try again...
      if (!this.activeThread) {
        this._onRemoteConnectionTimeout();
        this._connect();
      }
    }.bind(this), Prefs.remoteTimeout);

    // Proceed with the connection normally.
    return true;
  },

  /**
   * Called when a remote connection timeout occurs.
   */
  _onRemoteConnectionTimeout: function DC__onRemoteConnectionTimeout() {
    Cu.reportError("Couldn't connect to " +
      Prefs.remoteHost + ":" + Prefs.remotePort);
  },

  /**
   * Initializes a debugger client and connects it to the debugger server,
   * wiring event handlers as necessary.
   */
  _connect: function DC__connect() {
    function callback() {
      window.dispatchEvent("Debugger:Connected");
    }

    let client;

    // Remote debugging gets the debuggee from a RemoteTarget object.
    if (this._target && this._target.isRemote) {
      window._isRemoteDebugger = true;

      client = this.client = this._target.client;
      this._target.on("close", this._onTabDetached);
      this._target.on("navigate", this._onTabNavigated);

      if (this._target.chrome) {
        let dbg = this._target.form.chromeDebugger;
        this._startChromeDebugging(client, dbg, callback);
      } else {
        this._startDebuggingTab(client, this._target.form, callback);
      }
      return;
    }

    // Content or chrome debugging can connect directly to the debuggee.
    // TODO: convert this to use a TabTarget.
    let transport = window._isChromeDebugger
      ? debuggerSocketConnect(Prefs.remoteHost, Prefs.remotePort)
      : DebuggerServer.connectPipe();

    client = this.client = new DebuggerClient(transport);
    client.addListener("tabNavigated", this._onTabNavigated);
    client.addListener("tabDetached", this._onTabDetached);

    client.connect(function(aType, aTraits) {
      client.listTabs(function(aResponse) {
        if (window._isChromeDebugger) {
          let dbg = aResponse.chromeDebugger;
          this._startChromeDebugging(client, dbg, callback);
        } else {
          let tab = aResponse.tabs[aResponse.selected];
          this._startDebuggingTab(client, tab, callback);
        }
      }.bind(this));
    }.bind(this));
  },

  /**
   * Disconnects the debugger client and removes event handlers as necessary.
   */
  _disconnect: function DC__disconnect() {
    // Return early if the client didn't even have a chance to instantiate.
    if (!this.client) {
      return;
    }
    this.client.removeListener("tabNavigated", this._onTabNavigated);
    this.client.removeListener("tabDetached", this._onTabDetached);

    // When remote debugging, the connection is closed by the RemoteTarget.
    if (!window._isRemoteDebugger) {
      this.client.close();
    }

    this.client = null;
    this.tabClient = null;
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
  _onTabNavigated: function DC__onTabNavigated(aType, aPacket) {
    if (aPacket.state == "start") {
      DebuggerView._handleTabNavigation();
      return;
    }

    this.ThreadState._handleTabNavigation();
    this.StackFrames._handleTabNavigation();
    this.SourceScripts._handleTabNavigation();
  },

  /**
   * Called when the debugged tab is closed.
   */
  _onTabDetached: function DC__onTabDetached() {
    this._shutdownDebugger();
  },

  /**
   * Sets up a debugging session.
   *
   * @param DebuggerClient aClient
   *        The debugger client.
   * @param object aTabGrip
   *        The remote protocol grip of the tab.
   */
  _startDebuggingTab: function DC__startDebuggingTab(aClient, aTabGrip, aCallback) {
    if (!aClient) {
      Cu.reportError("No client found!");
      return;
    }
    this.client = aClient;

    aClient.attachTab(aTabGrip.actor, function(aResponse, aTabClient) {
      if (!aTabClient) {
        Cu.reportError("No tab client found!");
        return;
      }
      this.tabClient = aTabClient;

      aClient.attachThread(aResponse.threadActor, function(aResponse, aThreadClient) {
        if (!aThreadClient) {
          Cu.reportError("Couldn't attach to thread: " + aResponse.error);
          return;
        }
        this.activeThread = aThreadClient;

        this.ThreadState.connect();
        this.StackFrames.connect();
        this.SourceScripts.connect();
        aThreadClient.resume();

        if (aCallback) {
          aCallback();
        }
      }.bind(this));
    }.bind(this));
  },

  /**
   * Sets up a chrome debugging session.
   *
   * @param DebuggerClient aClient
   *        The debugger client.
   * @param object aChromeDebugger
   *        The remote protocol grip of the chrome debugger.
   */
  _startChromeDebugging: function DC__startChromeDebugging(aClient, aChromeDebugger, aCallback) {
    if (!aClient) {
      Cu.reportError("No client found!");
      return;
    }
    this.client = aClient;

    aClient.attachThread(aChromeDebugger, function(aResponse, aThreadClient) {
      if (!aThreadClient) {
        Cu.reportError("Couldn't attach to thread: " + aResponse.error);
        return;
      }
      this.activeThread = aThreadClient;

      this.ThreadState.connect();
      this.StackFrames.connect();
      this.SourceScripts.connect();
      aThreadClient.resume();

      if (aCallback) {
        aCallback();
      }
    }.bind(this));
  },

  /**
   * Attempts to quit the current process if allowed.
   */
  _quitApp: function DC__quitApp() {
    let canceled = Cc["@mozilla.org/supports-PRBool;1"]
      .createInstance(Ci.nsISupportsPRBool);

    Services.obs.notifyObservers(canceled, "quit-application-requested", null);

    // Somebody canceled our quit request.
    if (canceled.data) {
      return;
    }
    Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit);
  },

  /**
   * Convenience method, dispatching a custom event.
   *
   * @param string aType
   *        The name of the event.
   * @param any aDetail
   *        The data passed when initializing the event.
   */
  dispatchEvent: function DC_dispatchEvent(aType, aDetail) {
    let evt = document.createEvent("CustomEvent");
    evt.initCustomEvent(aType, true, false, aDetail);
    document.documentElement.dispatchEvent(evt);
  }
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
  connect: function TS_connect() {
    dumpn("ThreadState is connecting...");
    this.activeThread.addListener("paused", this._update);
    this.activeThread.addListener("resumed", this._update);
    this.activeThread.addListener("detached", this._update);
    this._handleTabNavigation();
  },

  /**
   * Disconnect from the client.
   */
  disconnect: function TS_disconnect() {
    if (!this.activeThread) {
      return;
    }
    dumpn("ThreadState is disconnecting...");
    this.activeThread.removeListener("paused", this._update);
    this.activeThread.removeListener("resumed", this._update);
    this.activeThread.removeListener("detached", this._update);
  },

  /**
   * Handles any initialization on a tab navigation event issued by the client.
   */
  _handleTabNavigation: function TS__handleTabNavigation() {
    if (!this.activeThread) {
      return;
    }
    dumpn("Handling tab navigation in the ThreadState");
    this._update(this.activeThread.state);
  },

  /**
   * Update the UI after a thread state change.
   */
  _update: function TS__update(aEvent) {
    DebuggerView.Toolbar.toggleResumeButtonState(this.activeThread.state);
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

  /**
   * Connect to the current thread client.
   */
  connect: function SF_connect() {
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
  disconnect: function SF_disconnect() {
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
  _handleTabNavigation: function SF__handleTabNavigation() {
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
  _onPaused: function SF__onPaused(aEvent, aPacket) {
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
    }

    this.activeThread.fillFrames(CALL_STACK_PAGE_SIZE);
    DebuggerView.editor.focus();
  },

  /**
   * Handler for the thread client's resumed notification.
   */
  _onResumed: function SF__onResumed() {
    DebuggerView.editor.setDebugLocation(-1);

    // Prepare the watch expression evaluation string for the next pause.
    if (!this._isWatchExpressionsEvaluation) {
      this.currentWatchExpressions = this.syncedWatchExpressions;
    }
  },

  /**
   * Handler for the thread client's framesadded notification.
   */
  _onFrames: function SF__onFrames() {
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
        this.activeThread.resume();
        return;
      }
    }


    // Watch expressions are evaluated in the context of the topmost frame,
    // and the results and displayed in the variables view.
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


    // Make sure all the previous stackframes are removed before re-adding them.
    DebuggerView.StackFrames.empty();

    for (let frame of this.activeThread.cachedFrames) {
      this._addFrame(frame);
    }
    if (this.currentFrame == null) {
      this.selectFrame(0);
    }
    if (this.activeThread.moreFrames) {
      DebuggerView.StackFrames.dirty = true;
    }
  },

  /**
   * Handler for the thread client's framescleared notification.
   */
  _onFramesCleared: function SF__onFramesCleared() {
    this.currentFrame = null;
    this.currentWatchExpressions = null;
    this.currentBreakpointLocation = null;
    this.currentEvaluation = null;
    this.currentException = null;
    // After each frame step (in, over, out), framescleared is fired, which
    // forces the UI to be emptied and rebuilt on framesadded. Most of the times
    // this is not necessary, and will result in a brief redraw flicker.
    // To avoid it, invalidate the UI only after a short time if necessary.
    window.setTimeout(this._afterFramesCleared, FRAME_STEP_CLEAR_DELAY);
  },

  /**
   * Called soon after the thread client's framescleared notification.
   */
  _afterFramesCleared: function SF__afterFramesCleared() {
    // Ignore useless notifications.
    if (this.activeThread.cachedFrames.length) {
      return;
    }
    DebuggerView.StackFrames.empty();
    DebuggerView.Variables.empty(0);
    DebuggerView.Breakpoints.unhighlightBreakpoint();
    DebuggerView.WatchExpressions.toggleContents(true);
    window.dispatchEvent("Debugger:AfterFramesCleared");
  },

  /**
   * Marks the stack frame at the specified depth as selected and updates the
   * properties view with the stack frame's data.
   *
   * @param number aDepth
   *        The depth of the frame in the stack.
   */
  selectFrame: function SF_selectFrame(aDepth) {
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
    // Highlight the stack frame at the specified depth.
    DebuggerView.StackFrames.highlightFrame(aDepth);
    // Highlight the breakpoint at the specified url and line if it exists.
    DebuggerView.Breakpoints.highlightBreakpoint(url, line);
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
      let arrow = L10N.getStr("watchExpressionsSeparatorLabel");
      let scope = DebuggerView.Variables.addScope(label);
      scope.separator = arrow;
      scope.showDescriptorTooltip = false;
      scope.allowNameInput = true;
      scope.allowDeletion = true;
      scope.contextMenu = "debuggerWatchExpressionsContextMenu";
      scope.switch = DebuggerView.WatchExpressions.switchExpression;
      scope.delete = DebuggerView.WatchExpressions.deleteExpression;

      // The evaluation hasn't thrown, so display the returned results and
      // always expand the watch expressions scope by default.
      this._fetchWatchExpressions(scope, watchExpressionsEvaluation);
      scope.expand();
    }

    do {
      // Create a scope to contain all the inspected variables.
      let label = this._getScopeLabel(environment);
      let scope = DebuggerView.Variables.addScope(label);

      // Special additions to the innermost scope.
      if (environment == frame.environment) {
        this._insertScopeFrameReferences(scope, frame);
        this._fetchScopeVariables(scope, environment);
        // Always expand the innermost scope by default.
        scope.expand();
      }
      // Lazily add nodes for every other environment scope.
      else {
        this._addScopeExpander(scope, environment);
        this.autoScopeExpand && scope.expand();
      }
    } while (environment = environment.parent);

    // Signal that variables have been fetched.
    window.dispatchEvent("Debugger:FetchedVariables");
    DebuggerView.Variables.commitHierarchy();
  },

  /**
   * Adds an 'onexpand' callback for a scope, lazily handling
   * the addition of new variables.
   *
   * @param Scope aScope
   *        The scope where the variables will be placed into.
   * @param object aEnv
   *        The scope's environment.
   */
  _addScopeExpander: function SF__addScopeExpander(aScope, aEnv) {
    let callback = this._fetchScopeVariables.bind(this, aScope, aEnv);

    // It's a good idea to be prepared in case of an expansion.
    aScope.onmouseover = callback;
    // Make sure that variables are always available on expansion.
    aScope.onexpand = callback;
  },

  /**
   * Adds an 'onexpand' callback for a variable, lazily handling
   * the addition of new properties.
   *
   * @param Variable aVar
   *        The variable where the properties will be placed into.
   * @param any aGrip
   *        The grip of the variable.
   */
  _addVarExpander: function SF__addVarExpander(aVar, aGrip) {
    // No need for expansion for primitive values.
    if (VariablesView.isPrimitive({ value: aGrip })) {
      return;
    }
    let callback = this._fetchVarProperties.bind(this, aVar, aGrip);

    // Some variables are likely to contain a very large number of properties.
    // It's a good idea to be prepared in case of an expansion.
    if (aVar.name == "window" || aVar.name == "this") {
      aVar.onmouseover = callback;
    }
    // Make sure that properties are always available on expansion.
    aVar.onexpand = callback;
  },

  /**
   * Adds the watch expressions evaluation results to a scope in the view.
   *
   * @param Scope aScope
   *        The scope where the watch expressions will be placed into.
   * @param object aExp
   *        The grip of the evaluation results.
   */
  _fetchWatchExpressions: function SF__fetchWatchExpressions(aScope, aExp) {
    // Retrieve the expressions only once.
    if (aScope.fetched) {
      return;
    }
    aScope.fetched = true;

    // Add nodes for every watch expression in scope.
    this.activeThread.pauseGrip(aExp).getPrototypeAndProperties(function(aResponse) {
      let ownProperties = aResponse.ownProperties;
      let totalExpressions = DebuggerView.WatchExpressions.totalItems;

      for (let i = 0; i < totalExpressions; i++) {
        let name = DebuggerView.WatchExpressions.getExpression(i);
        let expVal = ownProperties[i].value;
        let expRef = aScope.addVar(name, ownProperties[i]);
        this._addVarExpander(expRef, expVal);
      }

      // Signal that watch expressions have been fetched.
      window.dispatchEvent("Debugger:FetchedWatchExpressions");
      DebuggerView.Variables.commitHierarchy();
    }.bind(this));
  },

  /**
   * Adds variables to a scope in the view. Triggered when a scope is
   * expanded or is hovered. It does not expand the scope.
   *
   * @param Scope aScope
   *        The scope where the variables will be placed into.
   * @param object aEnv
   *        The scope's environment.
   */
  _fetchScopeVariables: function SF__fetchScopeVariables(aScope, aEnv) {
    // Retrieve the variables only once.
    if (aScope.fetched) {
      return;
    }
    aScope.fetched = true;

    switch (aEnv.type) {
      case "with":
      case "object":
        // Add nodes for every variable in scope.
        this.activeThread.pauseGrip(aEnv.object).getPrototypeAndProperties(function(aResponse) {
          this._insertScopeVariables(aResponse.ownProperties, aScope);

          // Signal that variables have been fetched.
          window.dispatchEvent("Debugger:FetchedVariables");
          DebuggerView.Variables.commitHierarchy();
        }.bind(this));
        break;
      case "block":
      case "function":
        // Add nodes for every argument and every other variable in scope.
        this._insertScopeArguments(aEnv.bindings.arguments, aScope);
        this._insertScopeVariables(aEnv.bindings.variables, aScope);
        break;
      default:
        Cu.reportError("Unknown Debugger.Environment type: " + aEnv.type);
        break;
    }
  },

  /**
   * Add nodes for special frame references in the innermost scope.
   *
   * @param Scope aScope
   *        The scope where the references will be placed into.
   * @param object aFrame
   *        The frame to get some references from.
   */
  _insertScopeFrameReferences: function SF__insertScopeFrameReferences(aScope, aFrame) {
    // Add any thrown exception.
    if (this.currentException) {
      let excRef = aScope.addVar("<exception>", { value: this.currentException });
      this._addVarExpander(excRef, this.currentException);
    }
    // Add "this".
    if (aFrame.this) {
      let thisRef = aScope.addVar("this", { value: aFrame.this });
      this._addVarExpander(thisRef, aFrame.this);
    }
  },

  /**
   * Add nodes for every argument in scope.
   *
   * @param object aArguments
   *        The map of names to arguments, as specified in the protocol.
   * @param Scope aScope
   *        The scope where the nodes will be placed into.
   */
  _insertScopeArguments: function SF__insertScopeArguments(aArguments, aScope) {
    if (!aArguments) {
      return;
    }
    for (let argument of aArguments) {
      let name = Object.getOwnPropertyNames(argument)[0];
      let argRef = aScope.addVar(name, argument[name]);
      let argVal = argument[name].value;
      this._addVarExpander(argRef, argVal);
    }
  },

  /**
   * Add nodes for every variable in scope.
   *
   * @param object aVariables
   *        The map of names to variables, as specified in the protocol.
   * @param Scope aScope
   *        The scope where the nodes will be placed into.
   */
  _insertScopeVariables: function SF__insertScopeVariables(aVariables, aScope) {
    if (!aVariables) {
      return;
    }
    let variableNames = Object.keys(aVariables);

    // Sort all of the variables before adding them if preferred.
    if (Prefs.variablesSortingEnabled) {
      variableNames.sort();
    }
    // Add the sorted variables to the specified scope.
    for (let name of variableNames) {
      let varRef = aScope.addVar(name, aVariables[name]);
      let varVal = aVariables[name].value;
      this._addVarExpander(varRef, varVal);
    }
  },

  /**
   * Adds properties to a variable in the view. Triggered when a variable is
   * expanded or certain variables are hovered. It does not expand the variable.
   *
   * @param Variable aVar
   *        The variable where the properties will be placed into.
   * @param any aGrip
   *        The grip of the variable.
   */
  _fetchVarProperties: function SF__fetchVarProperties(aVar, aGrip) {
    // Retrieve the properties only once.
    if (aVar.fetched) {
      return;
    }
    aVar.fetched = true;

    this.activeThread.pauseGrip(aGrip).getPrototypeAndProperties(function(aResponse) {
      let { ownProperties, prototype } = aResponse;

      // Add all the variable properties.
      if (ownProperties) {
        aVar.addProperties(ownProperties);
        // Expansion handlers must be set after the properties are added.
        for (let name in ownProperties) {
          this._addVarExpander(aVar.get(name), ownProperties[name].value);
        }
      }

      // Add the variable's __proto__.
      if (prototype && prototype.type != "null") {
        aVar.addProperty("__proto__", { value: prototype });
        // Expansion handlers must be set after the properties are added.
        this._addVarExpander(aVar.get("__proto__"), prototype);
      }

      aVar._retrieved = true;

      // Signal that properties have been fetched.
      window.dispatchEvent("Debugger:FetchedProperties");
      DebuggerView.Variables.commitHierarchy();
    }.bind(this));
  },

  /**
   * Constructs a scope label based on its environment.
   *
   * @param object aEnv
   *        The scope's environment.
   * @return string
   *         The scope's label.
   */
  _getScopeLabel: function SV__getScopeLabel(aEnv) {
    let name = "";

    // Name the outermost scope Global.
    if (!aEnv.parent) {
      name = L10N.getStr("globalScopeLabel");
    }
    // Otherwise construct the scope name.
    else {
      name = aEnv.type.charAt(0).toUpperCase() + aEnv.type.slice(1);
    }

    let label = L10N.getFormatStr("scopeLabel", [name]);
    switch (aEnv.type) {
      case "with":
      case "object":
        label += " [" + aEnv.object.class + "]";
        break;
      case "function":
        let f = aEnv.function;
        label += " [" + (f.name || f.userDisplayName || f.displayName ||
                         "(anonymous)") + "]";
        break;
    }
    return label;
  },

  /**
   * Adds the specified stack frame to the list.
   *
   * @param object aFrame
   *        The new frame to add.
   */
  _addFrame: function SF__addFrame(aFrame) {
    let depth = aFrame.depth;
    let { url, line } = aFrame.where;

    let startText = StackFrameUtils.getFrameTitle(aFrame);
    let endText = SourceUtils.getSourceLabel(url) + ":" + line;
    DebuggerView.StackFrames.addFrame(startText, endText, depth);
  },

  /**
   * Loads more stack frames from the debugger server cache.
   */
  addMoreFrames: function SF_addMoreFrames() {
    this.activeThread.fillFrames(
      this.activeThread.cachedFrames.length + CALL_STACK_PAGE_SIZE);
  },

  /**
   * Updates a list of watch expressions to evaluate on each pause.
   */
  syncWatchExpressions: function SF_syncWatchExpressions() {
    let list = DebuggerView.WatchExpressions.getExpressions();

    if (list.length) {
      this.syncedWatchExpressions =
        this.currentWatchExpressions = "[" + list.map(function(str)
          // Avoid yielding an empty pseudo-array when evaluating `arguments`,
          // since they're overridden by the expression's closure scope.
          "(function(arguments) {" +
            // Make sure all the quotes are escaped in the expression's syntax.
            "try { return eval(\"" + str.replace(/"/g, "\\$&") + "\"); }" +
            "catch(e) { return e.name + ': ' + e.message; }" +
          "})(arguments)"
        ).join(",") + "]";
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
  evaluate: function SF_evaluate(aExpression, aFrame = this.currentFrame || 0) {
    let frame = this.activeThread.cachedFrames[aFrame];
    this.activeThread.eval(frame.actor, aExpression);
  }
};

/**
 * Keeps the source script list up-to-date, using the thread client's
 * source script cache.
 *
 * FIXME: Currently, "sources" are actually "scripts", this should change in
 * Bug 795368 - Add "sources" and "newSource" packets to the RDP, and use them
 * instead of "scripts" and "newScript".
 */
function SourceScripts() {
  this._onNewScript = this._onNewScript.bind(this);
  this._onNewGlobal = this._onNewGlobal.bind(this);
  this._onScriptsAdded = this._onScriptsAdded.bind(this);
}

SourceScripts.prototype = {
  get activeThread() DebuggerController.activeThread,
  get debuggerClient() DebuggerController.client,

  /**
   * Connect to the current thread client.
   */
  connect: function SS_connect() {
    dumpn("SourceScripts is connecting...");
    this.debuggerClient.addListener("newScript", this._onNewScript);
    this.debuggerClient.addListener("newGlobal", this._onNewGlobal);
    this._handleTabNavigation();
  },

  /**
   * Disconnect from the client.
   */
  disconnect: function SS_disconnect() {
    if (!this.activeThread) {
      return;
    }
    dumpn("SourceScripts is disconnecting...");
    this.debuggerClient.removeListener("newScript", this._onNewScript);
    this.debuggerClient.removeListener("newGlobal", this._onNewGlobal);
  },

  /**
   * Handles any initialization on a tab navigation event issued by the client.
   */
  _handleTabNavigation: function SS__handleTabNavigation() {
    if (!this.activeThread) {
      return;
    }
    dumpn("Handling tab navigation in the SourceScripts");

    // Retrieve the list of script sources known to the server from before
    // the client was ready to handle "newScript" notifications.
    this.activeThread.getScripts(this._onScriptsAdded);
  },

  /**
   * Handler for the debugger client's unsolicited newScript notification.
   */
  _onNewScript: function SS__onNewScript(aNotification, aPacket) {
    // Ignore scripts generated from 'clientEvaluate' packets.
    if (aPacket.url == "debugger eval code") {
      return;
    }

    // Add the source in the debugger view sources container.
    this._addSource({
      url: aPacket.url,
      startLine: aPacket.startLine,
      source: aPacket.source
    }, {
      forced: true
    });

    let container = DebuggerView.Sources;
    let preferredValue = container.preferredValue;

    // Select this source if it's the preferred one.
    if (aPacket.url == preferredValue) {
      container.selectedValue = preferredValue;
    }
    // ..or the first entry if there's none selected yet after a while
    else {
      window.setTimeout(function() {
        // If after a certain delay the preferred source still wasn't received,
        // just give up on waiting and display the first entry.
        if (!container.selectedValue) {
          container.selectedIndex = 0;
        }
      }, NEW_SCRIPT_DISPLAY_DELAY);
    }

    // If there are any stored breakpoints for this source, display them again,
    // both in the editor and the breakpoints pane.
    DebuggerController.Breakpoints.updateEditorBreakpoints();
    DebuggerController.Breakpoints.updatePaneBreakpoints();

    // Signal that a new script has been added.
    window.dispatchEvent("Debugger:AfterNewScript");
  },

  /**
   * Handler for the debugger client's unsolicited newGlobal notification.
   */
  _onNewGlobal: function SS__onNewGlobal(aNotification, aPacket) {
    // TODO: bug 806775, update the globals list using aPacket.hostAnnotations
    // from bug 801084.
  },

  /**
   * Callback for the debugger's active thread getScripts() method.
   */
  _onScriptsAdded: function SS__onScriptsAdded(aResponse) {
    // Add all the sources in the debugger view sources container.
    for (let script of aResponse.scripts) {
      // Ignore scripts generated from 'clientEvaluate' packets.
      if (script.url == "debugger eval code") {
        continue;
      }
      this._addSource(script);
    }

    let container = DebuggerView.Sources;
    let preferredValue = container.preferredValue;

    // Flushes all the prepared sources into the sources container.
    container.commit();

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
    window.dispatchEvent("Debugger:AfterScriptsAdded");
  },

  /**
   * Add the specified source to the debugger view sources list.
   *
   * @param object aScript
   *        The source object coming from the active thread.
   * @param object aOptions [optional]
   *        Additional options for adding the source. Supported options:
   *        - forced: force the source to be immediately added
   */
  _addSource: function SS__addSource(aSource, aOptions = {}) {
    let url = aSource.url;
    let label = SourceUtils.getSourceLabel(url);

    DebuggerView.Sources.push(label, url, {
      forced: aOptions.forced,
      attachment: aSource
    });
  },

  /**
   * Gets a specified source's text.
   *
   * @param object aSource
   *        The source object coming from the active thread.
   * @param function aCallback
   *        Function called after the source text has been loaded.
   */
  getText: function SS_getText(aSource, aCallback) {
    // If already loaded, return the source text immediately.
    if (aSource.loaded) {
      aCallback(aSource.url, aSource.text);
      return;
    }

    // Get the source text from the active thread.
    this.activeThread.source(aSource.source).source(function(aResponse) {
      if (aResponse.error) {
        Cu.reportError("Error loading " + aUrl);
        return;
      }
      aSource.loaded = true;
      aSource.text = aResponse.source;
      aCallback(aSource.url, aResponse.source);
    });
  }
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
  initialize: function BP_initialize() {
    this.editor.addEventListener(
      SourceEditor.EVENTS.BREAKPOINT_CHANGE, this._onEditorBreakpointChange);
  },

  /**
   * Removes the source editor breakpoint handlers & all the added breakpoints.
   */
  destroy: function BP_destroy() {
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
  _onEditorBreakpointChange: function BP__onEditorBreakpointChange(aEvent) {
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
  _onEditorBreakpointAdd: function BP__onEditorBreakpointAdd(aEditorBreakpoint) {
    let url = DebuggerView.Sources.selectedValue;
    let line = aEditorBreakpoint.line + 1;

    this.addBreakpoint({ url: url, line: line }, function(aBreakpointClient) {
      // If the breakpoint client has an "actualLocation" attached, then
      // the original requested placement for the breakpoint wasn't accepted.
      // In this case, we need to update the editor with the new location.
      if (aBreakpointClient.actualLocation) {
        this.editor.removeBreakpoint(line - 1);
        this.editor.addBreakpoint(aBreakpointClient.actualLocation.line - 1);
      }
    }.bind(this));
  },

  /**
   * Event handler for breakpoints that are removed from the editor.
   *
   * @param object aEditorBreakpoint
   *        The breakpoint object that was removed from the editor.
   */
  _onEditorBreakpointRemove: function BP__onEditorBreakpointRemove(aEditorBreakpoint) {
    let url = DebuggerView.Sources.selectedValue;
    let line = aEditorBreakpoint.line + 1;

    this.removeBreakpoint(this.getBreakpoint(url, line));
  },

  /**
   * Update the breakpoints in the editor view. This function takes the list of
   * breakpoints in the debugger and adds them back into the editor view.
   * This is invoked when the selected script is changed.
   */
  updateEditorBreakpoints: function BP_updateEditorBreakpoints() {
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
  updatePaneBreakpoints: function BP_updatePaneBreakpoints() {
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
  addBreakpoint:
  function BP_addBreakpoint(aLocation, aCallback, aFlags = {}) {
    let breakpointClient = this.getBreakpoint(aLocation.url, aLocation.line);

    // If the breakpoint was already added, callback immediately.
    if (breakpointClient) {
      aCallback && aCallback(breakpointClient);
      return;
    }

    this.activeThread.setBreakpoint(aLocation, function(aResponse, aBreakpointClient) {
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

      // Preserve some information about the breakpoint's source url and line
      // to display in the breakpoints pane.
      aBreakpointClient.lineText = DebuggerView.getEditorLine(line - 1);
      aBreakpointClient.lineInfo = SourceUtils.getSourceLabel(url) + ":" + line;

      // Show the breakpoint in the editor and breakpoints pane.
      this._showBreakpoint(aBreakpointClient, aFlags);

      // We're done here.
      aCallback && aCallback(aBreakpointClient, aResponse.error);
    }.bind(this));
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
  removeBreakpoint:
  function BP_removeBreakpoint(aBreakpointClient, aCallback, aFlags = {}) {
    let breakpointActor = (aBreakpointClient || {}).actor;

    // If the breakpoint was already removed, callback immediately.
    if (!this.store[breakpointActor]) {
      aCallback && aCallback(aBreakpointClient.location);
      return;
    }

    aBreakpointClient.remove(function() {
      // Delete the breakpoint client from the store.
      delete this.store[breakpointActor];

      // Hide the breakpoint from the editor and breakpoints pane.
      this._hideBreakpoint(aBreakpointClient, aFlags);

      // We're done here.
      aCallback && aCallback(aBreakpointClient.location);
    }.bind(this));
  },

  /**
   * Update the editor and breakpoints pane to show a specified breakpoint.
   *
   * @param object aBreakpointClient
   *        The BreakpointActor client object to show.
   * @param object aFlags [optional]
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  _showBreakpoint: function BP__showBreakpoint(aBreakpointClient, aFlags = {}) {
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
      let { lineText, lineInfo, actor } = aBreakpointClient;
      let conditionalFlag = aBreakpointClient.conditionalExpression !== undefined;
      let openPopupFlag = aFlags.openPopup;

      DebuggerView.Breakpoints.addBreakpoint(
        url, line, actor, lineInfo, lineText, conditionalFlag, openPopupFlag);
    }
    // Highlight the breakpoint in the pane if required.
    if (!aFlags.noPaneHighlight) {
      DebuggerView.Breakpoints.highlightBreakpoint(url, line);
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
  _hideBreakpoint: function BP__hideBreakpoint(aBreakpointClient, aFlags = {}) {
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
      DebuggerView.Breakpoints.removeBreakpoint(url, line);
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
  getBreakpoint: function BP_getBreakpoint(aUrl, aLine) {
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
let L10N = {
  /**
   * L10N shortcut function.
   *
   * @param string aName
   * @return string
   */
  getStr: function L10N_getStr(aName) {
    return this.stringBundle.GetStringFromName(aName);
  },

  /**
   * L10N shortcut function.
   *
   * @param string aName
   * @param array aArray
   * @return string
   */
  getFormatStr: function L10N_getFormatStr(aName, aArray) {
    return this.stringBundle.formatStringFromName(aName, aArray, aArray.length);
  }
};

XPCOMUtils.defineLazyGetter(L10N, "stringBundle", function() {
  return Services.strings.createBundle(DBG_STRINGS_URI);
});

XPCOMUtils.defineLazyGetter(L10N, "ellipsis", function() {
  return Services.prefs.getComplexValue("intl.ellipsis", Ci.nsIPrefLocalizedString).data;
});

/**
 * Shortcuts for accessing various debugger preferences.
 */
let Prefs = {
  /**
   * Helper method for getting a pref value.
   *
   * @param string aType
   * @param string aPrefName
   * @return any
   */
  _get: function P__get(aType, aPrefName) {
    if (this[aPrefName] === undefined) {
      this[aPrefName] = Services.prefs["get" + aType + "Pref"](aPrefName);
    }
    return this[aPrefName];
  },

  /**
   * Helper method for setting a pref value.
   *
   * @param string aType
   * @param string aPrefName
   * @param any aValue
   */
  _set: function P__set(aType, aPrefName, aValue) {
    Services.prefs["set" + aType + "Pref"](aPrefName, aValue);
    this[aPrefName] = aValue;
  },

  /**
   * Maps a property name to a pref, defining lazy getters and setters.
   *
   * @param string aType
   * @param string aPropertyName
   * @param string aPrefName
   */
  map: function P_map(aType, aPropertyName, aPrefName) {
    Object.defineProperty(this, aPropertyName, {
      get: function() this._get(aType, aPrefName),
      set: function(aValue) this._set(aType, aPrefName, aValue)
    });
  }
};

Prefs.map("Int", "windowX", "devtools.debugger.ui.win-x");
Prefs.map("Int", "windowY", "devtools.debugger.ui.win-y");
Prefs.map("Int", "windowWidth", "devtools.debugger.ui.win-width");
Prefs.map("Int", "windowHeight", "devtools.debugger.ui.win-height");
Prefs.map("Int", "stackframesWidth", "devtools.debugger.ui.stackframes-width");
Prefs.map("Int", "variablesWidth", "devtools.debugger.ui.variables-width");
Prefs.map("Bool", "panesVisibleOnStartup", "devtools.debugger.ui.panes-visible-on-startup");
Prefs.map("Bool", "variablesSortingEnabled", "devtools.debugger.ui.variables-sorting-enabled");
Prefs.map("Bool", "variablesOnlyEnumVisible", "devtools.debugger.ui.variables-only-enum-visible");
Prefs.map("Bool", "variablesSearchboxVisible", "devtools.debugger.ui.variables-searchbox-visible");
Prefs.map("Char", "remoteHost", "devtools.debugger.remote-host");
Prefs.map("Int", "remotePort", "devtools.debugger.remote-port");
Prefs.map("Bool", "remoteAutoConnect", "devtools.debugger.remote-autoconnect");
Prefs.map("Int", "remoteConnectionRetries", "devtools.debugger.remote-connection-retries");
Prefs.map("Int", "remoteTimeout", "devtools.debugger.remote-timeout");

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
DebuggerController.ThreadState = new ThreadState();
DebuggerController.StackFrames = new StackFrames();
DebuggerController.SourceScripts = new SourceScripts();
DebuggerController.Breakpoints = new Breakpoints();

/**
 * Export some properties to the global scope for easier access.
 */
Object.defineProperties(window, {
  "gClient": {
    get: function() DebuggerController.client
  },
  "gTabClient": {
    get: function() DebuggerController.tabClient
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
  },
  "dispatchEvent": {
    get: function() DebuggerController.dispatchEvent,
  },
  "editor": {
    get: function() DebuggerView.editor
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
