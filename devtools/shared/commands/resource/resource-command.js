/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { throttle } = require("devtools/shared/throttle");

const BROWSERTOOLBOX_FISSION_ENABLED = "devtools.browsertoolbox.fission";

class ResourceCommand {
  /**
   * This class helps retrieving existing and listening to resources.
   * A resource is something that:
   *  - the target you are debugging exposes
   *  - can be created as early as the process/worker/page starts loading
   *  - can already exist, or will be created later on
   *  - doesn't require any user data to be fetched, only a type/category
   *
   * @param object commands
   *        The commands object with all interfaces defined from devtools/shared/commands/
   */
  constructor({ commands }) {
    this.targetCommand = commands.targetCommand;

    this._onTargetAvailable = this._onTargetAvailable.bind(this);
    this._onTargetDestroyed = this._onTargetDestroyed.bind(this);

    this._onResourceAvailable = this._onResourceAvailable.bind(this);
    this._onResourceDestroyed = this._onResourceDestroyed.bind(this);

    // Array of all the currently registered watchers, which contains object with attributes:
    // - {String} resources: list of all resource watched by this one watcher
    // - {Function} onAvailable: watcher's function to call when a new resource is available
    // - {Function} onUpdated: watcher's function to call when a resource has been updated
    // - {Function} onDestroyed: watcher's function to call when a resource is destroyed
    this._watchers = [];

    // Set of watchers currently going through watchResources, only used to handle
    // early calls to unwatchResources. Using a Set instead of an array for easier
    // delete operations.
    this._pendingWatchers = new Set();

    // Cache for all resources by the order that the resource was taken.
    this._cache = [];
    this._listenedResources = new Set();

    // WeakMap used to avoid starting a legacy listener twice for the same
    // target + resource-type pair. Legacy listener creation can be subject to
    // race conditions.
    // Maps a target front to an array of resource types.
    this._existingLegacyListeners = new WeakMap();
    this._processingExistingResources = new Set();

    // List of targetFront event listener unregistration functions keyed by target front.
    // These are called when unwatching resources, so if a consumer starts watching resources again,
    // we don't have listeners registered twice.
    this._offTargetFrontListeners = new Map();

    this._notifyWatchers = this._notifyWatchers.bind(this);
    this._throttledNotifyWatchers = throttle(this._notifyWatchers, 100);
  }

  get watcherFront() {
    return this.targetCommand.watcherFront;
  }

  /**
   * Return all specified resources cached in this watcher.
   *
   * @param {String} resourceType
   * @return {Array} resources cached in this watcher
   */
  getAllResources(resourceType) {
    return this._cache.filter(r => r.resourceType === resourceType);
  }

  /**
   * Return the specified resource cached in this watcher.
   *
   * @param {String} resourceType
   * @param {String} resourceId
   * @return {Object} resource cached in this watcher
   */
  getResourceById(resourceType, resourceId) {
    return this._cache.find(
      r => r.resourceType === resourceType && r.resourceId === resourceId
    );
  }

  /**
   * Request to start retrieving all already existing instances of given
   * type of resources and also start watching for the one to be created after.
   *
   * @param {Array:string} resources
   *        List of all resources which should be fetched and observed.
   * @param {Object} options
   *        - {Function} onAvailable: This attribute is mandatory.
   *                                  Function which will be called with an array of resources
   *                                  each time resource(s) are created.
   *                                  A second dictionary argument with `areExistingResources` boolean
   *                                  attribute helps knowing if that's live resources, or some coming
   *                                  from ResourceCommand cache.
   *        - {Function} onUpdated:   This attribute is optional.
   *                                  Function which will be called with an array of updates resources
   *                                  each time resource(s) are updated.
   *                                  These resources were previously notified via onAvailable.
   *        - {Function} onDestroyed: This attribute is optional.
   *                                  Function which will be called with an array of deleted resources
   *                                  each time resource(s) are destroyed.
   *        - {boolean} ignoreExistingResources:
   *                                  This attribute is optional. Default value is false.
   *                                  If set to true, onAvailable won't be called with
   *                                  existing resources.
   */
  async watchResources(resources, options) {
    const {
      onAvailable,
      onUpdated,
      onDestroyed,
      ignoreExistingResources = false,
    } = options;

    if (typeof onAvailable !== "function") {
      throw new Error(
        "ResourceCommand.watchResources expects an onAvailable function as argument"
      );
    }

    for (const type of resources) {
      if (!this._isValidResourceType(type)) {
        throw new Error(
          `ResourceCommand.watchResources invoked with an unknown type: "${type}"`
        );
      }
    }

    // Pending watchers are used in unwatchResources to remove watchers which
    // are not fully registered yet. Store `onAvailable` which is the unique key
    // for a watcher, as well as the resources array, so that unwatchResources
    // can update the array if we stop watching a specific resource.
    const pendingWatcher = {
      resources,
      onAvailable,
    };
    this._pendingWatchers.add(pendingWatcher);

    // Bug 1675763: Watcher actor is not available in all situations yet.
    if (!this._listenerRegistered && this.watcherFront) {
      this._listenerRegistered = true;
      // Resources watched from the parent process will be emitted on the Watcher Actor.
      // So that we also have to listen for this event on it, in addition to all targets.
      this.watcherFront.on(
        "resource-available-form",
        this._onResourceAvailable.bind(this, {
          watcherFront: this.watcherFront,
        })
      );
      this.watcherFront.on(
        "resource-updated-form",
        this._onResourceUpdated.bind(this, { watcherFront: this.watcherFront })
      );
      this.watcherFront.on(
        "resource-destroyed-form",
        this._onResourceDestroyed.bind(this, {
          watcherFront: this.watcherFront,
        })
      );
    }

    const promises = [];
    for (const resource of resources) {
      promises.push(this._startListening(resource));
    }
    await Promise.all(promises);

    // The resource cache is immediately filled when receiving the sources, but they are
    // emitted with a delay due to throttling. Since the cache can contain resources that
    // will soon be emitted, we have to flush it before adding the new listeners.
    // Otherwise _forwardExistingResources might emit resources that will also be emitted by
    // the next `_notifyWatchers` call done when calling `_startListening`, which will pull the
    // "already existing" resources.
    this._notifyWatchers();

    // Update the _pendingWatchers set before adding the watcher to _watchers.
    this._pendingWatchers.delete(pendingWatcher);

    // If unwatchResources was called in the meantime, use pendingWatcher's
    // resources to get the updated list of watched resources.
    const watchedResources = pendingWatcher.resources;

    // If no resource needs to be watched anymore, do not add an empty watcher
    // to _watchers, and do not notify about cached resources.
    if (!watchedResources.length) {
      return;
    }

    // Register the watcher just after calling _startListening in order to avoid it being called
    // for already existing resources, which will optionally be notified via _forwardExistingResources
    this._watchers.push({
      resources: watchedResources,
      onAvailable,
      onUpdated,
      onDestroyed,
      pendingEvents: [],
    });

    if (!ignoreExistingResources) {
      await this._forwardExistingResources(watchedResources, onAvailable);
    }
  }

  /**
   * Stop watching for given type of resources.
   * See `watchResources` for the arguments as both methods receive the same.
   * Note that `onUpdated` and `onDestroyed` attributes of `options` aren't used here.
   * Only `onAvailable` attribute is looked up and we unregister all the other registered callbacks
   * when a matching available callback is found.
   */
  unwatchResources(resources, options) {
    const { onAvailable } = options;

    if (typeof onAvailable !== "function") {
      throw new Error(
        "ResourceCommand.unwatchResources expects an onAvailable function as argument"
      );
    }

    for (const type of resources) {
      if (!this._isValidResourceType(type)) {
        throw new Error(
          `ResourceCommand.unwatchResources invoked with an unknown type: "${type}"`
        );
      }
    }

    // Unregister the callbacks from the watchers registries.
    // Check _watchers for the fully initialized watchers, as well as
    // `_pendingWatchers` for new watchers still being created by `watchResources`
    const allWatchers = [...this._watchers, ...this._pendingWatchers];
    for (const watcherEntry of allWatchers) {
      // onAvailable is the only mandatory argument which ends up being used to match
      // the right watcher entry.
      if (watcherEntry.onAvailable == onAvailable) {
        // Remove all resources that we stop watching. We may still watch for some others.
        watcherEntry.resources = watcherEntry.resources.filter(resourceType => {
          return !resources.includes(resourceType);
        });
      }
    }
    this._watchers = this._watchers.filter(entry => {
      // Remove entries entirely if it isn't watching for any resource type
      return entry.resources.length > 0;
    });

    // Stop listening to all resources for which we removed the last watcher
    for (const resource of resources) {
      const isResourceWatched = allWatchers.some(watcherEntry =>
        watcherEntry.resources.includes(resource)
      );

      // Also check in _listenedResources as we may call unwatchResources
      // for resources that we haven't started watching for.
      if (!isResourceWatched && this._listenedResources.has(resource)) {
        this._stopListening(resource);
      }
    }

    // Stop watching for targets if we removed the last listener.
    if (this._listenedResources.size == 0) {
      this._unwatchAllTargets();
    }
  }

  /**
   * Wait for a single resource of the provided resourceType.
   *
   * @param {String} resourceType
   *        One of ResourceCommand.TYPES, type of the expected resource.
   * @param {Object} additional options
   *        - {Boolean} ignoreExistingResources: ignore existing resources or not.
   *        - {Function} predicate: if provided, will wait until a resource makes
   *          predicate(resource) return true.
   * @return {Promise<Object>}
   *         Return a promise which resolves once we fully settle the resource listener.
   *         You should await for its resolution before doing the action which may fire
   *         your resource.
   *         This promise will expose an object with `onResource` attribute,
   *         itself being a promise, which will resolve once a matching resource is received.
   */
  async waitForNextResource(
    resourceType,
    { ignoreExistingResources = false, predicate } = {}
  ) {
    // If no predicate was provided, convert to boolean to avoid resolving for
    // empty `resources` arrays.
    predicate = predicate || (resource => !!resource);

    let resolve;
    const promise = new Promise(r => (resolve = r));
    const onAvailable = async resources => {
      const matchingResource = resources.find(resource => predicate(resource));
      if (matchingResource) {
        this.unwatchResources([resourceType], { onAvailable });
        resolve(matchingResource);
      }
    };

    await this.watchResources([resourceType], {
      ignoreExistingResources,
      onAvailable,
    });
    return { onResource: promise };
  }

  /**
   * Start watching for all already existing and future targets.
   *
   * We are using ALL_TYPES, but this won't force listening to all types.
   * It will only listen for types which are defined by `TargetCommand.startListening`.
   */
  async _watchAllTargets() {
    if (!this._watchTargetsPromise) {
      // If this is the very first listener registered, of all kind of resource types:
      // * we want to start observing targets via TargetCommand
      // * _onTargetAvailable will be called for each already existing targets and the next one to come
      this._watchTargetsPromise = this.targetCommand.watchTargets({
        types: this.targetCommand.ALL_TYPES,
        onAvailable: this._onTargetAvailable,
        onDestroyed: this._onTargetDestroyed,
      });
    }
    return this._watchTargetsPromise;
  }

  _unwatchAllTargets() {
    if (!this._watchTargetsPromise) {
      return;
    }

    for (const offList of this._offTargetFrontListeners.values()) {
      offList.forEach(off => off());
    }
    this._offTargetFrontListeners.clear();

    this._watchTargetsPromise = null;
    this.targetCommand.unwatchTargets({
      types: this.targetCommand.ALL_TYPES,
      onAvailable: this._onTargetAvailable,
      onDestroyed: this._onTargetDestroyed,
    });
  }

  /**
   * For a given resource type, start the legacy listeners for all already existing targets.
   * Do that only if we have to. If this resourceType requires legacy listeners.
   */
  async _startLegacyListenersForExistingTargets(resourceType) {
    // If we were already listening to targets, we want to start the legacy listeners
    // for all already existing targets.
    const shouldRunLegacyListeners =
      !this.hasResourceCommandSupport(resourceType) ||
      this._shouldRunLegacyListenerEvenWithWatcherSupport(resourceType);
    if (shouldRunLegacyListeners) {
      const promises = [];
      const targets = this.targetCommand.getAllTargets(
        this.targetCommand.ALL_TYPES
      );
      for (const targetFront of targets) {
        // We disable warning in case we already registered the legacy listener for this target
        // as this code may race with the call from onTargetAvailable if we end up having multiple
        // calls to _startListening in parallel.
        promises.push(
          this._watchResourcesForTarget({
            targetFront,
            resourceType,
            disableWarning: true,
          })
        );
      }
      await Promise.all(promises);
    }
  }

  /**
   * Method called by the TargetCommand for each already existing or target which has just been created.
   *
   * @param {Front} targetFront
   *        The Front of the target that is available.
   *        This Front inherits from TargetMixin and is typically
   *        composed of a WindowGlobalTargetFront or ContentProcessTargetFront.
   */
  async _onTargetAvailable({ targetFront, isTargetSwitching }) {
    const resources = [];
    if (isTargetSwitching) {
      // WatcherActor currently only watches additional frame targets and
      // explicitely ignores top level one that may be created when navigating
      // to a new process.
      // In order to keep working resources that are being watched via the
      // Watcher actor, we have to unregister and re-register the resource
      // types. This will force calling `Resources.watchResources` on the new top
      // level target.
      for (const resourceType of Object.values(ResourceCommand.TYPES)) {
        // ...which has at least one listener...
        if (!this._listenedResources.has(resourceType)) {
          continue;
        }

        if (this._shouldRestartListenerOnTargetSwitching(resourceType)) {
          this._stopListening(resourceType, {
            bypassListenerCount: true,
          });
          resources.push(resourceType);
        }
      }
    }

    if (targetFront.isDestroyed()) {
      return;
    }

    // If we are target switching, we already stop & start listening to all the
    // currently monitored resources.
    if (!isTargetSwitching) {
      // For each resource type...
      for (const resourceType of Object.values(ResourceCommand.TYPES)) {
        // ...which has at least one listener...
        if (!this._listenedResources.has(resourceType)) {
          continue;
        }
        // ...request existing resource and new one to come from this one target
        // *but* only do that for backward compat, where we don't have the watcher API
        // (See bug 1626647)
        await this._watchResourcesForTarget({ targetFront, resourceType });
      }
    }

    // Compared to the TargetCommand and Watcher.watchTargets,
    // We do call Watcher.watchResources, but the events are fired on the target.
    // That's because the Watcher runs in the parent process/main thread, while resources
    // are available from the target's process/thread.
    const offResourceAvailable = targetFront.on(
      "resource-available-form",
      this._onResourceAvailable.bind(this, { targetFront })
    );
    const offResourceUpdated = targetFront.on(
      "resource-updated-form",
      this._onResourceUpdated.bind(this, { targetFront })
    );
    const offResourceDestroyed = targetFront.on(
      "resource-destroyed-form",
      this._onResourceDestroyed.bind(this, { targetFront })
    );

    const offList = this._offTargetFrontListeners.get(targetFront) || [];
    offList.push(
      offResourceAvailable,
      offResourceUpdated,
      offResourceDestroyed
    );

    if (isTargetSwitching) {
      await Promise.all(
        resources.map(resourceType =>
          this._startListening(resourceType, {
            bypassListenerCount: true,
          })
        )
      );
    }

    // DOCUMENT_EVENT's will-navigate should replace target actor's will-navigate event,
    // but only for targets provided by the watcher actor.
    // Emit a fake DOCUMENT_EVENT's "will-navigate" out of target actor's will-navigate
    // until watcher actor is supported by all descriptors (bug 1675763).
    if (!this.targetCommand.hasTargetWatcherSupport()) {
      const offWillNavigate = targetFront.on(
        "will-navigate",
        ({ url, isFrameSwitching }) => {
          targetFront.emit("resource-available-form", [
            {
              resourceType: this.TYPES.DOCUMENT_EVENT,
              name: "will-navigate",
              time: Date.now(), // will-navigate was not passing any timestamp
              isFrameSwitching,
              newURI: url,
            },
          ]);
        }
      );
      offList.push(offWillNavigate);
    }

    this._offTargetFrontListeners.set(targetFront, offList);
  }

  _shouldRestartListenerOnTargetSwitching(resourceType) {
    // Note that we aren't using isServerTargetSwitchingEnabled, nor checking the
    // server side target switching preference as we may have server side targets
    // even when this is false/disabled.
    // This will happen for bfcache navigations, even with server side targets disabled.
    // `followWindowGlobalLifeCycle` will be false for the first top level target
    // and only become true when doing a bfcache navigation.
    // (only server side targets follow the WindowGlobal lifecycle)
    // When server side targets are enabled, this will always be true.
    const isServerSideTarget = this.targetCommand.targetFront.targetForm
      .followWindowGlobalLifeCycle;
    if (isServerSideTarget) {
      // For top-level targets created from the server, only restart legacy
      // listeners.
      return !this.hasResourceCommandSupport(resourceType);
    }

    // For top-level targets created from the client we should always restart
    // listeners.
    return true;
  }

  /**
   * Method called by the TargetCommand when a target has just been destroyed
   * See _onTargetAvailable for arguments, they are the same.
   */
  _onTargetDestroyed({ targetFront }) {
    // Clear the map of legacy listeners for this target.
    this._existingLegacyListeners.set(targetFront, []);
    this._offTargetFrontListeners.delete(targetFront);

    // Purge the cache from any resource related to the destroyed target.
    // Top level BrowsingContext target will be purge via DOCUMENT_EVENT will-navigate events.
    // If we were to clean resources from target-destroyed, we will clear resources
    // happening between will-navigate and target-destroyed. Typically the navigation request
    if (!targetFront.isTopLevel || !targetFront.isBrowsingContext) {
      this._cache = this._cache.filter(
        cachedResource => cachedResource.targetFront !== targetFront
      );
    }

    //TODO: Is there a point in doing anything else?
    //
    // We could remove the available/destroyed event, but as the target is destroyed
    // its listeners will be destroyed anyway.
  }

  /**
   * Method called either by:
   * - the backward compatibility code (LegacyListeners)
   * - target actors RDP events
   * whenever an already existing resource is being listed or when a new one
   * has been created.
   *
   * @param {Object} source
   *        A dictionary object with only one of these two attributes:
   *        - targetFront: a Target Front, if the resource is watched from the target process or thread
   *        - watcherFront: a Watcher Front, if the resource is watched from the parent process
   * @param {Array<json/Front>} resources
   *        Depending on the resource Type, it can be an Array composed of either JSON objects or Fronts,
   *        which describes the resource.
   */
  async _onResourceAvailable({ targetFront, watcherFront }, resources) {
    let includesDocumentEventWillNavigate = false;
    let includesDocumentEventDomLoading = false;
    for (let resource of resources) {
      const { resourceType } = resource;

      if (watcherFront) {
        targetFront = await this._getTargetForWatcherResource(resource);
      }

      // isAlreadyExistingResource indicates that the resources already existed before
      // the resource command started watching for this type of resource.
      resource.isAlreadyExistingResource = this._processingExistingResources.has(
        resourceType
      );

      // Put the targetFront on the resource for easy retrieval.
      // (Resources from the legacy listeners may already have the attribute set)
      if (!resource.targetFront) {
        resource.targetFront = targetFront;
      }

      if (ResourceTransformers[resourceType]) {
        resource = ResourceTransformers[resourceType]({
          resource,
          targetCommand: this.targetCommand,
          targetFront,
          watcherFront: this.watcherFront,
        });
      }

      // Only consider top level document, and ignore remote iframes top document
      const isWillNavigate =
        resourceType == ResourceCommand.TYPES.DOCUMENT_EVENT &&
        resource.name == "will-navigate";
      if (isWillNavigate && resource.targetFront.isTopLevel) {
        includesDocumentEventWillNavigate = true;
        this._onWillNavigate(resource.targetFront);
      }

      if (
        resourceType == ResourceCommand.TYPES.DOCUMENT_EVENT &&
        resource.name == "dom-loading" &&
        resource.targetFront.isTopLevel
      ) {
        includesDocumentEventDomLoading = true;
      }

      this._queueResourceEvent("available", resourceType, resource);

      // Avoid storing will-navigate resource and consider it as a transcient resource.
      // We do that to prevent leaking this resource (and its target) on navigation.
      // We do clear _cache in _onWillNavigate, that we call a few lines before this.
      if (!isWillNavigate) {
        this._cache.push(resource);
      }
    }

    // If we receive the DOCUMENT_EVENT for:
    // - will-navigate
    // - dom-loading + we're using the service worker legacy listener
    // then flush immediately the resources to notify about the navigation sooner than later.
    // (this is especially useful for tests, even if they should probably avoid depending on this...)
    if (
      includesDocumentEventWillNavigate ||
      (includesDocumentEventDomLoading &&
        !this.targetCommand.hasTargetWatcherSupport("service_worker"))
    ) {
      this._notifyWatchers();
    } else {
      this._throttledNotifyWatchers();
    }
  }

  /**
   * Method called either by:
   * - the backward compatibility code (LegacyListeners)
   * - target actors RDP events
   * Called everytime a resource is updated in the remote target.
   *
   * @param {Object} source
   *        Please see _onResourceAvailable for this parameter.
   * @param {Array<Object>} updates
   *        Depending on the listener.
   *
   *        Among the element in the array, the following attributes are given special handling.
   *          - resourceType {String}:
   *            The type of resource to be updated.
   *          - resourceId {String}:
   *            The id of resource to be updated.
   *          - resourceUpdates {Object}:
   *            If resourceUpdates is in the element, a cached resource specified by resourceType
   *            and resourceId is updated by Object.assign(cachedResource, resourceUpdates).
   *          - nestedResourceUpdates {Object}:
   *            If `nestedResourceUpdates` is passed, update one nested attribute with a new value
   *            This allows updating one attribute of an object stored in a resource's attribute,
   *            as well as adding new elements to arrays.
   *            `path` is an array mentioning all nested attribute to walk through.
   *            `value` is the new nested attribute value to set.
   *
   *        And also, the element is passed to the listener as it is as “update” object.
   *        So if we don't want to update a cached resource but have information want to
   *        pass on to the listener, can pass it on using attributes other than the ones
   *        listed above.
   *        For example, if the element consists of like
   *        "{ resourceType:… resourceId:…, testValue: “test”, }”,
   *        the listener can receive the value as follows.
   *
   *        onResourceUpdate({ update }) {
   *          console.log(update.testValue); // “test” should be displayed
   *        }
   */
  async _onResourceUpdated({ targetFront, watcherFront }, updates) {
    for (const update of updates) {
      const {
        resourceType,
        resourceId,
        resourceUpdates,
        nestedResourceUpdates,
      } = update;

      if (!resourceId) {
        console.warn(`Expected resource ${resourceType} to have a resourceId`);
      }

      const existingResource = this._cache.find(
        cachedResource =>
          cachedResource.resourceType === resourceType &&
          cachedResource.resourceId === resourceId
      );

      if (!existingResource) {
        continue;
      }

      if (resourceUpdates) {
        Object.assign(existingResource, resourceUpdates);
      }

      if (nestedResourceUpdates) {
        for (const { path, value } of nestedResourceUpdates) {
          let target = existingResource;

          for (let i = 0; i < path.length - 1; i++) {
            target = target[path[i]];
          }

          target[path[path.length - 1]] = value;
        }
      }
      this._queueResourceEvent("updated", resourceType, {
        resource: existingResource,
        update,
      });
    }
    this._throttledNotifyWatchers();
  }

  /**
   * Called everytime a resource is destroyed in the remote target.
   * See _onResourceAvailable for the argument description.
   */
  async _onResourceDestroyed({ targetFront, watcherFront }, resources) {
    for (const resource of resources) {
      const { resourceType, resourceId } = resource;

      let index = -1;
      if (resourceId) {
        index = this._cache.findIndex(
          cachedResource =>
            cachedResource.resourceType == resourceType &&
            cachedResource.resourceId == resourceId
        );
      } else {
        index = this._cache.indexOf(resource);
      }
      if (index >= 0) {
        this._cache.splice(index, 1);
      } else {
        console.warn(
          `Resource ${resourceId || ""} of ${resourceType} was not found.`
        );
      }

      this._queueResourceEvent("destroyed", resourceType, resource);
    }
    this._throttledNotifyWatchers();
  }

  _queueResourceEvent(callbackType, resourceType, update) {
    for (const { resources, pendingEvents } of this._watchers) {
      // This watcher doesn't listen to this type of resource
      if (!resources.includes(resourceType)) {
        continue;
      }
      // If we receive a new event of the same type, accumulate the new update in the last event
      if (pendingEvents.length > 0) {
        const lastEvent = pendingEvents[pendingEvents.length - 1];
        if (lastEvent.callbackType == callbackType) {
          lastEvent.updates.push(update);
          continue;
        }
      }
      // Otherwise, pile up a new event, which will force calling watcher
      // callback a new time
      pendingEvents.push({
        callbackType,
        updates: [update],
      });
    }
  }

  /**
   * Flush the pending event and notify all the currently registered watchers
   * about all the available, updated and destroyed events that have been accumulated in
   * `_watchers`'s `pendingEvents` arrays.
   */
  _notifyWatchers() {
    for (const watcherEntry of this._watchers) {
      const {
        onAvailable,
        onUpdated,
        onDestroyed,
        pendingEvents,
      } = watcherEntry;
      // Immediately clear the buffer in order to avoid possible races, where an event listener
      // would end up somehow adding a new throttled resource
      watcherEntry.pendingEvents = [];

      for (const { callbackType, updates } of pendingEvents) {
        try {
          if (callbackType == "available") {
            onAvailable(updates, { areExistingResources: false });
          } else if (callbackType == "updated" && onUpdated) {
            onUpdated(updates);
          } else if (callbackType == "destroyed" && onDestroyed) {
            onDestroyed(updates);
          }
        } catch (e) {
          console.error(
            "Exception while calling a ResourceCommand",
            callbackType,
            "callback",
            ":",
            e
          );
        }
      }
    }
  }

  // Compute the target front if the resource comes from the Watcher Actor.
  // (`targetFront` will be null as the watcher is in the parent process
  // and targets are in distinct processes)
  _getTargetForWatcherResource(resource) {
    const { browsingContextID, innerWindowId, resourceType } = resource;

    // Some privileged resources aren't related to any BrowsingContext
    // and so aren't bound to any Target Front.
    // Server watchers should pass an explicit "-1" value in order to prevent
    // silently ignoring an undefined browsingContextID attribute.
    if (browsingContextID == -1) {
      return null;
    }

    if (
      innerWindowId &&
      this.targetCommand.descriptorFront.isServerTargetSwitchingEnabled()
    ) {
      return this.watcherFront.getWindowGlobalTargetByInnerWindowId(
        innerWindowId
      );
    } else if (browsingContextID) {
      return this.watcherFront.getWindowGlobalTarget(browsingContextID);
    }
    console.error(
      `Resource of ${resourceType} is missing a browsingContextID or innerWindowId attribute`
    );
    return null;
  }

  _onWillNavigate(targetFront) {
    // Special case for toolboxes debugging a document,
    // purge the cache entirely when we start navigating to a new document.
    // Other toolboxes and additional target for remote iframes or content process
    // will be purge from onTargetDestroyed.
    this._cache = [];
  }

  /**
   * Tells if the server supports listening to the given resource type
   * via the watcher actor's watchResources method.
   *
   * @return {Boolean} True, if the server supports this type.
   */
  hasResourceCommandSupport(resourceType) {
    // If we're in the browser console or browser toolbox and the browser
    // toolbox fission pref is disabled, we don't want to use watchers
    // (even if traits on the server are enabled).
    if (
      this.targetCommand.descriptorFront.isBrowserProcessDescriptor &&
      !Services.prefs.getBoolPref(BROWSERTOOLBOX_FISSION_ENABLED, false)
    ) {
      return false;
    }

    return this.watcherFront?.traits?.resources?.[resourceType];
  }

  /**
   * Tells if the server supports listening to the given resource type
   * via the watcher actor's watchResources method, and that, for a specific
   * target.
   *
   * @return {Boolean} True, if the server supports this type.
   */
  _hasResourceCommandSupportForTarget(resourceType, targetFront) {
    // First check if the watcher supports this target type.
    // If it doesn't, no resource type can be listened via the Watcher actor for this target.
    if (!this.targetCommand.hasTargetWatcherSupport(targetFront.targetType)) {
      return false;
    }

    return this.hasResourceCommandSupport(resourceType);
  }

  _isValidResourceType(type) {
    return this.ALL_TYPES.includes(type);
  }

  /**
   * Start listening for a given type of resource.
   * For backward compatibility code, we register the legacy listeners on
   * each individual target
   *
   * @param {String} resourceType
   *        One string of ResourceCommand.TYPES, which designates the types of resources
   *        to be listened.
   * @param {Object}
   *        - {Boolean} bypassListenerCount
   *          Pass true to avoid checking/updating the listenersCount map.
   *          Exclusively used when target switching, to stop & start listening
   *          to all resources.
   */
  async _startListening(resourceType, { bypassListenerCount = false } = {}) {
    if (!bypassListenerCount) {
      if (this._listenedResources.has(resourceType)) {
        return;
      }
      this._listenedResources.add(resourceType);
    }

    this._processingExistingResources.add(resourceType);

    // Ensuring enabling listening to targets.
    // This will be a no-op expect for the very first call to `_startListening`,
    // where it is going to call `onTargetAvailable` for all already existing targets,
    // as well as for those who will be created later.
    //
    // Do this *before* calling WatcherActor.watchResources in order to register "resource-available"
    // listeners on targets before these events start being emitted.
    await this._watchAllTargets(resourceType);

    // When we are calling _startListening for the first time, _watchAllTargets
    // will register legacylistener when it will call onTargetAvailable for all existing targets.
    // But for any next calls to _startListening, _watchAllTargets will be a no-op,
    // and nothing will start legacy listener for each already registered targets.
    await this._startLegacyListenersForExistingTargets(resourceType);

    // If the server supports the Watcher API and the Watcher supports
    // this resource type, use this API
    if (this.hasResourceCommandSupport(resourceType)) {
      await this.watcherFront.watchResources([resourceType]);
    }
    this._processingExistingResources.delete(resourceType);
  }

  /**
   * Return true if the resource should be watched via legacy listener,
   * even when watcher supports this resource type.
   *
   * Bug 1678385: In order to support watching for JS Source resource
   * for service workers and parent process workers, which aren't supported yet
   * by the watcher actor, we do not bail out here and allow to execute
   * the legacy listener for these targets.
   * Once bug 1608848 is fixed, we can remove this and never trigger
   * the legacy listeners codepath for these resource types.
   *
   * If this isn't fixed soon, we may add other resources we want to see
   * being fetched from these targets.
   */
  _shouldRunLegacyListenerEvenWithWatcherSupport(resourceType) {
    return (
      resourceType == ResourceCommand.TYPES.SOURCE ||
      resourceType == ResourceCommand.TYPES.THREAD_STATE
    );
  }

  async _forwardExistingResources(resourceTypes, onAvailable) {
    const existingResources = this._cache.filter(resource =>
      resourceTypes.includes(resource.resourceType)
    );
    if (existingResources.length > 0) {
      await onAvailable(existingResources, { areExistingResources: true });
    }
  }

  /**
   * Call backward compatibility code from `LegacyListeners` in order to listen for a given
   * type of resource from a given target.
   */
  async _watchResourcesForTarget({
    targetFront,
    resourceType,
    disableWarning = false,
  }) {
    if (this._hasResourceCommandSupportForTarget(resourceType, targetFront)) {
      // This resource / target pair should already be handled by the watcher,
      // no need to start legacy listeners.
      return;
    }

    if (targetFront.isDestroyed()) {
      return;
    }

    const onAvailable = this._onResourceAvailable.bind(this, { targetFront });
    const onUpdated = this._onResourceUpdated.bind(this, { targetFront });
    const onDestroyed = this._onResourceDestroyed.bind(this, { targetFront });

    if (!(resourceType in LegacyListeners)) {
      throw new Error(`Missing legacy listener for ${resourceType}`);
    }

    const legacyListeners =
      this._existingLegacyListeners.get(targetFront) || [];
    if (legacyListeners.includes(resourceType)) {
      if (!disableWarning) {
        console.warn(
          `Already started legacy listener for ${resourceType} on ${targetFront.actorID}`
        );
      }
      return;
    }
    this._existingLegacyListeners.set(
      targetFront,
      legacyListeners.concat(resourceType)
    );

    try {
      await LegacyListeners[resourceType]({
        targetCommand: this.targetCommand,
        targetFront,
        onAvailable,
        onDestroyed,
        onUpdated,
      });
    } catch (e) {
      // Swallow the error to avoid breaking calls to watchResources which will
      // loop on all existing targets to create legacy listeners.
      // If a legacy listener fails to handle a target for some reason, we
      // should still try to process other targets as much as possible.
      // See Bug 1687645.
      console.error(
        `Failed to start [${resourceType}] legacy listener for target ${targetFront.actorID}`,
        e
      );
    }
  }

  /**
   * Reverse of _startListening. Stop listening for a given type of resource.
   * For backward compatibility, we unregister from each individual target.
   *
   * See _startListening for parameters description.
   */
  _stopListening(resourceType, { bypassListenerCount = false } = {}) {
    if (!bypassListenerCount) {
      if (!this._listenedResources.has(resourceType)) {
        throw new Error(
          `Stopped listening for resource '${resourceType}' that isn't being listened to`
        );
      }
      this._listenedResources.delete(resourceType);
    }

    // Clear the cached resources of the type.
    this._cache = this._cache.filter(
      cachedResource => cachedResource.resourceType !== resourceType
    );

    // If the server supports the Watcher API and the Watcher supports
    // this resource type, use this API
    if (this.hasResourceCommandSupport(resourceType)) {
      if (!this.watcherFront.isDestroyed()) {
        this.watcherFront.unwatchResources([resourceType]);
      }

      const shouldRunLegacyListeners = this._shouldRunLegacyListenerEvenWithWatcherSupport(
        resourceType
      );
      if (!shouldRunLegacyListeners) {
        return;
      }
    }
    // Otherwise, fallback on backward compat mode and use LegacyListeners.

    // If this was the last listener, we should stop watching these events from the actors
    // and the actors should stop watching things from the platform
    const targets = this.targetCommand.getAllTargets(
      this.targetCommand.ALL_TYPES
    );
    for (const target of targets) {
      this._unwatchResourcesForTarget(target, resourceType);
    }
  }

  /**
   * Backward compatibility code, reverse of _watchResourcesForTarget.
   */
  _unwatchResourcesForTarget(targetFront, resourceType) {
    if (this._hasResourceCommandSupportForTarget(resourceType, targetFront)) {
      // This resource / target pair should already be handled by the watcher,
      // no need to stop legacy listeners.
    }
    // Is there really a point in:
    // - unregistering `onAvailable` RDP event callbacks from target-scoped actors?
    // - calling `stopListeners()` as we are most likely closing the toolbox and destroying everything?
    //
    // It is important to keep this method synchronous and do as less as possible
    // in the case of toolbox destroy.
    //
    // We are aware of one case where that might be useful.
    // When a panel is disabled via the options panel, after it has been opened.
    // Would that justify doing this? Is there another usecase?

    // XXX: This is most likely only needed to avoid growing the Map infinitely.
    // Unless in the "disabled panel" use case mentioned in the comment above,
    // we should not see the same target actorID again.
    const listeners = this._existingLegacyListeners.get(targetFront);
    if (listeners && listeners.includes(resourceType)) {
      const remainingListeners = listeners.filter(l => l !== resourceType);
      this._existingLegacyListeners.set(targetFront, remainingListeners);
    }
  }
}

ResourceCommand.TYPES = ResourceCommand.prototype.TYPES = {
  CONSOLE_MESSAGE: "console-message",
  CSS_CHANGE: "css-change",
  CSS_MESSAGE: "css-message",
  ERROR_MESSAGE: "error-message",
  PLATFORM_MESSAGE: "platform-message",
  // Legacy listener only. Can be removed in Bug 1625937.
  CLONED_CONTENT_PROCESS_MESSAGE: "cloned-content-process-message",
  DOCUMENT_EVENT: "document-event",
  ROOT_NODE: "root-node",
  STYLESHEET: "stylesheet",
  NETWORK_EVENT: "network-event",
  WEBSOCKET: "websocket",
  COOKIE: "cookies",
  LOCAL_STORAGE: "local-storage",
  SESSION_STORAGE: "session-storage",
  CACHE_STORAGE: "Cache",
  EXTENSION_STORAGE: "extension-storage",
  INDEXED_DB: "indexed-db",
  NETWORK_EVENT_STACKTRACE: "network-event-stacktrace",
  REFLOW: "reflow",
  SOURCE: "source",
  THREAD_STATE: "thread-state",
  SERVER_SENT_EVENT: "server-sent-event",
};
ResourceCommand.ALL_TYPES = ResourceCommand.prototype.ALL_TYPES = Object.values(
  ResourceCommand.TYPES
);
module.exports = ResourceCommand;

// Backward compat code for each type of resource.
// Each section added here should eventually be removed once the equivalent server
// code is implement in Firefox, in its release channel.
const LegacyListeners = {
  [ResourceCommand.TYPES
    .CONSOLE_MESSAGE]: require("devtools/shared/commands/resource/legacy-listeners/console-messages"),
  [ResourceCommand.TYPES
    .CSS_CHANGE]: require("devtools/shared/commands/resource/legacy-listeners/css-changes"),
  [ResourceCommand.TYPES
    .CSS_MESSAGE]: require("devtools/shared/commands/resource/legacy-listeners/css-messages"),
  [ResourceCommand.TYPES
    .ERROR_MESSAGE]: require("devtools/shared/commands/resource/legacy-listeners/error-messages"),
  [ResourceCommand.TYPES
    .PLATFORM_MESSAGE]: require("devtools/shared/commands/resource/legacy-listeners/platform-messages"),
  [ResourceCommand.TYPES
    .CLONED_CONTENT_PROCESS_MESSAGE]: require("devtools/shared/commands/resource/legacy-listeners/cloned-content-process-messages"),
  async [ResourceCommand.TYPES.DOCUMENT_EVENT]({
    targetCommand,
    targetFront,
    onAvailable,
  }) {
    // DocumentEventsListener of webconsole handles only top level document.
    if (!targetFront.isTopLevel) {
      return;
    }

    const webConsoleFront = await targetFront.getFront("console");
    webConsoleFront.on("documentEvent", event => {
      event.resourceType = ResourceCommand.TYPES.DOCUMENT_EVENT;
      onAvailable([event]);
    });
    await webConsoleFront.startListeners(["DocumentEvents"]);
  },
  [ResourceCommand.TYPES
    .ROOT_NODE]: require("devtools/shared/commands/resource/legacy-listeners/root-node"),
  [ResourceCommand.TYPES
    .STYLESHEET]: require("devtools/shared/commands/resource/legacy-listeners/stylesheet"),
  [ResourceCommand.TYPES
    .NETWORK_EVENT]: require("devtools/shared/commands/resource/legacy-listeners/network-events"),
  [ResourceCommand.TYPES
    .WEBSOCKET]: require("devtools/shared/commands/resource/legacy-listeners/websocket"),
  [ResourceCommand.TYPES
    .COOKIE]: require("devtools/shared/commands/resource/legacy-listeners/cookie"),
  [ResourceCommand.TYPES
    .CACHE_STORAGE]: require("devtools/shared/commands/resource/legacy-listeners/cache-storage"),
  [ResourceCommand.TYPES
    .LOCAL_STORAGE]: require("devtools/shared/commands/resource/legacy-listeners/local-storage"),
  [ResourceCommand.TYPES
    .SESSION_STORAGE]: require("devtools/shared/commands/resource/legacy-listeners/session-storage"),
  [ResourceCommand.TYPES
    .EXTENSION_STORAGE]: require("devtools/shared/commands/resource/legacy-listeners/extension-storage"),
  [ResourceCommand.TYPES
    .INDEXED_DB]: require("devtools/shared/commands/resource/legacy-listeners/indexed-db"),
  [ResourceCommand.TYPES
    .NETWORK_EVENT_STACKTRACE]: require("devtools/shared/commands/resource/legacy-listeners/network-event-stacktraces"),
  [ResourceCommand.TYPES
    .SOURCE]: require("devtools/shared/commands/resource/legacy-listeners/source"),
  [ResourceCommand.TYPES
    .THREAD_STATE]: require("devtools/shared/commands/resource/legacy-listeners/thread-states"),
  [ResourceCommand.TYPES
    .SERVER_SENT_EVENT]: require("devtools/shared/commands/resource/legacy-listeners/server-sent-events"),
  [ResourceCommand.TYPES
    .REFLOW]: require("devtools/shared/commands/resource/legacy-listeners/reflow"),
};

// Optional transformers for each type of resource.
// Each module added here should be a function that will receive the resource, the target, …
// and perform some transformation on the resource before it will be emitted.
// This is a good place to handle backward compatibility and manual resource marshalling.
const ResourceTransformers = {
  [ResourceCommand.TYPES
    .CONSOLE_MESSAGE]: require("devtools/shared/commands/resource/transformers/console-messages"),
  [ResourceCommand.TYPES
    .ERROR_MESSAGE]: require("devtools/shared/commands/resource/transformers/error-messages"),
  [ResourceCommand.TYPES
    .CACHE_STORAGE]: require("devtools/shared/commands/resource/transformers/storage-cache.js"),
  [ResourceCommand.TYPES
    .COOKIE]: require("devtools/shared/commands/resource/transformers/storage-cookie.js"),
  [ResourceCommand.TYPES
    .INDEXED_DB]: require("devtools/shared/commands/resource/transformers/storage-indexed-db.js"),
  [ResourceCommand.TYPES
    .LOCAL_STORAGE]: require("devtools/shared/commands/resource/transformers/storage-local-storage.js"),
  [ResourceCommand.TYPES
    .SESSION_STORAGE]: require("devtools/shared/commands/resource/transformers/storage-session-storage.js"),
  [ResourceCommand.TYPES
    .NETWORK_EVENT]: require("devtools/shared/commands/resource/transformers/network-events"),
  [ResourceCommand.TYPES
    .THREAD_STATE]: require("devtools/shared/commands/resource/transformers/thread-states"),
};
