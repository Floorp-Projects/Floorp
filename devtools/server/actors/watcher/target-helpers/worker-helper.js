/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEVTOOLS_WORKER_JS_WINDOW_ACTOR_NAME = "DevToolsWorker";

/**
 * Force creating targets for all existing workers for a given Watcher Actor.
 *
 * @param WatcherActor watcher
 *        The Watcher Actor requesting to watch for new targets.
 */
async function createTargets(watcher) {
  // Go over all existing BrowsingContext in order to:
  // - Force the instantiation of a DevToolsWorkerChild
  // - Have the DevToolsWorkerChild to spawn the WorkerTargetActors
  const browsingContexts = watcher.getAllBrowsingContexts({
    acceptSameProcessIframes: true,
    forceAcceptTopLevelTarget: true,
  });
  const promises = [];
  for (const browsingContext of browsingContexts) {
    const promise = browsingContext.currentWindowGlobal
      .getActor(DEVTOOLS_WORKER_JS_WINDOW_ACTOR_NAME)
      .instantiateWorkerTargets({
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
  // Go over all existing BrowsingContext in order to destroy all targets
  const browsingContexts = watcher.getAllBrowsingContexts({
    acceptSameProcessIframes: true,
    forceAcceptTopLevelTarget: true,
  });
  for (const browsingContext of browsingContexts) {
    let windowActor;
    try {
      windowActor = browsingContext.currentWindowGlobal.getActor(
        DEVTOOLS_WORKER_JS_WINDOW_ACTOR_NAME
      );
    } catch (e) {
      continue;
    }

    windowActor.destroyWorkerTargets({
      watcherActorID: watcher.actorID,
      sessionContext: watcher.sessionContext,
    });
  }
}

/**
 * Go over all existing BrowsingContext in order to communicate about new data entries
 *
 * @param WatcherActor watcher
 *        The Watcher Actor requesting to stop watching for new targets.
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
  const browsingContexts = watcher.getAllBrowsingContexts({
    acceptSameProcessIframes: true,
    forceAcceptTopLevelTarget: true,
  });
  const promises = [];
  for (const browsingContext of browsingContexts) {
    const promise = browsingContext.currentWindowGlobal
      .getActor(DEVTOOLS_WORKER_JS_WINDOW_ACTOR_NAME)
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
  const browsingContexts = watcher.getAllBrowsingContexts({
    acceptSameProcessIframes: true,
    forceAcceptTopLevelTarget: true,
  });
  for (const browsingContext of browsingContexts) {
    browsingContext.currentWindowGlobal
      .getActor(DEVTOOLS_WORKER_JS_WINDOW_ACTOR_NAME)
      .removeSessionDataEntry({
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
