/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ContentProcessWatcherRegistry } from "resource://devtools/server/connectors/js-process-actor/ContentProcessWatcherRegistry.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "wdm",
  "@mozilla.org/dom/workers/workerdebuggermanager;1",
  "nsIWorkerDebuggerManager"
);

const { TYPE_DEDICATED, TYPE_SERVICE, TYPE_SHARED } = Ci.nsIWorkerDebugger;

export class WorkerTargetWatcherClass {
  constructor(workerTargetType = "worker") {
    this.#workerTargetType = workerTargetType;
    this.#workerDebuggerListener = {
      onRegister: this.#onWorkerRegister.bind(this),
      onUnregister: this.#onWorkerUnregister.bind(this),
    };
  }

  // {String}
  #workerTargetType;
  // {nsIWorkerDebuggerListener}
  #workerDebuggerListener;

  watch() {
    lazy.wdm.addListener(this.#workerDebuggerListener);
  }

  unwatch() {
    lazy.wdm.removeListener(this.#workerDebuggerListener);
  }

  createTargetsForWatcher(watcherDataObject) {
    const { sessionData } = watcherDataObject;
    for (const dbg of lazy.wdm.getWorkerDebuggerEnumerator()) {
      if (!this.shouldHandleWorker(sessionData, dbg, this.#workerTargetType)) {
        continue;
      }
      this.createWorkerTargetActor(watcherDataObject, dbg);
    }
  }

  async addOrSetSessionDataEntry(watcherDataObject, type, entries, updateType) {
    // Collect the SessionData update into `pendingWorkers` in order to notify
    // about the updates to workers which are still in process of being hooked by devtools.
    for (const concurrentSessionUpdates of watcherDataObject.pendingWorkers) {
      concurrentSessionUpdates.push({
        type,
        entries,
        updateType,
      });
    }

    const promises = [];
    for (const {
      dbg,
      workerThreadServerForwardingPrefix,
    } of watcherDataObject.workers) {
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
   * Called whenever a new Worker is instantiated in the current process
   *
   * @param {WorkerDebugger} dbg
   */
  #onWorkerRegister(dbg) {
    // Create a Target Actor for each watcher currently watching for Workers
    for (const watcherDataObject of ContentProcessWatcherRegistry.getAllWatchersDataObjects(
      this.#workerTargetType
    )) {
      const { sessionData } = watcherDataObject;
      if (this.shouldHandleWorker(sessionData, dbg, this.#workerTargetType)) {
        this.createWorkerTargetActor(watcherDataObject, dbg);
      }
    }
  }

  /**
   * Called whenever a Worker is destroyed in the current process
   *
   * @param {WorkerDebugger} dbg
   */
  #onWorkerUnregister(dbg) {
    for (const watcherDataObject of ContentProcessWatcherRegistry.getAllWatchersDataObjects(
      this.#workerTargetType
    )) {
      const { watcherActorID, workers } = watcherDataObject;
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
      // Close the transport made to the worker thread
      transport.close();

      try {
        watcherDataObject.jsProcessActor.sendAsyncMessage(
          "DevToolsProcessChild:targetDestroyed",
          {
            actors: [
              {
                watcherActorID,
                targetActorForm: workerTargetForm,
              },
            ],
            options: {},
          }
        );
      } catch (e) {
        // This often throws as the JSActor is being destroyed when DevTools closes
        // and we are trying to notify about the destroyed targets.
      }

      workers.splice(unregisteredActorIndex, 1);
    }
  }

  /**
   * Instantiate a worker target actor related to a given WorkerDebugger object
   * and for a given watcher actor.
   *
   * @param {Object} watcherDataObject
   * @param {WorkerDebugger} dbg
   */
  async createWorkerTargetActor(watcherDataObject, dbg) {
    // Prevent the debuggee from executing in this worker until the client has
    // finished attaching to it. This call will throw if the debugger is already "registered"
    // (i.e. if this is called outside of the register listener)
    // See https://searchfox.org/mozilla-central/rev/84922363f4014eae684aabc4f1d06380066494c5/dom/workers/nsIWorkerDebugger.idl#55-66
    try {
      dbg.setDebuggerReady(false);
    } catch (e) {
      if (!e.message.startsWith("Component returned failure code")) {
        throw e;
      }
    }

    const { watcherActorID } = watcherDataObject;
    const { connection, loader } =
      ContentProcessWatcherRegistry.getOrCreateConnectionForWatcher(
        watcherActorID
      );

    // Compute a unique prefix for the bridge made between this content process main thread
    // and the worker thread.
    const workerThreadServerForwardingPrefix =
      connection.allocID("workerTarget");

    const { connectToWorker } = loader.require(
      "resource://devtools/server/connectors/worker-connector.js"
    );

    // Create the actual worker target actor, in the worker thread.
    const { sessionData, sessionContext } = watcherDataObject;
    const onConnectToWorker = connectToWorker(
      connection,
      dbg,
      workerThreadServerForwardingPrefix,
      {
        sessionData,
        sessionContext,
      }
    );

    // Only add data to the connection if we successfully send the
    // workerTargetAvailable message.
    const workerInfo = {
      dbg,
      workerThreadServerForwardingPrefix,
    };
    watcherDataObject.workers.push(workerInfo);

    // The onConnectToWorker is async and we may receive new Session Data (e.g breakpoints)
    // while we are instantiating the worker targets.
    // Let cache the pending session data and flush it after the targets are being instantiated.
    const concurrentSessionUpdates = [];
    watcherDataObject.pendingWorkers.add(concurrentSessionUpdates);

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
      // Also unregister the worker
      watcherDataObject.workers.splice(
        watcherDataObject.workers.indexOf(workerInfo),
        1
      );
      watcherDataObject.pendingWorkers.delete(concurrentSessionUpdates);
      return;
    }
    watcherDataObject.pendingWorkers.delete(concurrentSessionUpdates);

    const { workerTargetForm, transport } = await onConnectToWorker;
    workerInfo.workerTargetForm = workerTargetForm;
    workerInfo.transport = transport;

    // Bail out and cleanup the actor by closing the transport,
    // if we stopped listening for workers while waiting for onConnectToWorker resolution.
    if (!watcherDataObject.workers.includes(workerInfo)) {
      transport.close();
      return;
    }

    const { forwardingPrefix } = watcherDataObject;
    // Immediately queue a message for the parent process, before applying any SessionData
    // as it may start emitting RDP events on the target actor and be lost if the client
    // didn't get notified about the target actor first
    try {
      watcherDataObject.jsProcessActor.sendAsyncMessage(
        "DevToolsProcessChild:targetAvailable",
        {
          watcherActorID,
          forwardingPrefix,
          targetActorForm: workerTargetForm,
        }
      );
    } catch (e) {
      // If there was an error while sending the message, we are not going to use this
      // connection to communicate with the worker.
      transport.close();
      // Also unregister the worker
      watcherDataObject.workers.splice(
        watcherDataObject.workers.indexOf(workerInfo),
        1
      );
      return;
    }

    // Dispatch to the worker thread any SessionData updates which may have been notified
    // while we were waiting for onConnectToWorker to resolve.
    const promises = [];
    for (const { type, entries, updateType } of concurrentSessionUpdates) {
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

  destroyTargetsForWatcher(watcherDataObject) {
    // Notify to all worker threads to destroy their target actor running in them
    for (const { transport } of watcherDataObject.workers) {
      // The transport may not be set if the worker is still being connected to from createWorkerTargetActor.
      if (transport) {
        // Clean the DevToolsTransport created in the main thread to bridge RDP to the worker thread.
        // This will also send a last message to the worker to clean things up in the other thread.
        transport.close();
      }
    }
    // Wipe all workers info
    watcherDataObject.workers = [];
  }

  /**
   * Indicates whether or not we should handle the worker debugger
   *
   * @param {Object} sessionData
   *        The session data for a given watcher, which includes metadata
   *        about the debugged context.
   * @param {WorkerDebugger} dbg
   *        The worker debugger we want to check.
   * @param {String} targetType
   *        The expected worker target type.
   * @returns {Boolean}
   */
  shouldHandleWorker(sessionData, dbg, targetType) {
    if (!isWorkerDebuggerAlive(dbg)) {
      return false;
    }

    if (
      (dbg.type === TYPE_DEDICATED && targetType != "worker") ||
      (dbg.type === TYPE_SERVICE && targetType != "service_worker") ||
      (dbg.type === TYPE_SHARED && targetType != "shared_worker")
    ) {
      return false;
    }

    // subprocess workers are ignored because they take several seconds to
    // attach to when opening the browser toolbox. See bug 1594597.
    if (
      /resource:\/\/gre\/modules\/subprocess\/subprocess_.*\.worker\.js/.test(
        dbg.url
      )
    ) {
      return false;
    }

    const { type: sessionContextType } = sessionData.sessionContext;
    if (sessionContextType == "all") {
      return true;
    }
    if (sessionContextType == "content-process") {
      throw new Error(
        "Content process session type shouldn't try to spawn workers"
      );
    }
    if (sessionContextType == "worker") {
      throw new Error(
        "worker session type should spawn only one target via the WorkerDescriptor"
      );
    }

    if (dbg.type === TYPE_DEDICATED) {
      // Assume that all dedicated workers executes in the same process as the debugged document.
      const browsingContext = BrowsingContext.getCurrentTopByBrowserId(
        sessionData.sessionContext.browserId
      );
      // If we aren't executing in the same process as the worker and its BrowsingContext,
      // it will be undefined.
      if (!browsingContext) {
        return false;
      }
      for (const subBrowsingContext of browsingContext.getAllBrowsingContextsInSubtree()) {
        if (
          subBrowsingContext.currentWindowContext &&
          dbg.windowIDs.includes(
            subBrowsingContext.currentWindowContext.innerWindowId
          )
        ) {
          return true;
        }
      }
      return false;
    }

    if (dbg.type === TYPE_SERVICE) {
      // Accessing `nsIPrincipal.host` may easily throw on non-http URLs.
      // Ignore all non-HTTP as they most likely don't have any valid host name.
      if (!dbg.principal.scheme.startsWith("http")) {
        return false;
      }

      const workerHost = dbg.principal.hostPort;
      return workerHost == sessionData["browser-element-host"][0];
    }

    if (dbg.type === TYPE_SHARED) {
      // Don't expose shared workers when debugging a tab.
      // For now, they are only exposed in the browser toolbox, when Session Context Type is set to "all".
      return false;
    }

    return false;
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
  if (!isWorkerDebuggerAlive(dbg)) {
    return Promise.resolve();
  }

  return new Promise(resolve => {
    // Wait until we're notified by the worker that the resources are watched.
    // This is important so we know existing resources were handled.
    const listener = {
      onMessage: message => {
        message = JSON.parse(message);
        if (message.type === "session-data-entry-added-or-set") {
          dbg.removeListener(listener);
          resolve();
        }
      },
      // Resolve if the worker is being destroyed so we don't have a dangling promise.
      onClose: () => {
        dbg.removeListener(listener);
        resolve();
      },
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

function isWorkerDebuggerAlive(dbg) {
  if (dbg.isClosed) {
    return false;
  }
  // Some workers are zombies. `isClosed` is false, but nothing works.
  // `postMessage` is a noop, `addListener`'s `onClosed` doesn't work.
  return (
    dbg.window?.docShell ||
    // consider dbg without `window` as being alive, as they aren't related
    // to any docShell and probably do not suffer from this issue
    !dbg.window
  );
}

export const WorkerTargetWatcher = new WorkerTargetWatcherClass();
