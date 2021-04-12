/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Helper module around `sharedData` object that helps storing the state
 * of all observed Targets and Resources, that, for all DevTools connections.
 * Here is a few words about the C++ implementation of sharedData:
 * https://searchfox.org/mozilla-central/rev/bc3600def806859c31b2c7ac06e3d69271052a89/dom/ipc/SharedMap.h#30-55
 *
 * We may have more than one DevToolsServer and one server may have more than one
 * client. This module will be the single source of truth in the parent process,
 * in order to know which targets/resources are currently observed. It will also
 * be used to declare when something starts/stops being observed.
 *
 * `sharedData` is a platform API that helps sharing JS Objects across processes.
 * We use it in order to communicate to the content process which targets and resources
 * should be observed. Content processes read this data only once, as soon as they are created.
 * It isn't used beyond this point. Content processes are not going to update it.
 * We will notify about changes in observed targets and resources for already running
 * processes by some other means. (Via JS Window Actor queries "DevTools:(un)watch(Resources|Target)")
 * This means that only this module will update the "DevTools:watchedPerWatcher" value.
 * From the parent process, we should be going through this module to fetch the data,
 * while from the content process, we will read `sharedData` directly.
 */

var EXPORTED_SYMBOLS = ["WatcherRegistry"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { ActorManagerParent } = ChromeUtils.import(
  "resource://gre/modules/ActorManagerParent.jsm"
);
const { WatchedDataHelpers } = ChromeUtils.import(
  "resource://devtools/server/actors/watcher/WatchedDataHelpers.jsm"
);
const { SUPPORTED_DATA } = WatchedDataHelpers;

// Define the Map that will be saved in `sharedData`.
// It is keyed by WatcherActor ID and values contains following attributes:
// - targets: Set of strings, refering to target types to be listened to
// - resources: Set of strings, refering to resource types to be observed
// - browserId: Optional, if set, restrict the observation to one specific Browser Element tree.
//              It can be a tab, a top-level window or a top-level iframe (e.g. special privileged iframe)
//              See https://searchfox.org/mozilla-central/rev/31d8600b73dc85b4cdbabf45ac3f1a9c11700d8e/dom/chrome-webidl/BrowsingContext.webidl#114-121
//              for more information.
// - connectionPrefix: The DevToolsConnection prefix of the watcher actor. Used to compute new actor ID in the content processes.
//
// Unfortunately, `sharedData` is subject to race condition and may have side effect
// when read/written from multiple places in the same process,
// which is why this map should be considered as the single source of truth.
const watchedDataByWatcherActor = new Map();

// In parallel to the previous map, keep all the WatcherActor keyed by the same WatcherActor ID,
// the WatcherActor ID. We don't (and can't) propagate the WatcherActor instances to the content
// processes, but still would like to match them by their ID.
const watcherActors = new Map();

// Name of the attribute into which we save this Map in `sharedData` object.
const SHARED_DATA_KEY_NAME = "DevTools:watchedPerWatcher";

/**
 * Use `sharedData` to allow processes, early during their creation,
 * to know which resources should be listened to. This will be read
 * from the Target actor, when it gets created early during process start,
 * in order to start listening to the expected resource types.
 */
function persistMapToSharedData() {
  Services.ppmm.sharedData.set(SHARED_DATA_KEY_NAME, watchedDataByWatcherActor);
  // Request to immediately flush the data to the content processes in order to prevent
  // races (bug 1644649). Otherwise content process may have outdated sharedData
  // and try to create targets for Watcher actor that already stopped watching for targets.
  Services.ppmm.sharedData.flush();
}

const WatcherRegistry = {
  /**
   * Tells if a given watcher currently watches for a given target type.
   *
   * @param WatcherActor watcher
   *               The WatcherActor which should be listening.
   * @param string targetType
   *               The new target type to query.
   * @return boolean
   *         Returns true if already watching.
   */
  isWatchingTargets(watcher, targetType) {
    const watchedData = this.getWatchedData(watcher);
    return watchedData && watchedData.targets.includes(targetType);
  },

  /**
   * Retrieve the data saved into `sharedData` that is used to know
   * about which type of targets and resources we care listening about.
   * `watchedDataByWatcherActor` is saved into `sharedData` after each mutation,
   * but `watchedDataByWatcherActor` is the source of truth.
   *
   * @param WatcherActor watcher
   *               The related WatcherActor which starts/stops observing.
   * @param object options (optional)
   *               A dictionary object with `createData` boolean attribute.
   *               If this attribute is set to true, we create the data structure in the Map
   *               if none exists for this prefix.
   */
  getWatchedData(watcher, { createData = false } = {}) {
    // Use WatcherActor ID as a key as we may have multiple clients willing to watch for targets.
    // For example, a Browser Toolbox debugging everything and a Content Toolbox debugging
    // just one tab. We might also have multiple watchers, on the same connection when using about:debugging.
    const watcherActorID = watcher.actorID;
    let watchedData = watchedDataByWatcherActor.get(watcherActorID);
    if (!watchedData && createData) {
      watchedData = {
        // The Browser ID will be helpful to identify which BrowsingContext should be considered
        // when running code in the content process. Browser ID, compared to BrowsingContext ID won't change
        // if we navigate to the parent process or if a new BrowsingContext is used for the <browser> element
        // we are currently inspecting.
        browserId: watcher.browserId,
        // The DevToolsServerConnection prefix will be used to compute actor IDs created in the content process
        connectionPrefix: watcher.conn.prefix,
        // Expose watcher traits so we can retrieve them in content process.
        // This should be removed as part of Bug 1700092.
        watcherTraits: watcher.form().traits,
      };
      // Define empty default array for all data
      for (const name of Object.values(SUPPORTED_DATA)) {
        watchedData[name] = [];
      }
      watchedDataByWatcherActor.set(watcherActorID, watchedData);
      watcherActors.set(watcherActorID, watcher);
    }
    return watchedData;
  },

  /**
   * Given a Watcher Actor ID, return the related Watcher Actor instance.
   *
   * @param String actorID
   *        The Watcher Actor ID to search for.
   * @return WatcherActor
   *         The Watcher Actor instance.
   */
  getWatcher(actorID) {
    return watcherActors.get(actorID);
  },

  /**
   * Notify that a given watcher added an entry in a given data type.
   *
   * @param WatcherActor watcher
   *               The WatcherActor which starts observing.
   * @param string type
   *               The type of data to be added
   * @param Array<Object> entries
   *               The values to be added to this type of data
   */
  addWatcherDataEntry(watcher, type, entries) {
    const watchedData = this.getWatchedData(watcher, {
      createData: true,
    });

    if (!(type in watchedData)) {
      throw new Error(`Unsupported watcher data type: ${type}`);
    }

    WatchedDataHelpers.addWatchedDataEntry(watchedData, type, entries);

    // Register the JS Window Actor the first time we start watching for something (e.g. resource, target, …).
    registerJSWindowActor();

    persistMapToSharedData();
  },

  /**
   * Notify that a given watcher removed an entry in a given data type.
   *
   * See `addWatcherDataEntry` for argument definition.
   *
   * @return boolean
   *         True if we such entry was already registered, for this watcher actor.
   */
  removeWatcherDataEntry(watcher, type, entries) {
    const watchedData = this.getWatchedData(watcher);
    if (!watchedData) {
      return false;
    }

    if (!(type in watchedData)) {
      throw new Error(`Unsupported watcher data type: ${type}`);
    }

    if (
      !WatchedDataHelpers.removeWatchedDataEntry(watchedData, type, entries)
    ) {
      return false;
    }

    const isWatchingSomething = Object.values(SUPPORTED_DATA).some(
      dataType => watchedData[dataType].length > 0
    );
    if (!isWatchingSomething) {
      watchedDataByWatcherActor.delete(watcher.actorID);
      watcherActors.delete(watcher.actorID);
    }

    persistMapToSharedData();

    return true;
  },

  /**
   * Cleanup everything about a given watcher actor.
   * Remove it from any registry so that we stop interacting with it.
   *
   * The watcher would be automatically unregistered from removeWatcherEntry,
   * if we remove all entries. But we aren't removing all breakpoints.
   * So here, we force clearing any reference to the watcher actor when it destroys.
   */
  unregisterWatcher(watcher) {
    watchedDataByWatcherActor.delete(watcher.actorID);
    watcherActors.delete(watcher.actorID);
    this.maybeUnregisteringJSWindowActor();
  },

  /**
   * Notify that a given watcher starts observing a new target type.
   *
   * @param WatcherActor watcher
   *               The WatcherActor which starts observing.
   * @param string targetType
   *               The new target type to start listening to.
   */
  watchTargets(watcher, targetType) {
    this.addWatcherDataEntry(watcher, SUPPORTED_DATA.TARGETS, [targetType]);
  },

  /**
   * Notify that a given watcher stops observing a given target type.
   *
   * See `watchTargets` for argument definition.
   *
   * @return boolean
   *         True if we were watching for this target type, for this watcher actor.
   */
  unwatchTargets(watcher, targetType) {
    return this.removeWatcherDataEntry(watcher, SUPPORTED_DATA.TARGETS, [
      targetType,
    ]);
  },

  /**
   * Notify that a given watcher starts observing new resource types.
   *
   * @param WatcherActor watcher
   *               The WatcherActor which starts observing.
   * @param Array<string> resourceTypes
   *               The new resource types to start listening to.
   */
  watchResources(watcher, resourceTypes) {
    this.addWatcherDataEntry(watcher, SUPPORTED_DATA.RESOURCES, resourceTypes);
  },

  /**
   * Notify that a given watcher stops observing given resource types.
   *
   * See `watchResources` for argument definition.
   *
   * @return boolean
   *         True if we were watching for this resource type, for this watcher actor.
   */
  unwatchResources(watcher, resourceTypes) {
    return this.removeWatcherDataEntry(
      watcher,
      SUPPORTED_DATA.RESOURCES,
      resourceTypes
    );
  },

  /**
   * Unregister the JS Window Actor if there is no more DevTools code observing any target/resource.
   */
  maybeUnregisteringJSWindowActor() {
    if (watchedDataByWatcherActor.size == 0) {
      unregisterJSWindowActor();
    }
  },
};

// Boolean flag to know if the DevToolsFrame JS Window Actor is currently registered
let isJSWindowActorRegistered = false;

/**
 * Register the JSWindowActor pair "DevToolsFrame".
 *
 * We should call this method before we try to use this JS Window Actor from the parent process
 * (via `WindowGlobal.getActor("DevToolsFrame")` or `WindowGlobal.getActor("DevToolsWorker")`).
 * Also, registering it will automatically force spawing the content process JSWindow Actor
 * anytime a new document is opened (via DOMWindowCreated event).
 */

const JSWindowActorsConfig = {
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
        pageshow: {},
        pagehide: {},
      },
    },
    allFrames: true,
  },
  DevToolsWorker: {
    parent: {
      moduleURI:
        "resource://devtools/server/connectors/js-window-actor/DevToolsWorkerParent.jsm",
    },
    child: {
      moduleURI:
        "resource://devtools/server/connectors/js-window-actor/DevToolsWorkerChild.jsm",
      events: {
        DOMWindowCreated: {},
      },
    },
    allFrames: true,
  },
};

function registerJSWindowActor() {
  if (isJSWindowActorRegistered) {
    return;
  }
  isJSWindowActorRegistered = true;
  ActorManagerParent.addJSWindowActors(JSWindowActorsConfig);
}

function unregisterJSWindowActor() {
  if (!isJSWindowActorRegistered) {
    return;
  }
  isJSWindowActorRegistered = false;

  for (const JSWindowActorName of Object.keys(JSWindowActorsConfig)) {
    // ActorManagerParent doesn't expose a "removeActors" method, but it would be equivalent to that:
    ChromeUtils.unregisterWindowActor(JSWindowActorName);
  }
}
