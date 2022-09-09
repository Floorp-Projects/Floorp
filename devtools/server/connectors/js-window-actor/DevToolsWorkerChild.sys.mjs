/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "wdm",
  "@mozilla.org/dom/workers/workerdebuggermanager;1",
  "nsIWorkerDebuggerManager"
);

XPCOMUtils.defineLazyGetter(lazy, "Loader", () =>
  ChromeUtils.import("resource://devtools/shared/loader/Loader.jsm")
);

XPCOMUtils.defineLazyGetter(lazy, "DevToolsUtils", () =>
  lazy.Loader.require("devtools/shared/DevToolsUtils")
);
XPCOMUtils.defineLazyModuleGetters(lazy, {
  SessionDataHelpers:
    "resource://devtools/server/actors/watcher/SessionDataHelpers.jsm",
});
ChromeUtils.defineESModuleGetters(lazy, {
  isWindowGlobalPartOfContext:
    "resource://devtools/server/actors/watcher/browsing-context-helpers.sys.mjs",
});

// Name of the attribute into which we save data in `sharedData` object.
const SHARED_DATA_KEY_NAME = "DevTools:watchedPerWatcher";

export class DevToolsWorkerChild extends JSWindowActorChild {
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
    // - sessionData: Data (targets, resources, …) the watcher wants to be notified about.
    //   See WatcherRegistry.getSessionData to see the full list of properties.
    this._connections = new Map();

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
    const sessionDataByWatcherActor = sharedData.get(SHARED_DATA_KEY_NAME);
    if (!sessionDataByWatcherActor) {
      throw new Error(
        "Request to instantiate the target(s) for the Worker, but `sharedData` is empty about watched targets"
      );
    }

    // Create one Target actor for each prefix/client which listen to workers
    for (const [watcherActorID, sessionData] of sessionDataByWatcherActor) {
      const { targets, connectionPrefix, sessionContext } = sessionData;
      if (
        targets?.includes("worker") &&
        lazy.isWindowGlobalPartOfContext(this.manager, sessionContext, {
          acceptInitialDocument: true,
          forceAcceptTopLevelTarget: true,
          acceptSameProcessIframes: true,
        })
      ) {
        this._watchWorkerTargets({
          watcherActorID,
          parentConnectionPrefix: connectionPrefix,
          sessionData,
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
    // All messages pass `sessionContext` (except packet) and are expected
    // to match isWindowGlobalPartOfContext result.
    if (message.name != "DevToolsWorkerParent:packet") {
      const { browserId } = message.data.sessionContext;
      // Re-check here, just to ensure that both parent and content processes agree
      // on what should or should not be watched.
      if (
        this.manager.browsingContext.browserId != browserId &&
        !lazy.isWindowGlobalPartOfContext(
          this.manager,
          message.data.sessionContext,
          {
            acceptInitialDocument: true,
          }
        )
      ) {
        throw new Error(
          "Mismatch between DevToolsWorkerParent and DevToolsWorkerChild  " +
            (this.manager.browsingContext.browserId == browserId
              ? "window global shouldn't be notified (isWindowGlobalPartOfContext mismatch)"
              : `expected browsing context with ID ${browserId}, but got ${this.manager.browsingContext.browserId}`)
        );
      }
    }

    switch (message.name) {
      case "DevToolsWorkerParent:instantiate-already-available": {
        const { watcherActorID, connectionPrefix, sessionData } = message.data;

        return this._watchWorkerTargets({
          watcherActorID,
          parentConnectionPrefix: connectionPrefix,
          sessionData,
        });
      }
      case "DevToolsWorkerParent:destroy": {
        const { watcherActorID } = message.data;
        return this._destroyTargetActors(watcherActorID);
      }
      case "DevToolsWorkerParent:addSessionDataEntry": {
        const { watcherActorID, type, entries } = message.data;
        return this._addSessionDataEntry(watcherActorID, type, entries);
      }
      case "DevToolsWorkerParent:removeSessionDataEntry": {
        const { watcherActorID, type, entries } = message.data;
        return this._removeSessionDataEntry(watcherActorID, type, entries);
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
   * for resources on those workers, for given connection and context. Targets are sent
   * to the DevToolsWorkerParent via the DevToolsWorkerChild:workerTargetAvailable message.
   *
   * @param {Object} options
   * @param {Integer} options.watcherActorID: The ID of the WatcherActor who requested to
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
      lazy.wdm.addListener(this._workerDebuggerListener);
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
      sessionData,
    });

    await Promise.all(
      Array.from(lazy.wdm.getWorkerDebuggerEnumerator())
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

  _createConnection(forwardingPrefix) {
    const { DevToolsServer } = lazy.Loader.require(
      "devtools/server/devtools-server"
    );

    DevToolsServer.init();

    // We want a special server without any root actor and only target-scoped actors.
    // We are going to spawn a WorkerTargetActor instance in the next few lines,
    // it is going to act like a root actor without being one.
    DevToolsServer.registerActors({ target: true });

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
      lazy.DevToolsUtils.isWorkerDebuggerAlive(dbg) &&
      dbg.type === Ci.nsIWorkerDebugger.TYPE_DEDICATED &&
      dbg.windowIDs.includes(this.manager.innerWindowId)
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
    const { sessionData } = watcherConnectionData;
    const workerThreadServerForwardingPrefix = connection.allocID(
      "workerTarget"
    );

    // Create the actual worker target actor, in the worker thread.
    const { connectToWorker } = lazy.Loader.require(
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

  async sendPacket(packet, prefix) {
    return this.sendAsyncMessage("DevToolsWorkerChild:packet", {
      packet,
      prefix,
    });
  }

  async _addSessionDataEntry(watcherActorID, type, entries) {
    const watcherConnectionData = this._connections.get(watcherActorID);
    if (!watcherConnectionData) {
      return;
    }

    lazy.SessionDataHelpers.addSessionDataEntry(
      watcherConnectionData.sessionData,
      type,
      entries
    );

    const promises = [];
    for (const {
      dbg,
      workerThreadServerForwardingPrefix,
    } of watcherConnectionData.workers) {
      promises.push(
        addSessionDataEntryInWorkerTarget({
          dbg,
          workerThreadServerForwardingPrefix,
          type,
          entries,
        })
      );
    }
    await Promise.all(promises);
  }

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

  handleEvent({ type }) {
    // DOMWindowCreated is registered from the WatcherRegistry via `ActorManagerParent.addJSWindowActors`
    // as a DOM event to be listened to and so is fired by JSWindowActor platform code.
    if (type == "DOMWindowCreated") {
      this.onDOMWindowCreated();
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
 * @returns {Promise} Returns a Promise that resolves once the data entry were handled
 *                    by the worker target.
 */
function addSessionDataEntryInWorkerTarget({
  dbg,
  workerThreadServerForwardingPrefix,
  type,
  entries,
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
        if (message.type === "session-data-entry-added") {
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
        type: "add-session-data-entry",
        forwardingPrefix: workerThreadServerForwardingPrefix,
        dataEntryType: type,
        entries,
      })
    );
  });
}
