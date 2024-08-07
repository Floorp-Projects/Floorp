/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { EventEmitter } from "resource://gre/modules/EventEmitter.sys.mjs";
import { ContentProcessWatcherRegistry } from "resource://devtools/server/connectors/js-process-actor/ContentProcessWatcherRegistry.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(
  lazy,
  {
    ProcessTargetWatcher:
      "resource://devtools/server/connectors/js-process-actor/target-watchers/process.sys.mjs",
    SessionDataHelpers:
      "resource://devtools/server/actors/watcher/SessionDataHelpers.sys.mjs",
    ServiceWorkerTargetWatcher:
      "resource://devtools/server/connectors/js-process-actor/target-watchers/service_worker.sys.mjs",
    SharedWorkerTargetWatcher:
      "resource://devtools/server/connectors/js-process-actor/target-watchers/shared_worker.sys.mjs",
    WorkerTargetWatcher:
      "resource://devtools/server/connectors/js-process-actor/target-watchers/worker.sys.mjs",
    WindowGlobalTargetWatcher:
      "resource://devtools/server/connectors/js-process-actor/target-watchers/window-global.sys.mjs",
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

export class DevToolsProcessChild extends JSProcessActorChild {
  constructor() {
    super();

    // The EventEmitter interface is used for DevToolsTransport's packet-received event.
    EventEmitter.decorate(this);
  }

  #watchers = {
    // Keys are target types, which are defined in this CommonJS Module:
    // https://searchfox.org/mozilla-central/rev/0e9ea50a999420d93df0e4e27094952af48dd3b8/devtools/server/actors/targets/index.js#7-14
    // We avoid loading it as this ESM should be lightweight and avoid spawning DevTools CommonJS Loader until
    // whe know we have to instantiate a Target Actor.
    frame: {
      // Number of active watcher actors currently watching for the given target type
      activeListener: 0,

      // Instance of a target watcher class whose task is to observe new target instances
      get watcher() {
        return lazy.WindowGlobalTargetWatcher;
      },
    },

    process: {
      activeListener: 0,
      get watcher() {
        return lazy.ProcessTargetWatcher;
      },
    },

    worker: {
      activeListener: 0,
      get watcher() {
        return lazy.WorkerTargetWatcher;
      },
    },

    service_worker: {
      activeListener: 0,
      get watcher() {
        return lazy.ServiceWorkerTargetWatcher;
      },
    },

    shared_worker: {
      activeListener: 0,
      get watcher() {
        return lazy.SharedWorkerTargetWatcher;
      },
    },
  };

  #initialized = false;

  /**
   * Called when this JSProcess Actor instantiate either when we start observing for first target types,
   * or when the process just started.
   */
  instantiate() {
    if (this.#initialized) {
      return;
    }
    this.#initialized = true;
    // Create and watch for future target actors for each watcher currently watching some target types
    for (const watcherDataObject of ContentProcessWatcherRegistry.getAllWatchersDataObjects()) {
      this.#watchInitialTargetsForWatcher(watcherDataObject);
    }
  }

  /**
   * Instantiate and watch future target actors based on the already watched targets.
   *
   * @param Object watcherDataObject
   *        See ContentProcessWatcherRegistry.
   */
  #watchInitialTargetsForWatcher(watcherDataObject) {
    const { sessionData, sessionContext } = watcherDataObject;

    // About WebExtension, see note in addOrSetSessionDataEntry.
    // Their target actor aren't created by this class, but session data is still managed by it
    // and we need to pass the initial session data coming to already instantiated target actor.
    if (sessionContext.type == "webextension") {
      const { watcherActorID } = watcherDataObject;
      const connectionPrefix = watcherActorID.replace(/watcher\d+$/, "");
      const targetActors = lazy.TargetActorRegistry.getTargetActors(
        sessionContext,
        connectionPrefix
      );
      if (targetActors.length) {
        // Pass initialization data to the target actor
        for (const type in sessionData) {
          // `sessionData` will also contain `browserId` as well as entries with empty arrays,
          // which shouldn't be processed.
          const entries = sessionData[type];
          if (!Array.isArray(entries) || !entries.length) {
            continue;
          }
          targetActors[0].addOrSetSessionDataEntry(type, entries, false, "set");
        }
      }
    }

    // Ignore the call if the watched targets property isn't populated yet.
    // This typically happens when instantiating the JS Process Actor on toolbox opening,
    // where the actor is spawn early and a watchTarget message comes later with the `targets` array set.
    if (!sessionData.targets) {
      return;
    }

    for (const targetType of sessionData.targets) {
      this.#watchNewTargetTypeForWatcher(watcherDataObject, targetType, true);
    }
  }

  /**
   * Instantiate and watch future target actors based on the already watched targets.
   *
   * @param Object watcherDataObject
   *        See ContentProcessWatcherRegistry.
   * @param String targetType
   *        New typeof target to start watching.
   * @param Boolean isProcessActorStartup
   *        True when we are watching for targets during this JS Process actor instantiation.
   *        It shouldn't be the case on toolbox opening, but only when a new process starts.
   *        On toolbox opening, the Actor will receive an explicit watchTargets query.
   */
  #watchNewTargetTypeForWatcher(
    watcherDataObject,
    targetType,
    isProcessActorStartup
  ) {
    const { watchingTargetTypes } = watcherDataObject;
    // Ensure creating and watching only once per target type and watcher actor.
    if (watchingTargetTypes.includes(targetType)) {
      return;
    }
    watchingTargetTypes.push(targetType);

    // Update sessionData as watched target types are a Session Data
    // used later for example by worker target watcher
    lazy.SessionDataHelpers.addOrSetSessionDataEntry(
      watcherDataObject.sessionData,
      "targets",
      [targetType],
      "add"
    );

    this.#watchers[targetType].activeListener++;

    // Start listening for platform events when we are observing this type for the first time
    if (this.#watchers[targetType].activeListener === 1) {
      this.#watchers[targetType].watcher.watch();
    }

    // And instantiate targets for the already existing instances
    this.#watchers[targetType].watcher.createTargetsForWatcher(
      watcherDataObject,
      isProcessActorStartup
    );
  }

  /**
   * Stop watching for all target types and destroy all existing targets actor
   * related to a given watcher actor.
   *
   * @param {Object} watcherDataObject
   * @param {String} targetType
   * @param {Object} options
   */
  #unwatchTargetsForWatcher(watcherDataObject, targetType, options) {
    const { watchingTargetTypes } = watcherDataObject;
    const targetTypeIndex = watchingTargetTypes.indexOf(targetType);
    // Ignore targetTypes which were not observed
    if (targetTypeIndex === -1) {
      return;
    }
    // Update to the new list of currently watched target types
    watchingTargetTypes.splice(targetTypeIndex, 1);

    // Update sessionData as watched target types are a Session Data
    // used later for example by worker target watcher
    lazy.SessionDataHelpers.removeSessionDataEntry(
      watcherDataObject.sessionData,
      "targets",
      [targetType]
    );

    this.#watchers[targetType].activeListener--;

    // Stop observing for platform events
    if (this.#watchers[targetType].activeListener === 0) {
      this.#watchers[targetType].watcher.unwatch();
    }

    // Destroy all targets which are still instantiated for this type
    this.#watchers[targetType].watcher.destroyTargetsForWatcher(
      watcherDataObject,
      options
    );

    // Unregister the watcher if we stopped watching for all target types
    if (!watchingTargetTypes.length) {
      ContentProcessWatcherRegistry.remove(watcherDataObject);
    }

    // If we removed the last watcher, clean the internal state of this class.
    if (ContentProcessWatcherRegistry.isEmpty()) {
      this.didDestroy(options);
    }
  }

  /**
   * Cleanup everything around a given watcher actor
   *
   * @param {Object} watcherDataObject
   */
  #destroyWatcher(watcherDataObject) {
    const { watchingTargetTypes } = watcherDataObject;
    // Clone the array as it will be modified during the loop execution
    for (const targetType of [...watchingTargetTypes]) {
      this.#unwatchTargetsForWatcher(watcherDataObject, targetType);
    }
  }

  /**
   * Used by DevTools Transport to send packets to the content process.
   *
   * @param {JSON} packet
   * @param {String} prefix
   */
  sendPacket(packet, prefix) {
    this.sendAsyncMessage("DevToolsProcessChild:packet", { packet, prefix });
  }

  /**
   * JsWindowActor API
   */

  async sendQuery(msg, args) {
    try {
      const res = await super.sendQuery(msg, args);
      return res;
    } catch (e) {
      console.error("Failed to sendQuery in DevToolsProcessChild", msg);
      console.error(e.toString());
      throw e;
    }
  }

  /**
   * Called by the JSProcessActor API when the process process sent us a message.
   */
  receiveMessage(message) {
    switch (message.name) {
      case "DevToolsProcessParent:watchTargets": {
        const { watcherActorID, targetType } = message.data;
        const watcherDataObject =
          ContentProcessWatcherRegistry.getWatcherDataObject(watcherActorID);
        return this.#watchNewTargetTypeForWatcher(
          watcherDataObject,
          targetType
        );
      }
      case "DevToolsProcessParent:unwatchTargets": {
        const { watcherActorID, targetType, options } = message.data;
        const watcherDataObject =
          ContentProcessWatcherRegistry.getWatcherDataObject(watcherActorID);
        return this.#unwatchTargetsForWatcher(
          watcherDataObject,
          targetType,
          options
        );
      }
      case "DevToolsProcessParent:addOrSetSessionDataEntry": {
        const { watcherActorID, type, entries, updateType } = message.data;
        return this.#addOrSetSessionDataEntry(
          watcherActorID,
          type,
          entries,
          updateType
        );
      }
      case "DevToolsProcessParent:removeSessionDataEntry": {
        const { watcherActorID, type, entries } = message.data;
        return this.#removeSessionDataEntry(watcherActorID, type, entries);
      }
      case "DevToolsProcessParent:destroyWatcher": {
        const { watcherActorID } = message.data;
        const watcherDataObject =
          ContentProcessWatcherRegistry.getWatcherDataObject(
            watcherActorID,
            true
          );
        // The watcher may already be destroyed if the client unwatched for all target types.
        if (watcherDataObject) {
          return this.#destroyWatcher(watcherDataObject);
        }
        return null;
      }
      case "DevToolsProcessParent:packet":
        return this.emit("packet-received", message);
      default:
        throw new Error(
          "Unsupported message in DevToolsProcessParent: " + message.name
        );
    }
  }

  /**
   * The parent process requested that some session data have been added or set.
   *
   * @param {String} watcherActorID
   *        The Watcher Actor ID requesting to add new session data
   * @param {String} type
   *        The type of data to be added
   * @param {Array<Object>} entries
   *        The values to be added to this type of data
   * @param {String} updateType
   *        "add" will only add the new entries in the existing data set.
   *        "set" will update the data set with the new entries.
   */
  async #addOrSetSessionDataEntry(watcherActorID, type, entries, updateType) {
    const watcherDataObject =
      ContentProcessWatcherRegistry.getWatcherDataObject(watcherActorID);

    // Maintain the copy of `sessionData` so that it is up-to-date when
    // a new worker target needs to be instantiated
    const { sessionData } = watcherDataObject;
    lazy.SessionDataHelpers.addOrSetSessionDataEntry(
      sessionData,
      type,
      entries,
      updateType
    );

    // This type is really specific to Service Workers and doesn't need to be transferred to any target.
    // We only need to instantiate and destroy the target actors based on this new host.
    const { watchingTargetTypes } = watcherDataObject;
    if (type == "browser-element-host") {
      if (watchingTargetTypes.includes("service_worker")) {
        this.#watchers.service_worker.watcher.updateBrowserElementHost(
          watcherDataObject
        );
      }
      return;
    }

    const promises = [];
    for (const targetActor of watcherDataObject.actors) {
      promises.push(
        targetActor.addOrSetSessionDataEntry(type, entries, false, updateType)
      );
    }

    // Very special codepath for Web Extensions.
    // Their WebExtension Target Actor is still created manually by WebExtensionDescritpor.getTarget,
    // via a message manager. That, instead of being instantiated via the WatcherActor.watchTargets and this JSProcess actor.
    // The Watcher Actor will still instantiate a JS Actor for the WebExt DOM Content Process
    // and send the addOrSetSessionDataEntry query. But as the target actor isn't managed by the JS Actor,
    // we have to manually retrieve it via the TargetActorRegistry.
    if (sessionData.sessionContext.type == "webextension") {
      const connectionPrefix = watcherActorID.replace(/watcher\d+$/, "");
      const targetActors = lazy.TargetActorRegistry.getTargetActors(
        sessionData.sessionContext,
        connectionPrefix
      );
      // We will have a single match only in the DOM Process where the add-on runs
      if (targetActors.length) {
        promises.push(
          targetActors[0].addOrSetSessionDataEntry(
            type,
            entries,
            false,
            updateType
          )
        );
      }
    }
    await Promise.all(promises);

    if (watchingTargetTypes.includes("worker")) {
      await this.#watchers.worker.watcher.addOrSetSessionDataEntry(
        watcherDataObject,
        type,
        entries,
        updateType
      );
    }
    if (watchingTargetTypes.includes("service_worker")) {
      await this.#watchers.service_worker.watcher.addOrSetSessionDataEntry(
        watcherDataObject,
        type,
        entries,
        updateType
      );
    }
    if (watchingTargetTypes.includes("shared_worker")) {
      await this.#watchers.shared_worker.watcher.addOrSetSessionDataEntry(
        watcherDataObject,
        type,
        entries,
        updateType
      );
    }
  }

  /**
   * The parent process requested that some session data have been removed.
   *
   * @param {String} watcherActorID
   *        The Watcher Actor ID requesting to remove session data
   * @param {String}} type
   *        The type of data to be removed
   * @param {Array<Object>} entries
   *        The values to be removed to this type of data
   */
  #removeSessionDataEntry(watcherActorID, type, entries) {
    const watcherDataObject =
      ContentProcessWatcherRegistry.getWatcherDataObject(watcherActorID, true);

    // When we unwatch resources after targets during the devtools shutdown,
    // the watcher will be removed on last target type unwatch.
    if (!watcherDataObject) {
      return;
    }
    const { actors, sessionData, watchingTargetTypes } = watcherDataObject;

    // Maintain the copy of `sessionData` so that it is up-to-date when
    // a new worker target needs to be instantiated
    lazy.SessionDataHelpers.removeSessionDataEntry(sessionData, type, entries);

    for (const targetActor of actors) {
      targetActor.removeSessionDataEntry(type, entries);
    }

    // Special code paths for webextension toolboxes and worker targets
    // See addOrSetSessionDataEntry for more details.

    if (sessionData.sessionContext.type == "webextension") {
      const connectionPrefix = watcherActorID.replace(/watcher\d+$/, "");
      const targetActors = lazy.TargetActorRegistry.getTargetActors(
        sessionData.sessionContext,
        connectionPrefix
      );
      if (targetActors.length) {
        targetActors[0].removeSessionDataEntry(type, entries);
      }
    }

    if (watchingTargetTypes.includes("worker")) {
      this.#watchers.worker.watcher.removeSessionDataEntry(
        watcherDataObject,
        type,
        entries
      );
    }
    if (watchingTargetTypes.includes("service_worker")) {
      this.#watchers.service_worker.watcher.removeSessionDataEntry(
        watcherDataObject,
        type,
        entries
      );
    }
    if (watchingTargetTypes.includes("shared_worker")) {
      this.#watchers.shared_worker.watcher.removeSessionDataEntry(
        watcherDataObject,
        type,
        entries
      );
    }
  }

  /**
   * Observer service notification handler.
   *
   * @param {DOMWindow|Document} subject
   *        A window for *-document-global-created
   *        A document for *-page-{shown|hide}
   * @param {String} topic
   */
  observe = (subject, topic) => {
    if (topic === "init-devtools-content-process-actor") {
      // This is triggered by the process actor registration and some code in process-helper.js
      // which defines a unique topic to be observed
      this.instantiate();
    }
  };

  /**
   * Called by JS Process Actor API when the current process is destroyed,
   * but also within this class when the last watcher stopped watching for targets.
   */
  didDestroy() {
    // Unregister all the active watchers.
    // This will destroy all the active target actors and unregister the target observers.
    for (const watcherDataObject of ContentProcessWatcherRegistry.getAllWatchersDataObjects()) {
      this.#destroyWatcher(watcherDataObject);
    }

    // The previous for loop should have removed all the elements,
    // but just to be safe, wipe all stored data to avoid any possible leak.
    ContentProcessWatcherRegistry.clear();
  }
}

export class BrowserToolboxDevToolsProcessChild extends DevToolsProcessChild {}
