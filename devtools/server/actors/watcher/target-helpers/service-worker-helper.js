/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { waitForTick } = require("resource://devtools/shared/DevToolsUtils.js");

const PROCESS_SCRIPT_URL =
  "resource://devtools/server/actors/watcher/target-helpers/service-worker-jsprocessactor-startup.js";

const PROCESS_ACTOR_NAME = "DevToolsServiceWorker";
const PROCESS_ACTOR_OPTIONS = {
  // Ignore the parent process.
  includeParent: false,

  parent: {
    esModuleURI:
      "resource://devtools/server/connectors/process-actor/DevToolsServiceWorkerParent.sys.mjs",
  },

  child: {
    esModuleURI:
      "resource://devtools/server/connectors/process-actor/DevToolsServiceWorkerChild.sys.mjs",

    observers: [
      // Tried various notification to ensure starting the actor
      // from webServiceWorker processes... but none of them worked.
      /*
      "chrome-event-target-created",
      "webnavigation-create",
      "chrome-webnavigation-create",
      "webnavigation-destroy",
      "chrome-webnavigation-destroy",
      "browsing-context-did-set-embedder",
      "browsing-context-discarded",
      "ipc:content-initializing",
      "ipc:content-created",
      */

      // Fallback on firing a very custom notification from a "process script" (loadProcessScript)
      "init-devtools-service-worker-actor",
    ],
  },
};

// List of all active watchers
const gWatchers = new Set();

/**
 * Register the DevToolsServiceWorker JS Process Actor,
 * if we are registering the first watcher actor.
 *
 * @param {Watcher Actor} watcher
 */
function maybeRegisterProcessActor(watcher) {
  const sizeBefore = gWatchers.size;
  gWatchers.add(watcher);

  if (sizeBefore == 0 && gWatchers.size == 1) {
    ChromeUtils.registerProcessActor(PROCESS_ACTOR_NAME, PROCESS_ACTOR_OPTIONS);

    // For some reason JSProcessActor doesn't work out of the box for `webServiceWorker` content processes.
    // So manually spawn our JSProcessActor from a process script emitting an observer service notification...
    // The Process script are correctly executed on all process types during their early startup.
    Services.ppmm.loadProcessScript(PROCESS_SCRIPT_URL, true);
  }
}

/**
 * Unregister the DevToolsServiceWorker JS Process Actor,
 * if we are unregistering the last watcher actor.
 *
 * @param {Watcher Actor} watcher
 */
function maybeUnregisterProcessActor(watcher) {
  const sizeBefore = gWatchers.size;
  gWatchers.delete(watcher);

  if (sizeBefore == 1 && gWatchers.size == 0) {
    ChromeUtils.unregisterProcessActor(
      PROCESS_ACTOR_NAME,
      PROCESS_ACTOR_OPTIONS
    );

    Services.ppmm.removeDelayedProcessScript(PROCESS_SCRIPT_URL);
  }
}

/**
 * Return the list of all DOM Processes except the one for the parent process
 *
 * @return Array<nsIDOMProcessParent>
 */
function getAllContentProcesses() {
  return ChromeUtils.getAllDOMProcesses().filter(
    process => process.childID !== 0
  );
}

/**
 * Force creating targets for all existing service workers for a given Watcher Actor.
 *
 * @param WatcherActor watcher
 *        The Watcher Actor requesting to watch for new targets.
 */
async function createTargets(watcher) {
  maybeRegisterProcessActor(watcher);
  // Go over all existing content process in order to:
  // - Force the instantiation of a DevToolsServiceWorkerChild
  // - Have the DevToolsServiceWorkerChild to spawn the WorkerTargetActors

  const promises = [];
  for (const process of getAllContentProcesses()) {
    const promise = process
      .getActor(PROCESS_ACTOR_NAME)
      .instantiateServiceWorkerTargets({
        watcherActorID: watcher.actorID,
        connectionPrefix: watcher.conn.prefix,
        sessionContext: watcher.sessionContext,
        sessionData: watcher.sessionData,
      });
    promises.push(promise);
  }

  // Await for the different queries in order to try to resolve only *after* we received
  // the already available worker targets.
  return Promise.all(promises);
}

/**
 * Force destroying all worker targets which were related to a given watcher.
 *
 * @param WatcherActor watcher
 *        The Watcher Actor requesting to stop watching for new targets.
 */
async function destroyTargets(watcher) {
  // Go over all existing content processes in order to destroy all targets
  for (const process of getAllContentProcesses()) {
    let processActor;
    try {
      processActor = process.getActor(PROCESS_ACTOR_NAME);
    } catch (e) {
      // Ignore any exception during destroy as we may be closing firefox/devtools/tab
      // and that can easily lead to many exceptions.
      continue;
    }

    processActor.destroyServiceWorkerTargets({
      watcherActorID: watcher.actorID,
      sessionContext: watcher.sessionContext,
    });
  }

  // browser_dbg-breakpoints-columns.js crashes if we unregister the Process Actor
  // in the same event loop as we call destroyServiceWorkerTargets.
  await waitForTick();

  maybeUnregisterProcessActor(watcher);
}

/**
 * Go over all existing JSProcessActor in order to communicate about new data entries
 *
 * @param WatcherActor watcher
 *        The Watcher Actor requesting to update data entries.
 * @param string type
 *        The type of data to be added
 * @param Array<Object> entries
 *        The values to be added to this type of data
 * @param String updateType
 *        "add" will only add the new entries in the existing data set.
 *        "set" will update the data set with the new entries.
 */
async function addOrSetSessionDataEntry({
  watcher,
  type,
  entries,
  updateType,
}) {
  maybeRegisterProcessActor(watcher);
  const promises = [];
  for (const process of getAllContentProcesses()) {
    const promise = process
      .getActor(PROCESS_ACTOR_NAME)
      .addOrSetSessionDataEntry({
        watcherActorID: watcher.actorID,
        sessionContext: watcher.sessionContext,
        type,
        entries,
        updateType,
      });
    promises.push(promise);
  }
  // Await for the queries in order to try to resolve only *after* the remote code processed the new data
  return Promise.all(promises);
}

/**
 * Notify all existing frame targets that some data entries have been removed
 *
 * See addOrSetSessionDataEntry for argument documentation.
 */
function removeSessionDataEntry({ watcher, type, entries }) {
  for (const process of getAllContentProcesses()) {
    process.getActor(PROCESS_ACTOR_NAME).removeSessionDataEntry({
      watcherActorID: watcher.actorID,
      sessionContext: watcher.sessionContext,
      type,
      entries,
    });
  }
}

module.exports = {
  createTargets,
  destroyTargets,
  addOrSetSessionDataEntry,
  removeSessionDataEntry,
};
