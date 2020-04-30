/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["registerWatcher", "unregisterWatcher", "getWatcher"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "ActorManagerParent",
  "resource://gre/modules/ActorManagerParent.jsm"
);

// We record a Map of all available FrameWatchers, indexed by their Connection's prefix string.
// This helps notifying about the target actor, created from the content processes
// via the other JS Window actor pair, i.e. DevToolsFrameChild.
// DevToolsFrameParent will receive the target actor, and be able to retrieve
// the right watcher from the connection's prefix.
// Map of [DevToolsServerConnection's prefix => FrameWatcher]
const watchers = new Map();

/**
 * Register a new WatcherActor.
 *
 * This will save a reference to it in the global `watchers` Map and allow
 * DevToolsFrameParent to use getWatcher method in order to retrieve watcher
 * for the same connection prefix.
 *
 * @param WatcherActor watcher
 *        A watcher actor to register
 * @param Number watchedBrowsingContextID (optional)
 *        If the watched is specific to one precise Browsing Context, pass its ID.
 *        If not pass, this will go through all Browsing Contexts:
 *        All top level windows and alls its inner frames,
 *        including <browser> elements and any inner frames of them.
 */
async function registerWatcher(watcher, watchedBrowsingContextID) {
  const prefix = watcher.conn.prefix;
  if (watchers.has(prefix)) {
    throw new Error(
      `A watcher has already been registered via prefix ${prefix}.`
    );
  }
  watchers.set(prefix, watcher);
  if (watchers.size == 1) {
    // Register the JSWindowActor pair "DevToolsFrame" only once we register our first WindowGlobal Watcher
    ActorManagerParent.addActors({
      DevToolsFrame: {
        parent: {
          moduleURI:
            "resource://devtools/server/connectors/js-window-actor/DevToolsFrameParent.jsm",
        },
        child: {
          moduleURI:
            "resource://devtools/server/connectors/js-window-actor/DevToolsFrameChild.jsm",
          events: {
            DOMWindowCreated: {},
          },
        },
        allFrames: true,
      },
    });
    // Force the immediate activation of this JSWindow Actor
    // so that just a few lines after, `currentWindowGlobal.getActor("DevToolsFrame")` do work.
    // (Note that I had to spin the event loop between flush and getActor in order to make it work while prototyping...)
    ActorManagerParent.flush();
  }

  // Go over all existing BrowsingContext in order to:
  // - Force the instantiation of a DevToolsFrameChild
  // - Have the DevToolsFrameChild to spawn the BrowsingContextTargetActor
  const browsingContexts = getAllRemoteBrowsingContexts(
    watchedBrowsingContextID
  );
  for (const browsingContext of browsingContexts) {
    if (
      !shouldNotifyWindowGlobal(
        browsingContext.currentWindowGlobal,
        watchedBrowsingContextID
      )
    ) {
      continue;
    }
    logWindowGlobal(
      browsingContext.currentWindowGlobal,
      "Existing WindowGlobal"
    );

    // Await for the query in order to try to resolve only *after* we received these
    // already available targets.
    await browsingContext.currentWindowGlobal
      .getActor("DevToolsFrame")
      .instantiateTarget({
        prefix,
        browsingContextID: watchedBrowsingContextID,
      });
  }
}

function unregisterWatcher(watcher) {
  watchers.delete(watcher.conn.prefix);
  if (watchers.size == 0) {
    // ActorManagerParent doesn't expose a "removeActors" method, but it would be equivalent to that:
    ChromeUtils.unregisterWindowActor("DevToolsFrame");
  }
}

function getWatcher(parentConnectionPrefix) {
  return watchers.get(parentConnectionPrefix);
}

/**
 * Get all the BrowsingContexts.
 *
 * Really all of them:
 * - For all the privileged windows (browser.xhtml, browser console, ...)
 * - For all chrome *and* content contexts (privileged windows, as well as <browser> elements and their inner content documents)
 * - For all nested browsing context. We fetch the contexts recursively.
 *
 * @param Number browsingContextID (optional)
 *        If defined, this will restrict to only the Browsing Context matching this ID
 *        and any of its (nested) children.
 */
function getAllRemoteBrowsingContexts(browsingContextID) {
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

  // If a browsingContextID is passed, only walk through the given BrowsingContext
  if (browsingContextID) {
    walk(BrowsingContext.get(browsingContextID));
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
 * Helper function to know if a given WindowGlobal should be exposed via watchTargets(window-global) API
 * XXX: We probably want to share this function with DevToolsFrameChild,
 * but may be not, it looks like the checks are really differents because WindowGlobalParent and WindowGlobalChild
 * expose very different attributes. (WindowGlobalChild exposes much less!)
 */
function shouldNotifyWindowGlobal(windowGlobal, watchedBrowsingContextID) {
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

  if (
    watchedBrowsingContextID &&
    browsingContext.top.id != watchedBrowsingContextID
  ) {
    return false;
  }

  // For now, we only mention the "remote frames".
  // i.e. the frames which are in a distinct process compared to their parent document
  return (
    !browsingContext.parent ||
    // In content process, `currentWindowGlobal` doesn't exists
    windowGlobal.osPid !=
      (
        browsingContext.parent.currentWindowGlobal ||
        browsingContext.parent.window.windowGlobalChild
      ).osPid
  );
}

// If true, log info about WindowGlobal's being watched.
const DEBUG = false;

function logWindowGlobal(windowGlobal, message) {
  if (!DEBUG) {
    return;
  }
  const browsingContext = windowGlobal.browsingContext;
  dump(
    message +
      " | BrowsingContext.id: " +
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
