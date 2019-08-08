/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { Ci, Cc } = require("chrome");
var Services = require("Services");
var { ActorRegistry } = require("devtools/server/actors/utils/actor-registry");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var { dumpn } = DevToolsUtils;

loader.lazyRequireGetter(
  this,
  "DebuggerServerConnection",
  "devtools/server/debugger-server-connection",
  true
);
loader.lazyRequireGetter(
  this,
  "Authentication",
  "devtools/shared/security/auth"
);
loader.lazyRequireGetter(
  this,
  "LocalDebuggerTransport",
  "devtools/shared/transport/local-transport",
  true
);
loader.lazyRequireGetter(
  this,
  "ChildDebuggerTransport",
  "devtools/shared/transport/child-transport",
  true
);
loader.lazyRequireGetter(
  this,
  "WorkerThreadWorkerDebuggerTransport",
  "devtools/shared/transport/worker-transport",
  true
);
loader.lazyRequireGetter(
  this,
  "MainThreadWorkerDebuggerTransport",
  "devtools/shared/transport/worker-transport",
  true
);

loader.lazyGetter(this, "generateUUID", () => {
  // eslint-disable-next-line no-shadow
  const { generateUUID } = Cc["@mozilla.org/uuid-generator;1"].getService(
    Ci.nsIUUIDGenerator
  );
  return generateUUID;
});

const CONTENT_PROCESS_SERVER_STARTUP_SCRIPT =
  "resource://devtools/server/startup/content-process.js";

loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");

/**
 * DebuggerServer is a singleton that has several responsibilities. It will
 * register the DevTools server actors that are relevant to the context.
 * It can also create other DebuggerServer, that will live in the same
 * environment as the debugged target (content page, worker...).
 *
 * For instance a regular Toolbox will be linked to DebuggerClient connected to
 * a DebuggerServer running in the same process as the Toolbox (main process).
 * But another DebuggerServer will be created in the same process as the page
 * targeted by the Toolbox.
 *
 * Despite being a singleton, the DebuggerServer still has a lifecycle and a
 * state. When a consumer needs to spawn a DebuggerServer, the init() method
 * should be called. Then you should either call registerAllActors or
 * registerActors to setup the server.
 * When the server is no longer needed, destroy() should be called.
 *
 */
var DebuggerServer = {
  _listeners: [],
  _initialized: false,
  // Flag to check if the content process server startup script was already loaded.
  _contentProcessServerStartupScriptLoaded: false,
  // Map of global actor names to actor constructors.
  globalActorFactories: {},
  // Map of target-scoped actor names to actor constructors.
  targetScopedActorFactories: {},

  LONG_STRING_LENGTH: 10000,
  LONG_STRING_INITIAL_LENGTH: 1000,
  LONG_STRING_READ_LENGTH: 65 * 1024,

  /**
   * The windowtype of the chrome window to use for actors that use the global
   * window (i.e the global style editor). Set this to your main window type,
   * for example "navigator:browser".
   */
  chromeWindowType: "navigator:browser",

  /**
   * Allow debugging chrome of (parent or child) processes.
   */
  allowChromeProcess: false,

  /**
   * Flag used to check if the server can be destroyed when all connections have been
   * removed. Firefox on Android runs a single shared DebuggerServer, and should not be
   * closed even if no client is connected.
   */
  keepAlive: false,

  /**
   * We run a special server in child process whose main actor is an instance
   * of FrameTargetActor, but that isn't a root actor. Instead there is no root
   * actor registered on DebuggerServer.
   */
  get rootlessServer() {
    return !this.createRootActor;
  },

  /**
   * Initialize the debugger server.
   */
  init() {
    if (this.initialized) {
      return;
    }

    this._connections = {};
    ActorRegistry.init(this._connections);
    this._nextConnID = 0;

    this._initialized = true;
    this._onSocketListenerAccepted = this._onSocketListenerAccepted.bind(this);
  },

  get protocol() {
    return require("devtools/shared/protocol");
  },

  get initialized() {
    return this._initialized;
  },

  hasConnection() {
    return Object.keys(this._connections).length > 0;
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

    for (const connID of Object.getOwnPropertyNames(this._connections)) {
      this._connections[connID].close();
    }

    ActorRegistry.destroy();

    this.closeAllSocketListeners();
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

    if (!this.rootlessServer && !this.createRootActor) {
      throw new Error(
        "Use DebuggerServer.setRootActor() to add a root actor " +
          "implementation."
      );
    }
  },

  /**
   * Register different type of actors. Only register the one that are not already
   * registered.
   *
   * @param root boolean
   *        Registers the root actor from webbrowser module, which is used to
   *        connect to and fetch any other actor.
   * @param browser boolean
   *        Registers all the parent process actors useful for debugging the
   *        runtime itself, like preferences and addons actors.
   * @param target boolean
   *        Registers all the target-scoped actors like console, script, etc.
   *        for debugging a target context.
   */
  registerActors({ root, browser, target }) {
    if (browser) {
      ActorRegistry.addBrowserActors();
    }

    if (root) {
      const { createRootActor } = require("devtools/server/actors/webbrowser");
      this.setRootActor(createRootActor);
    }

    if (target) {
      ActorRegistry.addTargetScopedActors();
    }
  },

  /**
   * Register all possible actors for this DebuggerServer.
   */
  registerAllActors() {
    this.registerActors({ root: true, browser: true, target: true });
  },

  get listeningSockets() {
    return this._listeners.length;
  },

  /**
   * Add a SocketListener instance to the server's set of active
   * SocketListeners.  This is called by a SocketListener after it is opened.
   */
  addSocketListener(listener) {
    if (!Services.prefs.getBoolPref("devtools.debugger.remote-enabled")) {
      throw new Error("Can't add a SocketListener, remote debugging disabled");
    }
    this._checkInit();

    listener.on("accepted", this._onSocketListenerAccepted);
    this._listeners.push(listener);
  },

  /**
   * Remove a SocketListener instance from the server's set of active
   * SocketListeners.  This is called by a SocketListener after it is closed.
   */
  removeSocketListener(listener) {
    // Remove connections that were accepted in the listener.
    for (const connID of Object.getOwnPropertyNames(this._connections)) {
      const connection = this._connections[connID];
      if (connection.isAcceptedBy(listener)) {
        connection.close();
      }
    }

    this._listeners = this._listeners.filter(l => l !== listener);
    listener.off("accepted", this._onSocketListenerAccepted);
  },

  /**
   * Closes and forgets all previously opened listeners.
   *
   * @return boolean
   *         Whether any listeners were actually closed.
   */
  closeAllSocketListeners() {
    if (!this.listeningSockets) {
      return false;
    }

    for (const listener of this._listeners) {
      listener.close();
    }

    return true;
  },

  _onSocketListenerAccepted(transport, listener) {
    this._onConnection(transport, null, false, listener);
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

    const serverTransport = new LocalDebuggerTransport();
    const clientTransport = new LocalDebuggerTransport(serverTransport);
    serverTransport.other = clientTransport;
    const connection = this._onConnection(serverTransport, prefix);

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

    const transport = isWorker
      ? new WorkerThreadWorkerDebuggerTransport(scopeOrManager, prefix)
      : new ChildDebuggerTransport(scopeOrManager, prefix);

    return this._onConnection(transport, prefix, true);
  },

  /**
   * Start a DevTools server in a content process (representing the entire process, not
   * just a single frame) and add it as a child server for an active connection.
   */
  connectToContentProcess(connection, mm, onDestroy) {
    return new Promise(resolve => {
      const prefix = connection.allocID("content-process");
      let actor, childTransport;

      mm.addMessageListener("debug:content-process-actor", function listener(
        msg
      ) {
        // Arbitrarily choose the first content process to reply
        // XXX: This code needs to be updated if we use more than one content process
        mm.removeMessageListener("debug:content-process-actor", listener);

        // Pipe Debugger message from/to parent/child via the message manager
        childTransport = new ChildDebuggerTransport(mm, prefix);
        childTransport.hooks = {
          onPacket: connection.send.bind(connection),
          onClosed() {},
        };
        childTransport.ready();

        connection.setForwarding(prefix, childTransport);

        dumpn(`Start forwarding for process with prefix ${prefix}`);

        actor = msg.json.actor;

        resolve(actor);
      });

      // Load the content process server startup script only once.
      if (!this._contentProcessServerStartupScriptLoaded) {
        // Load the process script that will receive the debug:init-content-server message
        Services.ppmm.loadProcessScript(
          CONTENT_PROCESS_SERVER_STARTUP_SCRIPT,
          true
        );
        this._contentProcessServerStartupScriptLoaded = true;
      }

      // Send a message to the content process server startup script to forward it the
      // prefix.
      mm.sendAsyncMessage("debug:init-content-server", {
        prefix: prefix,
      });

      function onClose() {
        Services.obs.removeObserver(
          onMessageManagerClose,
          "message-manager-close"
        );
        EventEmitter.off(connection, "closed", onClose);
        if (childTransport) {
          // If we have a child transport, the actor has already
          // been created. We need to stop using this message manager.
          childTransport.close();
          childTransport = null;
          connection.cancelForwarding(prefix);

          // ... and notify the child process to clean the target-scoped actors.
          try {
            mm.sendAsyncMessage("debug:content-process-destroy");
          } catch (e) {
            // Nothing to do
          }
        }

        if (onDestroy) {
          onDestroy(mm);
        }
      }

      const onMessageManagerClose = DevToolsUtils.makeInfallible(
        (subject, topic, data) => {
          if (subject == mm) {
            onClose();
            connection.send({ from: actor.actor, type: "tabDetached" });
          }
        }
      );
      Services.obs.addObserver(onMessageManagerClose, "message-manager-close");

      EventEmitter.on(connection, "closed", onClose);
    });
  },

  /**
   * Start a DevTools server in a worker and add it as a child server for an active
   * connection.
   */
  connectToWorker(connection, dbg, id, options) {
    return new Promise((resolve, reject) => {
      // Step 1: Ensure the worker debugger is initialized.
      if (!dbg.isInitialized) {
        dbg.initialize("resource://devtools/server/startup/worker.js");

        // Create a listener for rpc requests from the worker debugger. Only do
        // this once, when the worker debugger is first initialized, rather than
        // for each connection.
        const listener = {
          onClose: () => {
            dbg.removeListener(listener);
          },

          onMessage: message => {
            message = JSON.parse(message);
            if (message.type !== "rpc") {
              if (message.type == "attached") {
                // The thread actor has finished attaching and can hit installed
                // breakpoints. Allow content to begin executing in the worker.
                dbg.setDebuggerReady(true);
              }
              return;
            }

            Promise.resolve()
              .then(() => {
                const method = {
                  fetch: DevToolsUtils.fetch,
                }[message.method];
                if (!method) {
                  throw Error("Unknown method: " + message.method);
                }

                return method.apply(undefined, message.params);
              })
              .then(
                value => {
                  dbg.postMessage(
                    JSON.stringify({
                      type: "rpc",
                      result: value,
                      error: null,
                      id: message.id,
                    })
                  );
                },
                reason => {
                  dbg.postMessage(
                    JSON.stringify({
                      type: "rpc",
                      result: null,
                      error: reason,
                      id: message.id,
                    })
                  );
                }
              );
          },
        };

        dbg.addListener(listener);
      }

      // Step 2: Send a connect request to the worker debugger.
      dbg.postMessage(
        JSON.stringify({
          type: "connect",
          id,
          options,
        })
      );

      // Steps 3-5 are performed on the worker thread (see worker.js).

      // Step 6: Wait for a connection response from the worker debugger.
      const listener = {
        onClose: () => {
          dbg.removeListener(listener);

          reject("closed");
        },

        onMessage: message => {
          message = JSON.parse(message);
          if (message.type !== "connected" || message.id !== id) {
            return;
          }

          // The initial connection message has been received, don't
          // need to listen any longer
          dbg.removeListener(listener);

          // Step 7: Create a transport for the connection to the worker.
          const transport = new MainThreadWorkerDebuggerTransport(dbg, id);
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
                  dbg.postMessage(
                    JSON.stringify({
                      type: "disconnect",
                      id,
                    })
                  );
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

            onPacket: packet => {
              // Ensure that any packets received from the server on the worker
              // thread are forwarded to the client on the main thread, as if
              // they had been sent by the server on the main thread.
              connection.send(packet);
            },
          };

          // Ensure that any packets received from the client on the main thread
          // to actors on the worker thread are forwarded to the server on the
          // worker thread.
          connection.setForwarding(id, transport);

          resolve({
            threadActor: message.threadActor,
            consoleActor: message.consoleActor,
            transport: transport,
          });
        },
      };
      dbg.addListener(listener);
    });
  },

  /**
   * Check if the server is running in the child process.
   */
  get isInChildProcess() {
    return (
      Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT
    );
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
    return new Promise(done => {
      // If waitForEval is set, pass a unique id and expect child.js to send
      // a message back once the code in child is evaluated.
      if (typeof waitForEval != "boolean") {
        waitForEval = false;
      }
      let count = this._childMessageManagers.size;
      const id = waitForEval ? generateUUID().toString() : null;

      this._childMessageManagers.forEach(mm => {
        if (waitForEval) {
          // Listen for the end of each child execution
          const evalListener = msg => {
            if (msg.data.id !== id) {
              return;
            }
            mm.removeMessageListener(
              "debug:setup-in-child-response",
              evalListener
            );
            if (--count === 0) {
              done();
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

      if (!waitForEval) {
        done();
      }
    });
  },

  /**
   * Live list of all currenctly attached child's message managers.
   */
  _childMessageManagers: new Set(),

  /**
   * Start a DevTools server in a remote frame's process and add it as a child server for
   * an active connection.
   *
   * @param object connection
   *        The debugger server connection to use.
   * @param Element frame
   *        The frame element with remote content to connect to.
   * @param function [onDestroy]
   *        Optional function to invoke when the child process closes or the connection
   *        shuts down. (Need to forget about the related target actor.)
   * @return object
   *         A promise object that is resolved once the connection is established.
   */
  connectToFrame(connection, frame, onDestroy, { addonId } = {}) {
    return new Promise(resolve => {
      // Get messageManager from XUL browser (which might be a specialized tunnel for RDM)
      // or else fallback to asking the frameLoader itself.
      let mm = frame.messageManager || frame.frameLoader.messageManager;
      mm.loadFrameScript("resource://devtools/server/startup/frame.js", false);

      const trackMessageManager = () => {
        frame.addEventListener("DevTools:BrowserSwap", onBrowserSwap);
        mm.addMessageListener("debug:setup-in-parent", onSetupInParent);
        mm.addMessageListener(
          "debug:spawn-actor-in-parent",
          onSpawnActorInParent
        );
        if (!actor) {
          mm.addMessageListener("debug:actor", onActorCreated);
        }
        DebuggerServer._childMessageManagers.add(mm);
      };

      const untrackMessageManager = () => {
        frame.removeEventListener("DevTools:BrowserSwap", onBrowserSwap);
        mm.removeMessageListener("debug:setup-in-parent", onSetupInParent);
        mm.removeMessageListener(
          "debug:spawn-actor-in-parent",
          onSpawnActorInParent
        );
        if (!actor) {
          mm.removeMessageListener("debug:actor", onActorCreated);
        }
        DebuggerServer._childMessageManagers.delete(mm);
      };

      let actor, childTransport;
      const prefix = connection.allocID("child");
      // Compute the same prefix that's used by DebuggerServerConnection
      const connPrefix = prefix + "/";

      // provides hook to actor modules that need to exchange messages
      // between e10s parent and child processes
      const parentModules = [];
      const onSetupInParent = function(msg) {
        // We may have multiple connectToFrame instance running for the same frame and
        // need to filter the messages.
        if (msg.json.prefix != connPrefix) {
          return false;
        }

        const { module, setupParent } = msg.json;
        let m;

        try {
          m = require(module);

          if (!(setupParent in m)) {
            dumpn(`ERROR: module '${module}' does not export '${setupParent}'`);
            return false;
          }

          parentModules.push(m[setupParent]({ mm, prefix: connPrefix }));

          return true;
        } catch (e) {
          const errorMessage =
            "Exception during actor module setup running in the parent process: ";
          DevToolsUtils.reportException(errorMessage + e);
          dumpn(
            `ERROR: ${errorMessage}\n\t module: '${module}'\n\t ` +
              `setupParent: '${setupParent}'\n${DevToolsUtils.safeErrorString(
                e
              )}`
          );
          return false;
        }
      };

      const parentActors = [];
      const onSpawnActorInParent = function(msg) {
        // We may have multiple connectToFrame instance running for the same tab
        // and need to filter the messages.
        if (msg.json.prefix != connPrefix) {
          return;
        }

        const { module, constructor, args, spawnedByActorID } = msg.json;
        let m;

        try {
          m = require(module);

          if (!(constructor in m)) {
            dump(`ERROR: module '${module}' does not export '${constructor}'`);
            return;
          }

          const Constructor = m[constructor];
          // Bind the actor to parent process connection so that these actors
          // directly communicates with the client as regular actors instanciated from
          // parent process
          const instance = new Constructor(connection, ...args, mm);
          instance.conn = connection;
          instance.parentID = spawnedByActorID;

          // Manually set the actor ID in order to insert parent actorID as prefix
          // in order to help identifying actor hiearchy via actor IDs.
          // Remove `/` as it may confuse message forwarding between processes.
          const contentPrefix = spawnedByActorID
            .replace(connection.prefix, "")
            .replace("/", "-");
          instance.actorID = connection.allocID(
            contentPrefix + "/" + instance.typeName
          );
          connection.addActor(instance);

          mm.sendAsyncMessage("debug:spawn-actor-in-parent:actor", {
            prefix: connPrefix,
            actorID: instance.actorID,
          });

          parentActors.push(instance);
        } catch (e) {
          const errorMessage =
            "Exception during actor module setup running in the parent process: ";
          DevToolsUtils.reportException(errorMessage + e + "\n" + e.stack);
          dumpn(
            `ERROR: ${errorMessage}\n\t module: '${module}'\n\t ` +
              `constructor: '${constructor}'\n${DevToolsUtils.safeErrorString(
                e
              )}`
          );
        }
      };

      const onActorCreated = DevToolsUtils.makeInfallible(function(msg) {
        if (msg.json.prefix != prefix) {
          return;
        }
        mm.removeMessageListener("debug:actor", onActorCreated);

        // Pipe Debugger message from/to parent/child via the message manager
        childTransport = new ChildDebuggerTransport(mm, prefix);
        childTransport.hooks = {
          // Pipe all the messages from content process actors back to the client
          // through the parent process connection.
          onPacket: connection.send.bind(connection),
          onClosed() {},
        };
        childTransport.ready();

        connection.setForwarding(prefix, childTransport);

        dumpn(`Start forwarding for frame with prefix ${prefix}`);

        actor = msg.json.actor;
        resolve(actor);
      }).bind(this);

      // Listen for browser frame swap
      const onBrowserSwap = ({ detail: newFrame }) => {
        // Remove listeners from old frame and mm
        untrackMessageManager();
        // Update frame and mm to point to the new browser frame
        frame = newFrame;
        // Get messageManager from XUL browser (which might be a specialized tunnel for
        // RDM) or else fallback to asking the frameLoader itself.
        mm = frame.messageManager || frame.frameLoader.messageManager;
        // Add listeners to new frame and mm
        trackMessageManager();

        // provides hook to actor modules that need to exchange messages
        // between e10s parent and child processes
        parentModules.forEach(mod => {
          if (mod.onBrowserSwap) {
            mod.onBrowserSwap(mm);
          }
        });

        // Also notify actors spawned in the parent process about the new message manager.
        parentActors.forEach(parentActor => {
          if (parentActor.onBrowserSwap) {
            parentActor.onBrowserSwap(mm);
          }
        });

        if (childTransport) {
          childTransport.swapBrowser(mm);
        }
      };

      const destroy = DevToolsUtils.makeInfallible(function() {
        EventEmitter.off(connection, "closed", destroy);
        Services.obs.removeObserver(
          onMessageManagerClose,
          "message-manager-close"
        );

        // provides hook to actor modules that need to exchange messages
        // between e10s parent and child processes
        parentModules.forEach(mod => {
          if (mod.onDisconnected) {
            mod.onDisconnected();
          }
        });
        // TODO: Remove this deprecated path once it's no longer needed by add-ons.
        DebuggerServer.emit("disconnected-from-child:" + connPrefix, {
          mm,
          prefix: connPrefix,
        });

        if (childTransport) {
          // If we have a child transport, the actor has already
          // been created. We need to stop using this message manager.
          childTransport.close();
          childTransport = null;
          connection.cancelForwarding(prefix);

          // ... and notify the child process to clean the target-scoped actors.
          try {
            // Bug 1169643: Ignore any exception as the child process
            // may already be destroyed by now.
            mm.sendAsyncMessage("debug:disconnect", { prefix });
          } catch (e) {
            // Nothing to do
          }
        } else {
          // Otherwise, the frame has been closed before the actor
          // had a chance to be created, so we are not able to create
          // the actor.
          resolve(null);
        }
        if (actor) {
          // The FrameTargetActor within the child process doesn't necessary
          // have time to uninitialize itself when the frame is closed/killed.
          // So ensure telling the client that the related actor is detached.
          connection.send({ from: actor.actor, type: "tabDetached" });
          actor = null;
        }

        if (onDestroy) {
          onDestroy(mm);
        }

        // Cleanup all listeners
        untrackMessageManager();
      });

      // Listen for various messages and frame events
      trackMessageManager();

      // Listen for app process exit
      const onMessageManagerClose = function(subject, topic, data) {
        if (subject == mm) {
          destroy();
        }
      };
      Services.obs.addObserver(onMessageManagerClose, "message-manager-close");

      // Listen for connection close to cleanup things
      // when user unplug the device or we lose the connection somehow.
      EventEmitter.on(connection, "closed", destroy);

      mm.sendAsyncMessage("debug:connect", { prefix, addonId });
    });
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
  _onConnection(
    transport,
    forwardingPrefix,
    noRootActor = false,
    socketListener = null
  ) {
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

    const conn = new DebuggerServerConnection(
      connID,
      transport,
      socketListener
    );
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
   * Called when DevTools are unloaded to remove the contend process server startup script
   * for the list of scripts loaded for each new content process. Will also remove message
   * listeners from already loaded scripts.
   */
  removeContentServerScript() {
    Services.ppmm.removeDelayedProcessScript(
      CONTENT_PROCESS_SERVER_STARTUP_SCRIPT
    );
    try {
      Services.ppmm.broadcastAsyncMessage("debug:close-content-server");
    } catch (e) {
      // Nothing to do
    }
  },

  /**
   * Searches all active connections for an actor matching an ID.
   *
   * ⚠ TO BE USED ONLY FROM SERVER CODE OR TESTING ONLY! ⚠`
   *
   * This is helpful for some tests which depend on reaching into the server to check some
   * properties of an actor, and it is also used by the actors related to the
   * DevTools WebExtensions API to be able to interact with the actors created for the
   * panels natively provided by the DevTools Toolbox.
   */
  searchAllConnectionsForActor(actorID) {
    // NOTE: the actor IDs are generated with the following format:
    //
    //   `server${loaderID}.conn${ConnectionID}${ActorPrefix}${ActorID}`
    //
    // as an optimization we can come up with a regexp to query only
    // the right connection via its id.
    for (const connID of Object.getOwnPropertyNames(this._connections)) {
      const actor = this._connections[connID].getActor(actorID);
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

exports.DebuggerServer = DebuggerServer;
