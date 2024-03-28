/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ContentProcessWatcherRegistry } from "resource://devtools/server/connectors/js-process-actor/ContentProcessWatcherRegistry.sys.mjs";

function watch() {
  // There is nothing to watch. This JS Process Actor will automatically be spawned
  // for each new DOM Process.
}
function unwatch() {}

function createTargetsForWatcher(watcherDataObject) {
  // Always ignore the parent process. A special WindowGlobal target actor will be spawned.
  if (ChromeUtils.domProcessChild.childID == 0) {
    return;
  }

  createContentProcessTargetActor(watcherDataObject);
}

/**
 * Instantiate a content process target actor for the current process
 * and for a given watcher actor.
 *
 * @param {Object} watcherDataObject
 */
function createContentProcessTargetActor(watcherDataObject) {
  logDOMProcess(
    ChromeUtils.domProcessChild,
    "Instantiate ContentProcessTarget"
  );

  const { connection, loader } =
    ContentProcessWatcherRegistry.getOrCreateConnectionForWatcher(
      watcherDataObject.watcherActorID
    );

  const { ContentProcessTargetActor } = loader.require(
    "devtools/server/actors/targets/content-process"
  );

  // Create the actual target actor.
  const targetActor = new ContentProcessTargetActor(connection, {
    sessionContext: watcherDataObject.sessionContext,
  });

  ContentProcessWatcherRegistry.onNewTargetActor(
    watcherDataObject,
    targetActor
  );
}

function destroyTargetsForWatcher(watcherDataObject, options) {
  // Unregister and destroy the existing target actors for this target type
  const actorsToDestroy = watcherDataObject.actors.filter(
    actor => actor.targetType == "process"
  );
  watcherDataObject.actors = watcherDataObject.actors.filter(
    actor => actor.targetType != "process"
  );

  for (const actor of actorsToDestroy) {
    ContentProcessWatcherRegistry.destroyTargetActor(
      watcherDataObject,
      actor,
      options
    );
  }
}

// If true, log info about DOMProcess's being created.
const DEBUG = false;

/**
 * Print information about operation being done against each content process.
 *
 * @param {nsIDOMProcessChild} domProcessChild
 *        The process for which we should log a message.
 * @param {String} message
 *        Message to log.
 */
function logDOMProcess(domProcessChild, message) {
  if (!DEBUG) {
    return;
  }
  dump(" [pid:" + domProcessChild + "] " + message + "\n");
}

export const ProcessTargetWatcher = {
  watch,
  unwatch,
  createTargetsForWatcher,
  destroyTargetsForWatcher,
};
