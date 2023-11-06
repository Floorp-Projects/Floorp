/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { WatcherRegistry } = ChromeUtils.importESModule(
  "resource://devtools/server/actors/watcher/WatcherRegistry.sys.mjs",
  {
    // WatcherRegistry needs to be a true singleton and loads ActorManagerParent
    // which also has to be a true singleton.
    loadInDevToolsLoader: false,
  }
);
const { WindowGlobalLogger } = ChromeUtils.importESModule(
  "resource://devtools/server/connectors/js-window-actor/WindowGlobalLogger.sys.mjs"
);
const Targets = require("resource://devtools/server/actors/targets/index.js");

const browsingContextAttachedObserverByWatcher = new Map();

/**
 * Force creating targets for all existing BrowsingContext, that, for a given Watcher Actor.
 *
 * @param WatcherActor watcher
 *        The Watcher Actor requesting to watch for new targets.
 */
async function createTargets(watcher) {
  // Go over all existing BrowsingContext in order to:
  // - Force the instantiation of a DevToolsFrameChild
  // - Have the DevToolsFrameChild to spawn the WindowGlobalTargetActor

  // If we have a browserElement, set the watchedByDevTools flag on its related browsing context
  // TODO: We should also set the flag for the "parent process" browsing context when we're
  // in the browser toolbox. This is blocked by Bug 1675763, and should be handled as part
  // of Bug 1709529.
  if (watcher.sessionContext.type == "browser-element") {
    // The `watchedByDevTools` enables gecko behavior tied to this flag, such as:
    //  - reporting the contents of HTML loaded in the docshells
    //  - capturing stacks for the network monitor.
    watcher.browserElement.browsingContext.watchedByDevTools = true;
  }

  if (!browsingContextAttachedObserverByWatcher.has(watcher)) {
    // We store the browserId here as watcher.browserElement.browserId can momentary be
    // set to 0 when there's a navigation to a new browsing context.
    const browserId = watcher.sessionContext.browserId;
    const onBrowsingContextAttached = browsingContext => {
      // We want to set watchedByDevTools on new top-level browsing contexts:
      // - in the case of the BrowserToolbox/BrowserConsole, that would be the browsing
      //   contexts of all the tabs we want to handle.
      // - for the regular toolbox, browsing context that are being created when navigating
      //   to a page that forces a new browsing context.
      // Then BrowsingContext will propagate to all the tree of children BrowsingContext's.
      if (
        !browsingContext.parent &&
        (watcher.sessionContext.type != "browser-element" ||
          browserId === browsingContext.browserId)
      ) {
        browsingContext.watchedByDevTools = true;
      }
    };
    Services.obs.addObserver(
      onBrowsingContextAttached,
      "browsing-context-attached"
    );
    // We store the observer so we can retrieve it elsewhere (e.g. for removal in destroyTargets).
    browsingContextAttachedObserverByWatcher.set(
      watcher,
      onBrowsingContextAttached
    );
  }

  if (
    watcher.sessionContext.isServerTargetSwitchingEnabled &&
    watcher.sessionContext.type == "browser-element"
  ) {
    // If server side target switching is enabled, process the top level browsing context first,
    // so that we guarantee it is notified to the client first.
    // If it is disabled, the top level target will be created from the client instead.
    await createTargetForBrowsingContext({
      watcher,
      browsingContext: watcher.browserElement.browsingContext,
      retryOnAbortError: true,
    });
  }

  const browsingContexts = watcher.getAllBrowsingContexts().filter(
    // Filter out the top browsing context we just processed.
    browsingContext =>
      browsingContext != watcher.browserElement?.browsingContext
  );
  // Await for the all the queries in order to resolve only *after* we received all
  // already available targets.
  // i.e. each call to `createTargetForBrowsingContext` should end up emitting
  // a target-available-form event via the WatcherActor.
  await Promise.allSettled(
    browsingContexts.map(browsingContext =>
      createTargetForBrowsingContext({ watcher, browsingContext })
    )
  );
}

/**
 * (internal helper method) Force creating the target actor for a given BrowsingContext.
 *
 * @param WatcherActor watcher
 *        The Watcher Actor requesting to watch for new targets.
 * @param BrowsingContext browsingContext
 *        The context for which a target should be created.
 * @param Boolean retryOnAbortError
 *        Set to true to retry creating existing targets when receiving an AbortError.
 *        An AbortError is sent when the JSWindowActor pair was destroyed before the query
 *        was complete, which can happen if the document navigates while the query is pending.
 */
async function createTargetForBrowsingContext({
  watcher,
  browsingContext,
  retryOnAbortError = false,
}) {
  logWindowGlobal(browsingContext.currentWindowGlobal, "Existing WindowGlobal");

  // We need to set the watchedByDevTools flag on all top-level browsing context. In the
  // case of a content toolbox, this is done in the tab descriptor, but when we're in the
  // browser toolbox, such descriptor is not created.
  // Then BrowsingContext will propagate to all the tree of children BbrowsingContext's.
  if (!browsingContext.parent) {
    browsingContext.watchedByDevTools = true;
  }

  try {
    await browsingContext.currentWindowGlobal
      .getActor("DevToolsFrame")
      .instantiateTarget({
        watcherActorID: watcher.actorID,
        connectionPrefix: watcher.conn.prefix,
        sessionContext: watcher.sessionContext,
        sessionData: watcher.sessionData,
      });
  } catch (e) {
    console.warn(
      "Failed to create DevTools Frame target for browsingContext",
      browsingContext.id,
      ": ",
      e,
      retryOnAbortError ? "retrying" : ""
    );
    if (retryOnAbortError && e.name === "AbortError") {
      await createTargetForBrowsingContext({
        watcher,
        browsingContext,
        retryOnAbortError,
      });
    } else {
      throw e;
    }
  }
}

/**
 * Force destroying all BrowsingContext targets which were related to a given watcher.
 *
 * @param WatcherActor watcher
 *        The Watcher Actor requesting to stop watching for new targets.
 * @param {object} options
 * @param {boolean} options.isModeSwitching
 *        true when this is called as the result of a change to the devtools.browsertoolbox.scope pref
 */
function destroyTargets(watcher, options) {
  // Go over all existing BrowsingContext in order to destroy all targets
  const browsingContexts = watcher.getAllBrowsingContexts();

  for (const browsingContext of browsingContexts) {
    logWindowGlobal(
      browsingContext.currentWindowGlobal,
      "Existing WindowGlobal"
    );

    if (!browsingContext.parent) {
      browsingContext.watchedByDevTools = false;
    }

    browsingContext.currentWindowGlobal
      .getActor("DevToolsFrame")
      .destroyTarget({
        watcherActorID: watcher.actorID,
        sessionContext: watcher.sessionContext,
        options,
      });
  }

  if (watcher.sessionContext.type == "browser-element") {
    watcher.browserElement.browsingContext.watchedByDevTools = false;
  }

  if (browsingContextAttachedObserverByWatcher.has(watcher)) {
    Services.obs.removeObserver(
      browsingContextAttachedObserverByWatcher.get(watcher),
      "browsing-context-attached"
    );
    browsingContextAttachedObserverByWatcher.delete(watcher);
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
  const browsingContexts = getWatchingBrowsingContexts(watcher);
  const promises = [];
  for (const browsingContext of browsingContexts) {
    logWindowGlobal(
      browsingContext.currentWindowGlobal,
      "Existing WindowGlobal"
    );

    const promise = browsingContext.currentWindowGlobal
      .getActor("DevToolsFrame")
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
  const browsingContexts = getWatchingBrowsingContexts(watcher);
  for (const browsingContext of browsingContexts) {
    logWindowGlobal(
      browsingContext.currentWindowGlobal,
      "Existing WindowGlobal"
    );

    browsingContext.currentWindowGlobal
      .getActor("DevToolsFrame")
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

/**
 * Return the list of BrowsingContexts which should be targeted in order to communicate
 * updated session data.
 *
 * @param WatcherActor watcher
 *        The watcher actor will be used to know which target we debug
 *        and what BrowsingContext should be considered.
 */
function getWatchingBrowsingContexts(watcher) {
  // If we are watching for additional frame targets, it means that the multiprocess or fission mode is enabled,
  // either for a content toolbox or a BrowserToolbox via scope set to everything.
  const watchingAdditionalTargets = WatcherRegistry.isWatchingTargets(
    watcher,
    Targets.TYPES.FRAME
  );
  if (watchingAdditionalTargets) {
    return watcher.getAllBrowsingContexts();
  }
  // By default, when we are no longer watching for frame targets, we should no longer try to
  // communicate with any browsing-context. But.
  //
  // For "browser-element" debugging, all targets are provided by watching by watching for frame targets.
  // So, when we are no longer watching for frame, we don't expect to have any frame target to talk to.
  // => we should no longer reach any browsing context.
  //
  // For "all" (=browser toolbox), there is only the special ParentProcessTargetActor we might want to return here.
  // But this is actually handled by the WatcherActor which uses `WatcherActor.getTargetActorInParentProcess` to convey session data.
  // => we should no longer reach any browsing context.
  //
  // For "webextension" debugging, there is the special WebExtensionTargetActor, which doesn't run in the parent process,
  // so that we can't rely on the same code as the browser toolbox.
  // => we should always reach out this particular browsing context.
  if (watcher.sessionContext.type == "webextension") {
    const browsingContext = BrowsingContext.get(
      watcher.sessionContext.addonBrowsingContextID
    );
    // The add-on browsing context may be destroying, in which case we shouldn't try to communicate with it
    if (browsingContext.currentWindowGlobal) {
      return [browsingContext];
    }
  }
  return [];
}

// Set to true to log info about about WindowGlobal's being watched.
const DEBUG = false;

function logWindowGlobal(windowGlobal, message) {
  if (!DEBUG) {
    return;
  }

  WindowGlobalLogger.logWindowGlobal(windowGlobal, message);
}
