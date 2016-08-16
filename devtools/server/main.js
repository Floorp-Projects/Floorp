/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Toolkit glue for the remote debugging protocol, loaded into the
 * debugging global.
 */
var { Ci, Cc, CC, Cu, Cr } = require("chrome");
var Services = require("Services");
var { ActorPool, OriginalLocation, RegisteredActorFactory,
      ObservedActorFactory } = require("devtools/server/actors/common");
var { LocalDebuggerTransport, ChildDebuggerTransport, WorkerDebuggerTransport } =
  require("devtools/shared/transport/transport");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var { dumpn, dumpv } = DevToolsUtils;
var flags = require("devtools/shared/flags");
var EventEmitter = require("devtools/shared/event-emitter");
var Promise = require("promise");
var SyncPromise = require("devtools/shared/deprecated-sync-thenables");

DevToolsUtils.defineLazyGetter(this, "DebuggerSocket", () => {
  let { DebuggerSocket } = require("devtools/shared/security/socket");
  return DebuggerSocket;
});
DevToolsUtils.defineLazyGetter(this, "Authentication", () => {
  return require("devtools/shared/security/auth");
});
DevToolsUtils.defineLazyGetter(this, "generateUUID", () => {
  let { generateUUID } = Cc["@mozilla.org/uuid-generator;1"]
                           .getService(Ci.nsIUUIDGenerator);
  return generateUUID;
});

// On B2G, `this` != Global scope, so `Ci` won't be binded on `this`
// (i.e. this.Ci is undefined) Then later, when using loadSubScript,
// Ci,... won't be defined for sub scripts.
this.Ci = Ci;
this.Cc = Cc;
this.CC = CC;
this.Cu = Cu;
this.Cr = Cr;
this.Services = Services;
this.ActorPool = ActorPool;
this.DevToolsUtils = DevToolsUtils;
this.dumpn = dumpn;
this.dumpv = dumpv;

// Overload `Components` to prevent SDK loader exception on Components
// object usage
Object.defineProperty(this, "Components", {
  get() {
    return require("chrome").components;
  }
});

if (isWorker) {
  flags.wantLogging = true;
  flags.wantVerbose = true;
} else {
  const LOG_PREF = "devtools.debugger.log";
  const VERBOSE_PREF = "devtools.debugger.log.verbose";

  flags.wantLogging = Services.prefs.getBoolPref(LOG_PREF);
  flags.wantVerbose =
    Services.prefs.getPrefType(VERBOSE_PREF) !== Services.prefs.PREF_INVALID &&
    Services.prefs.getBoolPref(VERBOSE_PREF);
}

function loadSubScript(url) {
  try {
    let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
                   .getService(Ci.mozIJSSubScriptLoader);
    loader.loadSubScript(url, this);
  } catch (e) {
    let errorStr = "Error loading: " + url + ":\n" +
                   (e.fileName ? "at " + e.fileName + " : " + e.lineNumber + "\n" : "") +
                   e + " - " + e.stack + "\n";
    dump(errorStr);
    reportError(errorStr);
    throw e;
  }
}

loader.lazyRequireGetter(this, "events", "sdk/event/core");

var gRegisteredModules = Object.create(null);

/**
 * The ModuleAPI object is passed to modules loaded using the
 * DebuggerServer.registerModule() API.  Modules can use this
 * object to register actor factories.
 * Factories registered through the module API will be removed
 * when the module is unregistered or when the server is
 * destroyed.
 */
function ModuleAPI() {
  let activeTabActors = new Set();
  let activeGlobalActors = new Set();

  return {
    // See DebuggerServer.setRootActor for a description.
    setRootActor(factory) {
      DebuggerServer.setRootActor(factory);
    },

    // See DebuggerServer.addGlobalActor for a description.
    addGlobalActor(factory, name) {
      DebuggerServer.addGlobalActor(factory, name);
      activeGlobalActors.add(factory);
    },
    // See DebuggerServer.removeGlobalActor for a description.
    removeGlobalActor(factory) {
      DebuggerServer.removeGlobalActor(factory);
      activeGlobalActors.delete(factory);
    },

    // See DebuggerServer.addTabActor for a description.
    addTabActor(factory, name) {
      DebuggerServer.addTabActor(factory, name);
      activeTabActors.add(factory);
    },
    // See DebuggerServer.removeTabActor for a description.
    removeTabActor(factory) {
      DebuggerServer.removeTabActor(factory);
      activeTabActors.delete(factory);
    },

    // Destroy the module API object, unregistering any
    // factories registered by the module.
    destroy() {
      for (let factory of activeTabActors) {
        DebuggerServer.removeTabActor(factory);
      }
      activeTabActors = null;
      for (let factory of activeGlobalActors) {
        DebuggerServer.removeGlobalActor(factory);
      }
      activeGlobalActors = null;
    }
  };
}

/** *
 * Public API
 */
var DebuggerServer = {
  _listeners: [],
  _initialized: false,
  // Map of global actor names to actor constructors provided by extensions.
  globalActorFactories: {},
  // Map of tab actor names to actor constructors provided by extensions.
  tabActorFactories: {},

  LONG_STRING_LENGTH: 10000,
  LONG_STRING_INITIAL_LENGTH: 1000,
  LONG_STRING_READ_LENGTH: 65 * 1024,

  /**
   * The windowtype of the chrome window to use for actors that use the global
   * window (i.e the global style editor). Set this to your main window type,
   * for example "navigator:browser".
   */
  chromeWindowType: null,

  /**
   * Allow debugging chrome of (parent or child) processes.
   */
  allowChromeProcess: false,

  /**
   * Initialize the debugger server.
   */
  init() {
    if (this.initialized) {
      return;
    }

    this._connections = {};
    this._nextConnID = 0;

    this._initialized = true;
  },

  get protocol() {
    return require("devtools/shared/protocol");
  },

  get initialized() {
    return this._initialized;
  },

  /**
   * Performs cleanup tasks before shutting down the debugger server. Such tasks
   * include clearing any actor constructors added at runtime. This method
   * should be called whenever a debugger server is no longer useful, to avoid
   * memory leaks. After this method returns, the debugger server must be
   * initialized again before use.
   */
  destroy() {
    if (!this._initialized) {
      return;
    }

    for (let connID of Object.getOwnPropertyNames(this._connections)) {
      this._connections[connID].close();
    }

    for (let id of Object.getOwnPropertyNames(gRegisteredModules)) {
      this.unregisterModule(id);
    }
    gRegisteredModules = Object.create(null);

    this.closeAllListeners();
    this.globalActorFactories = {};
    this.tabActorFactories = {};
    this._initialized = false;

    dumpn("Debugger server is shut down.");
  },

  /**
   * Raises an exception if the server has not been properly initialized.
   */
  _checkInit() {
    if (!this._initialized) {
      throw new Error("DebuggerServer has not been initialized.");
    }

    if (!this.createRootActor) {
      throw new Error("Use DebuggerServer.addActors() to add a root actor " +
                      "implementation.");
    }
  },

  /**
   * Load a subscript into the debugging global.
   *
   * @param url string A url that will be loaded as a subscript into the
   *        debugging global.  The user must load at least one script
   *        that implements a createRootActor() function to create the
   *        server's root actor.
   */
  addActors(url) {
    loadSubScript.call(this, url);
  },

  /**
   * Register a CommonJS module with the debugger server.
   * @param id string
   *        The ID of a CommonJS module.  This module must export 'register'
   *        and 'unregister' functions if no `options` argument is given.
   *        If `options` is set, the actor is going to be registered
   *        immediately, but loaded only when a client starts sending packets
   *        to an actor with the same id.
   *
   * @param options object (optional)
   *        This parameter is still optional, but not providing it is
   *        deprecated and will result in eagerly loading the actor module
   *        with the memory overhead that entails.
   *        An object with 3 mandatory attributes:
   *        - prefix (string):
   *          The prefix of an actor is used to compute:
   *          - the `actorID` of each new actor instance (ex: prefix1).
   *            (See ActorPool.addActor)
   *          - the actor name in the listTabs request. Sending a listTabs
   *            request to the root actor returns actor IDs. IDs are in
   *            dictionaries, with actor names as keys and actor IDs as values.
   *            The actor name is the prefix to which the "Actor" string is
   *            appended. So for an actor with the `console` prefix, the actor
   *            name will be `consoleActor`.
   *        - constructor (string):
   *          the name of the exported symbol to be used as the actor
   *          constructor.
   *        - type (a dictionary of booleans with following attribute names):
   *          - "global"
   *            registers a global actor instance, if true.
   *            A global actor has the root actor as its parent.
   *          - "tab"
   *            registers a tab actor instance, if true.
   *            A new actor will be created for each tab and each app.
   */
  registerModule(id, options) {
    if (id in gRegisteredModules) {
      throw new Error("Tried to register a module twice: " + id + "\n");
    }

    if (options) {
      // Lazy loaded actors
      let {prefix, constructor, type} = options;
      if (typeof (prefix) !== "string") {
        throw new Error(`Lazy actor definition for '${id}' requires a string ` +
                        `'prefix' option.`);
      }
      if (typeof (constructor) !== "string") {
        throw new Error(`Lazy actor definition for '${id}' requires a string ` +
                        `'constructor' option.`);
      }
      if (!("global" in type) && !("tab" in type)) {
        throw new Error(`Lazy actor definition for '${id}' requires a dictionary ` +
                        `'type' option whose attributes can be 'global' or 'tab'.`);
      }
      let name = prefix + "Actor";
      let mod = {
        id: id,
        prefix: prefix,
        constructorName: constructor,
        type: type,
        globalActor: type.global,
        tabActor: type.tab
      };
      gRegisteredModules[id] = mod;
      if (mod.tabActor) {
        this.addTabActor(mod, name);
      }
      if (mod.globalActor) {
        this.addGlobalActor(mod, name);
      }
    } else {
      // Deprecated actors being loaded at startup
      let moduleAPI = ModuleAPI();
      let mod = require(id);
      mod.register(moduleAPI);
      gRegisteredModules[id] = {
        module: mod,
        api: moduleAPI
      };
    }
  },

  /**
   * Returns true if a module id has been registered.
   */
  isModuleRegistered(id) {
    return (id in gRegisteredModules);
  },

  /**
   * Unregister a previously-loaded CommonJS module from the debugger server.
   */
  unregisterModule(id) {
    let mod = gRegisteredModules[id];
    if (!mod) {
      throw new Error("Tried to unregister a module that was not previously registered.");
    }

    // Lazy actors
    if (mod.tabActor) {
      this.removeTabActor(mod);
    }
    if (mod.globalActor) {
      this.removeGlobalActor(mod);
    }

    if (mod.module) {
      // Deprecated non-lazy module API
      mod.module.unregister(mod.api);
      mod.api.destroy();
    }

    delete gRegisteredModules[id];
  },

  /**
   * Install Firefox-specific actors.
   *
   * /!\ Be careful when adding a new actor, especially global actors.
   * Any new global actor will be exposed and returned by the root actor.
   *
   * That's the reason why tab actors aren't loaded on demand via
   * restrictPrivileges=true, to prevent exposing them on b2g parent process's
   * root actor.
   */
  addBrowserActors(windowType = "navigator:browser", restrictPrivileges = false) {
    this.chromeWindowType = windowType;
    this.registerModule("devtools/server/actors/webbrowser");

    if (!restrictPrivileges) {
      this.addTabActors();
      this.registerModule("devtools/server/actors/preference", {
        prefix: "preference",
        constructor: "PreferenceActor",
        type: { global: true }
      });
      this.registerModule("devtools/server/actors/actor-registry", {
        prefix: "actorRegistry",
        constructor: "ActorRegistryActor",
        type: { global: true }
      });
    }
    if (Services.prefs.getBoolPref("dom.mozSettings.enabled")) {
      this.registerModule("devtools/server/actors/settings", {
        prefix: "settings",
        constructor: "SettingsActor",
        type: { global: true }
      });
    }
    this.registerModule("devtools/server/actors/addons", {
      prefix: "addons",
      constructor: "AddonsActor",
      type: { global: true }
    });
    this.registerModule("devtools/server/actors/webapps", {
      prefix: "webapps",
      constructor: "WebappsActor",
      type: { global: true }
    });
    this.registerModule("devtools/server/actors/device", {
      prefix: "device",
      constructor: "DeviceActor",
      type: { global: true }
    });
    this.registerModule("devtools/server/actors/director-registry", {
      prefix: "directorRegistry",
      constructor: "DirectorRegistryActor",
      type: { global: true }
    });
    this.registerModule("devtools/server/actors/heap-snapshot-file", {
      prefix: "heapSnapshotFile",
      constructor: "HeapSnapshotFileActor",
      type: { global: true }
    });
  },

  /**
   * Install tab actors in documents loaded in content childs
   */
  addChildActors() {
    // In case of apps being loaded in parent process, DebuggerServer is already
    // initialized and browser actors are already loaded,
    // but childtab.js hasn't been loaded yet.
    if (!DebuggerServer.tabActorFactories.hasOwnProperty("consoleActor")) {
      this.addTabActors();
    }
    // But webbrowser.js and childtab.js aren't loaded from shell.js.
    if (!this.isModuleRegistered("devtools/server/actors/webbrowser")) {
      this.registerModule("devtools/server/actors/webbrowser");
    }
    if (!("ContentActor" in this)) {
      this.addActors("resource://devtools/server/actors/childtab.js");
    }
  },

  /**
   * Install tab actors.
   */
  addTabActors() {
    this.registerModule("devtools/server/actors/webconsole", {
      prefix: "console",
      constructor: "WebConsoleActor",
      type: { tab: true }
    });
    this.registerModule("devtools/server/actors/inspector", {
      prefix: "inspector",
      constructor: "InspectorActor",
      type: { tab: true }
    });
    this.registerModule("devtools/server/actors/call-watcher", {
      prefix: "callWatcher",
      constructor: "CallWatcherActor",
      type: { tab: true }
    });
    this.registerModule("devtools/server/actors/canvas", {
      prefix: "canvas",
      constructor: "CanvasActor",
      type: { tab: true }
    });
    this.registerModule("devtools/server/actors/webgl", {
      prefix: "webgl",
      constructor: "WebGLActor",
      type: { tab: true }
    });
    this.registerModule("devtools/server/actors/webaudio", {
      prefix: "webaudio",
      constructor: "WebAudioActor",
      type: { tab: true }
    });
    this.registerModule("devtools/server/actors/stylesheets", {
      prefix: "styleSheets",
      constructor: "StyleSheetsActor",
      type: { tab: true }
    });
    this.registerModule("devtools/server/actors/styleeditor", {
      prefix: "styleEditor",
      constructor: "StyleEditorActor",
      type: { tab: true }
    });
    this.registerModule("devtools/server/actors/storage", {
      prefix: "storage",
      constructor: "StorageActor",
      type: { tab: true }
    });
    this.registerModule("devtools/server/actors/gcli", {
      prefix: "gcli",
      constructor: "GcliActor",
      type: { tab: true }
    });
    this.registerModule("devtools/server/actors/memory", {
      prefix: "memory",
      constructor: "MemoryActor",
      type: { tab: true }
    });
    this.registerModule("devtools/server/actors/framerate", {
      prefix: "framerate",
      constructor: "FramerateActor",
      type: { tab: true }
    });
    this.registerModule("devtools/server/actors/eventlooplag", {
      prefix: "eventLoopLag",
      constructor: "EventLoopLagActor",
      type: { tab: true }
    });
    this.registerModule("devtools/server/actors/layout", {
      prefix: "reflow",
      constructor: "ReflowActor",
      type: { tab: true }
    });
    this.registerModule("devtools/server/actors/css-properties", {
      prefix: "cssProperties",
      constructor: "CssPropertiesActor",
      type: { tab: true }
    });
    this.registerModule("devtools/server/actors/csscoverage", {
      prefix: "cssUsage",
      constructor: "CSSUsageActor",
      type: { tab: true }
    });
    this.registerModule("devtools/server/actors/monitor", {
      prefix: "monitor",
      constructor: "MonitorActor",
      type: { tab: true }
    });
    this.registerModule("devtools/server/actors/timeline", {
      prefix: "timeline",
      constructor: "TimelineActor",
      type: { tab: true }
    });
    this.registerModule("devtools/server/actors/director-manager", {
      prefix: "directorManager",
      constructor: "DirectorManagerActor",
      type: { global: false, tab: true }
    });
    if ("nsIProfiler" in Ci) {
      this.registerModule("devtools/server/actors/profiler", {
        prefix: "profiler",
        constructor: "ProfilerActor",
        type: { tab: true }
      });
      this.registerModule("devtools/server/actors/performance", {
        prefix: "performance",
        constructor: "PerformanceActor",
        type: { tab: true }
      });
    }
    this.registerModule("devtools/server/actors/animation", {
      prefix: "animations",
      constructor: "AnimationsActor",
      type: { tab: true }
    });
    this.registerModule("devtools/server/actors/promises", {
      prefix: "promises",
      constructor: "PromisesActor",
      type: { tab: true }
    });
    this.registerModule("devtools/server/actors/performance-entries", {
      prefix: "performanceEntries",
      constructor: "PerformanceEntriesActor",
      type: { tab: true }
    });
  },

  /**
   * Passes a set of options to the BrowserAddonActors for the given ID.
   *
   * @param id string
   *        The ID of the add-on to pass the options to
   * @param options object
   *        The options.
   * @return a promise that will be resolved when complete.
   */
  setAddonOptions(id, options) {
    if (!this._initialized) {
      return Promise.resolve();
    }

    let promises = [];

    // Pass to all connections
    for (let connID of Object.getOwnPropertyNames(this._connections)) {
      promises.push(this._connections[connID].setAddonOptions(id, options));
    }

    return SyncPromise.all(promises);
  },

  get listeningSockets() {
    return this._listeners.length;
  },

  /**
   * Creates a socket listener for remote debugger connections.
   *
   * After calling this, set some socket options, such as the port / path to
   * listen on, and then call |open| on the listener.
   *
   * See SocketListener in devtools/shared/security/socket.js for available
   * options.
   *
   * @return SocketListener
   *         A SocketListener instance that is waiting to be configured and
   *         opened is returned.  This single listener can be closed at any
   *         later time by calling |close| on the SocketListener.  If remote
   *         connections are disabled, an error is thrown.
   */
  createListener() {
    if (!Services.prefs.getBoolPref("devtools.debugger.remote-enabled")) {
      throw new Error("Can't create listener, remote debugging disabled");
    }
    this._checkInit();
    return DebuggerSocket.createListener();
  },

  /**
   * Add a SocketListener instance to the server's set of active
   * SocketListeners.  This is called by a SocketListener after it is opened.
   */
  _addListener(listener) {
    this._listeners.push(listener);
  },

  /**
   * Remove a SocketListener instance from the server's set of active
   * SocketListeners.  This is called by a SocketListener after it is closed.
   */
  _removeListener(listener) {
    this._listeners = this._listeners.filter(l => l !== listener);
  },

  /**
   * Closes and forgets all previously opened listeners.
   *
   * @return boolean
   *         Whether any listeners were actually closed.
   */
  closeAllListeners() {
    if (!this.listeningSockets) {
      return false;
    }

    for (let listener of this._listeners) {
      listener.close();
    }

    return true;
  },

  /**
   * Creates a new connection to the local debugger speaking over a fake
   * transport. This connection results in straightforward calls to the onPacket
   * handlers of each side.
   *
   * @param prefix string [optional]
   *    If given, all actors in this connection will have names starting
   *    with |prefix + '/'|.
   * @returns a client-side DebuggerTransport for communicating with
   *    the newly-created connection.
   */
  connectPipe(prefix) {
    this._checkInit();

    let serverTransport = new LocalDebuggerTransport();
    let clientTransport = new LocalDebuggerTransport(serverTransport);
    serverTransport.other = clientTransport;
    let connection = this._onConnection(serverTransport, prefix);

    // I'm putting this here because I trust you.
    //
    // There are times, when using a local connection, when you're going
    // to be tempted to just get direct access to the server.  Resist that
    // temptation!  If you succumb to that temptation, you will make the
    // fine developers that work on Fennec and Firefox OS sad.  They're
    // professionals, they'll try to act like they understand, but deep
    // down you'll know that you hurt them.
    //
    // This reference allows you to give in to that temptation.  There are
    // times this makes sense: tests, for example, and while porting a
    // previously local-only codebase to the remote protocol.
    //
    // But every time you use this, you will feel the shame of having
    // used a property that starts with a '_'.
    clientTransport._serverConnection = connection;

    return clientTransport;
  },

  /**
   * In a content child process, create a new connection that exchanges
   * nsIMessageSender messages with our parent process.
   *
   * @param prefix
   *    The prefix we should use in our nsIMessageSender message names and
   *    actor names. This connection will use messages named
   *    "debug:<prefix>:packet", and all its actors will have names
   *    beginning with "<prefix>/".
   */
  connectToParent(prefix, scopeOrManager) {
    this._checkInit();

    let transport = isWorker ?
                    new WorkerDebuggerTransport(scopeOrManager, prefix) :
                    new ChildDebuggerTransport(scopeOrManager, prefix);

    return this._onConnection(transport, prefix, true);
  },

  connectToContent(connection, mm) {
    let deferred = SyncPromise.defer();

    let prefix = connection.allocID("content-process");
    let actor, childTransport;

    mm.addMessageListener("debug:content-process-actor", function listener(msg) {
      // Arbitrarily choose the first content process to reply
      // XXX: This code needs to be updated if we use more than one content process
      mm.removeMessageListener("debug:content-process-actor", listener);

      // Pipe Debugger message from/to parent/child via the message manager
      childTransport = new ChildDebuggerTransport(mm, prefix);
      childTransport.hooks = {
        onPacket: connection.send.bind(connection),
        onClosed() {}
      };
      childTransport.ready();

      connection.setForwarding(prefix, childTransport);

      dumpn("establishing forwarding for process with prefix " + prefix);

      actor = msg.json.actor;

      deferred.resolve(actor);
    });

    mm.sendAsyncMessage("DevTools:InitDebuggerServer", {
      prefix: prefix
    });

    function onClose() {
      Services.obs.removeObserver(onMessageManagerClose, "message-manager-close");
      events.off(connection, "closed", onClose);
      if (childTransport) {
        // If we have a child transport, the actor has already
        // been created. We need to stop using this message manager.
        childTransport.close();
        childTransport = null;
        connection.cancelForwarding(prefix);

        // ... and notify the child process to clean the tab actors.
        try {
          mm.sendAsyncMessage("debug:content-process-destroy");
        } catch (e) {
          // Nothing to do
        }
      }
    }

    let onMessageManagerClose = DevToolsUtils.makeInfallible((subject, topic, data) => {
      if (subject == mm) {
        onClose();
        connection.send({ from: actor.actor, type: "tabDetached" });
      }
    });
    Services.obs.addObserver(onMessageManagerClose,
                             "message-manager-close", false);

    events.on(connection, "closed", onClose);

    return deferred.promise;
  },

  connectToWorker(connection, dbg, id, options) {
    return new Promise((resolve, reject) => {
      // Step 1: Ensure the worker debugger is initialized.
      if (!dbg.isInitialized) {
        dbg.initialize("resource://devtools/server/worker.js");

        // Create a listener for rpc requests from the worker debugger. Only do
        // this once, when the worker debugger is first initialized, rather than
        // for each connection.
        let listener = {
          onClose: () => {
            dbg.removeListener(listener);
          },

          onMessage: (message) => {
            message = JSON.parse(message);
            if (message.type !== "rpc") {
              return;
            }

            Promise.resolve().then(() => {
              let method = {
                "fetch": DevToolsUtils.fetch,
              }[message.method];
              if (!method) {
                throw Error("Unknown method: " + message.method);
              }

              return method.apply(undefined, message.params);
            }).then((value) => {
              dbg.postMessage(JSON.stringify({
                type: "rpc",
                result: value,
                error: null,
                id: message.id
              }));
            }, (reason) => {
              dbg.postMessage(JSON.stringify({
                type: "rpc",
                result: null,
                error: reason,
                id: message.id
              }));
            });
          }
        };

        dbg.addListener(listener);
      }

      // Step 2: Send a connect request to the worker debugger.
      dbg.postMessage(JSON.stringify({
        type: "connect",
        id,
        options,
      }));

      // Steps 3-5 are performed on the worker thread (see worker.js).

      // Step 6: Wait for a connection response from the worker debugger.
      let listener = {
        onClose: () => {
          dbg.removeListener(listener);

          reject("closed");
        },

        onMessage: (message) => {
          message = JSON.parse(message);
          if (message.type !== "connected" || message.id !== id) {
            return;
          }

          // The initial connection message has been received, don't
          // need to listen any longer
          dbg.removeListener(listener);

          // Step 7: Create a transport for the connection to the worker.
          let transport = new WorkerDebuggerTransport(dbg, id);
          transport.ready();
          transport.hooks = {
            onClosed: () => {
              if (!dbg.isClosed) {
                // If the worker happens to be shutting down while we are trying
                // to close the connection, there is a small interval during
                // which no more runnables can be dispatched to the worker, but
                // the worker debugger has not yet been closed. In that case,
                // the call to postMessage below will fail. The onClosed hook on
                // DebuggerTransport is not supposed to throw exceptions, so we
                // need to make sure to catch these early.
                try {
                  dbg.postMessage(JSON.stringify({
                    type: "disconnect",
                    id,
                  }));
                } catch (e) {
                  // We can safely ignore these exceptions. The only time the
                  // call to postMessage can fail is if the worker is either
                  // shutting down, or has finished shutting down. In both
                  // cases, there is nothing to clean up, so we don't care
                  // whether this message arrives or not.
                }
              }

              connection.cancelForwarding(id);
            },

            onPacket: (packet) => {
              // Ensure that any packets received from the server on the worker
              // thread are forwarded to the client on the main thread, as if
              // they had been sent by the server on the main thread.
              connection.send(packet);
            }
          };

          // Ensure that any packets received from the client on the main thread
          // to actors on the worker thread are forwarded to the server on the
          // worker thread.
          connection.setForwarding(id, transport);

          resolve({
            threadActor: message.threadActor,
            consoleActor: message.consoleActor,
            transport: transport
          });
        }
      };
      dbg.addListener(listener);
    });
  },

  /**
   * Check if the server is running in the child process.
   */
  get isInChildProcess() {
    return Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
  },

  /**
   * In a chrome parent process, ask all content child processes
   * to execute a given module setup helper.
   *
   * @param module
   *        The module to be required
   * @param setupChild
   *        The name of the setup helper exported by the above module
   *        (setup helper signature: function ({mm}) { ... })
   * @param waitForEval (optional)
   *        If true, the returned promise only resolves once code in child
   *        is evaluated
   */
  setupInChild({ module, setupChild, args, waitForEval }) {
    if (this._childMessageManagers.size == 0) {
      return Promise.resolve();
    }
    let deferred = Promise.defer();

    // If waitForEval is set, pass a unique id and expect child.js to send
    // a message back once the code in child is evaluated.
    if (typeof (waitForEval) != "boolean") {
      waitForEval = false;
    }
    let count = this._childMessageManagers.size;
    let id = waitForEval ? generateUUID().toString() : null;

    this._childMessageManagers.forEach(mm => {
      if (waitForEval) {
        // Listen for the end of each child execution
        let evalListener = msg => {
          if (msg.data.id !== id) {
            return;
          }
          mm.removeMessageListener("debug:setup-in-child-response", evalListener);
          if (--count === 0) {
            deferred.resolve();
          }
        };
        mm.addMessageListener("debug:setup-in-child-response", evalListener);
      }
      mm.sendAsyncMessage("debug:setup-in-child", {
        module: module,
        setupChild: setupChild,
        args: args,
        id: id,
      });
    });

    if (waitForEval) {
      return deferred.promise;
    }
    return Promise.resolve();
  },

  /**
   * Live list of all currenctly attached child's message managers.
   */
  _childMessageManagers: new Set(),

  /**
   * Connect to a child process.
   *
   * @param object connection
   *        The debugger server connection to use.
   * @param nsIDOMElement frame
   *        The browser element that holds the child process.
   * @param function [onDestroy]
   *        Optional function to invoke when the child process closes
   *        or the connection shuts down. (Need to forget about the
   *        related TabActor)
   * @return object
   *         A promise object that is resolved once the connection is
   *         established.
   */
  connectToChild(connection, frame, onDestroy) {
    let deferred = SyncPromise.defer();

    // Get messageManager from XUL browser (which might be a specialized tunnel for RDM)
    // or else fallback to asking the frameLoader itself.
    let mm = frame.messageManager || frame.frameLoader.messageManager;
    mm.loadFrameScript("resource://devtools/server/child.js", false);
    this._childMessageManagers.add(mm);

    let actor, childTransport;
    let prefix = connection.allocID("child");
    let netMonitor = null;

    // provides hook to actor modules that need to exchange messages
    // between e10s parent and child processes
    let onSetupInParent = function (msg) {
      // We may have multiple connectToChild instance running for the same tab
      // and need to filter the messages. Also the DebuggerServerConnection's
      // prefix has an additional '/' and the end, so use `includes`.
      if (!msg.json.prefix.includes(prefix)) {
        return false;
      }

      let { module, setupParent } = msg.json;
      let m;

      try {
        m = require(module);

        if (!setupParent in m) {
          dumpn(`ERROR: module '${module}' does not export 'setupParent'`);
          return false;
        }

        m[setupParent]({ mm, prefix });

        return true;
      } catch (e) {
        let errorMessage =
          "Exception during actor module setup running in the parent process: ";
        DevToolsUtils.reportException(errorMessage + e);
        dumpn(`ERROR: ${errorMessage}\n\t module: '${module}'\n\t ` +
              `setupParent: '${setupParent}'\n${DevToolsUtils.safeErrorString(e)}`);
        return false;
      }
    };
    mm.addMessageListener("debug:setup-in-parent", onSetupInParent);

    let onActorCreated = DevToolsUtils.makeInfallible(function (msg) {
      if (msg.json.prefix != prefix) {
        return;
      }
      mm.removeMessageListener("debug:actor", onActorCreated);

      // Pipe Debugger message from/to parent/child via the message manager
      childTransport = new ChildDebuggerTransport(mm, prefix);
      childTransport.hooks = {
        onPacket: connection.send.bind(connection),
        onClosed() {}
      };
      childTransport.ready();

      connection.setForwarding(prefix, childTransport);

      dumpn("establishing forwarding for app with prefix " + prefix);

      actor = msg.json.actor;

      let { NetworkMonitorManager } = require("devtools/shared/webconsole/network-monitor");
      netMonitor = new NetworkMonitorManager(frame, actor.actor);

      events.emit(DebuggerServer, "new-child-process", { mm });

      deferred.resolve(actor);
    }).bind(this);
    mm.addMessageListener("debug:actor", onActorCreated);

    let destroy = DevToolsUtils.makeInfallible(function () {
      // provides hook to actor modules that need to exchange messages
      // between e10s parent and child processes
      DebuggerServer.emit("disconnected-from-child:" + prefix, { mm, prefix });

      if (childTransport) {
        // If we have a child transport, the actor has already
        // been created. We need to stop using this message manager.
        childTransport.close();
        childTransport = null;
        connection.cancelForwarding(prefix);

        // ... and notify the child process to clean the tab actors.
        try {
          // Bug 1169643: Ignore any exception as the child process
          // may already be destroyed by now.
          mm.sendAsyncMessage("debug:disconnect", { prefix });
        } catch (e) {
          // Nothing to do
        }
      } else {
        // Otherwise, the app has been closed before the actor
        // had a chance to be created, so we are not able to create
        // the actor.
        deferred.resolve(null);
      }
      if (actor) {
        // The ContentActor within the child process doesn't necessary
        // have time to uninitialize itself when the app is closed/killed.
        // So ensure telling the client that the related actor is detached.
        connection.send({ from: actor.actor, type: "tabDetached" });
        actor = null;
      }

      if (netMonitor) {
        netMonitor.destroy();
        netMonitor = null;
      }

      if (onDestroy) {
        onDestroy(mm);
      }

      // Cleanup all listeners
      Services.obs.removeObserver(onMessageManagerClose, "message-manager-close");
      mm.removeMessageListener("debug:setup-in-parent", onSetupInParent);
      if (!actor) {
        mm.removeMessageListener("debug:actor", onActorCreated);
      }
      events.off(connection, "closed", destroy);

      DebuggerServer._childMessageManagers.delete(mm);
    });

    // Listen for app process exit
    let onMessageManagerClose = function (subject, topic, data) {
      if (subject == mm) {
        destroy();
      }
    };
    Services.obs.addObserver(onMessageManagerClose,
                             "message-manager-close", false);

    // Listen for connection close to cleanup things
    // when user unplug the device or we lose the connection somehow.
    events.on(connection, "closed", destroy);

    mm.sendAsyncMessage("debug:connect", { prefix });

    return deferred.promise;
  },

  /**
   * Create a new debugger connection for the given transport. Called after
   * connectPipe(), from connectToParent, or from an incoming socket
   * connection handler.
   *
   * If present, |forwardingPrefix| is a forwarding prefix that a parent
   * server is using to recognizes messages intended for this server. Ensure
   * that all our actors have names beginning with |forwardingPrefix + '/'|.
   * In particular, the root actor's name will be |forwardingPrefix + '/root'|.
   */
  _onConnection(transport, forwardingPrefix, noRootActor = false) {
    let connID;
    if (forwardingPrefix) {
      connID = forwardingPrefix + "/";
    } else {
      // Multiple servers can be started at the same time, and when that's the
      // case, they are loaded in separate devtools loaders.
      // So, use the current loader ID to prefix the connection ID and make it
      // unique.
      connID = "server" + loader.id + ".conn" + this._nextConnID++ + ".";
    }

    let conn = new DebuggerServerConnection(connID, transport);
    this._connections[connID] = conn;

    // Create a root actor for the connection and send the hello packet.
    if (!noRootActor) {
      conn.rootActor = this.createRootActor(conn);
      if (forwardingPrefix) {
        conn.rootActor.actorID = forwardingPrefix + "/root";
      } else {
        conn.rootActor.actorID = "root";
      }
      conn.addActor(conn.rootActor);
      transport.send(conn.rootActor.sayHello());
    }
    transport.ready();

    this.emit("connectionchange", "opened", conn);
    return conn;
  },

  /**
   * Remove the connection from the debugging server.
   */
  _connectionClosed(connection) {
    delete this._connections[connection.prefix];
    this.emit("connectionchange", "closed", connection);
  },

  // DebuggerServer extension API.

  setRootActor(actorFactory) {
    this.createRootActor = actorFactory;
  },

  /**
   * Registers handlers for new tab-scoped request types defined dynamically.
   * This is used for example by add-ons to augment the functionality of the tab
   * actor. Note that the name or actorPrefix of the request type is not allowed
   * to clash with existing protocol packet properties, like 'title', 'url' or
   * 'actor', since that would break the protocol.
   *
   * @param actor function, object
   *      In case of function:
   *        The constructor function for this request type. This expects to be
   *        called as a constructor (i.e. with 'new'), and passed two
   *        arguments: the DebuggerServerConnection, and the BrowserTabActor
   *        with which it will be associated.
   *        Only used for deprecated eagerly loaded actors.
   *      In case of object:
   *        First argument of RegisteredActorFactory constructor.
   *        See the it's definition for more info.
   *
   * @param name string [optional]
   *        The name of the new request type. If this is not present, the
   *        actorPrefix property of the constructor prototype is used.
   */
  addTabActor(actor, name = actor.prototype.actorPrefix) {
    if (["title", "url", "actor"].indexOf(name) != -1) {
      throw Error(name + " is not allowed");
    }
    if (DebuggerServer.tabActorFactories.hasOwnProperty(name)) {
      throw Error(name + " already exists");
    }
    DebuggerServer.tabActorFactories[name] = new RegisteredActorFactory(actor, name);
  },

  /**
   * Unregisters the handler for the specified tab-scoped request type.
   * This may be used for example by add-ons when shutting down or upgrading.
   * When unregistering an existing tab actor remove related tab factory
   * as well as all existing instances of the actor.
   *
   * @param actor function, object
   *      In case of function:
   *        The constructor function for this request type.
   *      In case of object:
   *        Same object being given to related addTabActor call.
   */
  removeTabActor(actor) {
    for (let name in DebuggerServer.tabActorFactories) {
      let handler = DebuggerServer.tabActorFactories[name];
      if ((handler.name && handler.name == actor.name) ||
          (handler.id && handler.id == actor.id)) {
        delete DebuggerServer.tabActorFactories[name];
        for (let connID of Object.getOwnPropertyNames(this._connections)) {
          // DebuggerServerConnection in child process don't have rootActor
          if (this._connections[connID].rootActor) {
            this._connections[connID].rootActor.removeActorByName(name);
          }
        }
      }
    }
  },

  /**
   * Registers handlers for new browser-scoped request types defined
   * dynamically. This is used for example by add-ons to augment the
   * functionality of the root actor. Note that the name or actorPrefix of the
   * request type is not allowed to clash with existing protocol packet
   * properties, like 'from', 'tabs' or 'selected', since that would break the
   * protocol.
   *
   * @param actor function, object
   *      In case of function:
   *        The constructor function for this request type. This expects to be
   *        called as a constructor (i.e. with 'new'), and passed two
   *        arguments: the DebuggerServerConnection, and the BrowserRootActor
   *        with which it will be associated.
   *        Only used for deprecated eagerly loaded actors.
   *      In case of object:
   *        First argument of RegisteredActorFactory constructor.
   *        See the it's definition for more info.
   *
   * @param name string [optional]
   *        The name of the new request type. If this is not present, the
   *        actorPrefix property of the constructor prototype is used.
   */
  addGlobalActor(actor, name = actor.prototype.actorPrefix) {
    if (["from", "tabs", "selected"].indexOf(name) != -1) {
      throw Error(name + " is not allowed");
    }
    if (DebuggerServer.globalActorFactories.hasOwnProperty(name)) {
      throw Error(name + " already exists");
    }
    DebuggerServer.globalActorFactories[name] = new RegisteredActorFactory(actor, name);
  },

  /**
   * Unregisters the handler for the specified browser-scoped request type.
   * This may be used for example by add-ons when shutting down or upgrading.
   * When unregistering an existing global actor remove related global factory
   * as well as all existing instances of the actor.
   *
   * @param actor function, object
   *      In case of function:
   *        The constructor function for this request type.
   *      In case of object:
   *        Same object being given to related addGlobalActor call.
   */
  removeGlobalActor(actor) {
    for (let name in DebuggerServer.globalActorFactories) {
      let handler = DebuggerServer.globalActorFactories[name];
      if ((handler.name && handler.name == actor.name) ||
          (handler.id && handler.id == actor.id)) {
        delete DebuggerServer.globalActorFactories[name];
        for (let connID of Object.getOwnPropertyNames(this._connections)) {
          this._connections[connID].rootActor.removeActorByName(name);
        }
      }
    }
  },

  /**
   * ⚠ TESTING ONLY! ⚠ Searches all active connections for an actor matching an ID.
   * This is helpful for some tests which depend on reaching into the server to check some
   * properties of an actor.
   */
  _searchAllConnectionsForActor(actorID) {
    for (let connID of Object.getOwnPropertyNames(this._connections)) {
      let actor = this._connections[connID].getActor(actorID);
      if (actor) {
        return actor;
      }
    }
    return null;
  },
};

// Expose these to save callers the trouble of importing DebuggerSocket
DevToolsUtils.defineLazyGetter(DebuggerServer, "Authenticators", () => {
  return Authentication.Authenticators;
});
DevToolsUtils.defineLazyGetter(DebuggerServer, "AuthenticationResult", () => {
  return Authentication.AuthenticationResult;
});

EventEmitter.decorate(DebuggerServer);

if (this.exports) {
  exports.DebuggerServer = DebuggerServer;
  exports.ActorPool = ActorPool;
  exports.OriginalLocation = OriginalLocation;
}

// Needed on B2G (See header note)
this.DebuggerServer = DebuggerServer;
this.ActorPool = ActorPool;
this.OriginalLocation = OriginalLocation;

// When using DebuggerServer.addActors, some symbols are expected to be in
// the scope of the added actor even before the corresponding modules are
// loaded, so let's explicitly bind the expected symbols here.
var includes = ["Components", "Ci", "Cu", "require", "Services", "DebuggerServer",
                "ActorPool", "DevToolsUtils"];
includes.forEach(name => {
  DebuggerServer[name] = this[name];
});

/**
 * Creates a DebuggerServerConnection.
 *
 * Represents a connection to this debugging global from a client.
 * Manages a set of actors and actor pools, allocates actor ids, and
 * handles incoming requests.
 *
 * @param prefix string
 *        All actor IDs created by this connection should be prefixed
 *        with prefix.
 * @param transport transport
 *        Packet transport for the debugging protocol.
 */
function DebuggerServerConnection(prefix, transport) {
  this._prefix = prefix;
  this._transport = transport;
  this._transport.hooks = this;
  this._nextID = 1;

  this._actorPool = new ActorPool(this);
  this._extraPools = [this._actorPool];

  // Responses to a given actor must be returned the the client
  // in the same order as the requests that they're replying to, but
  // Implementations might finish serving requests in a different
  // order.  To keep things in order we generate a promise for each
  // request, chained to the promise for the request before it.
  // This map stores the latest request promise in the chain, keyed
  // by an actor ID string.
  this._actorResponses = new Map();

  /*
   * We can forward packets to other servers, if the actors on that server
   * all use a distinct prefix on their names. This is a map from prefixes
   * to transports: it maps a prefix P to a transport T if T conveys
   * packets to the server whose actors' names all begin with P + "/".
   */
  this._forwardingPrefixes = new Map();
}

DebuggerServerConnection.prototype = {
  _prefix: null,
  get prefix() {
    return this._prefix;
  },

  _transport: null,
  get transport() {
    return this._transport;
  },

  /**
   * Message manager used to communicate with the parent process,
   * set by child.js. Is only defined for connections instantiated
   * within a child process.
   */
  parentMessageManager: null,

  close() {
    if (this._transport) {
      this._transport.close();
    }
  },

  send(packet) {
    this.transport.send(packet);
  },

  /**
   * Used when sending a bulk reply from an actor.
   * @see DebuggerTransport.prototype.startBulkSend
   */
  startBulkSend(header) {
    return this.transport.startBulkSend(header);
  },

  allocID(prefix) {
    return this.prefix + (prefix || "") + this._nextID++;
  },

  /**
   * Add a map of actor IDs to the connection.
   */
  addActorPool(actorPool) {
    this._extraPools.push(actorPool);
  },

  /**
   * Remove a previously-added pool of actors to the connection.
   *
   * @param ActorPool actorPool
   *        The ActorPool instance you want to remove.
   * @param boolean noCleanup [optional]
   *        True if you don't want to disconnect each actor from the pool, false
   *        otherwise.
   */
  removeActorPool(actorPool, noCleanup) {
    // When a connection is closed, it removes each of its actor pools. When an
    // actor pool is removed, it calls the disconnect method on each of its
    // actors. Some actors, such as ThreadActor, manage their own actor pools.
    // When the disconnect method is called on these actors, they manually
    // remove their actor pools. Consequently, this method is reentrant.
    //
    // In addition, some actors, such as ThreadActor, perform asynchronous work
    // (in the case of ThreadActor, because they need to resume), before they
    // remove each of their actor pools. Since we don't wait for this work to
    // be completed, we can end up in this function recursively after the
    // connection already set this._extraPools to null.
    //
    // This is a bug: if the disconnect method can perform asynchronous work,
    // then we should wait for that work to be completed before setting this.
    // _extraPools to null. As a temporary solution, it should be acceptable
    // to just return early (if this._extraPools has been set to null, all
    // actors pools for this connection should already have been removed).
    if (this._extraPools === null) {
      return;
    }
    let index = this._extraPools.lastIndexOf(actorPool);
    if (index > -1) {
      let pool = this._extraPools.splice(index, 1);
      if (!noCleanup) {
        pool.forEach(p => p.destroy());
      }
    }
  },

  /**
   * Add an actor to the default actor pool for this connection.
   */
  addActor(actor) {
    this._actorPool.addActor(actor);
  },

  /**
   * Remove an actor to the default actor pool for this connection.
   */
  removeActor(actor) {
    this._actorPool.removeActor(actor);
  },

  /**
   * Match the api expected by the protocol library.
   */
  unmanage(actor) {
    return this.removeActor(actor);
  },

  /**
   * Look up an actor implementation for an actorID.  Will search
   * all the actor pools registered with the connection.
   *
   * @param actorID string
   *        Actor ID to look up.
   */
  getActor(actorID) {
    let pool = this.poolFor(actorID);
    if (pool) {
      return pool.get(actorID);
    }

    if (actorID === "root") {
      return this.rootActor;
    }

    return null;
  },

  _getOrCreateActor(actorID) {
    let actor = this.getActor(actorID);
    if (!actor) {
      this.transport.send({ from: actorID ? actorID : "root",
                            error: "noSuchActor",
                            message: "No such actor for ID: " + actorID });
      return null;
    }

    // Dynamically-loaded actors have to be created lazily.
    if (actor instanceof ObservedActorFactory) {
      try {
        actor = actor.createActor();
      } catch (e) {
        this.transport.send(this._unknownError(
          "Error occurred while creating actor '" + actor.name,
          e));
      }
    } else if (typeof (actor) !== "object") {
      // ActorPools should now contain only actor instances (i.e. objects)
      // or ObservedActorFactory instances.
      throw new Error("Unexpected actor constructor/function in ActorPool " +
                      "for actorID=" + actorID + ".");
    }

    return actor;
  },

  poolFor(actorID) {
    for (let pool of this._extraPools) {
      if (pool.has(actorID)) {
        return pool;
      }
    }
    return null;
  },

  _unknownError(prefix, error) {
    let errorString = prefix + ": " + DevToolsUtils.safeErrorString(error);
    reportError(errorString);
    dumpn(errorString);
    return {
      error: "unknownError",
      message: errorString
    };
  },

  _queueResponse: function (from, type, responseOrPromise) {
    let pendingResponse = this._actorResponses.get(from) || SyncPromise.resolve(null);
    let responsePromise = pendingResponse.then(() => {
      return responseOrPromise;
    }).then(response => {
      if (!response.from) {
        response.from = from;
      }
      this.transport.send(response);
    }).then(null, (e) => {
      let errorPacket = this._unknownError(
        "error occurred while processing '" + type, e);
      errorPacket.from = from;
      this.transport.send(errorPacket);
    });

    this._actorResponses.set(from, responsePromise);
  },

  /**
   * Passes a set of options to the BrowserAddonActors for the given ID.
   *
   * @param id string
   *        The ID of the add-on to pass the options to
   * @param options object
   *        The options.
   * @return a promise that will be resolved when complete.
   */
  setAddonOptions(id, options) {
    let addonList = this.rootActor._parameters.addonList;
    if (!addonList) {
      return SyncPromise.resolve();
    }
    return addonList.getList().then((addonActors) => {
      for (let actor of addonActors) {
        if (actor.id != id) {
          continue;
        }
        actor.setOptions(options);
        return;
      }
    });
  },

  /* Forwarding packets to other transports based on actor name prefixes. */

  /*
   * Arrange to forward packets to another server. This is how we
   * forward debugging connections to child processes.
   *
   * If we receive a packet for an actor whose name begins with |prefix|
   * followed by '/', then we will forward that packet to |transport|.
   *
   * This overrides any prior forwarding for |prefix|.
   *
   * @param prefix string
   *    The actor name prefix, not including the '/'.
   * @param transport object
   *    A packet transport to which we should forward packets to actors
   *    whose names begin with |(prefix + '/').|
   */
  setForwarding(prefix, transport) {
    this._forwardingPrefixes.set(prefix, transport);
  },

  /*
   * Stop forwarding messages to actors whose names begin with
   * |prefix+'/'|. Such messages will now elicit 'noSuchActor' errors.
   */
  cancelForwarding(prefix) {
    this._forwardingPrefixes.delete(prefix);

    // Notify the client that forwarding in now cancelled for this prefix.
    // There could be requests in progress that the client should abort rather leaving
    // handing indefinitely.
    if (this.rootActor) {
      this.send(this.rootActor.forwardingCancelled(prefix));
    }
  },

  sendActorEvent(actorID, eventName, event = {}) {
    event.from = actorID;
    event.type = eventName;
    this.send(event);
  },

  // Transport hooks.

  /**
   * Called by DebuggerTransport to dispatch incoming packets as appropriate.
   *
   * @param packet object
   *        The incoming packet.
   */
  onPacket(packet) {
    // If the actor's name begins with a prefix we've been asked to
    // forward, do so.
    //
    // Note that the presence of a prefix alone doesn't indicate that
    // forwarding is needed: in DebuggerServerConnection instances in child
    // processes, every actor has a prefixed name.
    if (this._forwardingPrefixes.size > 0) {
      let to = packet.to;
      let separator = to.lastIndexOf("/");
      while (separator >= 0) {
        to = to.substring(0, separator);
        let forwardTo = this._forwardingPrefixes.get(packet.to.substring(0, separator));
        if (forwardTo) {
          forwardTo.send(packet);
          return;
        }
        separator = to.lastIndexOf("/");
      }
    }

    let actor = this._getOrCreateActor(packet.to);
    if (!actor) {
      return;
    }

    let ret = null;

    // handle "requestTypes" RDP request.
    if (packet.type == "requestTypes") {
      ret = { from: actor.actorID, requestTypes: Object.keys(actor.requestTypes) };
    } else if (actor.requestTypes && actor.requestTypes[packet.type]) {
      // Dispatch the request to the actor.
      try {
        this.currentPacket = packet;
        ret = actor.requestTypes[packet.type].bind(actor)(packet, this);
      } catch (e) {
        this.transport.send(this._unknownError(
          "error occurred while processing '" + packet.type,
          e));
      } finally {
        this.currentPacket = undefined;
      }
    } else {
      ret = { error: "unrecognizedPacketType",
              message: ("Actor " + actor.actorID +
                        " does not recognize the packet type " +
                        packet.type) };
    }

    // There will not be a return value if a bulk reply is sent.
    if (ret) {
      this._queueResponse(packet.to, packet.type, ret);
    }
  },

  /**
   * Called by the DebuggerTransport to dispatch incoming bulk packets as
   * appropriate.
   *
   * @param packet object
   *        The incoming packet, which contains:
   *        * actor:  Name of actor that will receive the packet
   *        * type:   Name of actor's method that should be called on receipt
   *        * length: Size of the data to be read
   *        * stream: This input stream should only be used directly if you can
   *                  ensure that you will read exactly |length| bytes and will
   *                  not close the stream when reading is complete
   *        * done:   If you use the stream directly (instead of |copyTo|
   *                  below), you must signal completion by resolving /
   *                  rejecting this deferred.  If it's rejected, the transport
   *                  will be closed.  If an Error is supplied as a rejection
   *                  value, it will be logged via |dumpn|.  If you do use
   *                  |copyTo|, resolving is taken care of for you when copying
   *                  completes.
   *        * copyTo: A helper function for getting your data out of the stream
   *                  that meets the stream handling requirements above, and has
   *                  the following signature:
   *          @param  output nsIAsyncOutputStream
   *                  The stream to copy to.
   *          @return Promise
   *                  The promise is resolved when copying completes or rejected
   *                  if any (unexpected) errors occur.
   *                  This object also emits "progress" events for each chunk
   *                  that is copied.  See stream-utils.js.
   */
  onBulkPacket(packet) {
    let { actor: actorKey, type } = packet;

    let actor = this._getOrCreateActor(actorKey);
    if (!actor) {
      return;
    }

    // Dispatch the request to the actor.
    let ret;
    if (actor.requestTypes && actor.requestTypes[type]) {
      try {
        ret = actor.requestTypes[type].call(actor, packet);
      } catch (e) {
        this.transport.send(this._unknownError(
          "error occurred while processing bulk packet '" + type, e));
        packet.done.reject(e);
      }
    } else {
      let message = "Actor " + actorKey +
                    " does not recognize the bulk packet type " + type;
      ret = { error: "unrecognizedPacketType",
              message: message };
      packet.done.reject(new Error(message));
    }

    // If there is a JSON response, queue it for sending back to the client.
    if (ret) {
      this._queueResponse(actorKey, type, ret);
    }
  },

  /**
   * Called by DebuggerTransport when the underlying stream is closed.
   *
   * @param status nsresult
   *        The status code that corresponds to the reason for closing
   *        the stream.
   */
  onClosed(status) {
    dumpn("Cleaning up connection.");
    if (!this._actorPool) {
      // Ignore this call if the connection is already closed.
      return;
    }
    this._actorPool = null;

    events.emit(this, "closed", status);

    this._extraPools.forEach(p => p.destroy());
    this._extraPools = null;

    this.rootActor = null;
    this._transport = null;
    DebuggerServer._connectionClosed(this);
  },

  /*
   * Debugging helper for inspecting the state of the actor pools.
   */
  _dumpPools() {
    dumpn("/-------------------- dumping pools:");
    if (this._actorPool) {
      dumpn("--------------------- actorPool actors: " +
            uneval(Object.keys(this._actorPool._actors)));
    }
    for (let pool of this._extraPools) {
      if (pool !== this._actorPool) {
        dumpn("--------------------- extraPool actors: " +
              uneval(Object.keys(pool._actors)));
      }
    }
  },

  /*
   * Debugging helper for inspecting the state of an actor pool.
   */
  _dumpPool(pool) {
    dumpn("/-------------------- dumping pool:");
    dumpn("--------------------- actorPool actors: " +
          uneval(Object.keys(pool._actors)));
  },

  /**
   * In a content child process, ask the DebuggerServer in the parent process
   * to execute a given module setup helper.
   *
   * @param module
   *        The module to be required
   * @param setupParent
   *        The name of the setup helper exported by the above module
   *        (setup helper signature: function ({mm}) { ... })
   * @return boolean
   *         true if the setup helper returned successfully
   */
  setupInParent({ module, setupParent }) {
    if (!this.parentMessageManager) {
      return false;
    }

    let { sendSyncMessage } = this.parentMessageManager;

    return sendSyncMessage("debug:setup-in-parent", {
      prefix: this.prefix,
      module: module,
      setupParent: setupParent
    });
  },
};
