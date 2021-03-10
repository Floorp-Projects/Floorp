/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { throttle } = require("devtools/shared/throttle");

class ResourceWatcher {
  /**
   * This class helps retrieving existing and listening to resources.
   * A resource is something that:
   *  - the target you are debugging exposes
   *  - can be created as early as the process/worker/page starts loading
   *  - can already exist, or will be created later on
   *  - doesn't require any user data to be fetched, only a type/category
   *
   * @param {TargetList} targetList
   *        A TargetList instance, which helps communicating to the backend
   *        in order to iterate and listen over the requested resources.
   */

  constructor(targetList) {
    this.targetList = targetList;

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
    this._listenerCount = new Map();

    // WeakMap used to avoid starting a legacy listener twice for the same
    // target + resource-type pair. Legacy listener creation can be subject to
    // race conditions.
    // Maps a target front to an array of resource types.
    this._existingLegacyListeners = new WeakMap();
    this._processingExistingResources = new Set();

    this._notifyWatchers = this._notifyWatchers.bind(this);
    this._throttledNotifyWatchers = throttle(this._notifyWatchers, 100);
  }

  get watcherFront() {
    return this.targetList.watcherFront;
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
   *                                  Function which will be called once per existing
   *                                  resource and each time a resource is created.
   *        - {Function} onUpdated:   This attribute is optional.
   *                                  Function which will be called each time a resource,
   *                                  previously notified via onAvailable is updated.
   *        - {Function} onDestroyed: This attribute is optional.
   *                                  Function which will be called each time a resource in
   *                                  the remote target is destroyed.
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
        "ResourceWatcher.watchResources expects an onAvailable function as argument"
      );
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

    // First ensuring enabling listening to targets.
    // This will call onTargetAvailable for all already existing targets,
    // as well as for the one created later.
    // Do this *before* calling _startListening in order to register
    // "resource-available" listener before requesting for the resources in _startListening.
    await this._watchAllTargets();

    for (const resource of resources) {
      // If we are registering the first listener, so start listening from the server about
      // this one resource.
      if (!this._hasListenerForResource(resource)) {
        await this._startListening(resource);
      }
    }

    // The resource cache is immediately filled when receiving the sources, but they are
    // emitted with a delay due to throttling. Since the cache can contain resources that
    // will soon be emitted, we have to flush it before adding the new listeners.
    // Otherwise forwardCacheResources might emit resources that will also be emitted by
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
    // for already existing resources, which will optionally be notified via _forwardCachedResources
    this._watchers.push({
      resources: watchedResources,
      onAvailable,
      onUpdated,
      onDestroyed,
      pendingEvents: [],
    });

    if (!ignoreExistingResources) {
      await this._forwardCachedResources(watchedResources, onAvailable);
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
        "ResourceWatcher.unwatchResources expects an onAvailable function as argument"
      );
    }

    const watchedResources = [];
    for (const resource of resources) {
      if (this._hasListenerForResource(resource)) {
        watchedResources.push(resource);
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

    // Stop listening to all resources that no longer have any watcher callback
    for (const resource of watchedResources) {
      if (!this._hasListenerForResource(resource)) {
        this._stopListening(resource);
      }
    }

    // Stop watching for targets if we removed the last listener.
    let listeners = 0;
    for (const count of this._listenerCount.values()) {
      listeners += count;
    }
    if (listeners <= 0) {
      this._unwatchAllTargets();
    }
  }

  /**
   * Start watching for all already existing and future targets.
   *
   * We are using ALL_TYPES, but this won't force listening to all types.
   * It will only listen for types which are defined by `TargetList.startListening`.
   */
  async _watchAllTargets() {
    if (!this._watchTargetsPromise) {
      this._watchTargetsPromise = this.targetList.watchTargets(
        this.targetList.ALL_TYPES,
        this._onTargetAvailable,
        this._onTargetDestroyed
      );
    }
    return this._watchTargetsPromise;
  }

  _unwatchAllTargets() {
    if (!this._watchTargetsPromise) {
      return;
    }
    this._watchTargetsPromise = null;
    this.targetList.unwatchTargets(
      this.targetList.ALL_TYPES,
      this._onTargetAvailable,
      this._onTargetDestroyed
    );
  }

  /**
   * Method called by the TargetList for each already existing or target which has just been created.
   *
   * @param {Front} targetFront
   *        The Front of the target that is available.
   *        This Front inherits from TargetMixin and is typically
   *        composed of a BrowsingContextTargetFront or ContentProcessTargetFront.
   */
  async _onTargetAvailable({ targetFront, isTargetSwitching }) {
    const resources = [];
    if (isTargetSwitching) {
      this._onWillNavigate(targetFront);
      // WatcherActor currently only watches additional frame targets and
      // explicitely ignores top level one that may be created when navigating
      // to a new process.
      // In order to keep working resources that are being watched via the
      // Watcher actor, we have to unregister and re-register the resource
      // types. This will force calling `Resources.watchResources` on the new top
      // level target.
      for (const resourceType of Object.values(ResourceWatcher.TYPES)) {
        // ...which has at least one listener...
        if (!this._listenerCount.get(resourceType)) {
          continue;
        }
        await this._stopListening(resourceType, { bypassListenerCount: true });
        resources.push(resourceType);
      }
    }

    if (targetFront.isDestroyed()) {
      return;
    }

    targetFront.on("will-navigate", () => this._onWillNavigate(targetFront));

    // If we are target switching, we already stop & start listening to all the
    // currently monitored resources.
    if (!isTargetSwitching) {
      // For each resource type...
      for (const resourceType of Object.values(ResourceWatcher.TYPES)) {
        // ...which has at least one listener...
        if (!this._listenerCount.get(resourceType)) {
          continue;
        }
        // ...request existing resource and new one to come from this one target
        // *but* only do that for backward compat, where we don't have the watcher API
        // (See bug 1626647)
        await this._watchResourcesForTarget(targetFront, resourceType);
      }
    }

    // Compared to the TargetList and Watcher.watchTargets,
    // We do call Watcher.watchResources, but the events are fired on the target.
    // That's because the Watcher runs in the parent process/main thread, while resources
    // are available from the target's process/thread.
    targetFront.on(
      "resource-available-form",
      this._onResourceAvailable.bind(this, { targetFront })
    );
    targetFront.on(
      "resource-updated-form",
      this._onResourceUpdated.bind(this, { targetFront })
    );
    targetFront.on(
      "resource-destroyed-form",
      this._onResourceDestroyed.bind(this, { targetFront })
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
  }

  /**
   * Method called by the TargetList when a target has just been destroyed
   * See _onTargetAvailable for arguments, they are the same.
   */
  _onTargetDestroyed({ targetFront }) {
    // Clear the map of legacy listeners for this target.
    this._existingLegacyListeners.set(targetFront, []);

    // Remove pending events associated with this target
    for (const watcherEntry of this._watchers) {
      watcherEntry.pendingEvents = watcherEntry.pendingEvents.filter(
        ({ callbackType, updates }) => {
          updates = updates.filter(update => {
            if (callbackType !== "available" && callbackType !== "updated") {
              return true;
            }

            const resource =
              callbackType == "available" ? update : update.resource;
            return resource.targetFront !== targetFront;
          });

          // Filter out pendingEvents who don't have any updates
          return updates.length > 0;
        }
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
    for (let resource of resources) {
      const { resourceType } = resource;

      if (watcherFront) {
        targetFront = await this._getTargetForWatcherResource(resource);
      }

      if (targetFront && targetFront.isDestroyedOrBeingDestroyed()) {
        continue;
      }

      // isAlreadyExistingResource indicates that the resources already existed before
      // the resource watcher started watching for this type of resource.
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
          targetList: this.targetList,
          targetFront,
          watcherFront: this.watcherFront,
        });
      }

      this._queueResourceEvent("available", resourceType, resource);

      this._cache.push(resource);
    }
    this._throttledNotifyWatchers();
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

      if (
        !existingResource ||
        (existingResource.targetFront &&
          existingResource.targetFront.isDestroyedOrBeingDestroyed())
      ) {
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

  /**
   * Check if there is at least one listener registered for the given resource type.
   *
   * @param {String} resourceType
   *        Watched resource type
   */
  _hasListenerForResource(resourceType) {
    return this._watchers.some(({ resources }) => {
      return resources.includes(resourceType);
    });
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
            onAvailable(updates);
          } else if (callbackType == "updated" && onUpdated) {
            onUpdated(updates);
          } else if (callbackType == "destroyed" && onDestroyed) {
            onDestroyed(updates);
          }
        } catch (e) {
          console.error(
            "Exception while calling a ResourceWatcher",
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
    const { browsingContextID, resourceType } = resource;

    // Some privileged resources aren't related to any BrowsingContext
    // and so aren't bound to any Target Front.
    // Server watchers should pass an explicit "-1" value in order to prevent
    // silently ignoring an undefined browsingContextID attribute.
    if (browsingContextID == -1) {
      return null;
    }

    // Resource emitted from the Watcher Actor should all have a
    // browsingContextID attribute
    if (!browsingContextID) {
      console.error(
        `Resource of ${resourceType} is missing a browsingContextID attribute`
      );
      return null;
    }
    return this.watcherFront.getBrowsingContextTarget(browsingContextID);
  }

  _onWillNavigate(targetFront) {
    if (targetFront.isTopLevel) {
      this._cache = [];
      return;
    }

    this._cache = this._cache.filter(
      cachedResource => cachedResource.targetFront !== targetFront
    );
  }

  /**
   * Tells if the server supports listening to the given resource type
   * via the watcher actor's watchResources method.
   *
   * @return {Boolean} True, if the server supports this type.
   */
  hasResourceWatcherSupport(resourceType) {
    return this.watcherFront?.traits?.resources?.[resourceType];
  }

  /**
   * Tells if the server supports listening to the given resource type
   * via the watcher actor's watchResources method, and that, for a specific
   * target.
   *
   * @return {Boolean} True, if the server supports this type.
   */
  _hasResourceWatcherSupportForTarget(resourceType, targetFront) {
    // First check if the watcher supports this target type.
    // If it doesn't, no resource type can be listened via the Watcher actor for this target.
    if (!this.targetList.hasTargetWatcherSupport(targetFront.targetType)) {
      return false;
    }

    return this.hasResourceWatcherSupport(resourceType);
  }

  /**
   * Start listening for a given type of resource.
   * For backward compatibility code, we register the legacy listeners on
   * each individual target
   *
   * @param {String} resourceType
   *        One string of ResourceWatcher.TYPES, which designates the types of resources
   *        to be listened.
   * @param {Object}
   *        - {Boolean} bypassListenerCount
   *          Pass true to avoid checking/updating the listenersCount map.
   *          Exclusively used when target switching, to stop & start listening
   *          to all resources.
   */
  async _startListening(resourceType, { bypassListenerCount = false } = {}) {
    if (!bypassListenerCount) {
      let listeners = this._listenerCount.get(resourceType) || 0;
      listeners++;
      this._listenerCount.set(resourceType, listeners);

      if (listeners > 1) {
        return;
      }
    }

    this._processingExistingResources.add(resourceType);

    // If the server supports the Watcher API and the Watcher supports
    // this resource type, use this API
    if (this.hasResourceWatcherSupport(resourceType)) {
      await this.watcherFront.watchResources([resourceType]);

      const shouldRunLegacyListeners = this._shouldRunLegacyListenerEvenWithWatcherSupport(
        resourceType
      );
      if (!shouldRunLegacyListeners) {
        this._processingExistingResources.delete(resourceType);
        return;
      }
    }
    // Otherwise, fallback on backward compat mode and use LegacyListeners.

    // If this is the first listener for this type of resource,
    // we should go through all the existing targets as onTargetAvailable
    // has already been called for these existing targets.
    const promises = [];
    const targets = this.targetList.getAllTargets(this.targetList.ALL_TYPES);
    for (const target of targets) {
      promises.push(this._watchResourcesForTarget(target, resourceType));
    }
    await Promise.all(promises);
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
      resourceType == ResourceWatcher.TYPES.SOURCE ||
      resourceType == ResourceWatcher.TYPES.THREAD_STATE
    );
  }

  async _forwardCachedResources(resourceTypes, onAvailable) {
    const cachedResources = this._cache.filter(resource =>
      resourceTypes.includes(resource.resourceType)
    );
    if (cachedResources.length > 0) {
      await onAvailable(cachedResources);
    }
  }

  /**
   * Call backward compatibility code from `LegacyListeners` in order to listen for a given
   * type of resource from a given target.
   */
  async _watchResourcesForTarget(targetFront, resourceType) {
    if (this._hasResourceWatcherSupportForTarget(resourceType, targetFront)) {
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
      console.warn(
        `Already started legacy listener for ${resourceType} on ${targetFront.actorID}`
      );
      return;
    }
    this._existingLegacyListeners.set(
      targetFront,
      legacyListeners.concat(resourceType)
    );

    try {
      await LegacyListeners[resourceType]({
        targetList: this.targetList,
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
      let listeners = this._listenerCount.get(resourceType);
      if (!listeners || listeners <= 0) {
        throw new Error(
          `Stopped listening for resource '${resourceType}' that isn't being listened to`
        );
      }
      listeners--;
      this._listenerCount.set(resourceType, listeners);
      if (listeners > 0) {
        return;
      }
    }

    // Clear the cached resources of the type.
    this._cache = this._cache.filter(
      cachedResource => cachedResource.resourceType !== resourceType
    );

    // If the server supports the Watcher API and the Watcher supports
    // this resource type, use this API
    if (this.hasResourceWatcherSupport(resourceType)) {
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
    const targets = this.targetList.getAllTargets(this.targetList.ALL_TYPES);
    for (const target of targets) {
      this._unwatchResourcesForTarget(target, resourceType);
    }
  }

  /**
   * Backward compatibility code, reverse of _watchResourcesForTarget.
   */
  _unwatchResourcesForTarget(targetFront, resourceType) {
    if (this._hasResourceWatcherSupportForTarget(resourceType, targetFront)) {
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

ResourceWatcher.TYPES = ResourceWatcher.prototype.TYPES = {
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
  COOKIE: "cookie",
  LOCAL_STORAGE: "local-storage",
  SESSION_STORAGE: "session-storage",
  CACHE_STORAGE: "Cache",
  EXTENSION_STORAGE: "extension-storage",
  INDEXED_DB: "indexed-db",
  NETWORK_EVENT_STACKTRACE: "network-event-stacktrace",
  SOURCE: "source",
  THREAD_STATE: "thread-state",
  SERVER_SENT_EVENT: "server-sent-event",
};
module.exports = { ResourceWatcher, TYPES: ResourceWatcher.TYPES };

// Backward compat code for each type of resource.
// Each section added here should eventually be removed once the equivalent server
// code is implement in Firefox, in its release channel.
const LegacyListeners = {
  [ResourceWatcher.TYPES
    .CONSOLE_MESSAGE]: require("devtools/shared/resources/legacy-listeners/console-messages"),
  [ResourceWatcher.TYPES
    .CSS_CHANGE]: require("devtools/shared/resources/legacy-listeners/css-changes"),
  [ResourceWatcher.TYPES
    .CSS_MESSAGE]: require("devtools/shared/resources/legacy-listeners/css-messages"),
  [ResourceWatcher.TYPES
    .ERROR_MESSAGE]: require("devtools/shared/resources/legacy-listeners/error-messages"),
  [ResourceWatcher.TYPES
    .PLATFORM_MESSAGE]: require("devtools/shared/resources/legacy-listeners/platform-messages"),
  [ResourceWatcher.TYPES
    .CLONED_CONTENT_PROCESS_MESSAGE]: require("devtools/shared/resources/legacy-listeners/cloned-content-process-messages"),
  async [ResourceWatcher.TYPES.DOCUMENT_EVENT]({
    targetList,
    targetFront,
    onAvailable,
  }) {
    // DocumentEventsListener of webconsole handles only top level document.
    if (!targetFront.isTopLevel) {
      return;
    }

    const webConsoleFront = await targetFront.getFront("console");
    webConsoleFront.on("documentEvent", event => {
      event.resourceType = ResourceWatcher.TYPES.DOCUMENT_EVENT;
      onAvailable([event]);
    });
    await webConsoleFront.startListeners(["DocumentEvents"]);
  },
  [ResourceWatcher.TYPES
    .ROOT_NODE]: require("devtools/shared/resources/legacy-listeners/root-node"),
  [ResourceWatcher.TYPES
    .STYLESHEET]: require("devtools/shared/resources/legacy-listeners/stylesheet"),
  [ResourceWatcher.TYPES
    .NETWORK_EVENT]: require("devtools/shared/resources/legacy-listeners/network-events"),
  [ResourceWatcher.TYPES
    .WEBSOCKET]: require("devtools/shared/resources/legacy-listeners/websocket"),
  [ResourceWatcher.TYPES
    .COOKIE]: require("devtools/shared/resources/legacy-listeners/cookie"),
  [ResourceWatcher.TYPES
    .CACHE_STORAGE]: require("devtools/shared/resources/legacy-listeners/cache-storage"),
  [ResourceWatcher.TYPES
    .LOCAL_STORAGE]: require("devtools/shared/resources/legacy-listeners/local-storage"),
  [ResourceWatcher.TYPES
    .SESSION_STORAGE]: require("devtools/shared/resources/legacy-listeners/session-storage"),
  [ResourceWatcher.TYPES
    .EXTENSION_STORAGE]: require("devtools/shared/resources/legacy-listeners/extension-storage"),
  [ResourceWatcher.TYPES
    .INDEXED_DB]: require("devtools/shared/resources/legacy-listeners/indexed-db"),
  [ResourceWatcher.TYPES
    .NETWORK_EVENT_STACKTRACE]: require("devtools/shared/resources/legacy-listeners/network-event-stacktraces"),
  [ResourceWatcher.TYPES
    .SOURCE]: require("devtools/shared/resources/legacy-listeners/source"),
  [ResourceWatcher.TYPES
    .THREAD_STATE]: require("devtools/shared/resources/legacy-listeners/thread-states"),
  [ResourceWatcher.TYPES
    .SERVER_SENT_EVENT]: require("devtools/shared/resources/legacy-listeners/server-sent-events"),
};

// Optional transformers for each type of resource.
// Each module added here should be a function that will receive the resource, the target, …
// and perform some transformation on the resource before it will be emitted.
// This is a good place to handle backward compatibility and manual resource marshalling.
const ResourceTransformers = {
  [ResourceWatcher.TYPES
    .CONSOLE_MESSAGE]: require("devtools/shared/resources/transformers/console-messages"),
  [ResourceWatcher.TYPES
    .ERROR_MESSAGE]: require("devtools/shared/resources/transformers/error-messages"),
  [ResourceWatcher.TYPES
    .CACHE_STORAGE]: require("devtools/shared/resources/transformers/storage-cache.js"),
  [ResourceWatcher.TYPES
    .LOCAL_STORAGE]: require("devtools/shared/resources/transformers/storage-local-storage.js"),
  [ResourceWatcher.TYPES
    .SESSION_STORAGE]: require("devtools/shared/resources/transformers/storage-session-storage.js"),
  [ResourceWatcher.TYPES
    .NETWORK_EVENT]: require("devtools/shared/resources/transformers/network-events"),
  [ResourceWatcher.TYPES
    .THREAD_STATE]: require("devtools/shared/resources/transformers/thread-states"),
};
