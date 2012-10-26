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
    if (window._isRemoteDebugger && !this._prepareConnection()) {
      return;
    }
    let transport = (window._isChromeDebugger || window._isRemoteDebugger)
      ? debuggerSocketConnect(Prefs.remoteHost, Prefs.remotePort)
      : DebuggerServer.connectPipe();

    let client = this.client = new DebuggerClient(transport);
    client.addListener("tabNavigated", this._onTabNavigated);
    client.addListener("tabDetached", this._onTabDetached);

    client.connect(function(aType, aTraits) {
      client.listTabs(function(aResponse) {
        let tab = aResponse.tabs[aResponse.selected];
        this._startDebuggingTab(client, tab);
        window.dispatchEvent("Debugger:Connected");
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
    this.client.close();

    this.client = null;
    this.tabClient = null;
    this.activeThread = null;
  },

  /**
   * Called for each location change in the debugged tab.
   */
  _onTabNavigated: function DC__onTabNavigated() {
    DebuggerView._handleTabNavigation();
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
  _startDebuggingTab: function DC__startDebuggingTab(aClient, aTabGrip) {
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

      }.bind(this));
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
  currentFrame: null,
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
    // In case the pause was caused by an exception, store the exception value.
    if (aPacket.why.type == "exception") {
      this.currentException = aPacket.why.exception;
    }

    this.activeThread.fillFrames(CALL_STACK_PAGE_SIZE);
    DebuggerView.editor.focus();
  },

  /**
   * Handler for the thread client's resumed notification.
   */
  _onResumed: function SF__onResumed() {
    DebuggerView.editor.setDebugLocation(-1);
  },

  /**
   * Handler for the thread client's framesadded notification.
   */
  _onFrames: function SF__onFrames() {
    // Ignore useless notifications.
    if (!this.activeThread.cachedFrames.length) {
      return;
    }
    DebuggerView.StackFrames.empty();

    for (let frame of this.activeThread.cachedFrames) {
      this._addFrame(frame);
    }
    if (!this.currentFrame) {
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
    let env = frame.environment;
    let { url, line } = frame.where;

    // Check if the frame does not represent the evaluation of debuggee code.
    if (!env) {
      return;
    }

    // Move the editor's caret to the proper url and line.
    DebuggerView.updateEditor(url, line);
    // Highlight the stack frame at the specified depth.
    DebuggerView.StackFrames.highlightFrame(aDepth);
    // Highlight the breakpoint at the specified url and line if it exists.
    DebuggerView.Breakpoints.highlightBreakpoint(url, line);
    // Start recording any added variables or properties in any scope.
    DebuggerView.Variables.createHierarchy();
    // Clear existing scopes and create each one dynamically.
    DebuggerView.Variables.empty();

    let self = this;
    let name = "";

    do {
      // Name the outermost scope Global.
      if (!env.parent) {
        name = L10N.getStr("globalScopeLabel");
      }
      // Otherwise construct the scope name.
      else {
        name = env.type.charAt(0).toUpperCase() + env.type.slice(1);
      }

      let label = L10N.getFormatStr("scopeLabel", [name]);
      switch (env.type) {
        case "with":
        case "object":
          label += " [" + env.object.class + "]";
          break;
        case "function":
          label += " [" + env.functionName + "]";
          break;
      }

      // Create a scope to contain all the inspected variables.
      let scope = DebuggerView.Variables.addScope(label);

      // Special additions to the innermost scope.
      if (env == frame.environment) {
        // Add any thrown exception.
        if (aDepth == 0 && this.currentException) {
          let excVar = scope.addVar("<exception>", { value: this.currentException });
          this._addExpander(excVar, this.currentException);
        }
        // Add "this".
        if (frame.this) {
          let thisVar = scope.addVar("this", { value: frame.this });
          this._addExpander(thisVar, frame.this);
        }
        // Expand the innermost scope by default.
        scope.expand(true);
      }

      switch (env.type) {
        case "with":
        case "object":
          // Add nodes for all variables in the environment object scope.
          this.activeThread.pauseGrip(env.object).getPrototypeAndProperties(function(aResponse) {
            self._addScopeVariables(aResponse.ownProperties, scope);

            // Signal that variables have been fetched.
            window.dispatchEvent("Debugger:FetchedVariables");
            DebuggerView.Variables.commitHierarchy();
          });
          break;
        case "block":
        case "function":
          // Add nodes for every argument.
          for (let variable of env.bindings.arguments) {
            let name = Object.getOwnPropertyNames(variable)[0];
            let paramVar = scope.addVar(name, variable[name]);
            let paramVal = variable[name].value;
            this._addExpander(paramVar, paramVal);
          }
          // Add nodes for every other variable in scope.
          this._addScopeVariables(env.bindings.variables, scope);
          break;
        default:
          Cu.reportError("Unknown Debugger.Environment type: " + env.type);
          break;
      }
    } while (env = env.parent);

    // Signal that variables have been fetched.
    window.dispatchEvent("Debugger:FetchedVariables");
    DebuggerView.Variables.commitHierarchy();
  },

  /**
   * Add nodes for every variable in scope.
   *
   * @param object aVariables
   *        The map of names to variables, as specified in the Remote
   *        Debugging Protocol.
   * @param Scope aScope
   *        The scope where the nodes will be placed into.
   */
  _addScopeVariables: function SF_addScopeVariables(aVariables, aScope) {
    if (!aVariables) {
      return;
    }
    // Sort all of the variables before adding them.
    let sortedVariableNames = Object.keys(aVariables).sort();

    // Add the sorted variables to the specified scope.
    for (let name of sortedVariableNames) {
      let paramVar = aScope.addVar(name, aVariables[name]);
      let paramVal = aVariables[name].value;
      this._addExpander(paramVar, paramVal);
    }
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
  _addExpander: function SF__addExpander(aVar, aGrip) {
    // No need for expansion for primitive values.
    if (VariablesView.isPrimitive({ value: aGrip })) {
      return;
    }
    aVar.onexpand = this._addVarProperties.bind(this, aVar, aGrip);
  },

  /**
   * Adds properties to a variable in the view. Triggered when a variable is
   * expanded.
   *
   * @param Variable aVar
   *        The variable where the properties will be placed into.
   * @param any aGrip
   *        The grip of the variable.
   */
  _addVarProperties: function SF__addVarProperties(aVar, aGrip) {
    // Retrieve the properties only once.
    if (aVar.fetched) {
      return;
    }

    this.activeThread.pauseGrip(aGrip).getPrototypeAndProperties(function(aResponse) {
      let { ownProperties, prototype } = aResponse;

      // Add all the variable properties.
      if (ownProperties) {
        aVar.addProperties(ownProperties);
        // Expansion handlers must be set after the properties are added.
        for (let name in ownProperties) {
          this._addExpander(aVar.get(name), ownProperties[name].value);
        }
      }

      // Add the variable's __proto__.
      if (prototype.type != "null") {
        aVar.addProperties({ "__proto__ ": { value: prototype } });
        // Expansion handlers must be set after the properties are added.
        this._addExpander(aVar.get("__proto__ "), prototype);
      }

      aVar.fetched = true;

      // Signal that properties have been fetched.
      window.dispatchEvent("Debugger:FetchedProperties");
      DebuggerView.Variables.commitHierarchy();
    }.bind(this));
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

    DebuggerView.StackFrames.addFrame(startText, endText, depth, {
      attachment: aFrame
    });
  },

  /**
   * Loads more stack frames from the debugger server cache.
   */
  addMoreFrames: function SF_addMoreFrames() {
    this.activeThread.fillFrames(
      this.activeThread.cachedFrames.length + CALL_STACK_PAGE_SIZE);
  },

  /**
   * Evaluate an expression in the context of the selected frame. This is used
   * for modifying the value of variables or properties in scope.
   *
   * @param string aExpression
   *        The expression to evaluate.
   */
  evaluate: function SF_evaluate(aExpression) {
    let frame = this.activeThread.cachedFrames[this.currentFrame];
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
  this._onScriptsAdded = this._onScriptsAdded.bind(this);
}

SourceScripts.prototype = {
  get activeThread() DebuggerController.activeThread,
  get debuggerClient() DebuggerController.client,

  /**
   * Connect to the current thread client.
   */
  connect: function SS_connect() {
    this.debuggerClient.addListener("newScript", this._onNewScript);
    this._handleTabNavigation();
  },

  /**
   * Disconnect from the client.
   */
  disconnect: function SS_disconnect() {
    if (!this.activeThread) {
      return;
    }
    this.debuggerClient.removeListener("newScript", this._onNewScript);
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
   * Callback for the debugger's active thread getScripts() method.
   */
  _onScriptsAdded: function SS__onScriptsAdded(aResponse) {
    // Add all the sources in the debugger view sources container.
    for (let script of aResponse.scripts) {
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
        this._showBreakpoint(breakpointClient, { noPaneUpdate: true });
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
        this._showBreakpoint(breakpointClient, { noEditorUpdate: true });
      }
    }
  },

  /**
   * Add a breakpoint.
   *
   * @param object aLocation
   *        The location where you want the breakpoint. This object must have
   *        two properties:
   *          - url - the url of the source.
   *          - line - the line number (starting from 1).
   * @param function aCallback [optional]
   *        Optional function to invoke once the breakpoint is added. The
   *        callback is invoked with two arguments:
   *          - aBreakpointClient: the BreakpointActor client object
   *          - aResponseError: if there was any error
   * @param object aFlags [optional]
   *        An object containing some of the following boolean properties:
   *          - noEditorUpdate: tells if you want to skip editor updates
   *          - noPaneUpdate: tells if you want to skip breakpoint pane updates
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

      // Prevent this new breakpoint from being repositioned on top of an
      // already existing one.
      if (this.getBreakpoint(url, line)) {
        this._hideBreakpoint(aBreakpointClient);
        aBreakpointClient.remove();
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
   *        An object containing some of the following boolean properties:
   *          - noEditorUpdate: tells if you want to skip editor updates
   *          - noPaneUpdate: tells if you want to skip breakpoint pane updates
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
   *        An object containing some of the following boolean properties:
   *          - noEditorUpdate: tells if you want to skip editor updates
   *          - noPaneUpdate: tells if you want to skip breakpoint pane updates
   */
  _showBreakpoint: function BP__showBreakpoint(aBreakpointClient, aFlags = {}) {
    let currentSourceUrl = DebuggerView.Sources.selectedValue;
    let { url, line } = aBreakpointClient.location;

    if (!aFlags.noEditorUpdate) {
      if (url == currentSourceUrl) {
        this._skipEditorBreakpointCallbacks = true;
        this.editor.addBreakpoint(line - 1);
        this._skipEditorBreakpointCallbacks = false;
      }
    }
    if (!aFlags.noPaneUpdate) {
      let { lineText, lineInfo } = aBreakpointClient;
      let actor = aBreakpointClient.actor;
      DebuggerView.Breakpoints.addBreakpoint(lineInfo, lineText, url, line, actor);
    }
  },

  /**
   * Update the editor and breakpoints pane to hide a specified breakpoint.
   *
   * @param object aBreakpointClient
   *        The BreakpointActor client object to hide.
   * @param object aFlags [optional]
   *        An object containing some of the following boolean properties:
   *          - noEditorUpdate: tells if you want to skip editor updates
   *          - noPaneUpdate: tells if you want to skip breakpoint pane updates
   */
  _hideBreakpoint: function BP__hideBreakpoint(aBreakpointClient, aFlags = {}) {
    let currentSourceUrl = DebuggerView.Sources.selectedValue;
    let { url, line } = aBreakpointClient.location;

    if (!aFlags.noEditorUpdate) {
      if (url == currentSourceUrl) {
        this._skipEditorBreakpointCallbacks = true;
        this.editor.removeBreakpoint(line - 1);
        this._skipEditorBreakpointCallbacks = false;
      }
    }
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

const STACKFRAMES_WIDTH = "devtools.debugger.ui.stackframes-width";
const STACKFRAMES_VISIBLE = "devtools.debugger.ui.stackframes-pane-visible";
const VARIABLES_WIDTH = "devtools.debugger.ui.variables-width";
const VARIABLES_PANE_VISIBLE = "devtools.debugger.ui.variables-pane-visible";
const NON_ENUM_VISIBLE = "devtools.debugger.ui.non-enum-visible";
const REMOTE_AUTO_CONNECT = "devtools.debugger.remote-autoconnect";
const REMOTE_HOST = "devtools.debugger.remote-host";
const REMOTE_PORT = "devtools.debugger.remote-port";
const REMOTE_CONNECTION_RETRIES = "devtools.debugger.remote-connection-retries";
const REMOTE_TIMEOUT = "devtools.debugger.remote-timeout";

/**
 * Shortcuts for accessing various debugger preferences.
 */
let Prefs = {
  /**
   * Gets the preferred stackframes pane width.
   * @return number
   */
  get stackframesWidth() {
    if (this._stackframesWidth === undefined) {
      this._stackframesWidth = Services.prefs.getIntPref(STACKFRAMES_WIDTH);
    }
    return this._stackframesWidth;
  },

  /**
   * Sets the preferred stackframes pane width.
   * @param number value
   */
  set stackframesWidth(value) {
    Services.prefs.setIntPref(STACKFRAMES_WIDTH, value);
    this._stackframesWidth = value;
  },

  /**
   * Gets the preferred stackframes pane visibility state.
   * @return boolean
   */
  get stackframesPaneVisible() {
    if (this._stackframesVisible === undefined) {
      this._stackframesVisible = Services.prefs.getBoolPref(STACKFRAMES_VISIBLE);
    }
    return this._stackframesVisible;
  },

  /**
   * Sets the preferred stackframes pane visibility state.
   * @param boolean value
   */
  set stackframesPaneVisible(value) {
    Services.prefs.setBoolPref(STACKFRAMES_VISIBLE, value);
    this._stackframesVisible = value;
  },

  /**
   * Gets the preferred variables pane width.
   * @return number
   */
  get variablesWidth() {
    if (this._variablesWidth === undefined) {
      this._variablesWidth = Services.prefs.getIntPref(VARIABLES_WIDTH);
    }
    return this._variablesWidth;
  },

  /**
   * Sets the preferred variables pane width.
   * @param number value
   */
  set variablesWidth(value) {
    Services.prefs.setIntPref(VARIABLES_WIDTH, value);
    this._variablesWidth = value;
  },

  /**
   * Gets the preferred variables pane visibility state.
   * @return boolean
   */
  get variablesPaneVisible() {
    if (this._variablesVisible === undefined) {
      this._variablesVisible = Services.prefs.getBoolPref(VARIABLES_PANE_VISIBLE);
    }
    return this._variablesVisible;
  },

  /**
   * Sets the preferred variables pane visibility state.
   * @param boolean value
   */
  set variablesPaneVisible(value) {
    Services.prefs.setBoolPref(VARIABLES_PANE_VISIBLE, value);
    this._variablesVisible = value;
  },

  /**
   * Gets a flag specifying if the debugger should automatically connect to
   * the default host and port number.
   * @return boolean
   */
  get remoteAutoConnect() {
    if (this._autoConnect === undefined) {
      this._autoConnect = Services.prefs.getBoolPref(REMOTE_AUTO_CONNECT);
    }
    return this._autoConnect;
  },

  /**
   * Sets a flag specifying if the debugger should automatically connect to
   * the default host and port number.
   * @param boolean value
   */
  set remoteAutoConnect(value) {
    Services.prefs.setBoolPref(REMOTE_AUTO_CONNECT, value);
    this._autoConnect = value;
  },

  /**
   * Gets a flag specifying if the debugger should show non-enumerable
   * properties and variables in the scope view.
   * @return boolean
   */
  get nonEnumVisible() {
    if (this._nonEnumVisible === undefined) {
      this._nonEnumVisible = Services.prefs.getBoolPref(NON_ENUM_VISIBLE);
    }
    return this._nonEnumVisible;
  },

  /**
   * Sets a flag specifying if the debugger should show non-enumerable
   * properties and variables in the scope view.
   * @param boolean value
   */
  set nonEnumVisible(value) {
    Services.prefs.setBoolPref(NON_ENUM_VISIBLE, value);
    this._nonEnumVisible = value;
  }
};

/**
 * Gets the preferred default remote debugging host.
 * @return string
 */
XPCOMUtils.defineLazyGetter(Prefs, "remoteHost", function() {
  return Services.prefs.getCharPref(REMOTE_HOST);
});

/**
 * Gets the preferred default remote debugging port.
 * @return number
 */
XPCOMUtils.defineLazyGetter(Prefs, "remotePort", function() {
  return Services.prefs.getIntPref(REMOTE_PORT);
});

/**
 * Gets the max number of attempts to reconnect to a remote server.
 * @return number
 */
XPCOMUtils.defineLazyGetter(Prefs, "remoteConnectionRetries", function() {
  return Services.prefs.getIntPref(REMOTE_CONNECTION_RETRIES);
});

/**
 * Gets the remote debugging connection timeout (in milliseconds).
 * @return number
 */
XPCOMUtils.defineLazyGetter(Prefs, "remoteTimeout", function() {
  return Services.prefs.getIntPref(REMOTE_TIMEOUT);
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
