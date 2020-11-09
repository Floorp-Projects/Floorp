/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["DevToolsWorkerChild"];

const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "wdm",
  "@mozilla.org/dom/workers/workerdebuggermanager;1",
  "nsIWorkerDebuggerManager"
);

XPCOMUtils.defineLazyGetter(this, "Loader", () =>
  ChromeUtils.import("resource://devtools/shared/Loader.jsm")
);

XPCOMUtils.defineLazyGetter(this, "DevToolsUtils", () =>
  Loader.require("devtools/shared/DevToolsUtils")
);

// Name of the attribute into which we save data in `sharedData` object.
const SHARED_DATA_KEY_NAME = "DevTools:watchedPerWatcher";

class DevToolsWorkerChild extends JSWindowActorChild {
  constructor() {
    super();

    // The map is indexed by the Watcher Actor ID.
    // The values are objects containing the following properties:
    // - connection: the DevToolsServerConnection itself
    // - workers: An array of object containing the following properties:
    //     - dbg: A WorkerDebuggerInstance
    //     - workerTargetForm: The associated worker target instance form
    //     - workerThreadServerForwardingPrefix: The prefix used to forward events to the
    //       worker target on the worker thread ().
    // - forwardingPrefix: Prefix used by the JSWindowActorTransport pair to communicate
    //   between content and parent processes.
    // - watchedData: Data (targets, resources, …) the watcher wants to be notified about.
    //   See WatcherRegistry.getWatchedData to see the full list of properties.
    this._connections = new Map();

    this._onConnectionChange = this._onConnectionChange.bind(this);

    EventEmitter.decorate(this);
  }

  _onWorkerRegistered(dbg) {
    if (!this._shouldHandleWorker(dbg)) {
      return;
    }

    for (const [watcherActorID, { connection, forwardingPrefix }] of this
      ._connections) {
      this._createWorkerTargetActor({
        dbg,
        connection,
        forwardingPrefix,
        watcherActorID,
      });
    }
  }

  _onWorkerUnregistered(dbg) {
    for (const [watcherActorID, { workers, forwardingPrefix }] of this
      ._connections) {
      // Check if the worker registration was handled for this watcherActorID.
      const unregisteredActorIndex = workers.findIndex(worker => {
        try {
          // Accessing the WorkerDebugger id might throw (NS_ERROR_UNEXPECTED).
          return worker.dbg.id === dbg.id;
        } catch (e) {
          return false;
        }
      });
      if (unregisteredActorIndex === -1) {
        continue;
      }

      const { workerTargetForm, transport } = workers[unregisteredActorIndex];
      transport.close();

      try {
        this.sendAsyncMessage("DevToolsWorkerChild:workerTargetDestroyed", {
          watcherActorID,
          forwardingPrefix,
          workerTargetForm,
        });
      } catch (e) {
        return;
      }

      workers.splice(unregisteredActorIndex, 1);
    }
  }

  onDOMWindowCreated() {
    const { sharedData } = Services.cpmm;
    const watchedDataByWatcherActor = sharedData.get(SHARED_DATA_KEY_NAME);
    if (!watchedDataByWatcherActor) {
      throw new Error(
        "Request to instantiate the target(s) for the Worker, but `sharedData` is empty about watched targets"
      );
    }

    // Create one Target actor for each prefix/client which listen to workers
    for (const [watcherActorID, watchedData] of watchedDataByWatcherActor) {
      const { targets, connectionPrefix, browserId } = watchedData;
      if (
        targets.includes("worker") &&
        shouldNotifyWindowGlobal(this.manager, browserId)
      ) {
        this._watchWorkerTargets({
          watcherActorID,
          parentConnectionPrefix: connectionPrefix,
          watchedData,
          // When navigating, this code is triggered _before_ the workers living on the page
          // we navigate from are terminated, which would create worker targets for them again.
          // Since at this point the new document can't have any workers yet, we are going to
          // ignore existing targets (i.e. the workers that belong to the previous document).
          ignoreExistingTargets: true,
        });
      }
    }
  }

  /**
   * Function handling messages sent by DevToolsWorkerParent (part of JSWindowActor API).
   *
   * @param {Object} message
   * @param {String} message.name
   * @param {*} message.data
   */
  receiveMessage(message) {
    // All messages pass `browserId` (except packet) and are expected
    // to match shouldNotifyWindowGlobal result.
    if (message.name != "DevToolsWorkerParent:packet") {
      const { browserId } = message.data;
      // Re-check here, just to ensure that both parent and content processes agree
      // on what should or should not be watched.
      if (
        this.manager.browsingContext.browserId != browserId &&
        !shouldNotifyWindowGlobal(this.manager, browserId)
      ) {
        throw new Error(
          "Mismatch between DevToolsWorkerParent and DevToolsWorkerChild  " +
            (this.manager.browsingContext.browserId == browserId
              ? "window global shouldn't be notified (shouldNotifyWindowGlobal mismatch)"
              : `expected browsing context with ID ${browserId}, but got ${this.manager.browsingContext.browserId}`)
        );
      }
    }

    switch (message.name) {
      case "DevToolsWorkerParent:instantiate-already-available": {
        const { watcherActorID, connectionPrefix, watchedData } = message.data;

        return this._watchWorkerTargets({
          watcherActorID,
          parentConnectionPrefix: connectionPrefix,
          watchedData,
        });
      }
      case "DevToolsWorkerParent:destroy": {
        const { watcherActorID } = message.data;
        return this._destroyTargetActors(watcherActorID);
      }
      case "DevToolsWorkerParent:addWatcherDataEntry": {
        const { watcherActorID, type, entries } = message.data;
        return this._addWatcherDataEntry(watcherActorID, type, entries);
      }
      case "DevToolsWorkerParent:removeWatcherDataEntry": {
        const { watcherActorID, type, entries } = message.data;
        return this._removeWatcherDataEntry(watcherActorID, type, entries);
      }
      case "DevToolsWorkerParent:packet":
        return this.emit("packet-received", message);
      default:
        throw new Error(
          "Unsupported message in DevToolsWorkerParent: " + message.name
        );
    }
  }

  /**
   * Instantiate targets for existing workers, watch for worker registration and listen
   * for resources on those workers, for given connection and browserId. Targets are sent
   * to the DevToolsWorkerParent via the DevToolsWorkerChild:workerTargetAvailable message.
   *
   * @param {Object} options
   * @param {Integer} options.watcherActorID: The ID of the WatcherActor who requested to
   *        observe and create these target actors.
   * @param {String} options.parentConnectionPrefix: The prefix of the DevToolsServerConnection
   *        of the Watcher Actor. This is used to compute a unique ID for the target actor.
   * @param {Object} options.watchedData: Data (targets, resources, …) the watcher wants
   *        to be notified about. See WatcherRegistry.getWatchedData to see the full list
   *        of properties.
   * @param {Boolean} options.ignoreExistingTargets: Set to true to not loop on existing
   *        workers. This is useful when this function is called at the very early stage
   *        of the life of a document, since workers of the previous document are still
   *        alive, and there's no way to filter them out.
   */
  async _watchWorkerTargets({
    watcherActorID,
    parentConnectionPrefix,
    ignoreExistingTargets,
    watchedData,
  }) {
    if (this._connections.has(watcherActorID)) {
      throw new Error(
        "DevToolsWorkerChild _watchWorkerTargets was called more than once" +
          ` for the same Watcher (Actor ID: "${watcherActorID}")`
      );
    }

    // Listen for new workers that will be spawned.
    if (!this._workerDebuggerListener) {
      this._workerDebuggerListener = {
        onRegister: this._onWorkerRegistered.bind(this),
        onUnregister: this._onWorkerUnregistered.bind(this),
      };
      wdm.addListener(this._workerDebuggerListener);
    }

    // Compute a unique prefix, just for this WindowGlobal,
    // which will be used to create a JSWindowActorTransport pair between content and parent processes.
    // This is slightly hacky as we typicaly compute Prefix and Actor ID via `DevToolsServerConnection.allocID()`,
    // but here, we can't have access to any DevTools connection as we are really early in the content process startup
    // WindowGlobalChild's innerWindowId should be unique across processes, so it should be safe?
    // (this.manager == WindowGlobalChild interface)
    const forwardingPrefix =
      parentConnectionPrefix + "workerGlobal" + this.manager.innerWindowId;

    const connection = this._createConnection(forwardingPrefix);

    this._connections.set(watcherActorID, {
      connection,
      workers: [],
      forwardingPrefix,
      watchedData,
    });

    if (ignoreExistingTargets !== true) {
      await Promise.all(
        Array.from(wdm.getWorkerDebuggerEnumerator())
          .filter(dbg => this._shouldHandleWorker(dbg))
          .map(dbg =>
            this._createWorkerTargetActor({
              dbg,
              connection,
              forwardingPrefix,
              watcherActorID,
            })
          )
      );
    }
  }

  _createConnection(forwardingPrefix) {
    const { DevToolsServer } = Loader.require(
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
   * Indicates whether or not we should handle the worker debugger
   *
   * @param {WorkerDebugger} dbg: The worker debugger we want to check.
   * @returns {Boolean}
   */
  _shouldHandleWorker(dbg) {
    // We only want to create targets for non-closed dedicated worker, in the same document
    return (
      DevToolsUtils.isWorkerDebuggerAlive(dbg) &&
      dbg.type === Ci.nsIWorkerDebugger.TYPE_DEDICATED &&
      dbg.window?.windowGlobalChild?.innerWindowId ===
        this.manager.innerWindowId
    );
  }

  async _createWorkerTargetActor({
    dbg,
    connection,
    forwardingPrefix,
    watcherActorID,
  }) {
    // Prevent the debuggee from executing in this worker until the client has
    // finished attaching to it. This call will throw if the debugger is already "registered"
    // (i.e. if this is called outside of the register listener)
    // See https://searchfox.org/mozilla-central/rev/84922363f4014eae684aabc4f1d06380066494c5/dom/workers/nsIWorkerDebugger.idl#55-66
    try {
      dbg.setDebuggerReady(false);
    } catch (e) {}

    const watcherConnectionData = this._connections.get(watcherActorID);
    const { watchedData } = watcherConnectionData;
    const workerThreadServerForwardingPrefix = connection.allocID(
      "workerTarget"
    );

    // Create the actual worker target actor, in the worker thread.
    const { connectToWorker } = Loader.require(
      "devtools/server/connectors/worker-connector"
    );

    const onConnectToWorker = connectToWorker(
      connection,
      dbg,
      workerThreadServerForwardingPrefix,
      {
        watchedData,
      }
    );

    try {
      await onConnectToWorker;
    } catch (e) {
      // onConnectToWorker can reject if the Worker Debugger is closed; so we only want to
      // resume the debugger if it is not closed (otherwise it can cause crashes).
      if (!dbg.isClosed) {
        dbg.setDebuggerReady(true);
      }
      return;
    }

    const { workerTargetForm, transport } = await onConnectToWorker;

    try {
      this.sendAsyncMessage("DevToolsWorkerChild:workerTargetAvailable", {
        watcherActorID,
        forwardingPrefix,
        workerTargetForm,
      });
    } catch (e) {
      // If there was an error while sending the message, we are not going to use this
      // connection to communicate with the worker.
      transport.close();
      return;
    }

    // Only add data to the connection if we successfully send the
    // workerTargetAvailable message.
    watcherConnectionData.workers.push({
      dbg,
      transport,
      workerTargetForm,
      workerThreadServerForwardingPrefix,
    });
  }

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
        if (DevToolsUtils.isWorkerDebuggerAlive(dbg)) {
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
    const { DevToolsServer } = Loader.require(
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

  async sendPacket(packet, prefix) {
    return this.sendAsyncMessage("DevToolsWorkerChild:packet", {
      packet,
      prefix,
    });
  }

  async _addWatcherDataEntry(watcherActorID, type, entries) {
    const watcherConnectionData = this._connections.get(watcherActorID);
    if (!watcherConnectionData) {
      return;
    }

    if (!watcherConnectionData.watchedData[type]) {
      watcherConnectionData.watchedData[type] = [];
    }
    watcherConnectionData.watchedData[type].push(...entries);

    const promises = [];
    for (const {
      dbg,
      workerThreadServerForwardingPrefix,
    } of watcherConnectionData.workers) {
      promises.push(
        addWatcherDataEntryInWorkerTarget({
          dbg,
          workerThreadServerForwardingPrefix,
          type,
          entries,
        })
      );
    }
    await Promise.all(promises);
  }

  _removeWatcherDataEntry(watcherActorID, type, entries) {
    const watcherConnectionData = this._connections.get(watcherActorID);

    if (!watcherConnectionData) {
      return;
    }

    if (watcherConnectionData.watchedData[type]) {
      watcherConnectionData.watchedData[
        type
      ] = watcherConnectionData.watchedData[type].filter(
        entry => !entries.includes(entry)
      );
    }

    for (const {
      dbg,
      workerThreadServerForwardingPrefix,
    } of watcherConnectionData.workers) {
      if (DevToolsUtils.isWorkerDebuggerAlive(dbg)) {
        dbg.postMessage(
          JSON.stringify({
            type: "remove-watcher-data-entry",
            forwardingPrefix: workerThreadServerForwardingPrefix,
            dataEntryType: type,
            entries,
          })
        );
      }
    }
  }

  handleEvent({ type }) {
    // DOMWindowCreated is registered from the WatcherRegistry via `ActorManagerParent.addJSWindowActors`
    // as a DOM event to be listened to and so is fired by JSWindowActor platform code.
    if (type == "DOMWindowCreated") {
      this.onDOMWindowCreated();
    }
  }

  _removeExistingWorkerDebuggerListener() {
    if (this._workerDebuggerListener) {
      wdm.removeListener(this._workerDebuggerListener);
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
 * Helper function to know if we should watch for workers on a given windowGlobal
 */
function shouldNotifyWindowGlobal(windowGlobal, watchedBrowserId) {
  const browsingContext = windowGlobal.browsingContext;

  // If we are focusing only on a sub-tree of Browsing Element, ignore elements that are
  // not part of it.
  if (watchedBrowserId && browsingContext.browserId != watchedBrowserId) {
    return false;
  }

  // `isInProcess` is always false, even if the window runs in the same process.
  // `osPid` attribute is not set on WindowGlobalChild
  // so it is hard to guess if the given WindowGlobal runs in this process or not,
  // which is what we want to know here. Here is a workaround way to know it :/
  // ---
  // Also. It might be a bit surprising to have a DevToolsWorkerChild/JSWindowActorChild
  // to be instantiated for WindowGlobals that aren't from this process... Is that expected?
  if (Cu.isRemoteProxy(windowGlobal.window)) {
    return false;
  }

  return true;
}

/**
 * Communicate the type and entries to the Worker Target actor, via the WorkerDebugger.
 *
 * @returns {Promise} Returns a Promise that resolves once the data entry were handled
 *                    by the worker target.
 */
function addWatcherDataEntryInWorkerTarget({
  dbg,
  workerThreadServerForwardingPrefix,
  type,
  entries,
}) {
  if (!DevToolsUtils.isWorkerDebuggerAlive(dbg)) {
    return Promise.resolve();
  }

  return new Promise(resolve => {
    // Wait until we're notified by the worker that the resources are watched.
    // This is important so we know existing resources were handled.
    const listener = {
      onMessage: message => {
        message = JSON.parse(message);
        if (message.type === "watcher-data-entry-added") {
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
        type: "add-watcher-data-entry",
        forwardingPrefix: workerThreadServerForwardingPrefix,
        dataEntryType: type,
        entries,
      })
    );
  });
}
