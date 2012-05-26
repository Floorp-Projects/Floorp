/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const FRAME_STEP_CACHE_DURATION = 100; // ms
const DBG_STRINGS_URI = "chrome://browser/locale/devtools/debugger.properties";

Cu.import("resource:///modules/source-editor.jsm");
Cu.import("resource://gre/modules/devtools/dbg-server.jsm");
Cu.import("resource://gre/modules/devtools/dbg-client.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import('resource://gre/modules/Services.jsm');

/**
 * Controls the debugger view by handling the source scripts, the current
 * thread state and thread stack frame cache.
 */
let DebuggerController = {

  /**
   * Makes a few preliminary changes and bindings to the controller.
   */
  init: function() {
    this._startupDebugger = this._startupDebugger.bind(this);
    this._shutdownDebugger = this._shutdownDebugger.bind(this);
    this._onTabNavigated = this._onTabNavigated.bind(this);
    this._onTabDetached = this._onTabDetached.bind(this);

    window.addEventListener("DOMContentLoaded", this._startupDebugger, true);
    window.addEventListener("unload", this._shutdownDebugger, true);
  },

  /**
   * Initializes the debugger view and connects a debugger client to the server.
   */
  _startupDebugger: function DC__startupDebugger() {
    if (this._isInitialized) {
      return;
    }
    this._isInitialized = true;
    window.removeEventListener("DOMContentLoaded", this._startupDebugger, true);

    DebuggerView.initializeEditor();
    DebuggerView.StackFrames.initialize();
    DebuggerView.Properties.initialize();
    DebuggerView.Scripts.initialize();
    DebuggerView.showCloseButton(!this._isRemoteDebugger && !this._isChromeDebugger);

    this.dispatchEvent("Debugger:Loaded");
    this._connect();
  },

  /**
   * Destroys the debugger view, disconnects the debugger client and cleans up
   * any active listeners.
   */
  _shutdownDebugger: function DC__shutdownDebugger() {
    if (this._isDestroyed) {
      return;
    }
    this._isDestroyed = true;
    window.removeEventListener("unload", this._shutdownDebugger, true);

    DebuggerView.destroyEditor();
    DebuggerView.Scripts.destroy();
    DebuggerView.StackFrames.destroy();
    DebuggerView.Properties.destroy();

    DebuggerController.SourceScripts.disconnect();
    DebuggerController.StackFrames.disconnect();
    DebuggerController.ThreadState.disconnect();

    this.dispatchEvent("Debugger:Unloaded");
    this._disconnect();
    this._isChromeDebugger && this._quitApp();
  },

  /**
   * Prepares the hostname and port number for a remote debugger connection
   * and handles connection retries and timeouts.
   *
   * @return boolean true if connection should proceed normally
   */
  _prepareConnection: function DC__prepareConnection() {
    // If we exceeded the total number of connection retries, bail.
    if (this._remoteConnectionTry === Prefs.remoteConnectionRetries) {
      Services.prompt.alert(null,
        L10N.getStr("remoteDebuggerPromptTitle"),
        L10N.getStr("remoteDebuggerConnectionFailedMessage"));
      this.dispatchEvent("Debugger:Close");
      return false;
    }

    // TODO: This is ugly, need to rethink the design for the UI in #751677.
    if (!Prefs.remoteAutoConnect) {
      let prompt = new RemoteDebuggerPrompt();
      let result = prompt.show(!!this._remoteConnectionTimeout);
      if (!result) {
        this.dispatchEvent("Debugger:Close");
        return false;
      }
      Prefs.remoteHost = prompt.uri.host;
      Prefs.remotePort = prompt.uri.port;
    }

    // If this debugger is connecting remotely to a server, we need to check
    // after a while if the connection actually succeeded.
    this._remoteConnectionTry = ++this._remoteConnectionTry || 1;
    this._remoteConnectionTimeout = window.setTimeout(function() {
      // If we couldn't connect to any server yet, try again...
      if (!DebuggerController.activeThread) {
        DebuggerController._connect();
      }
    }, Prefs.remoteTimeout);

    return true;
  },

  /**
   * Initializes a debugger client and connects it to the debugger server,
   * wiring event handlers as necessary.
   */
  _connect: function DC__connect() {
    if (this._isRemoteDebugger) {
      if (!this._prepareConnection()) {
        return;
      }
    }

    let transport = (this._isChromeDebugger || this._isRemoteDebugger)
      ? debuggerSocketConnect(Prefs.remoteHost, Prefs.remotePort)
      : DebuggerServer.connectPipe();

    let client = this.client = new DebuggerClient(transport);

    client.addListener("tabNavigated", this._onTabNavigated);
    client.addListener("tabDetached", this._onTabDetached);

    client.connect(function(aType, aTraits) {
      client.listTabs(function(aResponse) {
        let tab = aResponse.tabs[aResponse.selected];
        this._startDebuggingTab(client, tab);
        this.dispatchEvent("Debugger:Connecting");
      }.bind(this));
    }.bind(this));
  },

  /**
   * Closes the debugger client and removes event handlers as necessary.
   */
  _disconnect: function DC__disconnect() {
    this.client.removeListener("tabNavigated", this._onTabNavigated);
    this.client.removeListener("tabDetached", this._onTabDetached);
    this.client.close();

    this.client = null;
    this.tabClient = null;
    this.activeThread = null;
  },

  /**
   * Starts debugging the current tab. This function is called on each location
   * change in this tab.
   */
  _onTabNavigated: function DC__onTabNavigated(aNotification, aPacket) {
    let client = this.client;

    client.activeThread.detach(function() {
      client.activeTab.detach(function() {
        client.listTabs(function(aResponse) {
          let tab = aResponse.tabs[aResponse.selected];
          this._startDebuggingTab(client, tab);
          this.dispatchEvent("Debugger:Connecting");
        }.bind(this));
      }.bind(this));
    }.bind(this));
  },

  /**
   * Stops debugging the current tab.
   */
  _onTabDetached: function DC__onTabDetached() {
    this.dispatchEvent("Debugger:Close");
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

        DebuggerController.ThreadState.connect(function() {
          DebuggerController.StackFrames.connect(function() {
            DebuggerController.SourceScripts.connect(function() {
              aThreadClient.resume();
            });
          });
        });

      }.bind(this));
    }.bind(this));
  },

  /**
   * Returns true if this is a remote debugger instance.
   * @return boolean
   */
  get _isRemoteDebugger() {
    return window._remoteFlag;
  },

  /**
   * Returns true if this is a chrome debugger instance.
   * @return boolean
   */
  get _isChromeDebugger() {
    return !window.parent.content && !this._isRemoteDebugger;
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
   * @param string aDetail
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

  /**
   * Gets the current thread the client has connected to.
   */
  get activeThread() {
    return DebuggerController.activeThread;
  },

  /**
   * Connect to the current thread client.
   *
   * @param function aCallback
   *        The next function in the initialization sequence.
   */
  connect: function TS_connect(aCallback) {
    this.activeThread.addListener("paused", this._update);
    this.activeThread.addListener("resumed", this._update);
    this.activeThread.addListener("detached", this._update);

    this._update();

    aCallback && aCallback();
  },

  /**
   * Disconnect from the client.
   */
  disconnect: function TS_disconnect() {
    if (!this.activeThread) {
      return;
    }
    this.activeThread.removeListener("paused", this._update);
    this.activeThread.removeListener("resumed", this._update);
    this.activeThread.removeListener("detached", this._update);
  },

  /**
   * Update the UI after a thread state change.
   */
  _update: function TS__update(aEvent) {
    DebuggerView.StackFrames.updateState(this.activeThread.state);
  }
};

/**
 * Keeps the stack frame list up-to-date, using the thread client's stack frame
 * cache.
 */
function StackFrames() {
  this._onPaused = this._onPaused.bind(this);
  this._onResume = this._onResume.bind(this);
  this._onFrames = this._onFrames.bind(this);
  this._onFramesCleared = this._onFramesCleared.bind(this);
  this._afterFramesCleared = this._afterFramesCleared.bind(this);
}

StackFrames.prototype = {

  /**
   * The maximum number of frames allowed to be loaded at a time.
   */
  pageSize: 25,

  /**
   * The currently selected frame depth.
   */
  selectedFrame: null,

  /**
   * Gets the current thread the client has connected to.
   */
  get activeThread() {
    return DebuggerController.activeThread;
  },

  /**
   * Watch the given thread client.
   *
   * @param function aCallback
   *        The next function in the initialization sequence.
   */
  connect: function SF_connect(aCallback) {
    window.addEventListener("Debugger:FetchedVariables", this._onFetchedVars, false);

    this.activeThread.addListener("paused", this._onPaused);
    this.activeThread.addListener("resumed", this._onResume);
    this.activeThread.addListener("framesadded", this._onFrames);
    this.activeThread.addListener("framescleared", this._onFramesCleared);

    this._onFramesCleared();

    aCallback && aCallback();
  },

  /**
   * Disconnect from the client.
   */
  disconnect: function SF_disconnect() {
    window.removeEventListener("Debugger:FetchedVariables", this._onFetchedVars, false);

    if (!this.activeThread) {
      return;
    }
    this.activeThread.removeListener("paused", this._onPaused);
    this.activeThread.removeListener("resumed", this._onResume);
    this.activeThread.removeListener("framesadded", this._onFrames);
    this.activeThread.removeListener("framescleared", this._onFramesCleared);
  },

  /**
   * Handler for the thread client's paused notification.
   */
  _onPaused: function SF__onPaused() {
    this.activeThread.fillFrames(this.pageSize);
  },

  /**
   * Handler for the thread client's resumed notification.
   */
  _onResume: function SF__onResume() {
    DebuggerView.editor.setDebugLocation(-1);
  },

  /**
   * Handler for the thread client's framesadded notification.
   */
  _onFrames: function SF__onFrames() {
    if (!this.activeThread.cachedFrames.length) {
      DebuggerView.StackFrames.emptyText();
      return;
    }
    DebuggerView.StackFrames.empty();

    for each (let frame in this.activeThread.cachedFrames) {
      this._addFrame(frame);
    }
    if (!this.selectedFrame) {
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
    // After each frame step (in, over, out), framescleared is fired, which
    // forces the UI to be emptied and rebuilt on framesadded. Most of the times
    // this is not necessary, and will result in a brief redraw flicker.
    // To avoid it, invalidate the UI only after a short time if necessary.
    window.setTimeout(this._afterFramesCleared, FRAME_STEP_CACHE_DURATION);
    this.selectedFrame = null;
  },

  /**
   * Called soon after the thread client's framescleared notification.
   */
  _afterFramesCleared: function SF__afterFramesCleared() {
    if (!this.activeThread.cachedFrames.length) {
      DebuggerView.StackFrames.emptyText();
      DebuggerView.Properties.localScope.empty();
      DebuggerView.Properties.globalScope.empty();
      DebuggerController.dispatchEvent("Debugger:AfterFramesCleared");
    }
  },

  /**
   * Update the source editor's current debug location based on the selected
   * frame and script.
   */
  updateEditorLocation: function SF_updateEditorLocation() {
    let frame = this.activeThread.cachedFrames[this.selectedFrame];
    if (!frame) {
      return;
    }

    let url = frame.where.url;
    let line = frame.where.line;
    let editor = DebuggerView.editor;

    // Move the editor's caret to the proper line.
    if (DebuggerView.Scripts.isSelected(url) && line) {
      editor.setDebugLocation(line - 1);
    } else {
      editor.setDebugLocation(-1);
    }
  },

  /**
   * Marks the stack frame in the specified depth as selected and updates the
   * properties view with the stack frame's data.
   *
   * @param number aDepth
   *        The depth of the frame in the stack.
   */
  selectFrame: function SF_selectFrame(aDepth) {
    // Deselect any previously highlighted frame.
    if (this.selectedFrame !== null) {
      DebuggerView.StackFrames.unhighlightFrame(this.selectedFrame);
    }

    // Highlight the current frame.
    this.selectedFrame = aDepth;
    DebuggerView.StackFrames.highlightFrame(this.selectedFrame);

    let frame = this.activeThread.cachedFrames[aDepth];
    if (!frame) {
      return;
    }

    let url = frame.where.url;
    let line = frame.where.line;
    let editor = DebuggerView.editor;

    // Move the editor's caret to the proper line.
    if (DebuggerView.Scripts.isSelected(url) && line) {
      editor.setCaretPosition(line - 1);
      editor.setDebugLocation(line - 1);
    }
    else if (DebuggerView.Scripts.contains(url)) {
      DebuggerView.Scripts.selectScript(url);
      editor.setCaretPosition(line - 1);
    }
    else {
      editor.setDebugLocation(-1);
    }

    // Start recording any added variables or properties in any scope.
    DebuggerView.Properties.createHierarchyStore();

    // Display the local variables.
    let localScope = DebuggerView.Properties.localScope;
    localScope.empty();

    // Add "this".
    if (frame.this) {
      let thisVar = localScope.addVar("this");
      thisVar.setGrip({
        type: frame.this.type,
        class: frame.this.class
      });
      this._addExpander(thisVar, frame.this);
    }

    if (frame.environment) {
      // Add nodes for every argument.
      let variables = frame.environment.bindings.arguments;
      for each (let variable in variables) {
        let name = Object.getOwnPropertyNames(variable)[0];
        let paramVar = localScope.addVar(name);
        let paramVal = variable[name].value;
        paramVar.setGrip(paramVal);
        this._addExpander(paramVar, paramVal);
      }

      // Add nodes for every other variable in scope.
      variables = frame.environment.bindings.variables;
      for (let variable in variables) {
        let paramVar = localScope.addVar(variable);
        let paramVal = variables[variable].value;
        paramVar.setGrip(paramVal);
        this._addExpander(paramVar, paramVal);
      }
    }

    // Signal that variables have been fetched.
    DebuggerController.dispatchEvent("Debugger:FetchedVariables");
  },

  /**
   * Called afters variables have been fetched after a frame was selected.
   */
  _onFetchedVars: function SF__onFetchedVars() {
    DebuggerView.Properties.commitHierarchy();
  },

  /**
   * Adds an 'onexpand' callback for a variable, lazily handling the addition of
   * new properties.
   */
  _addExpander: function SF__addExpander(aVar, aObject) {
    // No need for expansion for null and undefined values.
    if (!aVar || !aObject || typeof aObject !== "object" ||
        aObject.type !== "object") {
      return;
    }

    // Force the twisty to show up.
    aVar.forceShowArrow();
    aVar.onexpand = this._addVarProperties.bind(this, aVar, aObject);
  },

  /**
   * Adds properties to a variable in the view. Triggered when a variable is
   * expanded.
   */
  _addVarProperties: function SF__addVarProperties(aVar, aObject) {
    // Retrieve the properties only once.
    if (aVar.fetched) {
      return;
    }

    let objClient = this.activeThread.pauseGrip(aObject);
    objClient.getPrototypeAndProperties(function SF_onProtoAndProps(aResponse) {
      // Add __proto__.
      if (aResponse.prototype.type !== "null") {
        let properties = { "__proto__ ": { value: aResponse.prototype } };
        aVar.addProperties(properties);

        // Expansion handlers must be set after the properties are added.
        this._addExpander(aVar["__proto__ "], aResponse.prototype);
      }

      // Sort all of the properties before adding them, for better UX.
      let properties = {};
      for each (let prop in Object.keys(aResponse.ownProperties).sort()) {
        properties[prop] = aResponse.ownProperties[prop];
      }
      aVar.addProperties(properties);

      // Expansion handlers must be set after the properties are added.
      for (let prop in aResponse.ownProperties) {
        this._addExpander(aVar[prop], aResponse.ownProperties[prop].value);
      }

      aVar.fetched = true;
    }.bind(this));
  },

  /**
   * Adds the specified stack frame to the list.
   *
   * @param Debugger.Frame aFrame
   *        The new frame to add.
   */
  _addFrame: function SF__addFrame(aFrame) {
    let depth = aFrame.depth;
    let label = DebuggerController.SourceScripts._getScriptLabel(aFrame.where.url);

    let startText = this._getFrameTitle(aFrame);
    let endText = label + ":" + aFrame.where.line;

    let frame = DebuggerView.StackFrames.addFrame(depth, startText, endText);
    if (frame) {
      frame.debuggerFrame = aFrame;
    }
  },

  /**
   * Loads more stack frames from the debugger server cache.
   */
  addMoreFrames: function SF_addMoreFrames() {
    this.activeThread.fillFrames(
      this.activeThread.cachedFrames.length + this.pageSize);
  },

  /**
   * Create a textual representation for the stack frame specified, for
   * displaying in the stack frame list.
   *
   * @param Debugger.Frame aFrame
   *        The stack frame to label.
   */
  _getFrameTitle: function SF__getFrameTitle(aFrame) {
    if (aFrame.type == "call") {
      return aFrame["calleeName"] ? aFrame["calleeName"] : "(anonymous)";
    }
    return "(" + aFrame.type + ")";
  },

  /**
   * Evaluate an expression in the context of the selected frame. This is used
   * for modifying the value of variables in scope.
   *
   * @param string aExpression
   *        The expression to evaluate.
   */
  evaluate: function SF_evaluate(aExpression) {
    let frame = this.activeThread.cachedFrames[this.selectedFrame];
    this.activeThread.eval(frame.actor, aExpression);
  }
};

/**
 * Keeps the source script list up-to-date, using the thread client's
 * source script cache.
 */
function SourceScripts() {
  this._onNewScript = this._onNewScript.bind(this);
  this._onScriptsAdded = this._onScriptsAdded.bind(this);
  this._onScriptsCleared = this._onScriptsCleared.bind(this);
  this._onShowScript = this._onShowScript.bind(this);
  this._onLoadSource = this._onLoadSource.bind(this);
  this._onLoadSourceFinished = this._onLoadSourceFinished.bind(this);
}

SourceScripts.prototype = {

  /**
   * A cache containing simplified labels from script urls.
   */
  _labelsCache: {},

  /**
   * Gets the current thread the client has connected to.
   */
  get activeThread() {
    return DebuggerController.activeThread;
  },

  /**
   * Gets the current debugger client.
   */
  get debuggerClient() {
    return DebuggerController.client;
  },

  /**
   * Watch the given thread client.
   *
   * @param function aCallback
   *        The next function in the initialization sequence.
   */
  connect: function SS_connect(aCallback) {
    window.addEventListener("Debugger:LoadSource", this._onLoadSource, false);

    this.debuggerClient.addListener("newScript", this._onNewScript);
    this.activeThread.addListener("scriptsadded", this._onScriptsAdded);
    this.activeThread.addListener("scriptscleared", this._onScriptsCleared);

    this._clearLabelsCache();
    this._onScriptsCleared();

    // Retrieve the list of scripts known to the server from before the client
    // was ready to handle new script notifications.
    this.activeThread.fillScripts();

    aCallback && aCallback();
  },

  /**
   * Disconnect from the client.
   */
  disconnect: function TS_disconnect() {
    window.removeEventListener("Debugger:LoadSource", this._onLoadSource, false);

    if (!this.activeThread) {
      return;
    }
    this.debuggerClient.removeListener("newScript", this._onNewScript);
    this.activeThread.removeListener("scriptsadded", this._onScriptsAdded);
    this.activeThread.removeListener("scriptscleared", this._onScriptsCleared);
  },

  /**
   * Handler for the debugger client's unsolicited newScript notification.
   */
  _onNewScript: function SS__onNewScript(aNotification, aPacket) {
    // Ignore scripts generated from 'clientEvaluate' packets.
    if (aPacket.url == "debugger eval code") {
      return;
    }

    this._addScript({ url: aPacket.url, startLine: aPacket.startLine }, true);
  },

  /**
   * Handler for the thread client's scriptsadded notification.
   */
  _onScriptsAdded: function SS__onScriptsAdded() {
    for each (let script in this.activeThread.cachedScripts) {
      this._addScript(script, false);
    }
    DebuggerView.Scripts.commitScripts();
  },

  /**
   * Handler for the thread client's scriptscleared notification.
   */
  _onScriptsCleared: function SS__onScriptsCleared() {
    DebuggerView.Scripts.empty();
  },

  /**
   * Sets the proper editor mode (JS or HTML) according to the specified
   * content type, or by determining the type from the URL.
   *
   * @param string aUrl
   *        The script URL.
   * @param string aContentType [optional]
   *        The script content type.
   */
  _setEditorMode: function SS__setEditorMode(aUrl, aContentType) {
    if (aContentType) {
      if (/javascript/.test(aContentType)) {
        DebuggerView.editor.setMode(SourceEditor.MODES.JAVASCRIPT);
      } else {
        DebuggerView.editor.setMode(SourceEditor.MODES.HTML);
      }
      return;
    }

    // Use JS mode for files with .js and .jsm extensions.
    if (/\.jsm?$/.test(this._trimUrlQuery(aUrl))) {
      DebuggerView.editor.setMode(SourceEditor.MODES.JAVASCRIPT);
    } else {
      DebuggerView.editor.setMode(SourceEditor.MODES.HTML);
    }
  },

  /**
   * Trims the query part of a url string, if necessary.
   *
   * @param string aUrl
   *        The script url.
   * @return string
   */
  _trimUrlQuery: function SS__trimUrlQuery(aUrl) {
    let q = aUrl.indexOf('?');
    if (q > -1) {
      return aUrl.slice(0, q);
    }
    return aUrl;
  },

  /**
   * Gets the prePath for a script URL.
   *
   * @param string aUrl
   *        The script url.
   * @return string
   *         The script prePath if the url is valid, null otherwise.
   */
  _getScriptPrePath: function SS__getScriptDomain(aUrl) {
    try {
      return Services.io.newURI(aUrl, null, null).prePath + "/";
    } catch (e) {
    }
    return null;
  },

  /**
   * Gets a unique, simplified label from a script url.
   * ex: a). ici://some.address.com/random/subrandom/
   *     b). ni://another.address.org/random/subrandom/page.html
   *     c). san://interesting.address.gro/random/script.js
   *     d). si://interesting.address.moc/random/another/script.js
   * =>
   *     a). subrandom/
   *     b). page.html
   *     c). script.js
   *     d). another/script.js
   *
   * @param string aUrl
   *        The script url.
   * @param string aHref
   *        The content location href to be used. If unspecified, it will
   *        default to the script url prepath.
   * @return string
   *         The simplified label.
   */
  _getScriptLabel: function SS__getScriptLabel(aUrl, aHref) {
    let url = this._trimUrlQuery(aUrl);

    if (this._labelsCache[url]) {
      return this._labelsCache[url];
    }

    let content = window.parent.content;
    let domain = content ? content.location.href : this._getScriptPrePath(aUrl);

    let href = aHref || domain;
    let pathElements = url.split("/");
    let label = pathElements.pop() || (pathElements.pop() + "/");

    // If the label as a leaf name is already present in the scripts list.
    if (DebuggerView.Scripts.containsLabel(label)) {
      label = url.replace(href.substring(0, href.lastIndexOf("/") + 1), "");

      // If the path/to/script is exactly the same, we're in different domains.
      if (DebuggerView.Scripts.containsLabel(label)) {
        label = url;
      }
    }

    return this._labelsCache[url] = label;
  },

  /**
   * Clears the labels cache, populated by SS_getScriptLabel.
   * This should be done every time the content location changes.
   */
  _clearLabelsCache: function SS__clearLabelsCache() {
    this._labelsCache = {};
  },

  /**
   * Add the specified script to the list.
   *
   * @param object aScript
   *        The script object coming from the active thread.
   * @param boolean aForceFlag
   *        True to force the script to be immediately added.
   */
  _addScript: function SS__addScript(aScript, aForceFlag) {
    DebuggerView.Scripts.addScript(
      this._getScriptLabel(aScript.url), aScript, aForceFlag);
  },

  /**
   * Load the editor with the script text if available, otherwise fire an event
   * to load and display the script text.
   *
   * @param object aScript
   *        The script object coming from the active thread.
   * @param object [aOptions]
   *        Additional options for showing the script. Supported options:
   *        - targetLine: place the editor at the given line number.
   */
  showScript: function SS_showScript(aScript, aOptions) {
    if (aScript.loaded) {
      this._onShowScript(aScript, aOptions);
      return;
    }

    let editor = DebuggerView.editor;
    editor.setMode(SourceEditor.MODES.TEXT);
    editor.setText(L10N.getStr("loadingText"));
    editor.resetUndo();

    // Notify that we need to load a script file.
    DebuggerController.dispatchEvent("Debugger:LoadSource", {
      url: aScript.url,
      options: aOptions
    });
  },

  /**
   * Display the script source once it loads.
   *
   * @private
   * @param object aScript
   *        The script object coming from the active thread.
   * @param object aOptions [optional]
   *        Additional options for showing the script. Supported options:
   *        - targetLine: place the editor at the given line number.
   */
  _onShowScript: function SS__onShowScript(aScript, aOptions) {
    aOptions = aOptions || {};

    this._setEditorMode(aScript.url, aScript.contentType);

    let editor = DebuggerView.editor;
    editor.setText(aScript.text);
    editor.resetUndo();

    DebuggerController.Breakpoints.updateEditorBreakpoints();
    DebuggerController.StackFrames.updateEditorLocation();

    // Handle any additional options for showing the script.
    if (aOptions.targetLine) {
      editor.setCaretPosition(aOptions.targetLine - 1);
    }

    // Notify that we shown script file.
    DebuggerController.dispatchEvent("Debugger:ScriptShown", {
      url: aScript.url
    });
  },

  /**
   * Handles notifications to load a source script from the cache or from a
   * local file.
   *
   * XXX: It may be better to use nsITraceableChannel to get to the sources
   * without relying on caching when we can (not for eval, etc.):
   * http://www.softwareishard.com/blog/firebug/nsitraceablechannel-intercept-http-traffic/
   */
  _onLoadSource: function SS__onLoadSource(aEvent) {
    let url = aEvent.detail.url;
    let options = aEvent.detail.options;
    let self = this;

    switch (Services.io.extractScheme(url)) {
      case "file":
      case "chrome":
      case "resource":
        try {
          NetUtil.asyncFetch(url, function onFetch(aStream, aStatus) {
            if (!Components.isSuccessCode(aStatus)) {
              return self._logError(url, aStatus);
            }
            let source = NetUtil.readInputStreamToString(aStream, aStream.available());
            self._onLoadSourceFinished(url, source, null, options);
            aStream.close();
          });
        } catch (ex) {
          return self._logError(url, ex.name);
        }
        break;

      default:
        let channel = Services.io.newChannel(url, null, null);
        let chunks = [];
        let streamListener = {
          onStartRequest: function(aRequest, aContext, aStatusCode) {
            if (!Components.isSuccessCode(aStatusCode)) {
              return self._logError(url, aStatusCode);
            }
          },
          onDataAvailable: function(aRequest, aContext, aStream, aOffset, aCount) {
            chunks.push(NetUtil.readInputStreamToString(aStream, aCount));
          },
          onStopRequest: function(aRequest, aContext, aStatusCode) {
            if (!Components.isSuccessCode(aStatusCode)) {
              return self._logError(url, aStatusCode);
            }
            self._onLoadSourceFinished(
              url, chunks.join(""), channel.contentType, options);
          }
        };

        channel.loadFlags = channel.LOAD_FROM_CACHE;
        channel.asyncOpen(streamListener, null);
        break;
    }
  },

  /**
   * Called when source has been loaded.
   *
   * @private
   * @param string aSourceUrl
   *        The URL of the source script.
   * @param string aSourceText
   *        The text of the source script.
   * @param string aContentType
   *        The content type of the source script.
   * @param object aOptions [optional]
   *        Additional options for showing the script. Supported options:
   *        - targetLine: place the editor at the given line number.
   */
  _onLoadSourceFinished:
  function SS__onLoadSourceFinished(aSourceUrl, aSourceText, aContentType, aOptions) {
    let scripts = document.getElementById("scripts");
    let element = scripts.getElementsByAttribute("value", aSourceUrl)[0];
    let script = element.getUserData("sourceScript");

    script.loaded = true;
    script.text = aSourceText;
    script.contentType = aContentType;
    element.setUserData("sourceScript", script, null);

    this.showScript(script, aOptions);
  },

  /**
   * Log an error message in the error console when a script fails to load.
   *
   * @param string aUrl
   *        The URL of the source script.
   * @param string aStatus
   *        The failure status code.
   */
  _logError: function SS__logError(aUrl, aStatus) {
    Cu.reportError(L10N.getFormatStr("loadingError", [aUrl, aStatus]));
  },
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
   *
   * @private
   * @type boolean
   */
  _skipEditorBreakpointChange: false,

  /**
   * The list of breakpoints in the debugger as tracked by the current
   * debugger instance. This an object where the values are BreakpointActor
   * objects received from the client, while the keys are actor names, for
   * example "conn0.breakpoint3".
   *
   * @type object
   */
  store: {},

  /**
   * Gets the current thread the client has connected to.
   */
  get activeThread() {
    return DebuggerController.ThreadState.activeThread;
  },

  /**
   * Gets the source editor in the debugger view.
   */
  get editor() {
    return DebuggerView.editor;
  },

  /**
   * Sets up the source editor breakpoint handlers.
   */
  initialize: function BP_initialize() {
    this.editor.addEventListener(
      SourceEditor.EVENTS.BREAKPOINT_CHANGE, this._onEditorBreakpointChange);
  },

  /**
   * Removes all currently added breakpoints.
   */
  destroy: function BP_destroy() {
    for each (let breakpoint in this.store) {
      this.removeBreakpoint(breakpoint);
    }
  },

  /**
   * Event handler for breakpoint changes that happen in the editor. This
   * function syncs the breakpoint changes in the editor to those in the
   * debugger.
   *
   * @private
   * @param object aEvent
   *        The SourceEditor.EVENTS.BREAKPOINT_CHANGE event object.
   */
  _onEditorBreakpointChange: function BP__onEditorBreakpointChange(aEvent) {
    if (this._skipEditorBreakpointChange) {
      return;
    }

    aEvent.added.forEach(this._onEditorBreakpointAdd, this);
    aEvent.removed.forEach(this._onEditorBreakpointRemove, this);
  },

  /**
   * Event handler for new breakpoints that come from the editor.
   *
   * @private
   * @param object aBreakpoint
   *        The breakpoint object coming from the editor.
   */
  _onEditorBreakpointAdd: function BP__onEditorBreakpointAdd(aBreakpoint) {
    let url = DebuggerView.Scripts.selected;
    if (!url) {
      return;
    }

    let line = aBreakpoint.line + 1;

    let callback = function(aClient, aError) {
      if (aError) {
        this._skipEditorBreakpointChange = true;
        let result = this.editor.removeBreakpoint(aBreakpoint.line);
        this._skipEditorBreakpointChange = false;
      }
    }.bind(this);
    this.addBreakpoint({ url: url, line: line }, callback, true);
  },

  /**
   * Event handler for breakpoints that are removed from the editor.
   *
   * @private
   * @param object aBreakpoint
   *        The breakpoint object that was removed from the editor.
   */
  _onEditorBreakpointRemove: function BP__onEditorBreakpointRemove(aBreakpoint) {
    let url = DebuggerView.Scripts.selected;
    if (!url) {
      return;
    }

    let line = aBreakpoint.line + 1;

    let breakpoint = this.getBreakpoint(url, line);
    if (breakpoint) {
      this.removeBreakpoint(breakpoint, null, true);
    }
  },

  /**
   * Update the breakpoints in the editor view. This function takes the list of
   * breakpoints in the debugger and adds them back into the editor view. This
   * is invoked when the selected script is changed.
   */
  updateEditorBreakpoints: function BP_updateEditorBreakpoints() {
    let url = DebuggerView.Scripts.selected;
    if (!url) {
      return;
    }

    this._skipEditorBreakpointChange = true;
    for each (let breakpoint in this.store) {
      if (breakpoint.location.url == url) {
        this.editor.addBreakpoint(breakpoint.location.line - 1);
      }
    }
    this._skipEditorBreakpointChange = false;
  },

  /**
   * Add a breakpoint.
   *
   * @param object aLocation
   *        The location where you want the breakpoint. This object must have
   *        two properties:
   *          - url - the URL of the script.
   *          - line - the line number (starting from 1).
   * @param function [aCallback]
   *        Optional function to invoke once the breakpoint is added. The
   *        callback is invoked with two arguments:
   *          - aBreakpointClient - the BreakpointActor client object, if the
   *          breakpoint has been added successfully.
   *          - aResponseError - if there was any error.
   * @param boolean [aNoEditorUpdate=false]
   *        Tells if you want to skip editor updates. Typically the editor is
   *        updated to visually indicate that a breakpoint has been added.
   */
  addBreakpoint:
  function BP_addBreakpoint(aLocation, aCallback, aNoEditorUpdate) {
    let breakpoint = this.getBreakpoint(aLocation.url, aLocation.line);
    if (breakpoint) {
      aCallback && aCallback(breakpoint);
      return;
    }

    this.activeThread.setBreakpoint(aLocation, function(aResponse, aBpClient) {
      if (!aResponse.error) {
        this.store[aBpClient.actor] = aBpClient;

        if (!aNoEditorUpdate) {
          let url = DebuggerView.Scripts.selected;
          if (url == aLocation.url) {
            this._skipEditorBreakpointChange = true;
            this.editor.addBreakpoint(aLocation.line - 1);
            this._skipEditorBreakpointChange = false;
          }
        }
      }

      aCallback && aCallback(aBpClient, aResponse.error);
    }.bind(this));
  },

  /**
   * Remove a breakpoint.
   *
   * @param object aBreakpoint
   *        The breakpoint you want to remove.
   * @param function [aCallback]
   *        Optional function to invoke once the breakpoint is removed. The
   *        callback is invoked with one argument: the breakpoint location
   *        object which holds the url and line properties.
   * @param boolean [aNoEditorUpdate=false]
   *        Tells if you want to skip editor updates. Typically the editor is
   *        updated to visually indicate that a breakpoint has been removed.
   */
  removeBreakpoint:
  function BP_removeBreakpoint(aBreakpoint, aCallback, aNoEditorUpdate) {
    if (!(aBreakpoint.actor in this.store)) {
      aCallback && aCallback(aBreakpoint.location);
      return;
    }

    aBreakpoint.remove(function() {
      delete this.store[aBreakpoint.actor];

      if (!aNoEditorUpdate) {
        let url = DebuggerView.Scripts.selected;
        if (url == aBreakpoint.location.url) {
          this._skipEditorBreakpointChange = true;
          this.editor.removeBreakpoint(aBreakpoint.location.line - 1);
          this._skipEditorBreakpointChange = false;
        }
      }

      aCallback && aCallback(aBreakpoint.location);
    }.bind(this));
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
    for each (let breakpoint in this.store) {
      if (breakpoint.location.url == aUrl && breakpoint.location.line == aLine) {
        return breakpoint;
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

/**
 * Shortcuts for accessing various debugger preferences.
 */
let Prefs = {

  /**
   * Gets a flag specifying if the the debugger should automatically connect to
   * the default host and port number.
   * @return boolean
   */
  get remoteAutoConnect() {
    if (this._autoConn === undefined) {
      this._autoConn = Services.prefs.getBoolPref("devtools.debugger.remote-autoconnect");
    }
    return this._autoConn;
  },

  /**
   * Sets a flag specifying if the the debugger should automatically connect.
   * @param boolean value
   */
  set remoteAutoConnect(value) {
    Services.prefs.setBoolPref("devtools.debugger.remote-autoconnect", value);
    this._autoConn = value;
  }
};

/**
 * Gets the preferred default remote debugging host.
 * @return string
 */
XPCOMUtils.defineLazyGetter(Prefs, "remoteHost", function() {
  return Services.prefs.getCharPref("devtools.debugger.remote-host");
});

/**
 * Gets the preferred default remote debugging port.
 * @return number
 */
XPCOMUtils.defineLazyGetter(Prefs, "remotePort", function() {
  return Services.prefs.getIntPref("devtools.debugger.remote-port");
});

/**
 * Gets the max number of attempts to reconnect to a remote server.
 * @return number
 */
XPCOMUtils.defineLazyGetter(Prefs, "remoteConnectionRetries", function() {
  return Services.prefs.getIntPref("devtools.debugger.remote-connection-retries");
});

/**
 * Gets the remote debugging connection timeout (in milliseconds).
 * @return number
 */
XPCOMUtils.defineLazyGetter(Prefs, "remoteTimeout", function() {
  return Services.prefs.getIntPref("devtools.debugger.remote-timeout");
});

/**
 * Preliminary setup for the DebuggerController object.
 */
DebuggerController.init();
DebuggerController.ThreadState = new ThreadState();
DebuggerController.StackFrames = new StackFrames();
DebuggerController.SourceScripts = new SourceScripts();
DebuggerController.Breakpoints = new Breakpoints();

/**
 * Export some properties to the global scope for easier access in tests.
 */
Object.defineProperty(window, "gClient", {
  get: function() { return DebuggerController.client; }
});

Object.defineProperty(window, "gTabClient", {
  get: function() { return DebuggerController.tabClient; }
});

Object.defineProperty(window, "gThreadClient", {
  get: function() { return DebuggerController.activeThread; }
});
