/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { WatcherRegistry } = ChromeUtils.importESModule(
  "resource://devtools/server/actors/watcher/WatcherRegistry.sys.mjs"
);

loader.lazyRequireGetter(
  this,
  "ChildDebuggerTransport",
  "devtools/shared/transport/child-transport",
  true
);

const CONTENT_PROCESS_SCRIPT =
  "resource://devtools/server/startup/content-process-script.js";

/**
 * Map a MessageManager key to an Array of ContentProcessTargetActor "description" objects.
 * A single MessageManager might be linked to several ContentProcessTargetActors if there are several
 * Watcher actors instantiated on the DevToolsServer, via a single connection (in theory), but rather
 * via distinct connections (ex: a content toolbox and the browser toolbox).
 * Note that if we spawn two DevToolsServer, this module will be instantiated twice.
 *
 * Each ContentProcessTargetActor "description" object is structured as follows
 * - {Object} actor: form of the content process target actor
 * - {String} prefix: forwarding prefix used to redirect all packet to the right content process's transport
 * - {ChildDebuggerTransport} childTransport: Transport forwarding all packets to the target's content process
 * - {WatcherActor} watcher: The Watcher actor for which we instantiated this content process target actor
 */
const actors = new WeakMap();

// Save the list of all watcher actors that are watching for processes
const watchers = new Set();

function onContentProcessActorCreated(msg) {
  const { watcherActorID, prefix, actor } = msg.data;
  const watcher = WatcherRegistry.getWatcher(watcherActorID);
  if (!watcher) {
    throw new Error(
      `Receiving a content process actor without a watcher actor ${watcherActorID}`
    );
  }
  // Ignore watchers of other connections.
  // We may have two browser toolbox connected to the same process.
  // This will spawn two distinct Watcher actor and two distinct process target helper module.
  // Avoid processing the event many times, otherwise we will notify about the same target
  // multiple times.
  if (!watchers.has(watcher)) {
    return;
  }
  const messageManager = msg.target;
  const connection = watcher.conn;

  // Pipe Debugger message from/to parent/child via the message manager
  const childTransport = new ChildDebuggerTransport(messageManager, prefix);
  childTransport.hooks = {
    onPacket: connection.send.bind(connection),
  };
  childTransport.ready();

  connection.setForwarding(prefix, childTransport);

  const list = actors.get(messageManager) || [];
  list.push({
    prefix,
    childTransport,
    actor,
    watcher,
  });
  actors.set(messageManager, list);

  watcher.notifyTargetAvailable(actor);
}

function onContentProcessActorDestroyed(msg) {
  const { watcherActorID } = msg.data;
  const watcher = WatcherRegistry.getWatcher(watcherActorID);
  if (!watcher) {
    throw new Error(
      `Receiving a content process actor destruction without a watcher actor ${watcherActorID}`
    );
  }
  // Ignore watchers of other connections.
  // We may have two browser toolbox connected to the same process.
  // This will spawn two distinct Watcher actor and two distinct process target helper module.
  // Avoid processing the event many times, otherwise we will notify about the same target
  // multiple times.
  if (!watchers.has(watcher)) {
    return;
  }
  const messageManager = msg.target;
  unregisterWatcherForMessageManager(watcher, messageManager);
}

function onMessageManagerClose(messageManager, topic, data) {
  const list = actors.get(messageManager);
  if (!list || !list.length) {
    return;
  }
  for (const { prefix, childTransport, actor, watcher } of list) {
    watcher.notifyTargetDestroyed(actor);

    // If we have a child transport, the actor has already
    // been created. We need to stop using this message manager.
    childTransport.close();
    watcher.conn.cancelForwarding(prefix);
  }
  actors.delete(messageManager);
}

/**
 * Unregister everything created for a given watcher against a precise message manager:
 * - clear up things from `actors` WeakMap,
 * - notify all related target actors as being destroyed,
 * - close all DevTools Transports being created for each Message Manager.
 *
 * @param {WatcherActor} watcher
 * @param {MessageManager}
 * @param {object} options
 * @param {boolean} options.isModeSwitching
 *        true when this is called as the result of a change to the devtools.browsertoolbox.scope pref
 */
function unregisterWatcherForMessageManager(watcher, messageManager, options) {
  const targetActorDescriptions = actors.get(messageManager);
  if (!targetActorDescriptions || !targetActorDescriptions.length) {
    return;
  }

  // Destroy all transports related to this watcher and tells the client to purge all related actors
  const matchingTargetActorDescriptions = targetActorDescriptions.filter(
    item => item.watcher === watcher
  );
  for (const {
    prefix,
    childTransport,
    actor,
  } of matchingTargetActorDescriptions) {
    watcher.notifyTargetDestroyed(actor, options);

    childTransport.close();
    watcher.conn.cancelForwarding(prefix);
  }

  // Then update global `actors` WeakMap by stripping all data about this watcher
  const remainingTargetActorDescriptions = targetActorDescriptions.filter(
    item => item.watcher !== watcher
  );
  if (!remainingTargetActorDescriptions.length) {
    actors.delete(messageManager);
  } else {
    actors.set(messageManager, remainingTargetActorDescriptions);
  }
}

/**
 * Destroy everything related to a given watcher that has been created in this module:
 * (See unregisterWatcherForMessageManager)
 *
 * @param {WatcherActor} watcher
 * @param {object} options
 * @param {boolean} options.isModeSwitching
 *        true when this is called as the result of a change to the devtools.browsertoolbox.scope pref
 */
function closeWatcherTransports(watcher, options) {
  for (let i = 0; i < Services.ppmm.childCount; i++) {
    const messageManager = Services.ppmm.getChildAt(i);
    unregisterWatcherForMessageManager(watcher, messageManager, options);
  }
}

function maybeRegisterMessageListeners(watcher) {
  const sizeBefore = watchers.size;
  watchers.add(watcher);
  if (sizeBefore == 0 && watchers.size == 1) {
    Services.ppmm.addMessageListener(
      "debug:content-process-actor",
      onContentProcessActorCreated
    );
    Services.ppmm.addMessageListener(
      "debug:content-process-actor-destroyed",
      onContentProcessActorDestroyed
    );
    Services.obs.addObserver(onMessageManagerClose, "message-manager-close");

    // Load the content process server startup script only once,
    // otherwise it will be evaluated twice, listen to events twice and create
    // target actors twice.
    // We may try to load it twice when opening one Browser Toolbox via about:debugging
    // and another regular Browser Toolbox. Both will spawn a WatcherActor and watch for processes.
    const isContentProcessScripLoaded = Services.ppmm
      .getDelayedProcessScripts()
      .some(([uri]) => uri === CONTENT_PROCESS_SCRIPT);
    if (!isContentProcessScripLoaded) {
      Services.ppmm.loadProcessScript(CONTENT_PROCESS_SCRIPT, true);
    }
  }
}

/**
 * @param {WatcherActor} watcher
 * @param {object} options
 * @param {boolean} options.isModeSwitching
 *        true when this is called as the result of a change to the devtools.browsertoolbox.scope pref
 */
function maybeUnregisterMessageListeners(watcher, options = {}) {
  const sizeBefore = watchers.size;
  watchers.delete(watcher);
  closeWatcherTransports(watcher, options);

  if (sizeBefore == 1 && watchers.size == 0) {
    Services.ppmm.removeMessageListener(
      "debug:content-process-actor",
      onContentProcessActorCreated
    );
    Services.ppmm.removeMessageListener(
      "debug:content-process-actor-destroyed",
      onContentProcessActorDestroyed
    );
    Services.obs.removeObserver(onMessageManagerClose, "message-manager-close");

    // We inconditionally remove the process script, while we should only remove it
    // once the last DevToolsServer stop watching for processes.
    // We might have many server, using distinct loaders, so that this module
    // will be spawn many times and we should remove the script only once the last
    // module unregister the last watcher of all.
    Services.ppmm.removeDelayedProcessScript(CONTENT_PROCESS_SCRIPT);

    Services.ppmm.broadcastAsyncMessage("debug:destroy-process-script", {
      options,
    });
  }
}

async function createTargets(watcher) {
  // XXX: Should this move to WatcherRegistry??
  maybeRegisterMessageListeners(watcher);

  // Bug 1648499: This could be simplified when migrating to JSProcessActor by using sendQuery.
  // For now, hack into WatcherActor in order to know when we created one target
  // actor for each existing content process.
  // Also, we substract one as the parent process has a message manager and is counted
  // in `childCount`, but we ignore it from the process script and it won't reply.
  let contentProcessCount = Services.ppmm.childCount - 1;
  if (contentProcessCount == 0) {
    return;
  }
  const onTargetsCreated = new Promise(resolve => {
    let receivedTargetCount = 0;
    const listener = () => {
      receivedTargetCount++;
      mayBeResolve();
    };
    watcher.on("target-available-form", listener);
    const onContentProcessClosed = () => {
      // Update the content process count as one has been just destroyed
      contentProcessCount--;
      mayBeResolve();
    };
    Services.obs.addObserver(onContentProcessClosed, "message-manager-close");
    function mayBeResolve() {
      if (receivedTargetCount >= contentProcessCount) {
        watcher.off("target-available-form", listener);
        Services.obs.removeObserver(
          onContentProcessClosed,
          "message-manager-close"
        );
        resolve();
      }
    }
  });

  Services.ppmm.broadcastAsyncMessage("debug:instantiate-already-available", {
    watcherActorID: watcher.actorID,
    connectionPrefix: watcher.conn.prefix,
    sessionData: watcher.sessionData,
  });

  await onTargetsCreated;
}

/**
 * @param {WatcherActor} watcher
 * @param {object} options
 * @param {boolean} options.isModeSwitching
 *        true when this is called as the result of a change to the devtools.browsertoolbox.scope pref
 */
function destroyTargets(watcher, options) {
  maybeUnregisterMessageListeners(watcher, options);

  Services.ppmm.broadcastAsyncMessage("debug:destroy-target", {
    watcherActorID: watcher.actorID,
  });
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
 */
async function addSessionDataEntry({ watcher, type, entries }) {
  let expectedCount = Services.ppmm.childCount - 1;
  if (expectedCount == 0) {
    return;
  }
  const onAllReplied = new Promise(resolve => {
    let count = 0;
    const listener = msg => {
      if (msg.data.watcherActorID != watcher.actorID) {
        return;
      }
      count++;
      maybeResolve();
    };
    Services.ppmm.addMessageListener(
      "debug:add-session-data-entry-done",
      listener
    );
    const onContentProcessClosed = (messageManager, topic, data) => {
      expectedCount--;
      maybeResolve();
    };
    const maybeResolve = () => {
      if (count == expectedCount) {
        Services.ppmm.removeMessageListener(
          "debug:add-session-data-entry-done",
          listener
        );
        Services.obs.removeObserver(
          onContentProcessClosed,
          "message-manager-close"
        );
        resolve();
      }
    };
    Services.obs.addObserver(onContentProcessClosed, "message-manager-close");
  });

  Services.ppmm.broadcastAsyncMessage("debug:add-session-data-entry", {
    watcherActorID: watcher.actorID,
    type,
    entries,
  });

  await onAllReplied;
}

/**
 * Notify all existing content processes that some data entries have been removed
 *
 * See addSessionDataEntry for argument documentation.
 */
function removeSessionDataEntry({ watcher, type, entries }) {
  Services.ppmm.broadcastAsyncMessage("debug:remove-session-data-entry", {
    watcherActorID: watcher.actorID,
    type,
    entries,
  });
}

module.exports = {
  createTargets,
  destroyTargets,
  addSessionDataEntry,
  removeSessionDataEntry,
};
