/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const {
  WatcherRegistry,
} = require("devtools/server/actors/descriptors/watcher/WatcherRegistry.jsm");

/**
 * Force creating targets for all existing BrowsingContext, that, for a given Watcher Actor.
 *
 * @param WatcherActor watcher
 *        The Watcher Actor requesting to watch for new targets.
 * @param Array<String> watchedResources
 *        List of currently watched resource types by this watcher actor.
 */
async function createTargets(watcher, watchedResources) {
  // Go over all existing BrowsingContext in order to:
  // - Force the instantiation of a DevToolsFrameChild
  // - Have the DevToolsFrameChild to spawn the BrowsingContextTargetActor
  const browsingContexts = getFilteredRemoteBrowsingContext(
    watcher.browserElement
  );
  const promises = [];
  for (const browsingContext of browsingContexts) {
    logWindowGlobal(
      browsingContext.currentWindowGlobal,
      "Existing WindowGlobal"
    );

    // Await for the query in order to try to resolve only *after* we received these
    // already available targets.
    const promise = browsingContext.currentWindowGlobal
      .getActor("DevToolsFrame")
      .instantiateTarget({
        watcherActorID: watcher.actorID,
        connectionPrefix: watcher.conn.prefix,
        browserId: watcher.browserId,
        watchedResources: watcher.watchedResources,
      });
    promises.push(promise);
  }
  return Promise.all(promises);
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
  for (const browsingContext of browsingContexts) {
    logWindowGlobal(
      browsingContext.currentWindowGlobal,
      "Existing WindowGlobal"
    );

    browsingContext.currentWindowGlobal
      .getActor("DevToolsFrame")
      .destroyTarget({
        watcherActorID: watcher.actorID,
        browserId: watcher.browserId,
      });
  }
}

/**
 * Go over all existing BrowsingContext in order to fetch already existing resources
 *
 * @param WatcherActor watcher
 *        The Watcher Actor requesting to stop watching for new targets.
 * @param Array<String> resourceTypes
 *        List of all resource types to fetch.
 */
async function watchResources({ watcher, resourceTypes }) {
  const browsingContexts = getWatchingBrowsingContexts(watcher);
  const promises = [];
  for (const browsingContext of browsingContexts) {
    logWindowGlobal(
      browsingContext.currentWindowGlobal,
      "Existing WindowGlobal"
    );

    const promise = browsingContext.currentWindowGlobal
      .getActor("DevToolsFrame")
      .watchFrameResources({
        watcherActorID: watcher.actorID,
        browserId: watcher.browserId,
        resourceTypes,
      });
    promises.push(promise);
  }
  // Await for the queries in order to try to resolve only *after* we received all resources
  return Promise.all(promises);
}

/**
 * Notify all existing frame targets to stop listening for some resource types.
 *
 * See watchResources for argument documentation.
 */
function unwatchResources({ watcher, resourceTypes }) {
  const browsingContexts = getWatchingBrowsingContexts(watcher);
  for (const browsingContext of browsingContexts) {
    logWindowGlobal(
      browsingContext.currentWindowGlobal,
      "Existing WindowGlobal"
    );

    browsingContext.currentWindowGlobal
      .getActor("DevToolsFrame")
      .unwatchFrameResources({
        watcherActorID: watcher.actorID,
        browserId: watcher.browserId,
        resourceTypes,
      });
  }
}

module.exports = {
  createTargets,
  destroyTargets,
  watchResources,
  unwatchResources,
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
  // either via devtools.contenttoolbox.fission or devtools.browsertoolbox.fission pref.
  const watchingAdditionalTargets = WatcherRegistry.isWatchingTargets(
    watcher,
    "frame"
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
    shouldNotifyWindowGlobal(
      browsingContext.currentWindowGlobal,
      browserElement?.browserId
    )
  );
}

/**
 * Get all the BrowsingContexts.
 *
 * Really all of them:
 * - For all the privileged windows (browser.xhtml, browser console, ...)
 * - For all chrome *and* content contexts (privileged windows, as well as <browser> elements and their inner content documents)
 * - For all nested browsing context. We fetch the contexts recursively.
 *
 * @param BrowsingContext topBrowsingContext (optional)
 *        If defined, this will restrict to this Browsing Context only
 *        and any of its (nested) children.
 */
function getAllRemoteBrowsingContexts(topBrowsingContext) {
  const browsingContexts = [];

  // For a given BrowsingContext, add the `browsingContext`
  // all of its children, that, recursively.
  function walk(browsingContext) {
    if (browsingContexts.includes(browsingContext)) {
      return;
    }
    browsingContexts.push(browsingContext);

    for (const child of browsingContext.children) {
      walk(child);
    }

    if (browsingContext.window) {
      // If the document is in the parent process, also iterate over each <browser>'s browsing context.
      // BrowsingContext.children doesn't cross chrome to content boundaries,
      // so we have to cross these boundaries by ourself.
      for (const browser of browsingContext.window.document.querySelectorAll(
        `browser[remote="true"]`
      )) {
        walk(browser.browsingContext);
      }
    }
  }

  // If a Browsing Context is passed, only walk through the given BrowsingContext
  if (topBrowsingContext) {
    walk(topBrowsingContext);
    // Remove the top level browsing context we just added by calling walk.
    browsingContexts.shift();
  } else {
    // Fetch all top level window's browsing contexts
    // Note that getWindowEnumerator works from all processes, including the content process.
    for (const window of Services.ww.getWindowEnumerator()) {
      if (window.docShell.browsingContext) {
        walk(window.docShell.browsingContext);
      }
    }
  }

  return browsingContexts;
}

/**
 * Helper function to know if a given WindowGlobal should be exposed via watchTargets("frame") API
 * XXX: We probably want to share this function with DevToolsFrameChild,
 * but may be not, it looks like the checks are really differents because WindowGlobalParent and WindowGlobalChild
 * expose very different attributes. (WindowGlobalChild exposes much less!)
 */
function shouldNotifyWindowGlobal(windowGlobal, watchedBrowserId) {
  const browsingContext = windowGlobal.browsingContext;
  // Ignore extension for now as attaching to them is special.
  if (browsingContext.currentRemoteType == "extension") {
    return false;
  }
  // Ignore globals running in the parent process for now as they won't be in a distinct process anyway.
  // And JSWindowActor will most likely only be created if we toggle includeChrome
  // on the JSWindowActor registration.
  if (windowGlobal.osPid == -1 && windowGlobal.isInProcess) {
    return false;
  }
  // Ignore about:blank which are quickly replaced and destroyed by the final URI
  // bug 1625026 aims at removing this workaround and allow debugging any about:blank load
  if (
    windowGlobal.documentURI &&
    windowGlobal.documentURI.spec == "about:blank"
  ) {
    return false;
  }

  if (watchedBrowserId && browsingContext.browserId != watchedBrowserId) {
    return false;
  }

  // For now, we only mention the "remote frames".
  // i.e. the frames which are in a distinct process compared to their parent document
  return (
    !browsingContext.parent ||
    windowGlobal.osPid != browsingContext.parent.currentWindowGlobal.osPid
  );
}

// Set to true to log info about about WindowGlobal's being watched.
const DEBUG = false;

function logWindowGlobal(windowGlobal, message) {
  if (!DEBUG) {
    return;
  }
  const browsingContext = windowGlobal.browsingContext;
  dump(
    message +
      " | BrowsingContext.browserId: " +
      browsingContext.browserId +
      " id: " +
      browsingContext.id +
      " Inner Window ID: " +
      windowGlobal.innerWindowId +
      " pid:" +
      windowGlobal.osPid +
      " isClosed:" +
      windowGlobal.isClosed +
      " isInProcess:" +
      windowGlobal.isInProcess +
      " isCurrentGlobal:" +
      windowGlobal.isCurrentGlobal +
      " currentRemoteType:" +
      browsingContext.currentRemoteType +
      " => " +
      (windowGlobal.documentURI ? windowGlobal.documentURI.spec : "no-uri") +
      "\n"
  );
}
