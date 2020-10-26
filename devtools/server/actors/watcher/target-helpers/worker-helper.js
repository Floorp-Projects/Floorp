/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  getAllRemoteBrowsingContexts,
  shouldNotifyWindowGlobal,
} = require("devtools/server/actors/watcher/target-helpers/utils.js");

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
  const browsingContexts = getFilteredBrowsingContext(watcher.browserElement);
  const promises = [];
  for (const browsingContext of browsingContexts) {
    const promise = browsingContext.currentWindowGlobal
      .getActor(DEVTOOLS_WORKER_JS_WINDOW_ACTOR_NAME)
      .instantiateWorkerTargets({
        watcherActorID: watcher.actorID,
        connectionPrefix: watcher.conn.prefix,
        browserId: watcher.browserId,
        watchedData: watcher.watchedData,
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
  const browsingContexts = getFilteredBrowsingContext(watcher.browserElement);
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
      watcher,
      browserId: watcher.browserId,
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
 */
async function addWatcherDataEntry({ watcher, type, entries }) {
  const browsingContexts = getFilteredBrowsingContext(watcher.browserElement);
  const promises = [];
  for (const browsingContext of browsingContexts) {
    const promise = browsingContext.currentWindowGlobal
      .getActor(DEVTOOLS_WORKER_JS_WINDOW_ACTOR_NAME)
      .addWatcherDataEntry({
        watcherActorID: watcher.actorID,
        browserId: watcher.browserId,
        type,
        entries,
      });
    promises.push(promise);
  }
  // Await for the queries in order to try to resolve only *after* the remote code processed the new data
  return Promise.all(promises);
}

/**
 * Notify all existing frame targets that some data entries have been removed
 *
 * See addWatcherDataEntry for argument documentation.
 */
function removeWatcherDataEntry({ watcher, type, entries }) {
  const browsingContexts = getFilteredBrowsingContext(watcher.browserElement);
  for (const browsingContext of browsingContexts) {
    browsingContext.currentWindowGlobal
      .getActor(DEVTOOLS_WORKER_JS_WINDOW_ACTOR_NAME)
      .removeWatcherDataEntry({
        watcherActorID: watcher.actorID,
        browserId: watcher.browserId,
        type,
        entries,
      });
  }
}

/**
 * Get the list of all BrowsingContext we should interact with.
 * The precise condition of which BrowsingContext we should interact with are defined
 * in `shouldNotifyWindowGlobal`
 *
 * @param BrowserElement browserElement (optional)
 *        If defined, this will restrict to only the Browsing Context matching this
 *        Browser Element and any of its (nested) children iframes.
 */
function getFilteredBrowsingContext(browserElement) {
  const browsingContexts = getAllRemoteBrowsingContexts(
    browserElement?.browsingContext
  );
  if (browserElement?.browsingContext) {
    browsingContexts.push(browserElement?.browsingContext);
  }
  return browsingContexts.filter(browsingContext =>
    shouldNotifyWindowGlobal(browsingContext, browserElement?.browserId, {
      acceptNonRemoteFrame: true,
    })
  );
}

module.exports = {
  createTargets,
  destroyTargets,
  addWatcherDataEntry,
  removeWatcherDataEntry,
};
