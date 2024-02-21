/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

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
 * Instantiate all Content Process targets in all the DOM Processes.
 *
 * @param {WatcherActor} watcher
 */
async function createTargets(watcher) {
  const promises = [];
  for (const domProcess of getAllContentProcesses()) {
    const processActor = domProcess.getActor("DevToolsProcess");
    promises.push(
      processActor.instantiateTarget({
        watcherActorID: watcher.actorID,
        connectionPrefix: watcher.conn.prefix,
        sessionContext: watcher.sessionContext,
        sessionData: watcher.sessionData,
      })
    );
  }
  await Promise.all(promises);
}

/**
 * @param {WatcherActor} watcher
 * @param {object} options
 * @param {boolean} options.isModeSwitching
 *        true when this is called as the result of a change to the devtools.browsertoolbox.scope pref
 */
function destroyTargets(watcher, options) {
  for (const domProcess of getAllContentProcesses()) {
    const processActor = domProcess.getActor("DevToolsProcess");
    processActor.destroyTarget({
      watcherActorID: watcher.actorID,
      isModeSwitching: options.isModeSwitching,
    });
  }
}

/**
 * Go over all existing content processes in order to communicate about new data entries
 *
 * @param {Object} options
 * @param {WatcherActor} options.watcher
 *        The Watcher Actor providing new data entries
 * @param {string} options.type
 *        The type of data to be added
 * @param {Array<Object>} options.entries
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
  const promises = [];
  for (const domProcess of getAllContentProcesses()) {
    const processActor = domProcess.getActor("DevToolsProcess");
    promises.push(
      processActor.addOrSetSessionDataEntry({
        watcherActorID: watcher.actorID,
        type,
        entries,
        updateType,
      })
    );
  }
  await Promise.all(promises);
}

/**
 * Notify all existing content processes that some data entries have been removed
 *
 * See addOrSetSessionDataEntry for argument documentation.
 */
async function removeSessionDataEntry({ watcher, type, entries }) {
  const promises = [];
  for (const domProcess of getAllContentProcesses()) {
    const processActor = domProcess.getActor("DevToolsProcess");
    promises.push(
      processActor.removeSessionDataEntry({
        watcherActorID: watcher.actorID,
        type,
        entries,
      })
    );
  }
  await Promise.all(promises);
}

module.exports = {
  createTargets,
  destroyTargets,
  addOrSetSessionDataEntry,
  removeSessionDataEntry,
};
