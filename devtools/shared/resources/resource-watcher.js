/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");

// eslint-disable-next-line mozilla/reject-some-requires
const { gDevTools } = require("devtools/client/framework/devtools");

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
    this.descriptorFront = targetList.descriptorFront;

    this._onTargetAvailable = this._onTargetAvailable.bind(this);
    this._onTargetDestroyed = this._onTargetDestroyed.bind(this);

    this._onResourceAvailable = this._onResourceAvailable.bind(this);
    this._onResourceDestroyed = this._onResourceDestroyed.bind(this);

    this._availableListeners = new EventEmitter();
    this._updatedListeners = new EventEmitter();
    this._destroyedListeners = new EventEmitter();

    // Cache for all resources by the order that the resource was taken.
    this._cache = [];
    this._listenerCount = new Map();
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
   * Request to start retrieving all already existing instances of given
   * type of resources and also start watching for the one to be created after.
   *
   * @param {Array:string} resources
   *        List of all resources which should be fetched and observed.
   * @param {Object} options
   *        - {Function} onAvailable: This attribute is mandatory.
   *                                  Function which will be called once per existing
   *                                  resource and each time a resource is created.
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

    // Cache the Watcher once for all, the first time we call `watch()`.
    // This `watcher` attribute may be then used in any function of the ResourceWatcher after this.
    if (!this.watcher) {
      const supportsWatcher = this.descriptorFront?.traits?.watcher;
      if (supportsWatcher) {
        this.watcher = await this.descriptorFront.getWatcher();
        // Resources watched from the parent process will be emitted on the Watcher Actor.
        // So that we also have to listen for this event on it, in addition to all targets.
        this.watcher.on(
          "resource-available-form",
          this._onResourceAvailable.bind(this, { watcherFront: this.watcher })
        );
        this.watcher.on(
          "resource-updated-form",
          this._onResourceUpdated.bind(this, { watcherFront: this.watcher })
        );
        this.watcher.on(
          "resource-destroyed-form",
          this._onResourceDestroyed.bind(this, { watcherFront: this.watcher })
        );
      }
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
      if (this._availableListeners.count(resource) === 0) {
        await this._startListening(resource);
      }
      this._availableListeners.on(resource, onAvailable);
      if (onUpdated) {
        this._updatedListeners.on(resource, onUpdated);
      }
      if (onDestroyed) {
        this._destroyedListeners.on(resource, onDestroyed);
      }
    }

    if (!ignoreExistingResources) {
      await this._forwardCachedResources(resources, onAvailable);
    }
  }

  /**
   * Stop watching for given type of resources.
   * See `watchResources` for the arguments as both methods receive the same.
   */
  unwatchResources(resources, options) {
    const { onAvailable, onUpdated, onDestroyed } = options;

    for (const resource of resources) {
      if (onUpdated) {
        this._updatedListeners.off(resource, onUpdated);
      }
      if (onDestroyed) {
        this._destroyedListeners.off(resource, onDestroyed);
      }

      const hadAtLeastOneListener =
        this._availableListeners.count(resource) > 0;
      this._availableListeners.off(resource, onAvailable);
      if (
        hadAtLeastOneListener &&
        this._availableListeners.count(resource) === 0
      ) {
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
    if (this._isWatchingTargets) {
      return;
    }
    this._isWatchingTargets = true;
    await this.targetList.watchTargets(
      this.targetList.ALL_TYPES,
      this._onTargetAvailable,
      this._onTargetDestroyed
    );
  }

  async _unwatchAllTargets() {
    if (!this._isWatchingTargets) {
      return;
    }
    this._isWatchingTargets = false;
    await this.targetList.unwatchTargets(
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
      // types. This will force calling `watchTargetResources` on the new top
      // level target.
      for (const resourceType of this._listenerCount.keys()) {
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
        if (this._hasWatcherSupport(resourceType)) {
          continue;
        }
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
      for (const resourceType of resources) {
        await this._startListening(resourceType, { bypassListenerCount: true });
      }
    }
  }

  /**
   * Method called by the TargetList when a target has just been destroyed
   * See _onTargetAvailable for arguments, they are the same.
   */
  _onTargetDestroyed({ targetFront }) {
    //TODO: Is there a point in doing anything?
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
        if (!targetFront) {
          continue;
        }
      }

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
          isFissionEnabledOnContentToolbox: gDevTools.isFissionContentToolboxEnabled(),
        });
      }

      this._availableListeners.emit(resourceType, {
        // XXX: We may want to read resource.resourceType instead of passing this resourceType argument?
        resourceType,
        targetFront,
        resource,
      });

      this._cache.push(resource);
    }
  }

  /**
   * Method called either by:
   * - the backward compatibility code (LegacyListeners)
   * - target actors RDP events
   * Called everytime a resource is updated in the remote target.
   *
   * See _onResourceAvailable for the argument description.
   */
  async _onResourceUpdated({ targetFront, watcherFront }, resources) {
    for (const resource of resources) {
      const { resourceType, resourceId } = resource;

      if (watcherFront) {
        targetFront = await this._getTargetForWatcherResource(resource);
        if (!targetFront) {
          continue;
        }
      }

      if (resourceId) {
        const index = this._cache.findIndex(
          cachedResource =>
            cachedResource.resourceType == resourceType &&
            cachedResource.resourceId == resourceId
        );
        if (index != -1) {
          this._cache.splice(index, 1, resource);
        }
      }

      this._updatedListeners.emit(resourceType, {
        resourceType,
        targetFront,
        resource,
      });
    }
  }

  /**
   * Called everytime a resource is destroyed in the remote target.
   * See _onResourceAvailable for the argument description.
   */
  async _onResourceDestroyed({ targetFront, watcherFront }, resources) {
    for (const resource of resources) {
      const { resourceType, resourceId } = resource;

      if (watcherFront) {
        targetFront = await this._getTargetForWatcherResource(resource);
        if (!targetFront) {
          continue;
        }
      }

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

      this._destroyedListeners.emit(resourceType, {
        resourceType,
        targetFront,
        resource,
      });
    }
  }

  // Compute the target front if the resource comes from the Watcher Actor.
  // (`targetFront` will be null as the watcher is in the parent process
  // and targets are in distinct processes)
  _getTargetForWatcherResource(resource) {
    const { browsingContextID, resourceType } = resource;

    // Resource emitted from the Watcher Actor should all have a
    // browsingContextID attribute
    if (!browsingContextID) {
      console.error(
        `Resource of ${resourceType} is missing a browsingContextID attribute`
      );
      return null;
    }
    return this.watcher.getBrowsingContextTarget(browsingContextID);
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

  _hasWatcherSupport(resourceType) {
    return this.watcher?.traits?.resources?.[resourceType];
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

    // If the server supports the Watcher API and the Watcher supports
    // this resource type, use this API
    if (this._hasWatcherSupport(resourceType)) {
      await this.watcher.watchResources([resourceType]);
      return;
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
  }

  async _forwardCachedResources(resourceTypes, onAvailable) {
    for (const resource of this._cache) {
      if (resourceTypes.includes(resource.resourceType)) {
        await onAvailable({
          resourceType: resource.resourceType,
          targetFront: resource.targetFront,
          resource,
        });
      }
    }
  }

  /**
   * Call backward compatibility code from `LegacyListeners` in order to listen for a given
   * type of resource from a given target.
   */
  _watchResourcesForTarget(targetFront, resourceType) {
    if (targetFront.isDestroyed()) {
      return Promise.resolve();
    }

    const onAvailable = this._onResourceAvailable.bind(this, { targetFront });
    const onDestroyed = this._onResourceDestroyed.bind(this, { targetFront });
    const onUpdated = this._onResourceUpdated.bind(this, { targetFront });
    return LegacyListeners[resourceType]({
      targetList: this.targetList,
      targetFront,
      isFissionEnabledOnContentToolbox: gDevTools.isFissionContentToolboxEnabled(),
      onAvailable,
      onDestroyed,
      onUpdated,
    });
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
    if (this._hasWatcherSupport(resourceType)) {
      this.watcher.unwatchResources([resourceType]);
      return;
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
  }
}

ResourceWatcher.TYPES = ResourceWatcher.prototype.TYPES = {
  CONSOLE_MESSAGE: "console-message",
  CSS_CHANGE: "css-change",
  CSS_MESSAGE: "css-message",
  ERROR_MESSAGE: "error-message",
  PLATFORM_MESSAGE: "platform-message",
  DOCUMENT_EVENT: "document-event",
  ROOT_NODE: "root-node",
  STYLESHEET: "stylesheet",
  NETWORK_EVENT: "network-event",
  WEBSOCKET: "websocket",
};
module.exports = { ResourceWatcher };

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
    .ROOT_NODE]: require("devtools/shared/resources/transformers/root-node"),
};
