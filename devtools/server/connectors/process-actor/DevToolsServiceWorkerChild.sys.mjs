/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { EventEmitter } from "resource://gre/modules/EventEmitter.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  loader: "resource://devtools/shared/loader/Loader.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "wdm",
  "@mozilla.org/dom/workers/workerdebuggermanager;1",
  "nsIWorkerDebuggerManager"
);

XPCOMUtils.defineLazyModuleGetters(lazy, {
  SessionDataHelpers:
    "resource://devtools/server/actors/watcher/SessionDataHelpers.jsm",
});

ChromeUtils.defineLazyGetter(lazy, "DevToolsUtils", () =>
  lazy.loader.require("devtools/shared/DevToolsUtils")
);

// Name of the attribute into which we save data in `sharedData` object.
const SHARED_DATA_KEY_NAME = "DevTools:watchedPerWatcher";

export class DevToolsServiceWorkerChild extends JSProcessActorChild {
  constructor() {
    super();

    // The map is indexed by the Watcher Actor ID.
    // The values are objects containing the following properties:
    // - connection: the DevToolsServerConnection itself
    // - workers: An array of object containing the following properties:
    //     - dbg: A WorkerDebuggerInstance
    //     - serviceWorkerTargetForm: The associated worker target instance form
    //     - workerThreadServerForwardingPrefix: The prefix used to forward events to the
    //       worker target on the worker thread ().
    // - forwardingPrefix: Prefix used by the JSWindowActorTransport pair to communicate
    //   between content and parent processes.
    // - sessionData: Data (targets, resources, …) the watcher wants to be notified about.
    //   See WatcherRegistry.getSessionData to see the full list of properties.
    this._connections = new Map();

    this._onConnectionChange = this._onConnectionChange.bind(this);

    EventEmitter.decorate(this);
  }

  /**
   * Called by nsIWorkerDebuggerManager when a worker get created.
   *
   * Go through all registered connections (in case we have more than one client connected)
   * to eventually instantiate a target actor for this worker.
   *
   * @param {nsIWorkerDebugger} dbg
   */
  _onWorkerRegistered(dbg) {
    // Only consider service workers
    if (dbg.type !== Ci.nsIWorkerDebugger.TYPE_SERVICE) {
      return;
    }

    for (const [
      watcherActorID,
      { connection, forwardingPrefix, sessionData },
    ] of this._connections) {
      if (this._shouldHandleWorker(sessionData, dbg)) {
        this._createWorkerTargetActor({
          dbg,
          connection,
          forwardingPrefix,
          watcherActorID,
        });
      }
    }
  }

  /**
   * Called by nsIWorkerDebuggerManager when a worker get destroyed.
   *
   * Go through all registered connections (in case we have more than one client connected)
   * to destroy the related target which may have been created for this worker.
   *
   * @param {nsIWorkerDebugger} dbg
   */
  _onWorkerUnregistered(dbg) {
    // Only consider service workers
    if (dbg.type !== Ci.nsIWorkerDebugger.TYPE_SERVICE) {
      return;
    }

    for (const [watcherActorID, watcherConnectionData] of this._connections) {
      this._destroyServiceWorkerTargetForWatcher(
        watcherActorID,
        watcherConnectionData,
        dbg
      );
    }
  }

  /**
   * To be called when we know a Service Worker target should be destroyed for a specific connection
   * for which we pass the related "watcher connection data".
   *
   * @param {String} watcherActorID
   *        Watcher actor ID for which we should unregister this service worker.
   * @param {Object} watcherConnectionData
   *        The metadata object for a given watcher, stored in the _connections Map.
   * @param {nsIWorkerDebugger} dbg
   */
  _destroyServiceWorkerTargetForWatcher(
    watcherActorID,
    watcherConnectionData,
    dbg
  ) {
    const { workers, forwardingPrefix } = watcherConnectionData;

    // Check if the worker registration was handled for this watcher.
    const unregisteredActorIndex = workers.findIndex(worker => {
      try {
        // Accessing the WorkerDebugger id might throw (NS_ERROR_UNEXPECTED).
        return worker.dbg.id === dbg.id;
      } catch (e) {
        return false;
      }
    });

    // Ignore this worker if it wasn't registered for this watcher.
    if (unregisteredActorIndex === -1) {
      return;
    }

    const { serviceWorkerTargetForm, transport } =
      workers[unregisteredActorIndex];

    // Remove the entry from this._connection dictionnary
    workers.splice(unregisteredActorIndex, 1);

    // Close the transport made against the worker thread.
    transport.close();

    // Note that we do not need to post the "disconnect" message from this destruction codepath
    // as this method is only called when the worker is unregistered and so,
    // we can't send any message anyway, and the worker is being destroyed anyway.

    // Also notify the parent process that this worker target got destroyed.
    // As the worker thread may be already destroyed, it may not have time to send a destroy event.
    try {
      this.sendAsyncMessage(
        "DevToolsServiceWorkerChild:serviceWorkerTargetDestroyed",
        {
          watcherActorID,
          forwardingPrefix,
          serviceWorkerTargetForm,
        }
      );
    } catch (e) {
      // Ignore exception which may happen on content process destruction
    }
  }

  /**
   * Function handling messages sent by DevToolsServiceWorkerParent (part of ProcessActor API).
   *
   * @param {Object} message
   * @param {String} message.name
   * @param {*} message.data
   */
  receiveMessage(message) {
    switch (message.name) {
      case "DevToolsServiceWorkerParent:instantiate-already-available": {
        const { watcherActorID, connectionPrefix, sessionData } = message.data;
        return this._watchWorkerTargets({
          watcherActorID,
          parentConnectionPrefix: connectionPrefix,
          sessionData,
        });
      }
      case "DevToolsServiceWorkerParent:destroy": {
        const { watcherActorID } = message.data;
        return this._destroyTargetActors(watcherActorID);
      }
      case "DevToolsServiceWorkerParent:addOrSetSessionDataEntry": {
        const { watcherActorID, type, entries, updateType } = message.data;
        return this._addOrSetSessionDataEntry(
          watcherActorID,
          type,
          entries,
          updateType
        );
      }
      case "DevToolsServiceWorkerParent:removeSessionDataEntry": {
        const { watcherActorID, type, entries } = message.data;
        return this._removeSessionDataEntry(watcherActorID, type, entries);
      }
      case "DevToolsServiceWorkerParent:packet":
        return this.emit("packet-received", message);
      default:
        throw new Error(
          "Unsupported message in DevToolsServiceWorkerParent: " + message.name
        );
    }
  }

  /**
   * "chrome-event-target-created" event handler. Supposed to be fired very early when the process starts
   */
  observe() {
    const { sharedData } = Services.cpmm;
    const sessionDataByWatcherActor = sharedData.get(SHARED_DATA_KEY_NAME);
    if (!sessionDataByWatcherActor) {
      throw new Error(
        "Request to instantiate the target(s) for the Service Worker, but `sharedData` is empty about watched targets"
      );
    }

    // Create one Target actor for each prefix/client which listen to workers
    for (const [watcherActorID, sessionData] of sessionDataByWatcherActor) {
      const { targets, connectionPrefix } = sessionData;
      if (targets?.includes("service_worker")) {
        this._watchWorkerTargets({
          watcherActorID,
          parentConnectionPrefix: connectionPrefix,
          sessionData,
        });
      }
    }
  }

  /**
   * Instantiate targets for existing workers, watch for worker registration and listen
   * for resources on those workers, for given connection and context. Targets are sent
   * to the DevToolsServiceWorkerParent via the DevToolsServiceWorkerChild:serviceWorkerTargetAvailable message.
   *
   * @param {Object} options
   * @param {String} options.watcherActorID: The ID of the WatcherActor who requested to
   *        observe and create these target actors.
   * @param {String} options.parentConnectionPrefix: The prefix of the DevToolsServerConnection
   *        of the Watcher Actor. This is used to compute a unique ID for the target actor.
   * @param {Object} options.sessionData: Data (targets, resources, …) the watcher wants
   *        to be notified about. See WatcherRegistry.getSessionData to see the full list
   *        of properties.
   */
  async _watchWorkerTargets({
    watcherActorID,
    parentConnectionPrefix,
    sessionData,
  }) {
    // We might already have been called from observe method if the process was initializing
    if (this._connections.has(watcherActorID)) {
      // In such case, wait for the promise in order to ensure resolving only after
      // we notified about the existing targets
      await this._connections.get(watcherActorID).watchPromise;
      return;
    }

    // Compute a unique prefix, just for this Service Worker,
    // which will be used to create a JSWindowActorTransport pair between content and parent processes.
    // This is slightly hacky as we typicaly compute Prefix and Actor ID via `DevToolsServerConnection.allocID()`,
    // but here, we can't have access to any DevTools connection as we are really early in the content process startup
    // WindowGlobalChild's innerWindowId should be unique across processes, so it should be safe?
    // (this.manager == WindowGlobalChild interface)
    const forwardingPrefix =
      parentConnectionPrefix + "serviceWorkerProcess" + this.manager.childID;

    const connection = this._createConnection(forwardingPrefix);

    // This method will be concurrently called from `observe()` and `DevToolsServiceWorkerParent:instantiate-already-available`
    // When the JSprocessActor initializes itself and when the watcher want to force instantiating existing targets.
    // Wait for the existing promise when the second call arise.
    //
    // Also, _connections has to be populated *before* calling _createWorkerTargetActor,
    // so create a deferred promise right away.
    let resolveWatchPromise;
    const watchPromise = new Promise(
      resolve => (resolveWatchPromise = resolve)
    );

    this._connections.set(watcherActorID, {
      connection,
      watchPromise,
      workers: [],
      forwardingPrefix,
      sessionData,
    });

    // Listen for new workers that will be spawned.
    if (!this._workerDebuggerListener) {
      this._workerDebuggerListener = {
        onRegister: this._onWorkerRegistered.bind(this),
        onUnregister: this._onWorkerUnregistered.bind(this),
      };
      lazy.wdm.addListener(this._workerDebuggerListener);
    }

    const promises = [];
    for (const dbg of lazy.wdm.getWorkerDebuggerEnumerator()) {
      if (!this._shouldHandleWorker(sessionData, dbg)) {
        continue;
      }
      promises.push(
        this._createWorkerTargetActor({
          dbg,
          connection,
          forwardingPrefix,
          watcherActorID,
        })
      );
    }
    await Promise.all(promises);
    resolveWatchPromise();
  }

  /**
   * Initialize a DevTools Server and return a new DevToolsServerConnection
   * using this server in order to communicate to the parent process via
   * the JSProcessActor message / queries.
   *
   * @param String forwardingPrefix
   *        A unique prefix used to distinguish message coming from distinct service workers.
   * @return DevToolsServerConnection
   *        A connection to communicate with the parent process.
   */
  _createConnection(forwardingPrefix) {
    const { DevToolsServer } = lazy.loader.require(
      "devtools/server/devtools-server"
    );

    DevToolsServer.init();

    // We want a special server without any root actor and only target-scoped actors.
    // We are going to spawn a WorkerTargetActor instance in the next few lines,
    // it is going to act like a root actor without being one.
    DevToolsServer.registerActors({ target: true });
    DevToolsServer.on("connectionchange", this._onConnectionChange);

    const connection = DevToolsServer.connectToParentWindowActor(
      this,
      forwardingPrefix
    );

    return connection;
  }

  /**
   * Indicates whether or not we should handle the worker debugger for a given
   * watcher's session data.
   *
   * @param {Object} sessionData
   *        The session data for a given watcher, which includes metadata
   *        about the debugged context.
   * @param {WorkerDebugger} dbg
   *        The worker debugger we want to check.
   *
   * @returns {Boolean}
   */
  _shouldHandleWorker(sessionData, dbg) {
    if (dbg.type !== Ci.nsIWorkerDebugger.TYPE_SERVICE) {
      return false;
    }
    // We only want to create targets for non-closed service worker
    if (!lazy.DevToolsUtils.isWorkerDebuggerAlive(dbg)) {
      return false;
    }

    // Accessing `nsIPrincipal.host` may easily throw on non-http URLs.
    // Ignore all non-HTTP as they most likely don't have any valid host name.
    if (!dbg.principal.scheme.startsWith("http")) {
      return false;
    }

    const workerHost = dbg.principal.host;
    return workerHost == sessionData["browser-element-host"][0];
  }

  async _createWorkerTargetActor({
    dbg,
    connection,
    forwardingPrefix,
    watcherActorID,
  }) {
    // Freeze the worker execution as soon as possible in order to wait for DevTools bootstrap.
    // We typically want to:
    //  - startup the Thread Actor,
    //  - pass the initial session data which includes breakpoints to the worker thread,
    //  - register the breakpoints,
    // before release its execution.
    // `connectToWorker` is going to call setDebuggerReady(true) when all of this is done.
    try {
      dbg.setDebuggerReady(false);
    } catch (e) {
      // This call will throw if the debugger is already "registered"
      // (i.e. if this is called outside of the register listener)
      // See https://searchfox.org/mozilla-central/rev/84922363f4014eae684aabc4f1d06380066494c5/dom/workers/nsIWorkerDebugger.idl#55-66
    }

    const watcherConnectionData = this._connections.get(watcherActorID);
    const { sessionData } = watcherConnectionData;
    const workerThreadServerForwardingPrefix = connection.allocID(
      "serviceWorkerTarget"
    );

    // Create the actual worker target actor, in the worker thread.
    const { connectToWorker } = lazy.loader.require(
      "devtools/server/connectors/worker-connector"
    );

    const onConnectToWorker = connectToWorker(
      connection,
      dbg,
      workerThreadServerForwardingPrefix,
      {
        sessionData,
        sessionContext: sessionData.sessionContext,
      }
    );

    try {
      await onConnectToWorker;
    } catch (e) {
      // connectToWorker is supposed to call setDebuggerReady(true) to release the worker execution.
      // But if anything goes wrong and an exception is thrown, ensure releasing its execution,
      // otherwise if devtools is broken, it will freeze the worker indefinitely.
      //
      // onConnectToWorker can reject if the Worker Debugger is closed; so we only want to
      // resume the debugger if it is not closed (otherwise it can cause crashes).
      if (!dbg.isClosed) {
        dbg.setDebuggerReady(true);
      }
      return;
    }

    const { workerTargetForm, transport } = await onConnectToWorker;

    try {
      this.sendAsyncMessage(
        "DevToolsServiceWorkerChild:serviceWorkerTargetAvailable",
        {
          watcherActorID,
          forwardingPrefix,
          serviceWorkerTargetForm: workerTargetForm,
        }
      );
    } catch (e) {
      // If there was an error while sending the message, we are not going to use this
      // connection to communicate with the worker.
      transport.close();
      return;
    }

    // Only add data to the connection if we successfully send the
    // serviceWorkerTargetAvailable message.
    watcherConnectionData.workers.push({
      dbg,
      transport,
      serviceWorkerTargetForm: workerTargetForm,
      workerThreadServerForwardingPrefix,
    });
  }

  /**
   * Request the service worker threads to destroy all their service worker Targets currently registered for a given Watcher actor.
   *
   * @param {String} watcherActorID
   */
  _destroyTargetActors(watcherActorID) {
    const watcherConnectionData = this._connections.get(watcherActorID);
    this._connections.delete(watcherActorID);

    // This connection has already been cleaned?
    if (!watcherConnectionData) {
      console.error(
        `Trying to destroy a target actor that doesn't exists, or has already been destroyed. Watcher Actor ID:${watcherActorID}`
      );
      return;
    }

    for (const {
      dbg,
      transport,
      workerThreadServerForwardingPrefix,
    } of watcherConnectionData.workers) {
      try {
        if (lazy.DevToolsUtils.isWorkerDebuggerAlive(dbg)) {
          dbg.postMessage(
            JSON.stringify({
              type: "disconnect",
              forwardingPrefix: workerThreadServerForwardingPrefix,
            })
          );
        }
      } catch (e) {}

      transport.close();
    }

    watcherConnectionData.connection.close();
  }

  /**
   * Destroy the server once its last connection closes. Note that multiple
   * worker scripts may be running in parallel and reuse the same server.
   */
  _onConnectionChange() {
    const { DevToolsServer } = lazy.loader.require(
      "devtools/server/devtools-server"
    );

    // Only destroy the server if there is no more connections to it. It may be
    // used to debug another tab running in the same process.
    if (DevToolsServer.hasConnection() || DevToolsServer.keepAlive) {
      return;
    }

    if (this._destroyed) {
      return;
    }
    this._destroyed = true;

    DevToolsServer.off("connectionchange", this._onConnectionChange);
    DevToolsServer.destroy();
  }

  /**
   * Used by DevTools transport layer to communicate with the parent process.
   *
   * @param {String} packet
   * @param {String prefix
   */
  async sendPacket(packet, prefix) {
    return this.sendAsyncMessage("DevToolsServiceWorkerChild:packet", {
      packet,
      prefix,
    });
  }

  /**
   * Go through all registered service workers for a given watcher actor
   * to send them new session data entries.
   *
   * See addOrSetSessionDataEntryInWorkerTarget for more info about arguments.
   */
  async _addOrSetSessionDataEntry(watcherActorID, type, entries, updateType) {
    const watcherConnectionData = this._connections.get(watcherActorID);
    if (!watcherConnectionData) {
      return;
    }

    lazy.SessionDataHelpers.addOrSetSessionDataEntry(
      watcherConnectionData.sessionData,
      type,
      entries,
      updateType
    );

    // This type is really specific to Service Workers and doesn't need to be transferred to the worker threads.
    // We only need to instantiate and destroy the target actors based on this new host.
    if (type == "browser-element-host") {
      this.updateBrowserElementHost(watcherActorID, watcherConnectionData);
      return;
    }

    const promises = [];
    for (const {
      dbg,
      workerThreadServerForwardingPrefix,
    } of watcherConnectionData.workers) {
      promises.push(
        addOrSetSessionDataEntryInWorkerTarget({
          dbg,
          workerThreadServerForwardingPrefix,
          type,
          entries,
          updateType,
        })
      );
    }
    await Promise.all(promises);
  }

  /**
   * Called whenever the debugged browser element navigates to a new page
   * and the URL's host changes.
   * This is used to maintain the list of active Service Worker targets
   * based on that host name.
   *
   * @param {String} watcherActorID
   *        Watcher actor ID for which we should unregister this service worker.
   * @param {Object} watcherConnectionData
   *        The metadata object for a given watcher, stored in the _connections Map.
   */
  async updateBrowserElementHost(watcherActorID, watcherConnectionData) {
    const { sessionData, connection, forwardingPrefix } = watcherConnectionData;

    // Create target actor matching this new host.
    // Note that we may be navigating to the same host name and the target will already exist.
    const dbgToInstantiate = [];
    for (const dbg of lazy.wdm.getWorkerDebuggerEnumerator()) {
      const alreadyCreated = watcherConnectionData.workers.some(
        info => info.dbg === dbg
      );
      if (this._shouldHandleWorker(sessionData, dbg) && !alreadyCreated) {
        dbgToInstantiate.push(dbg);
      }
    }
    await Promise.all(
      dbgToInstantiate.map(dbg => {
        return this._createWorkerTargetActor({
          dbg,
          connection,
          forwardingPrefix,
          watcherActorID,
        });
      })
    );
  }

  /**
   * Go through all registered service workers for a given watcher actor
   * to send them request to clear some session data entries.
   *
   * See addOrSetSessionDataEntryInWorkerTarget for more info about arguments.
   */
  _removeSessionDataEntry(watcherActorID, type, entries) {
    const watcherConnectionData = this._connections.get(watcherActorID);

    if (!watcherConnectionData) {
      return;
    }

    lazy.SessionDataHelpers.removeSessionDataEntry(
      watcherConnectionData.sessionData,
      type,
      entries
    );

    for (const {
      dbg,
      workerThreadServerForwardingPrefix,
    } of watcherConnectionData.workers) {
      if (lazy.DevToolsUtils.isWorkerDebuggerAlive(dbg)) {
        dbg.postMessage(
          JSON.stringify({
            type: "remove-session-data-entry",
            forwardingPrefix: workerThreadServerForwardingPrefix,
            dataEntryType: type,
            entries,
          })
        );
      }
    }
  }

  _removeExistingWorkerDebuggerListener() {
    if (this._workerDebuggerListener) {
      lazy.wdm.removeListener(this._workerDebuggerListener);
      this._workerDebuggerListener = null;
    }
  }

  /**
   * Part of JSActor API
   * https://searchfox.org/mozilla-central/rev/d9f92154813fbd4a528453c33886dc3a74f27abb/dom/chrome-webidl/JSActor.webidl#41-42,52
   *
   * > The didDestroy method, if present, will be called after the actor is no
   * > longer able to receive any more messages.
   */
  didDestroy() {
    this._removeExistingWorkerDebuggerListener();

    for (const [watcherActorID, watcherConnectionData] of this._connections) {
      const { connection } = watcherConnectionData;
      this._destroyTargetActors(watcherActorID);

      connection.close();
    }

    this._connections.clear();
  }
}

/**
 * Communicate the type and entries to the Worker Target actor, via the WorkerDebugger.
 *
 * @param {WorkerDebugger} dbg
 * @param {String} workerThreadServerForwardingPrefix
 * @param {String} type
 *        Session data type name
 * @param {Array} entries
 *        Session data entries to add or set.
 * @param {String} updateType
 *        Either "add" or "set", to control if we should only add some items,
 *        or replace the whole data set with the new entries.
 * @returns {Promise} Returns a Promise that resolves once the data entry were handled
 *                    by the worker target.
 */
function addOrSetSessionDataEntryInWorkerTarget({
  dbg,
  workerThreadServerForwardingPrefix,
  type,
  entries,
  updateType,
}) {
  if (!lazy.DevToolsUtils.isWorkerDebuggerAlive(dbg)) {
    return Promise.resolve();
  }

  return new Promise(resolve => {
    // Wait until we're notified by the worker that the resources are watched.
    // This is important so we know existing resources were handled.
    const listener = {
      onMessage: message => {
        message = JSON.parse(message);
        if (message.type === "session-data-entry-added-or-set") {
          resolve();
          dbg.removeListener(listener);
        }
      },
      // Resolve if the worker is being destroyed so we don't have a dangling promise.
      onClose: () => resolve(),
    };

    dbg.addListener(listener);

    dbg.postMessage(
      JSON.stringify({
        type: "add-or-set-session-data-entry",
        forwardingPrefix: workerThreadServerForwardingPrefix,
        dataEntryType: type,
        entries,
        updateType,
      })
    );
  });
}
