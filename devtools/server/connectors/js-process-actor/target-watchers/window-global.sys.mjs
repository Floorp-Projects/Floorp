/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ContentProcessWatcherRegistry } from "resource://devtools/server/connectors/js-process-actor/ContentProcessWatcherRegistry.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(
  lazy,
  {
    isWindowGlobalPartOfContext:
      "resource://devtools/server/actors/watcher/browsing-context-helpers.sys.mjs",
    WindowGlobalLogger:
      "resource://devtools/server/connectors/js-window-actor/WindowGlobalLogger.sys.mjs",
  },
  { global: "contextual" }
);

// TargetActorRegistery has to be shared between all devtools instances
// and so is loaded into the shared global.
ChromeUtils.defineESModuleGetters(
  lazy,
  {
    TargetActorRegistry:
      "resource://devtools/server/actors/targets/target-actor-registry.sys.mjs",
  },
  { global: "shared" }
);

const isEveryFrameTargetEnabled = Services.prefs.getBoolPref(
  "devtools.every-frame-target.enabled",
  false
);

// If true, log info about DOMProcess's being created.
const DEBUG = false;

/**
 * Print information about operation being done against each Window Global.
 *
 * @param {WindowGlobalChild} windowGlobal
 *        The window global for which we should log a message.
 * @param {String} message
 *        Message to log.
 */
function logWindowGlobal(windowGlobal, message) {
  if (!DEBUG) {
    return;
  }
  lazy.WindowGlobalLogger.logWindowGlobal(windowGlobal, message);
}

function watch() {
  // Set the following preference in this function, so that we can easily
  // toggle these preferences on and off from tests and have the new value being picked up.

  // bfcache-in-parent changes significantly how navigation behaves.
  // We may start reusing previously existing WindowGlobal and so reuse
  // previous set of JSWindowActor pairs (i.e. DevToolsProcessParent/DevToolsProcessChild).
  // When enabled, regular navigations may also change and spawn new BrowsingContexts.
  // If the page we navigate from supports being stored in bfcache,
  // the navigation will use a new BrowsingContext. And so force spawning
  // a new top-level target.
  ChromeUtils.defineLazyGetter(
    lazy,
    "isBfcacheInParentEnabled",
    () =>
      Services.appinfo.sessionHistoryInParent &&
      Services.prefs.getBoolPref("fission.bfcacheInParent", false)
  );

  // Observe for all necessary event to track new and destroyed WindowGlobals.
  Services.obs.addObserver(observe, "content-document-global-created");
  Services.obs.addObserver(observe, "chrome-document-global-created");
  Services.obs.addObserver(observe, "content-page-shown");
  Services.obs.addObserver(observe, "chrome-page-shown");
  Services.obs.addObserver(observe, "content-page-hidden");
  Services.obs.addObserver(observe, "chrome-page-hidden");
  Services.obs.addObserver(observe, "inner-window-destroyed");
  Services.obs.addObserver(observe, "initial-document-element-inserted");
}

function unwatch() {
  // Observe for all necessary event to track new and destroyed WindowGlobals.
  Services.obs.removeObserver(observe, "content-document-global-created");
  Services.obs.removeObserver(observe, "chrome-document-global-created");
  Services.obs.removeObserver(observe, "content-page-shown");
  Services.obs.removeObserver(observe, "chrome-page-shown");
  Services.obs.removeObserver(observe, "content-page-hidden");
  Services.obs.removeObserver(observe, "chrome-page-hidden");
  Services.obs.removeObserver(observe, "inner-window-destroyed");
  Services.obs.removeObserver(observe, "initial-document-element-inserted");
}

function createTargetsForWatcher(watcherDataObject, isProcessActorStartup) {
  const { sessionContext } = watcherDataObject;
  // Bug 1785266 - For now, in browser, when debugging the parent process (childID == 0),
  // we spawn only the ParentProcessTargetActor, which will debug all the BrowsingContext running in the process.
  // So that we have to avoid instantiating any here.
  if (
    sessionContext.type == "all" &&
    ChromeUtils.domProcessChild.childID === 0
  ) {
    return;
  }

  function lookupForTargets(window) {
    // Do not only track top level BrowsingContext in this content process,
    // but also any nested iframe which may be running in the same process.
    for (const browsingContext of window.docShell.browsingContext.getAllBrowsingContextsInSubtree()) {
      const { currentWindowContext } = browsingContext;
      // Only consider Window Global which are running in this process
      if (!currentWindowContext || !currentWindowContext.isInProcess) {
        continue;
      }

      // WindowContext's windowGlobalChild should be defined for WindowGlobal running in this process
      const { windowGlobalChild } = currentWindowContext;

      // getWindowEnumerator will expose somewhat unexpected WindowGlobal when a tab navigated.
      // This will expose WindowGlobals of past navigations. Document which are in the bfcache
      // and aren't the current WindowGlobal of their BrowsingContext.
      if (!windowGlobalChild.isCurrentGlobal) {
        continue;
      }

      // Accept the initial about:blank document:
      //  - only from createTargetsForWatcher, when instantiating the target for the already existing WindowGlobals,
      //  - when we do that on toolbox opening, to prevent creating one when the process is starting.
      //
      // This is to allow debugging blank tabs, which are on an initial about:blank document.
      //
      // We want to avoid creating transient targets for initial about blank when a new WindowGlobal
      // just get created as it will most likely navigate away just after and confuse the frontend with short lived target.
      const acceptInitialDocument = !isProcessActorStartup;

      if (
        lazy.isWindowGlobalPartOfContext(windowGlobalChild, sessionContext, {
          acceptInitialDocument,
        })
      ) {
        createWindowGlobalTargetActor(watcherDataObject, windowGlobalChild);
      } else if (
        !browsingContext.parent &&
        sessionContext.browserId &&
        browsingContext.browserId == sessionContext.browserId &&
        browsingContext.window.document.isInitialDocument
      ) {
        // In order to succesfully get the devtools-html-content event in SourcesManager,
        // we have to ensure flagging the initial about:blank document...
        // While we don't create a target for it, we need to set this flag for this event to be emitted.
        browsingContext.watchedByDevTools = true;
      }
    }
  }
  for (const window of Services.ww.getWindowEnumerator()) {
    lookupForTargets(window);

    // `lookupForTargets` uses `getAllBrowsingContextsInSubTree`, but this will ignore browser elements
    // using type="content". So manually retrieve the windows for these browser elements,
    // in case we have tabs opened on document loaded in the same process.
    // This codepath is meant when we are in the parent process, with browser.xhtml having these <browser type="content">
    // elements for tabs.
    for (const browser of window.document.querySelectorAll(
      `browser[type="content"]`
    )) {
      const childWindow = browser.browsingContext.window;
      // If the tab isn't on a document loaded in the parent process,
      // the window will be null.
      if (childWindow) {
        lookupForTargets(childWindow);
      }
    }
  }
}

function destroyTargetsForWatcher(watcherDataObject, options) {
  // Unregister and destroy the existing target actors for this target type
  const actorsToDestroy = watcherDataObject.actors.filter(
    actor => actor.targetType == "frame"
  );
  watcherDataObject.actors = watcherDataObject.actors.filter(
    actor => actor.targetType != "frame"
  );

  for (const actor of actorsToDestroy) {
    ContentProcessWatcherRegistry.destroyTargetActor(
      watcherDataObject,
      actor,
      options
    );
  }
}

/**
 * Called whenever a new WindowGlobal is instantiated either:
 *  - when navigating to a new page (DOMWindowCreated)
 *  - by a bfcache navigation (pageshow)
 *
 * @param {Window} window
 * @param {Object} options
 * @param {Boolean} options.isBFCache
 *        True, if the request to instantiate a new target comes from a bfcache navigation.
 *        i.e. when we receive a pageshow event with persisted=true.
 *        This will be true regardless of bfcacheInParent being enabled or disabled.
 * @param {Boolean} options.ignoreIfExisting
 *        By default to false. If true is passed, we avoid instantiating a target actor
 *        if one already exists for this windowGlobal.
 */
function onWindowGlobalCreated(
  window,
  { isBFCache = false, ignoreIfExisting = false } = {}
) {
  try {
    const windowGlobal = window.windowGlobalChild;

    // For bfcache navigations, we only create new targets when bfcacheInParent is enabled,
    // as this would be the only case where new DocShells will be created. This requires us to spawn a
    // new WindowGlobalTargetActor as such actor is bound to a unique DocShell.
    const forceAcceptTopLevelTarget =
      isBFCache && lazy.isBfcacheInParentEnabled;

    for (const watcherDataObject of ContentProcessWatcherRegistry.getAllWatchersDataObjects(
      "frame"
    )) {
      const { sessionContext } = watcherDataObject;
      if (
        lazy.isWindowGlobalPartOfContext(windowGlobal, sessionContext, {
          forceAcceptTopLevelTarget,
        })
      ) {
        // If this was triggered because of a navigation, we want to retrieve the existing
        // target we were debugging so we can destroy it before creating the new target.
        // This is important because we had cases where the destruction of an old target
        // was unsetting a flag on the **new** target document, breaking the toolbox (See Bug 1721398).

        // We're checking for an existing target given a watcherActorID + browserId + browsingContext.
        // Note that a target switch might create a new browsing context, so we wouldn't
        // retrieve the existing target here. We are okay with this as:
        // - this shouldn't happen much
        // - in such case we weren't seeing the issue of Bug 1721398 (the old target can't access the new document)
        const existingTarget = findTargetActor({
          watcherDataObject,
          innerWindowId: windowGlobal.innerWindowId,
        });

        // See comment in `observe()` method and `DOMDocElementInserted` condition to know why we sometime
        // ignore this method call if a target actor already exists.
        // It means that we got a previous DOMWindowCreated event, related to a non-about:blank document,
        // and we should ignore the DOMDocElementInserted.
        // In any other scenario, destroy the already existing target and re-create a new one.
        if (existingTarget && ignoreIfExisting) {
          continue;
        }

        // Bail if there is already an existing WindowGlobalTargetActor which wasn't
        // created from a JSWIndowActor.
        // This means we are reloading or navigating (same-process) a Target
        // which has not been created using the Watcher, but from the client (most likely
        // the initial target of a local-tab toolbox).
        // However, we force overriding the first message manager based target in case of
        // BFCache navigations.
        if (
          existingTarget &&
          !existingTarget.createdFromJsWindowActor &&
          !isBFCache
        ) {
          continue;
        }

        // If we decide to instantiate a new target and there was one before,
        // first destroy the previous one.
        // Otherwise its destroy sequence will be executed *after* the new one
        // is being initialized and may easily revert changes made against platform API.
        // (typically toggle platform boolean attributes back to default…)
        if (existingTarget) {
          existingTarget.destroy({ isTargetSwitching: true });
        }

        // When navigating to another process, the Watcher Actor won't have sent any query
        // to the new process JS Actor as the debugged tab was on another process before navigation.
        // But `sharedData` will have data about all the current watchers.
        // Here we have to ensure calling watchTargetsForWatcher in order to populate #connections
        // for the currently processed watcher actor and start listening for future targets.
        if (
          !ContentProcessWatcherRegistry.has(watcherDataObject.watcherActorID)
        ) {
          throw new Error("Watcher data seems out of sync");
        }

        createWindowGlobalTargetActor(watcherDataObject, windowGlobal, true);
      }
    }
  } catch (e) {
    // Ensure logging exception as they are silently ignore otherwise
    dump(
      " Exception while observing a new window: " + e + "\n" + e.stack + "\n"
    );
  }
}

/**
 * Called whenever a WindowGlobal just got destroyed, when closing the tab, or navigating to another one.
 *
 * @param {innerWindowId} innerWindowId
 *        The WindowGlobal's unique identifier.
 */
function onWindowGlobalDestroyed(innerWindowId) {
  for (const watcherDataObject of ContentProcessWatcherRegistry.getAllWatchersDataObjects(
    "frame"
  )) {
    const existingTarget = findTargetActor({
      watcherDataObject,
      innerWindowId,
    });

    if (!existingTarget) {
      continue;
    }

    // Do not do anything if both bfcache in parent and server targets are disabled
    // As history navigations will be handled within the same DocShell and by the
    // same WindowGlobalTargetActor. The actor will listen to pageshow/pagehide by itself.
    // We should not destroy any target.
    if (
      !lazy.isBfcacheInParentEnabled &&
      !watcherDataObject.sessionContext.isServerTargetSwitchingEnabled
    ) {
      continue;
    }
    // If the target actor isn't in watcher data object, it is a top level actor
    // instantiated via a Descriptor's getTarget method. It isn't registered into Watcher objects.
    // But we still want to destroy such target actor, and need to manually emit the targetDestroyed to the parent process.
    // Hopefully bug 1754452 should allow us to get rid of this workaround by making the top level actor
    // be created and managed by the watcher universe, like all the others.
    const isTopLevelActorRegisteredOutsideOfWatcherActor =
      !watcherDataObject.actors.find(
        actor => actor.innerWindowId == innerWindowId
      );
    const targetActorForm = isTopLevelActorRegisteredOutsideOfWatcherActor
      ? existingTarget.form()
      : null;

    existingTarget.destroy();

    if (isTopLevelActorRegisteredOutsideOfWatcherActor) {
      watcherDataObject.jsProcessActor.sendAsyncMessage(
        "DevToolsProcessChild:targetDestroyed",
        {
          actors: [
            {
              watcherActorID: watcherDataObject.watcherActorID,
              targetActorForm,
            },
          ],
          options: {},
        }
      );
    }
  }
}

/**
 * Instantiate a WindowGlobal target actor for a given browsing context
 * and for a given watcher actor.
 *
 * @param {Object} watcherDataObject
 * @param {BrowsingContext} windowGlobalChild
 * @param {Boolean} isDocumentCreation
 */
function createWindowGlobalTargetActor(
  watcherDataObject,
  windowGlobalChild,
  isDocumentCreation = false
) {
  logWindowGlobal(windowGlobalChild, "Instantiate WindowGlobalTarget");

  // When debugging privileged pages running a the shared system compartment, and we aren't in the browser toolbox (which already uses a distinct loader),
  // we have to use the distinct loader in order to ensure running DevTools in a distinct compartment than the page we are about to debug
  // Such page could be about:addons, chrome://browser/content/browser.xhtml,...
  const { browsingContext } = windowGlobalChild;
  const useDistinctLoader =
    browsingContext.associatedWindow.document.nodePrincipal.isSystemPrincipal;
  const { connection, loader } =
    ContentProcessWatcherRegistry.getOrCreateConnectionForWatcher(
      watcherDataObject.watcherActorID,
      useDistinctLoader
    );

  const { WindowGlobalTargetActor } = loader.require(
    "devtools/server/actors/targets/window-global"
  );

  // In the case of the browser toolbox, tab's BrowsingContext don't have
  // any parent BC and shouldn't be considered as top-level.
  // This is why we check for browserId's.
  const { sessionContext } = watcherDataObject;
  const isTopLevelTarget =
    !browsingContext.parent &&
    browsingContext.browserId == sessionContext.browserId;

  // Create the actual target actor.
  const targetActor = new WindowGlobalTargetActor(connection, {
    docShell: browsingContext.docShell,
    // Targets created from the server side, via Watcher actor and DevToolsProcess JSWindow
    // actor pairs are following WindowGlobal lifecycle. i.e. will be destroyed on any
    // type of navigation/reload.
    followWindowGlobalLifeCycle: true,
    isTopLevelTarget,
    ignoreSubFrames: isEveryFrameTargetEnabled,
    sessionContext,
  });
  targetActor.createdFromJsWindowActor = true;

  ContentProcessWatcherRegistry.onNewTargetActor(
    watcherDataObject,
    targetActor,
    isDocumentCreation
  );
}

/**
 * Observer service notification handler.
 *
 * @param {DOMWindow|Document} subject
 *        A window for *-document-global-created
 *        A document for *-page-{shown|hide}
 * @param {String} topic
 */
function observe(subject, topic) {
  if (
    topic == "content-document-global-created" ||
    topic == "chrome-document-global-created"
  ) {
    onWindowGlobalCreated(subject);
  } else if (topic == "inner-window-destroyed") {
    const innerWindowId = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
    onWindowGlobalDestroyed(innerWindowId);
  } else if (topic == "content-page-shown" || topic == "chrome-page-shown") {
    // The observer service notification doesn't receive the "persisted" DOM Event attribute,
    // but thanksfully is fired just before the dispatching of that DOM event.
    subject.defaultView.addEventListener("pageshow", handleEvent, {
      capture: true,
      once: true,
    });
  } else if (topic == "content-page-hidden" || topic == "chrome-page-hidden") {
    // Same as previous elseif branch
    subject.defaultView.addEventListener("pagehide", handleEvent, {
      capture: true,
      once: true,
    });
  } else if (topic == "initial-document-element-inserted") {
    // We may be notified about SVG documents which we don't care about here.
    if (!subject.location || !subject.defaultView) {
      return;
    }
    // We might have ignored the DOMWindowCreated event because it was the initial about:blank document.
    // But when loading same-process iframes, we reuse the WindowGlobal of the previously ignored about:bank document
    // to load the actual URL loaded in the iframe. This means we won't have a new DOMWindowCreated
    // for the actual document. But there is a DOMDocElementInserted fired just after, that we are processing here
    // to create a target for same-process iframes. We only have to tell onWindowGlobalCreated to ignore
    // the call if a target was created on the DOMWindowCreated event (if that was a non-about:blank document).
    //
    // All this means that we still do not create any target for the initial documents.
    // It is complex to instantiate targets for initial documents because:
    // - it would mean spawning two targets for the same WindowGlobal and sharing the same innerWindowId
    // - or have WindowGlobalTargets to handle more than one document (it would mean reusing will-navigate/window-ready events
    // both on client and server)
    onWindowGlobalCreated(subject.defaultView, { ignoreIfExisting: true });
  }
}

/**
 * DOM Event handler.
 *
 * @param {String} type
 *        DOM event name
 * @param {Boolean} persisted
 *        A flag set to true in cache of BFCache navigation
 * @param {Document} target
 *        The navigating document
 */
function handleEvent({ type, persisted, target }) {
  // If persisted=true, this is a BFCache navigation.
  //
  // With Fission enabled and bfcacheInParent, BFCache navigations will spawn a new DocShell
  // in the same process:
  // * the previous page won't be destroyed, its JSWindowActor will stay alive (`didDestroy` won't be called)
  //   and a 'pagehide' with persisted=true will be emitted on it.
  // * the new page page won't emit any DOMWindowCreated, but instead a pageshow with persisted=true
  //   will be emitted.
  if (type === "pageshow" && persisted) {
    // Notify all bfcache navigations, even the one for which we don't create a new target
    // as that's being useful for parent process storage resource watchers.
    for (const watcherDataObject of ContentProcessWatcherRegistry.getAllWatchersDataObjects()) {
      watcherDataObject.jsProcessActor.sendAsyncMessage(
        "DevToolsProcessChild:bf-cache-navigation-pageshow",
        {
          browsingContextId: target.defaultView.browsingContext.id,
        }
      );
    }

    // Here we are going to re-instantiate a target that got destroyed before while processing a pagehide event.
    // We force instantiating a new top level target, within `instantiate()` even if server targets are disabled.
    // But we only do that if bfcacheInParent is enabled. Otherwise for same-process, same-docshell bfcache navigation,
    // we don't want to spawn new targets.
    onWindowGlobalCreated(target.defaultView, {
      isBFCache: true,
    });
  }

  if (type === "pagehide" && persisted) {
    // Notify all bfcache navigations, even the one for which we don't create a new target
    // as that's being useful for parent process storage resource watchers.
    for (const watcherDataObject of ContentProcessWatcherRegistry.getAllWatchersDataObjects()) {
      watcherDataObject.jsProcessActor.sendAsyncMessage(
        "DevToolsProcessChild:bf-cache-navigation-pagehide",
        {
          browsingContextId: target.defaultView.browsingContext.id,
        }
      );
    }

    // We might navigate away for the first top level target,
    // which isn't using JSWindowActor (it still uses messages manager and is created by the client, via TabDescriptor.getTarget).
    // We have to unregister it from the TargetActorRegistry, otherwise,
    // if we navigate back to it, the next DOMWindowCreated won't create a new target for it.
    onWindowGlobalDestroyed(target.defaultView.windowGlobalChild.innerWindowId);
  }
}

/**
 * Return an existing Window Global target for given a WatcherActor
 * and against a given WindowGlobal.
 *
 * @param {Object} options
 * @param {String} options.watcherDataObject
 * @param {Number} options.innerWindowId
 *                 The WindowGlobal inner window ID.
 *
 * @returns {WindowGlobalTargetActor|null}
 */
function findTargetActor({ watcherDataObject, innerWindowId }) {
  // First let's check if a target was created for this watcher actor in this specific
  // DevToolsProcessChild instance.
  const targetActor = watcherDataObject.actors.find(
    actor => actor.innerWindowId == innerWindowId
  );
  if (targetActor) {
    return targetActor;
  }

  // Ensure retrieving the one target actor related to this connection.
  // This allows to distinguish actors created for various toolboxes.
  // For ex, regular toolbox versus browser console versus browser toolbox
  const connectionPrefix = watcherDataObject.watcherActorID.replace(
    /watcher\d+$/,
    ""
  );
  const targetActors = lazy.TargetActorRegistry.getTargetActors(
    watcherDataObject.sessionContext,
    connectionPrefix
  );

  return targetActors.find(actor => actor.innerWindowId == innerWindowId);
}

export const WindowGlobalTargetWatcher = {
  watch,
  unwatch,
  createTargetsForWatcher,
  destroyTargetsForWatcher,
};
