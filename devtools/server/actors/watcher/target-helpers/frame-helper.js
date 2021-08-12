/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const {
  WatcherRegistry,
} = require("devtools/server/actors/watcher/WatcherRegistry.jsm");
const {
  WindowGlobalLogger,
} = require("devtools/server/connectors/js-window-actor/WindowGlobalLogger.jsm");
const Targets = require("devtools/server/actors/targets/index");
const {
  getAllRemoteBrowsingContexts,
  shouldNotifyWindowGlobal,
} = require("devtools/server/actors/watcher/target-helpers/utils.js");

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
  // - Have the DevToolsFrameChild to spawn the BrowsingContextTargetActor

  // If we have a browserElement, set the watchedByDevTools flag on its related browsing context
  // TODO: We should also set the flag for the "parent process" browsing context when we're
  // in the browser toolbox. This is blocked by Bug 1675763, and should be handled as part
  // of Bug 1709529.
  if (watcher.browserElement) {
    // The `watchedByDevTools` enables gecko behavior tied to this flag, such as:
    //  - reporting the contents of HTML loaded in the docshells
    //  - capturing stacks for the network monitor.
    watcher.browserElement.browsingContext.watchedByDevTools = true;
  }

  if (!browsingContextAttachedObserverByWatcher.has(watcher)) {
    // We store the browserId here as watcher.browserElement.browserId can momentary be
    // set to 0 when there's a navigation to a new browsing context.
    const browserId = watcher.browserElement?.browserId;
    const onBrowsingContextAttached = browsingContext => {
      // We want to set watchedByDevTools on new top-level browsing contexts:
      // - in the case of the BrowserToolbox/BrowserConsole, that would be the browsing
      //   contexts of all the tabs we want to handle.
      // - for the regular toolbox, browsing context that are being created when navigating
      //   to a page that forces a new browsing context.
      // Then BrowsingContext will propagate to all the tree of children BbrowsingContext's.
      if (
        !browsingContext.parent &&
        (!watcher.browserElement || browserId === browsingContext.browserId)
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

  if (watcher.isServerTargetSwitchingEnabled && watcher.browserElement) {
    // If server side target switching is enabled, process the top level browsing context first,
    // so that we guarantee it is notified to the client first.
    // If it is disabled, the top level target will be created from the client instead.
    await createTargetForBrowsingContext({
      watcher,
      browsingContext: watcher.browserElement.browsingContext,
      retryOnAbortError: true,
    });
  }

  const browsingContexts = getFilteredRemoteBrowsingContext(
    watcher.browserElement
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
        browserId: watcher.browserId,
        watchedData: watcher.watchedData,
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
 */
function destroyTargets(watcher) {
  // Go over all existing BrowsingContext in order to destroy all targets
  const browsingContexts = getFilteredRemoteBrowsingContext(
    watcher.browserElement
  );
  if (watcher.isServerTargetSwitchingEnabled && watcher.browserElement) {
    // If server side target switching is enabled, we should also destroy the top level browsing context.
    // If it is disabled, the top level target will be destroyed from the client instead.
    browsingContexts.push(watcher.browserElement.browsingContext);
  }

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
        browserId: watcher.browserId,
      });
  }

  if (watcher.browserElement) {
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
 */
async function addWatcherDataEntry({ watcher, type, entries }) {
  const browsingContexts = getWatchingBrowsingContexts(watcher);
  const promises = [];
  for (const browsingContext of browsingContexts) {
    logWindowGlobal(
      browsingContext.currentWindowGlobal,
      "Existing WindowGlobal"
    );

    const promise = browsingContext.currentWindowGlobal
      .getActor("DevToolsFrame")
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
  const browsingContexts = getWatchingBrowsingContexts(watcher);
  for (const browsingContext of browsingContexts) {
    logWindowGlobal(
      browsingContext.currentWindowGlobal,
      "Existing WindowGlobal"
    );

    browsingContext.currentWindowGlobal
      .getActor("DevToolsFrame")
      .removeWatcherDataEntry({
        watcherActorID: watcher.actorID,
        browserId: watcher.browserId,
        type,
        entries,
      });
  }
}

module.exports = {
  createTargets,
  destroyTargets,
  addWatcherDataEntry,
  removeWatcherDataEntry,
};

/**
 * Return the list of BrowsingContexts which should be targeted in order to communicate
 * a new list of resource types to listen or stop listening to.
 *
 * @param WatcherActor watcher
 *        The watcher actor will be used to know which target we debug
 *        and what BrowsingContext should be considered.
 */
function getWatchingBrowsingContexts(watcher) {
  // If we are watching for additional frame targets, it means that fission mode is enabled,
  // either for a content toolbox or a BrowserToolbox via devtools.browsertoolbox.fission pref.
  const watchingAdditionalTargets = WatcherRegistry.isWatchingTargets(
    watcher,
    Targets.TYPES.FRAME
  );
  const { browserElement } = watcher;
  const browsingContexts = watchingAdditionalTargets
    ? getFilteredRemoteBrowsingContext(browserElement)
    : [];
  // Even if we aren't watching additional target, we want to process the top level target.
  // The top level target isn't returned by getFilteredRemoteBrowsingContext, so add it in both cases.
  if (browserElement) {
    const topBrowsingContext = browserElement.browsingContext;
    // Ignore if we are against a page running in the parent process,
    // which would not support JSWindowActor API
    // XXX May be we should toggle `includeChrome` and ensure watch/unwatch works
    // with such page?
    if (topBrowsingContext.currentWindowGlobal.osPid != -1) {
      browsingContexts.push(topBrowsingContext);
    }
  }
  return browsingContexts;
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
function getFilteredRemoteBrowsingContext(browserElement) {
  return getAllRemoteBrowsingContexts(
    browserElement?.browsingContext
  ).filter(browsingContext =>
    shouldNotifyWindowGlobal(browsingContext, browserElement?.browserId)
  );
}

// Set to true to log info about about WindowGlobal's being watched.
const DEBUG = false;

function logWindowGlobal(windowGlobal, message) {
  if (!DEBUG) {
    return;
  }

  WindowGlobalLogger.logWindowGlobal(windowGlobal, message);
}
