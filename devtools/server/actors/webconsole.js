/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global XPCNativeWrapper */

const Services = require("Services");
const { Cc, Ci, Cu } = require("chrome");
const { DebuggerServer, ActorPool } = require("devtools/server/main");
const { ThreadActor } = require("devtools/server/actors/thread");
const { ObjectActor } = require("devtools/server/actors/object");
const { LongStringActor } = require("devtools/server/actors/object/long-string");
const { createValueGrip, stringIsLong } = require("devtools/server/actors/object/utils");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const ErrorDocs = require("devtools/server/actors/errordocs");

loader.lazyRequireGetter(this, "NetworkMonitor", "devtools/shared/webconsole/network-monitor", true);
loader.lazyRequireGetter(this, "NetworkMonitorChild", "devtools/shared/webconsole/network-monitor", true);
loader.lazyRequireGetter(this, "ConsoleProgressListener", "devtools/shared/webconsole/network-monitor", true);
loader.lazyRequireGetter(this, "StackTraceCollector", "devtools/shared/webconsole/network-monitor", true);
loader.lazyRequireGetter(this, "JSPropertyProvider", "devtools/shared/webconsole/js-property-provider", true);
loader.lazyRequireGetter(this, "Parser", "resource://devtools/shared/Parser.jsm", true);
loader.lazyRequireGetter(this, "NetUtil", "resource://gre/modules/NetUtil.jsm", true);
loader.lazyRequireGetter(this, "addWebConsoleCommands", "devtools/server/actors/webconsole/utils", true);
loader.lazyRequireGetter(this, "CONSOLE_WORKER_IDS", "devtools/server/actors/webconsole/utils", true);
loader.lazyRequireGetter(this, "WebConsoleUtils", "devtools/server/actors/webconsole/utils", true);
loader.lazyRequireGetter(this, "EnvironmentActor", "devtools/server/actors/environment", true);
loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");

// Overwrite implemented listeners for workers so that we don't attempt
// to load an unsupported module.
if (isWorker) {
  loader.lazyRequireGetter(this, "ConsoleAPIListener", "devtools/server/actors/webconsole/worker-listeners", true);
  loader.lazyRequireGetter(this, "ConsoleServiceListener", "devtools/server/actors/webconsole/worker-listeners", true);
} else {
  loader.lazyRequireGetter(this, "ConsoleAPIListener", "devtools/server/actors/webconsole/listeners", true);
  loader.lazyRequireGetter(this, "ConsoleServiceListener", "devtools/server/actors/webconsole/listeners", true);
  loader.lazyRequireGetter(this, "ConsoleReflowListener", "devtools/server/actors/webconsole/listeners", true);
  loader.lazyRequireGetter(this, "ContentProcessListener", "devtools/server/actors/webconsole/listeners", true);
  loader.lazyRequireGetter(this, "DocumentEventsListener", "devtools/server/actors/webconsole/listeners", true);
}

function isObject(value) {
  return Object(value) === value;
}

/**
 * The WebConsoleActor implements capabilities needed for the Web Console
 * feature.
 *
 * @constructor
 * @param object connection
 *        The connection to the client, DebuggerServerConnection.
 * @param object [parentActor]
 *        Optional, the parent actor.
 */
function WebConsoleActor(connection, parentActor) {
  this.conn = connection;
  this.parentActor = parentActor;

  this._actorPool = new ActorPool(this.conn);
  this.conn.addActorPool(this._actorPool);

  this._prefs = {};

  this.dbg = this.parentActor.makeDebugger();

  this._netEvents = new Map();
  this._networkEventActorsByURL = new Map();
  this._gripDepth = 0;
  this._listeners = new Set();
  this._lastConsoleInputEvaluation = undefined;

  this.objectGrip = this.objectGrip.bind(this);
  this._onWillNavigate = this._onWillNavigate.bind(this);
  this._onChangedToplevelDocument = this._onChangedToplevelDocument.bind(this);
  EventEmitter.on(this.parentActor, "changed-toplevel-document",
            this._onChangedToplevelDocument);
  this._onObserverNotification = this._onObserverNotification.bind(this);
  if (this.parentActor.isRootActor) {
    Services.obs.addObserver(this._onObserverNotification,
                             "last-pb-context-exited");
  }

  this.traits = {
    evaluateJSAsync: true,
    transferredResponseSize: true,
    selectedObjectActor: true, // 44+
  };
}

WebConsoleActor.prototype =
{
  /**
   * Debugger instance.
   *
   * @see jsdebugger.jsm
   */
  dbg: null,

  /**
   * This is used by the ObjectActor to keep track of the depth of grip() calls.
   * @private
   * @type number
   */
  _gripDepth: null,

  /**
   * Actor pool for all of the actors we send to the client.
   * @private
   * @type object
   * @see ActorPool
   */
  _actorPool: null,

  /**
   * Web Console-related preferences.
   * @private
   * @type object
   */
  _prefs: null,

  /**
   * Holds a map between nsIChannel objects and NetworkEventActors for requests
   * created with sendHTTPRequest or found via the network listener.
   *
   * @private
   * @type Map
   */
  _netEvents: null,

  /**
   * Holds a map from URL to NetworkEventActors for requests noticed by the network
   * listener.  Requests are added when they start, so the actor might not yet have all
   * data for the request until it has completed.
   *
   * @private
   * @type Map
   */
  _networkEventActorsByURL: null,

  /**
   * Holds a set of all currently registered listeners.
   *
   * @private
   * @type Set
   */
  _listeners: null,

  /**
   * The debugger server connection instance.
   * @type object
   */
  conn: null,

  /**
   * List of supported features by the console actor.
   * @type object
   */
  traits: null,

  /**
   * The window or sandbox we work with.
   * Note that even if it is named `window` it refers to the current
   * global we are debugging, which can be a Sandbox for addons
   * or browser content toolbox.
   *
   * @type nsIDOMWindow or Sandbox
   */
  get window() {
    if (this.parentActor.isRootActor) {
      return this._getWindowForBrowserConsole();
    }
    return this.parentActor.window;
  },

  /**
   * Get a window to use for the browser console.
   *
   * @private
   * @return nsIDOMWindow
   *         The window to use, or null if no window could be found.
   */
  _getWindowForBrowserConsole: function() {
    // Check if our last used chrome window is still live.
    let window = this._lastChromeWindow && this._lastChromeWindow.get();
    // If not, look for a new one.
    if (!window || window.closed) {
      window = this.parentActor.window;
      if (!window) {
        // Try to find the Browser Console window to use instead.
        window = Services.wm.getMostRecentWindow("devtools:webconsole");
        // We prefer the normal chrome window over the console window,
        // so we'll look for those windows in order to replace our reference.
        let onChromeWindowOpened = () => {
          // We'll look for this window when someone next requests window()
          Services.obs.removeObserver(onChromeWindowOpened, "domwindowopened");
          this._lastChromeWindow = null;
        };
        Services.obs.addObserver(onChromeWindowOpened, "domwindowopened");
      }

      this._handleNewWindow(window);
    }

    return window;
  },

  /**
   * Store a newly found window on the actor to be used in the future.
   *
   * @private
   * @param nsIDOMWindow window
   *        The window to store on the actor (can be null).
   */
  _handleNewWindow: function(window) {
    if (window) {
      if (this._hadChromeWindow) {
        Services.console.logStringMessage("Webconsole context has changed");
      }
      this._lastChromeWindow = Cu.getWeakReference(window);
      this._hadChromeWindow = true;
    } else {
      this._lastChromeWindow = null;
    }
  },

  /**
   * Whether we've been using a window before.
   *
   * @private
   * @type boolean
   */
  _hadChromeWindow: false,

  /**
   * A weak reference to the last chrome window we used to work with.
   *
   * @private
   * @type nsIWeakReference
   */
  _lastChromeWindow: null,

  // The evalWindow is used at the scope for JS evaluation.
  _evalWindow: null,
  get evalWindow() {
    return this._evalWindow || this.window;
  },

  set evalWindow(window) {
    this._evalWindow = window;

    if (!this._progressListenerActive) {
      EventEmitter.on(this.parentActor, "will-navigate", this._onWillNavigate);
      this._progressListenerActive = true;
    }
  },

  /**
   * Flag used to track if we are listening for events from the progress
   * listener of the tab actor. We use the progress listener to clear
   * this.evalWindow on page navigation.
   *
   * @private
   * @type boolean
   */
  _progressListenerActive: false,

  /**
   * The ConsoleServiceListener instance.
   * @type object
   */
  consoleServiceListener: null,

  /**
   * The ConsoleAPIListener instance.
   */
  consoleAPIListener: null,

  /**
   * The NetworkMonitor instance.
   */
  networkMonitor: null,

  /**
   * The NetworkMonitor instance living in the same (child) process.
   */
  networkMonitorChild: null,

  /**
   * The ConsoleProgressListener instance.
   */
  consoleProgressListener: null,

  /**
   * The ConsoleReflowListener instance.
   */
  consoleReflowListener: null,

  /**
   * The Web Console Commands names cache.
   * @private
   * @type array
   */
  _webConsoleCommandsCache: null,

  actorPrefix: "console",

  get globalDebugObject() {
    return this.parentActor.threadActor.globalDebugObject;
  },

  grip: function() {
    return { actor: this.actorID };
  },

  hasNativeConsoleAPI: function(window) {
    if (isWorker) {
      // Can't use XPCNativeWrapper as a way to check for console API in workers
      return true;
    }

    let isNative = false;
    try {
      // We are very explicitly examining the "console" property of
      // the non-Xrayed object here.
      let console = window.wrappedJSObject.console;
      isNative = new XPCNativeWrapper(console).IS_NATIVE_CONSOLE;
    } catch (ex) {
      // ignored
    }
    return isNative;
  },

  _findProtoChain: ThreadActor.prototype._findProtoChain,
  _removeFromProtoChain: ThreadActor.prototype._removeFromProtoChain,

  /**
   * Destroy the current WebConsoleActor instance.
   */
  destroy() {
    if (this.consoleServiceListener) {
      this.consoleServiceListener.destroy();
      this.consoleServiceListener = null;
    }
    if (this.consoleAPIListener) {
      this.consoleAPIListener.destroy();
      this.consoleAPIListener = null;
    }
    if (this.networkMonitor) {
      this.networkMonitor.destroy();
      this.networkMonitor = null;
    }
    if (this.networkMonitorChild) {
      this.networkMonitorChild.destroy();
      this.networkMonitorChild = null;
    }
    if (this.stackTraceCollector) {
      this.stackTraceCollector.destroy();
      this.stackTraceCollector = null;
    }
    if (this.consoleProgressListener) {
      this.consoleProgressListener.destroy();
      this.consoleProgressListener = null;
    }
    if (this.consoleReflowListener) {
      this.consoleReflowListener.destroy();
      this.consoleReflowListener = null;
    }
    if (this.contentProcessListener) {
      this.contentProcessListener.destroy();
      this.contentProcessListener = null;
    }

    EventEmitter.off(this.parentActor, "changed-toplevel-document",
               this._onChangedToplevelDocument);

    this.conn.removeActorPool(this._actorPool);

    if (this.parentActor.isRootActor) {
      Services.obs.removeObserver(this._onObserverNotification,
                                  "last-pb-context-exited");
    }

    this._actorPool = null;
    this._webConsoleCommandsCache = null;
    this._lastConsoleInputEvaluation = null;
    this._evalWindow = null;
    this._netEvents.clear();
    this.dbg.enabled = false;
    this.dbg = null;
    this.conn = null;
  },

  /**
   * Create and return an environment actor that corresponds to the provided
   * Debugger.Environment. This is a straightforward clone of the ThreadActor's
   * method except that it stores the environment actor in the web console
   * actor's pool.
   *
   * @param Debugger.Environment environment
   *        The lexical environment we want to extract.
   * @return The EnvironmentActor for |environment| or |undefined| for host
   *         functions or functions scoped to a non-debuggee global.
   */
  createEnvironmentActor: function(environment) {
    if (!environment) {
      return undefined;
    }

    if (environment.actor) {
      return environment.actor;
    }

    let actor = new EnvironmentActor(environment, this);
    this._actorPool.addActor(actor);
    environment.actor = actor;

    return actor;
  },

  /**
   * Create a grip for the given value.
   *
   * @param mixed value
   * @return object
   */
  createValueGrip: function(value) {
    return createValueGrip(value, this._actorPool, this.objectGrip);
  },

  /**
   * Make a debuggee value for the given value.
   *
   * @param mixed value
   *        The value you want to get a debuggee value for.
   * @param boolean useObjectGlobal
   *        If |true| the object global is determined and added as a debuggee,
   *        otherwise |this.window| is used when makeDebuggeeValue() is invoked.
   * @return object
   *         Debuggee value for |value|.
   */
  makeDebuggeeValue: function(value, useObjectGlobal) {
    if (useObjectGlobal && isObject(value)) {
      try {
        let global = Cu.getGlobalForObject(value);
        let dbgGlobal = this.dbg.makeGlobalObjectReference(global);
        return dbgGlobal.makeDebuggeeValue(value);
      } catch (ex) {
        // The above can throw an exception if value is not an actual object
        // or 'Object in compartment marked as invisible to Debugger'
      }
    }
    let dbgGlobal = this.dbg.makeGlobalObjectReference(this.window);
    return dbgGlobal.makeDebuggeeValue(value);
  },

  /**
   * Create a grip for the given object.
   *
   * @param object object
   *        The object you want.
   * @param object pool
   *        An ActorPool where the new actor instance is added.
   * @param object
   *        The object grip.
   */
  objectGrip: function(object, pool) {
    let actor = new ObjectActor(object, {
      getGripDepth: () => this._gripDepth,
      incrementGripDepth: () => this._gripDepth++,
      decrementGripDepth: () => this._gripDepth--,
      createValueGrip: v => this.createValueGrip(v),
      sources: () => DevToolsUtils.reportException("WebConsoleActor",
        Error("sources not yet implemented")),
      createEnvironmentActor: (env) => this.createEnvironmentActor(env),
      getGlobalDebugObject: () => this.globalDebugObject
    });
    pool.addActor(actor);
    return actor.grip();
  },

  /**
   * Create a grip for the given string.
   *
   * @param string string
   *        The string you want to create the grip for.
   * @param object pool
   *        An ActorPool where the new actor instance is added.
   * @return object
   *         A LongStringActor object that wraps the given string.
   */
  longStringGrip: function(string, pool) {
    let actor = new LongStringActor(string);
    pool.addActor(actor);
    return actor.grip();
  },

  /**
   * Create a long string grip if needed for the given string.
   *
   * @private
   * @param string string
   *        The string you want to create a long string grip for.
   * @return string|object
   *         A string is returned if |string| is not a long string.
   *         A LongStringActor grip is returned if |string| is a long string.
   */
  _createStringGrip: function(string) {
    if (string && stringIsLong(string)) {
      return this.longStringGrip(string, this._actorPool);
    }
    return string;
  },

  /**
   * Get an object actor by its ID.
   *
   * @param string actorID
   * @return object
   */
  getActorByID: function(actorID) {
    return this._actorPool.get(actorID);
  },

  /**
   * Release an actor.
   *
   * @param object actor
   *        The actor instance you want to release.
   */
  releaseActor: function(actor) {
    this._actorPool.removeActor(actor);
  },

  /**
   * Returns the latest web console input evaluation.
   * This is undefined if no evaluations have been completed.
   *
   * @return object
   */
  getLastConsoleInputEvaluation: function() {
    return this._lastConsoleInputEvaluation;
  },

  /**
   * This helper is used by the WebExtensionInspectedWindowActor to
   * inspect an object in the developer toolbox.
   */
  inspectObject(dbgObj, inspectFromAnnotation) {
    this.conn.sendActorEvent(this.actorID, "inspectObject", {
      objectActor: this.createValueGrip(dbgObj),
      inspectFromAnnotation,
    });
  },

  // Request handlers for known packet types.

  /**
   * Handler for the "startListeners" request.
   *
   * @param object request
   *        The JSON request object received from the Web Console client.
   * @return object
   *         The response object which holds the startedListeners array.
   */
  onStartListeners: function(request) {
    let startedListeners = [];
    let window = !this.parentActor.isRootActor ? this.window : null;
    let messageManager = null;

    // Check if the actor is running in a child process (but only if
    // Services.appinfo exists, to prevent onStartListeners to fail
    // when the target is a Worker).
    let processBoundary = Services.appinfo && (
      Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT
    );

    // Retrieve a message manager from the parent actor if this actor is
    // not currently running in the main process.
    if (processBoundary) {
      messageManager = this.parentActor.messageManager;
    }

    while (request.listeners.length > 0) {
      let listener = request.listeners.shift();
      switch (listener) {
        case "PageError":
          // Workers don't support this message type yet
          if (isWorker) {
            break;
          }
          if (!this.consoleServiceListener) {
            this.consoleServiceListener =
              new ConsoleServiceListener(window, this);
            this.consoleServiceListener.init();
          }
          startedListeners.push(listener);
          break;
        case "ConsoleAPI":
          if (!this.consoleAPIListener) {
            // Create the consoleAPIListener
            // (and apply the filtering options defined in the parent actor).
            this.consoleAPIListener = new ConsoleAPIListener(
              window, this, this.parentActor.consoleAPIListenerOptions);
            this.consoleAPIListener.init();
          }
          startedListeners.push(listener);
          break;
        case "NetworkActivity":
          // Workers don't support this message type
          if (isWorker) {
            break;
          }
          if (!this.networkMonitor) {
            // Create a StackTraceCollector that's going to be shared both by
            // the NetworkMonitorChild (getting messages about requests from
            // parent) and by the NetworkMonitor that directly watches service
            // workers requests.
            this.stackTraceCollector = new StackTraceCollector({ window });
            this.stackTraceCollector.init();

            if (messageManager && processBoundary) {
              // Start a network monitor in the parent process to listen to
              // most requests than happen in parent
              this.networkMonitor =
                new NetworkMonitorChild(this.parentActor.outerWindowID,
                                        messageManager, this.conn, this);
              this.networkMonitor.init();
              // Spawn also one in the child to listen to service workers
              this.networkMonitorChild = new NetworkMonitor({ window }, this);
              this.networkMonitorChild.init();
            } else {
              this.networkMonitor = new NetworkMonitor({ window }, this);
              this.networkMonitor.init();
            }
          }
          startedListeners.push(listener);
          break;
        case "FileActivity":
          // Workers don't support this message type
          if (isWorker) {
            break;
          }
          if (this.window instanceof Ci.nsIDOMWindow) {
            if (!this.consoleProgressListener) {
              this.consoleProgressListener =
                new ConsoleProgressListener(this.window, this);
            }
            this.consoleProgressListener.startMonitor(this.consoleProgressListener
                                                      .MONITOR_FILE_ACTIVITY);
            startedListeners.push(listener);
          }
          break;
        case "ReflowActivity":
          // Workers don't support this message type
          if (isWorker) {
            break;
          }
          if (!this.consoleReflowListener) {
            this.consoleReflowListener =
              new ConsoleReflowListener(this.window, this);
          }
          startedListeners.push(listener);
          break;
        case "ContentProcessMessages":
          // Workers don't support this message type
          if (isWorker) {
            break;
          }
          if (!this.contentProcessListener) {
            this.contentProcessListener = new ContentProcessListener(this);
          }
          startedListeners.push(listener);
          break;
        case "DocumentEvents":
          // Workers don't support this message type
          if (isWorker) {
            break;
          }
          if (!this.documentEventsListener) {
            this.documentEventsListener = new DocumentEventsListener(this);
          }
          startedListeners.push(listener);
          break;
      }
    }

    // Update the live list of running listeners
    startedListeners.forEach(this._listeners.add, this._listeners);

    return {
      startedListeners: startedListeners,
      nativeConsoleAPI: this.hasNativeConsoleAPI(this.window),
      traits: this.traits,
    };
  },

  /**
   * Handler for the "stopListeners" request.
   *
   * @param object request
   *        The JSON request object received from the Web Console client.
   * @return object
   *         The response packet to send to the client: holds the
   *         stoppedListeners array.
   */
  onStopListeners: function(request) {
    let stoppedListeners = [];

    // If no specific listeners are requested to be detached, we stop all
    // listeners.
    let toDetach = request.listeners ||
      ["PageError", "ConsoleAPI", "NetworkActivity",
       "FileActivity", "ContentProcessMessages"];

    while (toDetach.length > 0) {
      let listener = toDetach.shift();
      switch (listener) {
        case "PageError":
          if (this.consoleServiceListener) {
            this.consoleServiceListener.destroy();
            this.consoleServiceListener = null;
          }
          stoppedListeners.push(listener);
          break;
        case "ConsoleAPI":
          if (this.consoleAPIListener) {
            this.consoleAPIListener.destroy();
            this.consoleAPIListener = null;
          }
          stoppedListeners.push(listener);
          break;
        case "NetworkActivity":
          if (this.networkMonitor) {
            this.networkMonitor.destroy();
            this.networkMonitor = null;
          }
          if (this.networkMonitorChild) {
            this.networkMonitorChild.destroy();
            this.networkMonitorChild = null;
          }
          if (this.stackTraceCollector) {
            this.stackTraceCollector.destroy();
            this.stackTraceCollector = null;
          }
          stoppedListeners.push(listener);
          break;
        case "FileActivity":
          if (this.consoleProgressListener) {
            this.consoleProgressListener.stopMonitor(this.consoleProgressListener
                                                     .MONITOR_FILE_ACTIVITY);
            this.consoleProgressListener = null;
          }
          stoppedListeners.push(listener);
          break;
        case "ReflowActivity":
          if (this.consoleReflowListener) {
            this.consoleReflowListener.destroy();
            this.consoleReflowListener = null;
          }
          stoppedListeners.push(listener);
          break;
        case "ContentProcessMessages":
          if (this.contentProcessListener) {
            this.contentProcessListener.destroy();
            this.contentProcessListener = null;
          }
          stoppedListeners.push(listener);
          break;
        case "DocumentEvents":
          if (this.documentEventsListener) {
            this.documentEventsListener.destroy();
            this.documentEventsListener = null;
          }
          stoppedListeners.push(listener);
          break;
      }
    }

    // Update the live list of running listeners
    stoppedListeners.forEach(this._listeners.delete, this._listeners);

    return { stoppedListeners: stoppedListeners };
  },

  /**
   * Handler for the "getCachedMessages" request. This method sends the cached
   * error messages and the window.console API calls to the client.
   *
   * @param object request
   *        The JSON request object received from the Web Console client.
   * @return object
   *         The response packet to send to the client: it holds the cached
   *         messages array.
   */
  onGetCachedMessages: function(request) {
    let types = request.messageTypes;
    if (!types) {
      return {
        error: "missingParameter",
        message: "The messageTypes parameter is missing.",
      };
    }

    let messages = [];

    while (types.length > 0) {
      let type = types.shift();
      switch (type) {
        case "ConsoleAPI": {
          if (!this.consoleAPIListener) {
            break;
          }

          // See `window` definition. It isn't always a DOM Window.
          let winStartTime = this.window && this.window.performance ?
            this.window.performance.timing.navigationStart : 0;

          let cache = this.consoleAPIListener
                      .getCachedMessages(!this.parentActor.isRootActor);
          cache.forEach((cachedMessage) => {
            // Filter out messages that came from a ServiceWorker but happened
            // before the page was requested.
            if (cachedMessage.innerID === "ServiceWorker" &&
                winStartTime > cachedMessage.timeStamp) {
              return;
            }

            let message = this.prepareConsoleMessageForRemote(cachedMessage);
            message._type = type;
            messages.push(message);
          });
          break;
        }
        case "PageError": {
          if (!this.consoleServiceListener) {
            break;
          }
          let cache = this.consoleServiceListener
                      .getCachedMessages(!this.parentActor.isRootActor);
          cache.forEach((cachedMessage) => {
            let message = null;
            if (cachedMessage instanceof Ci.nsIScriptError) {
              message = this.preparePageErrorForRemote(cachedMessage);
              message._type = type;
            } else {
              message = {
                _type: "LogMessage",
                message: this._createStringGrip(cachedMessage.message),
                timeStamp: cachedMessage.timeStamp,
              };
            }
            messages.push(message);
          });
          break;
        }
      }
    }

    return {
      from: this.actorID,
      messages: messages,
    };
  },

  /**
   * Handler for the "evaluateJSAsync" request. This method evaluates the given
   * JavaScript string and sends back a packet with a unique ID.
   * The result will be returned later as an unsolicited `evaluationResult`,
   * that can be associated back to this request via the `resultID` field.
   *
   * @param object request
   *        The JSON request object received from the Web Console client.
   * @return object
   *         The response packet to send to with the unique id in the
   *         `resultID` field.
   */
  onEvaluateJSAsync: function(request) {
    // We want to be able to run console commands without waiting
    // for the first to return (see Bug 1088861).

    // First, send a response packet with the id only.
    let resultID = Date.now();
    this.conn.send({
      from: this.actorID,
      resultID: resultID
    });

    // Then, execute the script that may pause.
    let response = this.onEvaluateJS(request);
    response.resultID = resultID;

    // Finally, send an unsolicited evaluationResult packet with
    // the normal return value
    this.conn.sendActorEvent(this.actorID, "evaluationResult", response);
  },

  /**
   * Handler for the "evaluateJS" request. This method evaluates the given
   * JavaScript string and sends back the result.
   *
   * @param object request
   *        The JSON request object received from the Web Console client.
   * @return object
   *         The evaluation response packet.
   */
  onEvaluateJS: function(request) {
    let input = request.text;
    let timestamp = Date.now();

    let evalOptions = {
      bindObjectActor: request.bindObjectActor,
      frameActor: request.frameActor,
      url: request.url,
      selectedNodeActor: request.selectedNodeActor,
      selectedObjectActor: request.selectedObjectActor,
    };

    let evalInfo = this.evalWithDebugger(input, evalOptions);
    let evalResult = evalInfo.result;
    let helperResult = evalInfo.helperResult;

    let result, errorDocURL, errorMessage, errorNotes = null, errorGrip = null,
      frame = null;
    if (evalResult) {
      if ("return" in evalResult) {
        result = evalResult.return;
      } else if ("yield" in evalResult) {
        result = evalResult.yield;
      } else if ("throw" in evalResult) {
        let error = evalResult.throw;

        errorGrip = this.createValueGrip(error);

        errorMessage = String(error);
        if (typeof error === "object" && error !== null) {
          try {
            errorMessage = DevToolsUtils.callPropertyOnObject(error, "toString");
          } catch (e) {
            // If the debuggee is not allowed to access the "toString" property
            // of the error object, calling this property from the debuggee's
            // compartment will fail. The debugger should show the error object
            // as it is seen by the debuggee, so this behavior is correct.
            //
            // Unfortunately, we have at least one test that assumes calling the
            // "toString" property of an error object will succeed if the
            // debugger is allowed to access it, regardless of whether the
            // debuggee is allowed to access it or not.
            //
            // To accomodate these tests, if calling the "toString" property
            // from the debuggee compartment fails, we rewrap the error object
            // in the debugger's compartment, and then call the "toString"
            // property from there.
            if (typeof error.unsafeDereference === "function") {
              errorMessage = error.unsafeDereference().toString();
            }
          }
        }

        // It is possible that we won't have permission to unwrap an
        // object and retrieve its errorMessageName.
        try {
          errorDocURL = ErrorDocs.GetURL(error);
        } catch (ex) {
          // ignored
        }

        try {
          let line = error.errorLineNumber;
          let column = error.errorColumnNumber;

          if (typeof line === "number" && typeof column === "number") {
            // Set frame only if we have line/column numbers.
            frame = {
              source: "debugger eval code",
              line,
              column
            };
          }
        } catch (ex) {
          // ignored
        }

        try {
          let notes = error.errorNotes;
          if (notes && notes.length) {
            errorNotes = [];
            for (let note of notes) {
              errorNotes.push({
                messageBody: this._createStringGrip(note.message),
                frame: {
                  source: note.fileName,
                  line: note.lineNumber,
                  column: note.columnNumber,
                }
              });
            }
          }
        } catch (ex) {
          // ignored
        }
      }
    }

    // If a value is encountered that the debugger server doesn't support yet,
    // the console should remain functional.
    let resultGrip;
    try {
      resultGrip = this.createValueGrip(result);
    } catch (e) {
      errorMessage = e;
    }

    this._lastConsoleInputEvaluation = result;

    return {
      from: this.actorID,
      input: input,
      result: resultGrip,
      timestamp: timestamp,
      exception: errorGrip,
      exceptionMessage: this._createStringGrip(errorMessage),
      exceptionDocURL: errorDocURL,
      frame,
      helperResult: helperResult,
      notes: errorNotes,
    };
  },

  /**
   * The Autocomplete request handler.
   *
   * @param object request
   *        The request message - what input to autocomplete.
   * @return object
   *         The response message - matched properties.
   */
  onAutocomplete: function(request) {
    let frameActorId = request.frameActor;
    let dbgObject = null;
    let environment = null;
    let hadDebuggee = false;

    // This is the case of the paused debugger
    if (frameActorId) {
      let frameActor = this.conn.getActor(frameActorId);
      try {
        // Need to try/catch since accessing frame.environment
        // can throw "Debugger.Frame is not live"
        let frame = frameActor.frame;
        environment = frame.environment;
      } catch (e) {
        DevToolsUtils.reportException("onAutocomplete",
          Error("The frame actor was not found: " + frameActorId));
      }
    } else {
      // This is the general case (non-paused debugger)
      hadDebuggee = this.dbg.hasDebuggee(this.evalWindow);
      dbgObject = this.dbg.addDebuggee(this.evalWindow);
    }

    let result = JSPropertyProvider(dbgObject, environment, request.text,
                                    request.cursor, frameActorId) || {};

    if (!hadDebuggee && dbgObject) {
      this.dbg.removeDebuggee(this.evalWindow);
    }

    let matches = result.matches || [];
    let reqText = request.text.substr(0, request.cursor);

    // We consider '$' as alphanumerc because it is used in the names of some
    // helper functions.
    let lastNonAlphaIsDot = /[.][a-zA-Z0-9$]*$/.test(reqText);
    if (!lastNonAlphaIsDot) {
      if (!this._webConsoleCommandsCache) {
        let helpers = {
          sandbox: Object.create(null)
        };
        addWebConsoleCommands(helpers);
        this._webConsoleCommandsCache =
          Object.getOwnPropertyNames(helpers.sandbox);
      }
      matches = matches.concat(this._webConsoleCommandsCache
          .filter(n => n.startsWith(result.matchProp)));
    }

    return {
      from: this.actorID,
      matches: matches.sort(),
      matchProp: result.matchProp,
    };
  },

  /**
   * The "clearMessagesCache" request handler.
   */
  onClearMessagesCache: function() {
    // TODO: Bug 717611 - Web Console clear button does not clear cached errors
    let windowId = !this.parentActor.isRootActor ?
                   WebConsoleUtils.getInnerWindowId(this.window) : null;
    let ConsoleAPIStorage = Cc["@mozilla.org/consoleAPI-storage;1"]
                              .getService(Ci.nsIConsoleAPIStorage);
    ConsoleAPIStorage.clearEvents(windowId);

    CONSOLE_WORKER_IDS.forEach((id) => {
      ConsoleAPIStorage.clearEvents(id);
    });

    if (this.parentActor.isRootActor) {
      Services.console.logStringMessage(null); // for the Error Console
      Services.console.reset();
    }
    return {};
  },

  /**
   * The "getPreferences" request handler.
   *
   * @param object request
   *        The request message - which preferences need to be retrieved.
   * @return object
   *         The response message - a { key: value } object map.
   */
  onGetPreferences: function(request) {
    let prefs = Object.create(null);
    for (let key of request.preferences) {
      prefs[key] = this._prefs[key];
    }
    return { preferences: prefs };
  },

  /**
   * The "setPreferences" request handler.
   *
   * @param object request
   *        The request message - which preferences need to be updated.
   */
  onSetPreferences: function(request) {
    for (let key in request.preferences) {
      this._prefs[key] = request.preferences[key];

      if (this.networkMonitor) {
        if (key == "NetworkMonitor.saveRequestAndResponseBodies") {
          this.networkMonitor.saveRequestAndResponseBodies = this._prefs[key];
          if (this.networkMonitorChild) {
            this.networkMonitorChild.saveRequestAndResponseBodies =
              this._prefs[key];
          }
        } else if (key == "NetworkMonitor.throttleData") {
          this.networkMonitor.throttleData = this._prefs[key];
          if (this.networkMonitorChild) {
            this.networkMonitorChild.throttleData = this._prefs[key];
          }
        }
      }
    }
    return { updated: Object.keys(request.preferences) };
  },

  // End of request handlers.

  /**
   * Create an object with the API we expose to the Web Console during
   * JavaScript evaluation.
   * This object inherits properties and methods from the Web Console actor.
   *
   * @private
   * @param object debuggerGlobal
   *        A Debugger.Object that wraps a content global. This is used for the
   *        Web Console Commands.
   * @return object
   *         The same object as |this|, but with an added |sandbox| property.
   *         The sandbox holds methods and properties that can be used as
   *         bindings during JS evaluation.
   */
  _getWebConsoleCommands: function(debuggerGlobal) {
    let helpers = {
      window: this.evalWindow,
      chromeWindow: this.chromeWindow.bind(this),
      makeDebuggeeValue: debuggerGlobal.makeDebuggeeValue.bind(debuggerGlobal),
      createValueGrip: this.createValueGrip.bind(this),
      sandbox: Object.create(null),
      helperResult: null,
      consoleActor: this,
    };
    addWebConsoleCommands(helpers);

    let evalWindow = this.evalWindow;
    function maybeExport(obj, name) {
      if (typeof obj[name] != "function") {
        return;
      }

      // By default, chrome-implemented functions that are exposed to content
      // refuse to accept arguments that are cross-origin for the caller. This
      // is generally the safe thing, but causes problems for certain console
      // helpers like cd(), where we users sometimes want to pass a cross-origin
      // window. To circumvent this restriction, we use exportFunction along
      // with a special option designed for this purpose. See bug 1051224.
      obj[name] =
        Cu.exportFunction(obj[name], evalWindow, { allowCrossOriginArguments: true });
    }
    for (let name in helpers.sandbox) {
      let desc = Object.getOwnPropertyDescriptor(helpers.sandbox, name);

      // Workers don't have access to Cu so won't be able to exportFunction.
      if (!isWorker) {
        maybeExport(desc, "get");
        maybeExport(desc, "set");
        maybeExport(desc, "value");
      }
      if (desc.value) {
        // Make sure the helpers can be used during eval.
        desc.value = debuggerGlobal.makeDebuggeeValue(desc.value);
      }
      Object.defineProperty(helpers.sandbox, name, desc);
    }
    return helpers;
  },

  /**
   * Evaluates a string using the debugger API.
   *
   * To allow the variables view to update properties from the Web Console we
   * provide the "bindObjectActor" mechanism: the Web Console tells the
   * ObjectActor ID for which it desires to evaluate an expression. The
   * Debugger.Object pointed at by the actor ID is bound such that it is
   * available during expression evaluation (executeInGlobalWithBindings()).
   *
   * Example:
   *   _self['foobar'] = 'test'
   * where |_self| refers to the desired object.
   *
   * The |frameActor| property allows the Web Console client to provide the
   * frame actor ID, such that the expression can be evaluated in the
   * user-selected stack frame.
   *
   * For the above to work we need the debugger and the Web Console to share
   * a connection, otherwise the Web Console actor will not find the frame
   * actor.
   *
   * The Debugger.Frame comes from the jsdebugger's Debugger instance, which
   * is different from the Web Console's Debugger instance. This means that
   * for evaluation to work, we need to create a new instance for the Web
   * Console Commands helpers - they need to be Debugger.Objects coming from the
   * jsdebugger's Debugger instance.
   *
   * When |bindObjectActor| is used objects can come from different iframes,
   * from different domains. To avoid permission-related errors when objects
   * come from a different window, we also determine the object's own global,
   * such that evaluation happens in the context of that global. This means that
   * evaluation will happen in the object's iframe, rather than the top level
   * window.
   *
   * @param string string
   *        String to evaluate.
   * @param object [options]
   *        Options for evaluation:
   *        - bindObjectActor: the ObjectActor ID to use for evaluation.
   *          |evalWithBindings()| will be called with one additional binding:
   *          |_self| which will point to the Debugger.Object of the given
   *          ObjectActor.
   *        - selectedObjectActor: Like bindObjectActor, but executes with the
   *          top level window as the global.
   *        - frameActor: the FrameActor ID to use for evaluation. The given
   *        debugger frame is used for evaluation, instead of the global window.
   *        - selectedNodeActor: the NodeActor ID of the currently selected node
   *        in the Inspector (or null, if there is no selection). This is used
   *        for helper functions that make reference to the currently selected
   *        node, like $0.
   *         - url: the url to evaluate the script as. Defaults to
   *         "debugger eval code".
   * @return object
   *         An object that holds the following properties:
   *         - dbg: the debugger where the string was evaluated.
   *         - frame: (optional) the frame where the string was evaluated.
   *         - window: the Debugger.Object for the global where the string was
   *         evaluated.
   *         - result: the result of the evaluation.
   *         - helperResult: any result coming from a Web Console commands
   *         function.
   */
  /* eslint-disable complexity */
  evalWithDebugger: function(string, options = {}) {
    let trimmedString = string.trim();
    // The help function needs to be easy to guess, so we make the () optional.
    if (trimmedString == "help" || trimmedString == "?") {
      string = "help()";
    }

    // Add easter egg for console.mihai().
    if (trimmedString == "console.mihai()" || trimmedString == "console.mihai();") {
      string = "\"http://incompleteness.me/blog/2015/02/09/console-dot-mihai/\"";
    }

    // Find the Debugger.Frame of the given FrameActor.
    let frame = null, frameActor = null;
    if (options.frameActor) {
      frameActor = this.conn.getActor(options.frameActor);
      if (frameActor) {
        frame = frameActor.frame;
      } else {
        DevToolsUtils.reportException("evalWithDebugger",
          Error("The frame actor was not found: " + options.frameActor));
      }
    }

    // If we've been given a frame actor in whose scope we should evaluate the
    // expression, be sure to use that frame's Debugger (that is, the JavaScript
    // debugger's Debugger) for the whole operation, not the console's Debugger.
    // (One Debugger will treat a different Debugger's Debugger.Object instances
    // as ordinary objects, not as references to be followed, so mixing
    // debuggers causes strange behaviors.)
    let dbg = frame ? frameActor.threadActor.dbg : this.dbg;
    let dbgWindow = dbg.makeGlobalObjectReference(this.evalWindow);

    // If we have an object to bind to |_self|, create a Debugger.Object
    // referring to that object, belonging to dbg.
    let bindSelf = null;
    if (options.bindObjectActor || options.selectedObjectActor) {
      let objActor = this.getActorByID(options.bindObjectActor ||
                                       options.selectedObjectActor);
      if (objActor) {
        let jsVal = objActor.rawValue();

        if (isObject(jsVal)) {
          // If we use the makeDebuggeeValue method of jsVal's own global, then
          // we'll get a D.O that sees jsVal as viewed from its own compartment -
          // that is, without wrappers. The evalWithBindings call will then wrap
          // jsVal appropriately for the evaluation compartment.
          bindSelf = dbgWindow.makeDebuggeeValue(jsVal);
          if (options.bindObjectActor) {
            let global = Cu.getGlobalForObject(jsVal);
            try {
              let _dbgWindow = dbg.makeGlobalObjectReference(global);
              dbgWindow = _dbgWindow;
            } catch (err) {
              // The above will throw if `global` is invisible to debugger.
            }
          }
        } else {
          bindSelf = jsVal;
        }
      }
    }

    // Get the Web Console commands for the given debugger window.
    let helpers = this._getWebConsoleCommands(dbgWindow);
    let bindings = helpers.sandbox;
    if (bindSelf) {
      bindings._self = bindSelf;
    }

    if (options.selectedNodeActor) {
      let actor = this.conn.getActor(options.selectedNodeActor);
      if (actor) {
        helpers.selectedNode = actor.rawNode;
      }
    }

    // Check if the Debugger.Frame or Debugger.Object for the global include
    // $ or $$. We will not overwrite these functions with the Web Console
    // commands.
    let found$ = false, found$$ = false;
    if (frame) {
      let env = frame.environment;
      if (env) {
        found$ = !!env.find("$");
        found$$ = !!env.find("$$");
      }
    } else {
      found$ = !!dbgWindow.getOwnPropertyDescriptor("$");
      found$$ = !!dbgWindow.getOwnPropertyDescriptor("$$");
    }

    let $ = null, $$ = null;
    if (found$) {
      $ = bindings.$;
      delete bindings.$;
    }
    if (found$$) {
      $$ = bindings.$$;
      delete bindings.$$;
    }

    // Ready to evaluate the string.
    helpers.evalInput = string;

    let evalOptions;
    if (typeof options.url == "string") {
      evalOptions = { url: options.url };
    }

    // If the debugger object is changed from the last evaluation,
    // adopt this._lastConsoleInputEvaluation value in the new debugger,
    // to prevents "Debugger.Object belongs to a different Debugger" exceptions
    // related to the $_ bindings.
    if (this._lastConsoleInputEvaluation &&
        this._lastConsoleInputEvaluation.global !== dbgWindow) {
      this._lastConsoleInputEvaluation = dbg.adoptDebuggeeValue(
        this._lastConsoleInputEvaluation
      );
    }

    let result;

    if (frame) {
      result = frame.evalWithBindings(string, bindings, evalOptions);
    } else {
      result = dbgWindow.executeInGlobalWithBindings(string, bindings, evalOptions);
      // Attempt to initialize any declarations found in the evaluated string
      // since they may now be stuck in an "initializing" state due to the
      // error. Already-initialized bindings will be ignored.
      if ("throw" in result) {
        let ast;
        // Parse errors will raise an exception. We can/should ignore the error
        // since it's already being handled elsewhere and we are only interested
        // in initializing bindings.
        try {
          ast = Parser.reflectionAPI.parse(string);
        } catch (ex) {
          ast = {"body": []};
        }
        for (let line of ast.body) {
          // Only let and const declarations put bindings into an
          // "initializing" state.
          if (!(line.kind == "let" || line.kind == "const")) {
            continue;
          }

          let identifiers = [];
          for (let decl of line.declarations) {
            switch (decl.id.type) {
              case "Identifier":
                // let foo = bar;
                identifiers.push(decl.id.name);
                break;
              case "ArrayPattern":
                // let [foo, bar]    = [1, 2];
                // let [foo=99, bar] = [1, 2];
                for (let e of decl.id.elements) {
                  if (e.type == "Identifier") {
                    identifiers.push(e.name);
                  } else if (e.type == "AssignmentExpression") {
                    identifiers.push(e.left.name);
                  }
                }
                break;
              case "ObjectPattern":
                // let {bilbo, my}    = {bilbo: "baggins", my: "precious"};
                // let {blah: foo}    = {blah: yabba()}
                // let {blah: foo=99} = {blah: yabba()}
                for (let prop of decl.id.properties) {
                  // key
                  if (prop.key.type == "Identifier") {
                    identifiers.push(prop.key.name);
                  }
                  // value
                  if (prop.value.type == "Identifier") {
                    identifiers.push(prop.value.name);
                  } else if (prop.value.type == "AssignmentExpression") {
                    identifiers.push(prop.value.left.name);
                  }
                }
                break;
            }
          }

          for (let name of identifiers) {
            dbgWindow.forceLexicalInitializationByName(name);
          }
        }
      }
    }

    let helperResult = helpers.helperResult;
    delete helpers.evalInput;
    delete helpers.helperResult;
    delete helpers.selectedNode;

    if ($) {
      bindings.$ = $;
    }
    if ($$) {
      bindings.$$ = $$;
    }

    if (bindings._self) {
      delete bindings._self;
    }

    return {
      result: result,
      helperResult: helperResult,
      dbg: dbg,
      frame: frame,
      window: dbgWindow,
    };
  },
  /* eslint-enable complexity */

  // Event handlers for various listeners.

  /**
   * Handler for messages received from the ConsoleServiceListener. This method
   * sends the nsIConsoleMessage to the remote Web Console client.
   *
   * @param nsIConsoleMessage message
   *        The message we need to send to the client.
   */
  onConsoleServiceMessage: function(message) {
    let packet;
    if (message instanceof Ci.nsIScriptError) {
      packet = {
        from: this.actorID,
        type: "pageError",
        pageError: this.preparePageErrorForRemote(message),
      };
    } else {
      packet = {
        from: this.actorID,
        type: "logMessage",
        message: this._createStringGrip(message.message),
        timeStamp: message.timeStamp,
      };
    }
    this.conn.send(packet);
  },

  /**
   * Prepare an nsIScriptError to be sent to the client.
   *
   * @param nsIScriptError pageError
   *        The page error we need to send to the client.
   * @return object
   *         The object you can send to the remote client.
   */
  preparePageErrorForRemote: function(pageError) {
    let stack = null;
    // Convert stack objects to the JSON attributes expected by client code
    // Bug 1348885: If the global from which this error came from has been
    // nuked, stack is going to be a dead wrapper.
    if (pageError.stack && !Cu.isDeadWrapper(pageError.stack)) {
      stack = [];
      let s = pageError.stack;
      while (s !== null) {
        stack.push({
          filename: s.source,
          lineNumber: s.line,
          columnNumber: s.column,
          functionName: s.functionDisplayName
        });
        s = s.parent;
      }
    }
    let lineText = pageError.sourceLine;
    if (lineText && lineText.length > DebuggerServer.LONG_STRING_INITIAL_LENGTH) {
      lineText = lineText.substr(0, DebuggerServer.LONG_STRING_INITIAL_LENGTH);
    }

    let notesArray = null;
    let notes = pageError.notes;
    if (notes && notes.length) {
      notesArray = [];
      for (let i = 0, len = notes.length; i < len; i++) {
        let note = notes.queryElementAt(i, Ci.nsIScriptErrorNote);
        notesArray.push({
          messageBody: this._createStringGrip(note.errorMessage),
          frame: {
            source: note.sourceName,
            line: note.lineNumber,
            column: note.columnNumber,
          }
        });
      }
    }

    return {
      errorMessage: this._createStringGrip(pageError.errorMessage),
      errorMessageName: pageError.errorMessageName,
      exceptionDocURL: ErrorDocs.GetURL(pageError),
      sourceName: pageError.sourceName,
      lineText: lineText,
      lineNumber: pageError.lineNumber,
      columnNumber: pageError.columnNumber,
      category: pageError.category,
      timeStamp: pageError.timeStamp,
      warning: !!(pageError.flags & pageError.warningFlag),
      error: !!(pageError.flags & pageError.errorFlag),
      exception: !!(pageError.flags & pageError.exceptionFlag),
      strict: !!(pageError.flags & pageError.strictFlag),
      info: !!(pageError.flags & pageError.infoFlag),
      private: pageError.isFromPrivateWindow,
      stacktrace: stack,
      notes: notesArray,
    };
  },

  /**
   * Handler for window.console API calls received from the ConsoleAPIListener.
   * This method sends the object to the remote Web Console client.
   *
   * @see ConsoleAPIListener
   * @param object message
   *        The console API call we need to send to the remote client.
   */
  onConsoleAPICall: function(message) {
    let packet = {
      from: this.actorID,
      type: "consoleAPICall",
      message: this.prepareConsoleMessageForRemote(message),
    };
    this.conn.send(packet);
  },

  /**
   * Handler for network events. This method is invoked when a new network event
   * is about to be recorded.
   *
   * @see NetworkEventActor
   * @see NetworkMonitor from webconsole/utils.js
   *
   * @param object event
   *        The initial network request event information.
   * @return object
   *         A new NetworkEventActor is returned. This is used for tracking the
   *         network request and response.
   */
  onNetworkEvent: function(event) {
    let actor = this.getNetworkEventActor(event.channelId);
    actor.init(event);

    this._networkEventActorsByURL.set(actor._request.url, actor);

    let packet = {
      from: this.actorID,
      type: "networkEvent",
      eventActor: actor.grip()
    };

    this.conn.send(packet);

    return actor;
  },

  /**
   * Get the NetworkEventActor for a nsIHttpChannel, if it exists,
   * otherwise create a new one.
   *
   * @param string channelId
   *        The id of the channel for the network event.
   * @return object
   *         The NetworkEventActor for the given channel.
   */
  getNetworkEventActor: function(channelId) {
    let actor = this._netEvents.get(channelId);
    if (actor) {
      // delete from map as we should only need to do this check once
      this._netEvents.delete(channelId);
      return actor;
    }

    actor = new NetworkEventActor(this);
    this._actorPool.addActor(actor);
    return actor;
  },

  /**
   * Get the NetworkEventActor for a given URL that may have been noticed by the network
   * listener.  Requests are added when they start, so the actor might not yet have all
   * data for the request until it has completed.
   *
   * @param string url
   *        The URL of the request to search for.
   */
  getNetworkEventActorForURL(url) {
    return this._networkEventActorsByURL.get(url);
  },

  /**
   * Send a new HTTP request from the target's window.
   *
   * @param object message
   *        Object with 'request' - the HTTP request details.
   */
  onSendHTTPRequest(message) {
    let { url, method, headers, body } = message.request;

    // Set the loadingNode and loadGroup to the target document - otherwise the
    // request won't show up in the opened netmonitor.
    let doc = this.window.document;

    let channel = NetUtil.newChannel({
      uri: NetUtil.newURI(url),
      loadingNode: doc,
      securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER
    });

    channel.QueryInterface(Ci.nsIHttpChannel);

    channel.loadGroup = doc.documentLoadGroup;
    channel.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE |
                         Ci.nsIRequest.INHIBIT_CACHING |
                         Ci.nsIRequest.LOAD_ANONYMOUS;

    channel.requestMethod = method;

    for (let {name, value} of headers) {
      channel.setRequestHeader(name, value, false);
    }

    if (body) {
      channel.QueryInterface(Ci.nsIUploadChannel2);
      let bodyStream = Cc["@mozilla.org/io/string-input-stream;1"]
        .createInstance(Ci.nsIStringInputStream);
      bodyStream.setData(body, body.length);
      channel.explicitSetUploadStream(bodyStream, null, -1, method, false);
    }

    NetUtil.asyncFetch(channel, () => {});

    let actor = this.getNetworkEventActor(channel.channelId);

    // map channel to actor so we can associate future events with it
    this._netEvents.set(channel.channelId, actor);

    return {
      from: this.actorID,
      eventActor: actor.grip()
    };
  },

  /**
   * Handler for file activity. This method sends the file request information
   * to the remote Web Console client.
   *
   * @see ConsoleProgressListener
   * @param string fileURI
   *        The requested file URI.
   */
  onFileActivity: function(fileURI) {
    let packet = {
      from: this.actorID,
      type: "fileActivity",
      uri: fileURI,
    };
    this.conn.send(packet);
  },

  /**
   * Handler for reflow activity. This method forwards reflow events to the
   * remote Web Console client.
   *
   * @see ConsoleReflowListener
   * @param Object reflowInfo
   */
  onReflowActivity: function(reflowInfo) {
    let packet = {
      from: this.actorID,
      type: "reflowActivity",
      interruptible: reflowInfo.interruptible,
      start: reflowInfo.start,
      end: reflowInfo.end,
      sourceURL: reflowInfo.sourceURL,
      sourceLine: reflowInfo.sourceLine,
      functionName: reflowInfo.functionName
    };

    this.conn.send(packet);
  },

  // End of event handlers for various listeners.

  /**
   * Prepare a message from the console API to be sent to the remote Web Console
   * instance.
   *
   * @param object message
   *        The original message received from console-api-log-event.
   * @param boolean aUseObjectGlobal
   *        If |true| the object global is determined and added as a debuggee,
   *        otherwise |this.window| is used when makeDebuggeeValue() is invoked.
   * @return object
   *         The object that can be sent to the remote client.
   */
  prepareConsoleMessageForRemote: function(message, useObjectGlobal = true) {
    let result = WebConsoleUtils.cloneObject(message);

    result.workerType = WebConsoleUtils.getWorkerType(result) || "none";

    delete result.wrappedJSObject;
    delete result.ID;
    delete result.innerID;
    delete result.consoleID;

    result.arguments = Array.map(message.arguments || [], (obj) => {
      let dbgObj = this.makeDebuggeeValue(obj, useObjectGlobal);
      return this.createValueGrip(dbgObj);
    });

    result.styles = Array.map(message.styles || [], (string) => {
      return this.createValueGrip(string);
    });

    result.category = message.category || "webdev";

    return result;
  },

  /**
   * Find the XUL window that owns the content window.
   *
   * @return Window
   *         The XUL window that owns the content window.
   */
  chromeWindow: function() {
    let window = null;
    try {
      window = this.window.QueryInterface(Ci.nsIInterfaceRequestor)
             .getInterface(Ci.nsIWebNavigation).QueryInterface(Ci.nsIDocShell)
             .chromeEventHandler.ownerGlobal;
    } catch (ex) {
      // The above can fail because chromeEventHandler is not available for all
      // kinds of |this.window|.
    }

    return window;
  },

  /**
   * Notification observer for the "last-pb-context-exited" topic.
   *
   * @private
   * @param object subject
   *        Notification subject - in this case it is the inner window ID that
   *        was destroyed.
   * @param string topic
   *        Notification topic.
   */
  _onObserverNotification: function(subject, topic) {
    switch (topic) {
      case "last-pb-context-exited":
        this.conn.send({
          from: this.actorID,
          type: "lastPrivateContextExited",
        });
        break;
    }
  },

  /**
   * The "will-navigate" progress listener. This is used to clear the current
   * eval scope.
   */
  _onWillNavigate: function({ window, isTopLevel }) {
    if (isTopLevel) {
      this._evalWindow = null;
      EventEmitter.off(this.parentActor, "will-navigate", this._onWillNavigate);
      this._progressListenerActive = false;
    }
  },

  /**
   * This listener is called when we switch to another frame,
   * mostly to unregister previous listeners and start listening on the new document.
   */
  _onChangedToplevelDocument: function() {
    // Convert the Set to an Array
    let listeners = [...this._listeners];

    // Unregister existing listener on the previous document
    // (pass a copy of the array as it will shift from it)
    this.onStopListeners({listeners: listeners.slice()});

    // This method is called after this.window is changed,
    // so we register new listener on this new window
    this.onStartListeners({listeners: listeners});

    // Also reset the cached top level chrome window being targeted
    this._lastChromeWindow = null;
  },
};

WebConsoleActor.prototype.requestTypes =
{
  startListeners: WebConsoleActor.prototype.onStartListeners,
  stopListeners: WebConsoleActor.prototype.onStopListeners,
  getCachedMessages: WebConsoleActor.prototype.onGetCachedMessages,
  evaluateJS: WebConsoleActor.prototype.onEvaluateJS,
  evaluateJSAsync: WebConsoleActor.prototype.onEvaluateJSAsync,
  autocomplete: WebConsoleActor.prototype.onAutocomplete,
  clearMessagesCache: WebConsoleActor.prototype.onClearMessagesCache,
  getPreferences: WebConsoleActor.prototype.onGetPreferences,
  setPreferences: WebConsoleActor.prototype.onSetPreferences,
  sendHTTPRequest: WebConsoleActor.prototype.onSendHTTPRequest
};

exports.WebConsoleActor = WebConsoleActor;

/**
 * Creates an actor for a network event.
 *
 * @constructor
 * @param object webConsoleActor
 *        The parent WebConsoleActor instance for this object.
 */
function NetworkEventActor(webConsoleActor) {
  this.parent = webConsoleActor;
  this.conn = this.parent.conn;

  this._request = {
    method: null,
    url: null,
    httpVersion: null,
    headers: [],
    cookies: [],
    headersSize: null,
    postData: {},
  };

  this._response = {
    headers: [],
    cookies: [],
    content: {},
  };

  this._timings = {};
  this._stackTrace = {};

  // Keep track of LongStringActors owned by this NetworkEventActor.
  this._longStringActors = new Set();
}

NetworkEventActor.prototype =
{
  _request: null,
  _response: null,
  _timings: null,
  _longStringActors: null,

  actorPrefix: "netEvent",

  /**
   * Returns a grip for this actor for returning in a protocol message.
   */
  grip: function() {
    return {
      actor: this.actorID,
      startedDateTime: this._startedDateTime,
      timeStamp: Date.parse(this._startedDateTime),
      url: this._request.url,
      method: this._request.method,
      isXHR: this._isXHR,
      cause: this._cause,
      fromCache: this._fromCache,
      fromServiceWorker: this._fromServiceWorker,
      private: this._private,
    };
  },

  /**
   * Releases this actor from the pool.
   */
  release: function() {
    for (let grip of this._longStringActors) {
      let actor = this.parent.getActorByID(grip.actor);
      if (actor) {
        this.parent.releaseActor(actor);
      }
    }
    this._longStringActors = new Set();

    if (this._request.url) {
      this.parent._networkEventActorsByURL.delete(this._request.url);
    }
    if (this.channel) {
      this.parent._netEvents.delete(this.channel);
    }
    this.parent.releaseActor(this);
  },

  /**
   * Handle a protocol request to release a grip.
   */
  onRelease: function() {
    this.release();
    return {};
  },

  /**
   * Set the properties of this actor based on it's corresponding
   * network event.
   *
   * @param object networkEvent
   *        The network event associated with this actor.
   */
  init: function(networkEvent) {
    this._startedDateTime = networkEvent.startedDateTime;
    this._isXHR = networkEvent.isXHR;
    this._cause = networkEvent.cause;
    this._fromCache = networkEvent.fromCache;
    this._fromServiceWorker = networkEvent.fromServiceWorker;

    // Stack trace info isn't sent automatically. The client
    // needs to request it explicitly using getStackTrace
    // packet.
    this._stackTrace = networkEvent.cause.stacktrace;
    delete networkEvent.cause.stacktrace;
    networkEvent.cause.stacktraceAvailable =
      !!(this._stackTrace && this._stackTrace.length);

    for (let prop of ["method", "url", "httpVersion", "headersSize"]) {
      this._request[prop] = networkEvent[prop];
    }

    this._discardRequestBody = networkEvent.discardRequestBody;
    this._discardResponseBody = networkEvent.discardResponseBody;
    this._truncated = false;
    this._private = networkEvent.private;
  },

  /**
   * The "getRequestHeaders" packet type handler.
   *
   * @return object
   *         The response packet - network request headers.
   */
  onGetRequestHeaders: function() {
    return {
      from: this.actorID,
      headers: this._request.headers,
      headersSize: this._request.headersSize,
      rawHeaders: this._request.rawHeaders,
    };
  },

  /**
   * The "getRequestCookies" packet type handler.
   *
   * @return object
   *         The response packet - network request cookies.
   */
  onGetRequestCookies: function() {
    return {
      from: this.actorID,
      cookies: this._request.cookies,
    };
  },

  /**
   * The "getRequestPostData" packet type handler.
   *
   * @return object
   *         The response packet - network POST data.
   */
  onGetRequestPostData: function() {
    return {
      from: this.actorID,
      postData: this._request.postData,
      postDataDiscarded: this._discardRequestBody,
    };
  },

  /**
   * The "getSecurityInfo" packet type handler.
   *
   * @return object
   *         The response packet - connection security information.
   */
  onGetSecurityInfo: function() {
    return {
      from: this.actorID,
      securityInfo: this._securityInfo,
    };
  },

  /**
   * The "getResponseHeaders" packet type handler.
   *
   * @return object
   *         The response packet - network response headers.
   */
  onGetResponseHeaders: function() {
    return {
      from: this.actorID,
      headers: this._response.headers,
      headersSize: this._response.headersSize,
      rawHeaders: this._response.rawHeaders,
    };
  },

  /**
   * The "getResponseCookies" packet type handler.
   *
   * @return object
   *         The response packet - network response cookies.
   */
  onGetResponseCookies: function() {
    return {
      from: this.actorID,
      cookies: this._response.cookies,
    };
  },

  /**
   * The "getResponseContent" packet type handler.
   *
   * @return object
   *         The response packet - network response content.
   */
  onGetResponseContent: function() {
    return {
      from: this.actorID,
      content: this._response.content,
      contentDiscarded: this._discardResponseBody,
    };
  },

  /**
   * The "getEventTimings" packet type handler.
   *
   * @return object
   *         The response packet - network event timings.
   */
  onGetEventTimings: function() {
    return {
      from: this.actorID,
      timings: this._timings,
      totalTime: this._totalTime,
      offsets: this._offsets
    };
  },

  /**
   * The "getStackTrace" packet type handler.
   *
   * @return object
   *         The response packet - stack trace.
   */
  onGetStackTrace: function() {
    return {
      from: this.actorID,
      stacktrace: this._stackTrace,
    };
  },

  /** ****************************************************************
   * Listeners for new network event data coming from NetworkMonitor.
   ******************************************************************/

  /**
   * Add network request headers.
   *
   * @param array headers
   *        The request headers array.
   * @param string rawHeaders
   *        The raw headers source.
   */
  addRequestHeaders: function(headers, rawHeaders) {
    this._request.headers = headers;
    this._prepareHeaders(headers);

    rawHeaders = this.parent._createStringGrip(rawHeaders);
    if (typeof rawHeaders == "object") {
      this._longStringActors.add(rawHeaders);
    }
    this._request.rawHeaders = rawHeaders;

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "requestHeaders",
      headers: headers.length,
      headersSize: this._request.headersSize,
    };

    this.conn.send(packet);
  },

  /**
   * Add network request cookies.
   *
   * @param array cookies
   *        The request cookies array.
   */
  addRequestCookies: function(cookies) {
    this._request.cookies = cookies;
    this._prepareHeaders(cookies);

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "requestCookies",
      cookies: cookies.length,
    };

    this.conn.send(packet);
  },

  /**
   * Add network request POST data.
   *
   * @param object postData
   *        The request POST data.
   */
  addRequestPostData: function(postData) {
    this._request.postData = postData;
    postData.text = this.parent._createStringGrip(postData.text);
    if (typeof postData.text == "object") {
      this._longStringActors.add(postData.text);
    }

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "requestPostData",
      dataSize: postData.text.length,
      discardRequestBody: this._discardRequestBody,
    };

    this.conn.send(packet);
  },

  /**
   * Add the initial network response information.
   *
   * @param object info
   *        The response information.
   * @param string rawHeaders
   *        The raw headers source.
   */
  addResponseStart: function(info, rawHeaders) {
    rawHeaders = this.parent._createStringGrip(rawHeaders);
    if (typeof rawHeaders == "object") {
      this._longStringActors.add(rawHeaders);
    }
    this._response.rawHeaders = rawHeaders;

    this._response.httpVersion = info.httpVersion;
    this._response.status = info.status;
    this._response.statusText = info.statusText;
    this._response.headersSize = info.headersSize;
    this._discardResponseBody = info.discardResponseBody;

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "responseStart",
      response: info
    };

    this.conn.send(packet);
  },

  /**
   * Add connection security information.
   *
   * @param object info
   *        The object containing security information.
   */
  addSecurityInfo: function(info) {
    this._securityInfo = info;

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "securityInfo",
      state: info.state,
    };

    this.conn.send(packet);
  },

  /**
   * Add network response headers.
   *
   * @param array headers
   *        The response headers array.
   */
  addResponseHeaders: function(headers) {
    this._response.headers = headers;
    this._prepareHeaders(headers);

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "responseHeaders",
      headers: headers.length,
      headersSize: this._response.headersSize,
    };

    this.conn.send(packet);
  },

  /**
   * Add network response cookies.
   *
   * @param array cookies
   *        The response cookies array.
   */
  addResponseCookies: function(cookies) {
    this._response.cookies = cookies;
    this._prepareHeaders(cookies);

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "responseCookies",
      cookies: cookies.length,
    };

    this.conn.send(packet);
  },

  /**
   * Add network response content.
   *
   * @param object content
   *        The response content.
   * @param object
   *        - boolean discardedResponseBody
   *          Tells if the response content was recorded or not.
   *        - boolean truncated
   *          Tells if the some of the response content is missing.
   */
  addResponseContent: function(content, {discardResponseBody, truncated}) {
    this._truncated = truncated;
    this._response.content = content;
    content.text = this.parent._createStringGrip(content.text);
    if (typeof content.text == "object") {
      this._longStringActors.add(content.text);
    }

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "responseContent",
      mimeType: content.mimeType,
      contentSize: content.size,
      encoding: content.encoding,
      transferredSize: content.transferredSize,
      discardResponseBody,
    };

    this.conn.send(packet);
  },

  /**
   * Add network event timing information.
   *
   * @param number total
   *        The total time of the network event.
   * @param object timings
   *        Timing details about the network event.
   */
  addEventTimings: function(total, timings, offsets) {
    this._totalTime = total;
    this._timings = timings;
    this._offsets = offsets;

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "eventTimings",
      totalTime: total
    };

    this.conn.send(packet);
  },

  /**
   * Prepare the headers array to be sent to the client by using the
   * LongStringActor for the header values, when needed.
   *
   * @private
   * @param array aHeaders
   */
  _prepareHeaders: function(headers) {
    for (let header of headers) {
      header.value = this.parent._createStringGrip(header.value);
      if (typeof header.value == "object") {
        this._longStringActors.add(header.value);
      }
    }
  },
};

NetworkEventActor.prototype.requestTypes =
{
  "release": NetworkEventActor.prototype.onRelease,
  "getRequestHeaders": NetworkEventActor.prototype.onGetRequestHeaders,
  "getRequestCookies": NetworkEventActor.prototype.onGetRequestCookies,
  "getRequestPostData": NetworkEventActor.prototype.onGetRequestPostData,
  "getResponseHeaders": NetworkEventActor.prototype.onGetResponseHeaders,
  "getResponseCookies": NetworkEventActor.prototype.onGetResponseCookies,
  "getResponseContent": NetworkEventActor.prototype.onGetResponseContent,
  "getEventTimings": NetworkEventActor.prototype.onGetEventTimings,
  "getSecurityInfo": NetworkEventActor.prototype.onGetSecurityInfo,
  "getStackTrace": NetworkEventActor.prototype.onGetStackTrace,
};
