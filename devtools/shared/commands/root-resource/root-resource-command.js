/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { throttle } = require("devtools/shared/throttle");

class RootResourceCommand {
  /**
   * This class helps retrieving existing and listening to "root" resources.
   *
   * This is a fork of ResourceCommand, but specific to context-less
   * resources which can be listened to right away when connecting to the RDP server.
   *
   * The main difference in term of implementation is that:
   * - we receive a root front as constructor argument (instead of `commands` object)
   * - we only listen for RDP events on the Root actor (instead of watcher and target actors)
   * - there is no legacy listener support
   * - there is no resource transformers
   * - there is a lot of logic around targets that is removed here.
   *
   * See ResourceCommand for comments and jsdoc.
   *
   * TODO Bug 1758530 - Investigate sharing code with ResourceCommand instead of forking.
   *
   * @param object commands
   *        The commands object with all interfaces defined from devtools/shared/commands/
   * @param object rootFront
   *        Front for the Root actor.
   */
  constructor({ commands, rootFront }) {
    this.rootFront = rootFront ? rootFront : commands.client.mainRoot;

    this._onResourceAvailable = this._onResourceAvailable.bind(this);
    this._onResourceDestroyed = this._onResourceDestroyed.bind(this);

    this._watchers = [];

    this._pendingWatchers = new Set();

    this._cache = [];
    this._listenedResources = new Set();

    this._processingExistingResources = new Set();

    this._notifyWatchers = this._notifyWatchers.bind(this);
    this._throttledNotifyWatchers = throttle(this._notifyWatchers, 100);
  }

  getAllResources(resourceType) {
    return this._cache.filter(r => r.resourceType === resourceType);
  }

  getResourceById(resourceType, resourceId) {
    return this._cache.find(
      r => r.resourceType === resourceType && r.resourceId === resourceId
    );
  }

  async watchResources(resources, options) {
    const {
      onAvailable,
      onUpdated,
      onDestroyed,
      ignoreExistingResources = false,
    } = options;

    if (typeof onAvailable !== "function") {
      throw new Error(
        "RootResourceCommand.watchResources expects an onAvailable function as argument"
      );
    }

    for (const type of resources) {
      if (!this._isValidResourceType(type)) {
        throw new Error(
          `RootResourceCommand.watchResources invoked with an unknown type: "${type}"`
        );
      }
    }

    const pendingWatcher = {
      resources,
      onAvailable,
    };
    this._pendingWatchers.add(pendingWatcher);

    if (!this._listenerRegistered) {
      this._listenerRegistered = true;
      this.rootFront.on("resource-available-form", this._onResourceAvailable);
      this.rootFront.on("resource-destroyed-form", this._onResourceDestroyed);
    }

    const promises = [];
    for (const resource of resources) {
      promises.push(this._startListening(resource));
    }
    await Promise.all(promises);

    this._notifyWatchers();

    this._pendingWatchers.delete(pendingWatcher);

    const watchedResources = pendingWatcher.resources;

    if (!watchedResources.length) {
      return;
    }

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

  unwatchResources(resources, options) {
    const { onAvailable } = options;

    if (typeof onAvailable !== "function") {
      throw new Error(
        "RootResourceCommand.unwatchResources expects an onAvailable function as argument"
      );
    }

    for (const type of resources) {
      if (!this._isValidResourceType(type)) {
        throw new Error(
          `RootResourceCommand.unwatchResources invoked with an unknown type: "${type}"`
        );
      }
    }

    const allWatchers = [...this._watchers, ...this._pendingWatchers];
    for (const watcherEntry of allWatchers) {
      if (watcherEntry.onAvailable == onAvailable) {
        watcherEntry.resources = watcherEntry.resources.filter(resourceType => {
          return !resources.includes(resourceType);
        });
      }
    }
    this._watchers = this._watchers.filter(entry => {
      return entry.resources.length > 0;
    });

    for (const resource of resources) {
      const isResourceWatched = allWatchers.some(watcherEntry =>
        watcherEntry.resources.includes(resource)
      );

      if (!isResourceWatched && this._listenedResources.has(resource)) {
        this._stopListening(resource);
      }
    }
  }

  clearResources(resourceTypes) {
    if (!Array.isArray(resourceTypes)) {
      throw new Error("clearResources expects an array of resource types");
    }
    // Clear the cached resources of the type.
    this._cache = this._cache.filter(
      cachedResource => !resourceTypes.includes(cachedResource.resourceType)
    );

    if (
      resourceTypes.length &&
      // @backward-compat { version 103 } The clearResources functionality was added in 103 and
      // not supported in old servers.
      this.rootFront.traits.supportsClearResources
    ) {
      this.rootFront.clearResources(resourceTypes);
    }
  }

  async waitForNextResource(
    resourceType,
    { ignoreExistingResources = false, predicate } = {}
  ) {
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

  async _onResourceAvailable(resources) {
    for (const resource of resources) {
      const { resourceType } = resource;

      resource.isAlreadyExistingResource = this._processingExistingResources.has(
        resourceType
      );

      this._queueResourceEvent("available", resourceType, resource);

      this._cache.push(resource);
    }

    this._throttledNotifyWatchers();
  }

  async _onResourceDestroyed(resources) {
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
      if (!resources.includes(resourceType)) {
        continue;
      }
      if (pendingEvents.length > 0) {
        const lastEvent = pendingEvents[pendingEvents.length - 1];
        if (lastEvent.callbackType == callbackType) {
          lastEvent.updates.push(update);
          continue;
        }
      }
      pendingEvents.push({
        callbackType,
        updates: [update],
      });
    }
  }

  _notifyWatchers() {
    for (const watcherEntry of this._watchers) {
      const { onAvailable, onDestroyed, pendingEvents } = watcherEntry;
      watcherEntry.pendingEvents = [];

      for (const { callbackType, updates } of pendingEvents) {
        try {
          if (callbackType == "available") {
            onAvailable(updates, { areExistingResources: false });
          } else if (callbackType == "destroyed" && onDestroyed) {
            onDestroyed(updates);
          }
        } catch (e) {
          console.error(
            "Exception while calling a RootResourceCommand",
            callbackType,
            "callback",
            ":",
            e
          );
        }
      }
    }
  }

  _isValidResourceType(type) {
    return this.ALL_TYPES.includes(type);
  }

  async _startListening(resourceType) {
    if (this._listenedResources.has(resourceType)) {
      return;
    }
    this._listenedResources.add(resourceType);

    this._processingExistingResources.add(resourceType);

    // For now, if the server doesn't support the resource type
    // act as if we were listening, but do nothing.
    // Calling watchResources/unwatchResources will work fine,
    // but no resource will be notified.
    if (this.rootFront.traits.resources?.[resourceType]) {
      await this.rootFront.watchResources([resourceType]);
    } else {
      console.warn(
        `Ignored watchRequest, resourceType "${resourceType}" not found in rootFront.traits.resources`
      );
    }
    this._processingExistingResources.delete(resourceType);
  }

  async _forwardExistingResources(resourceTypes, onAvailable) {
    const existingResources = this._cache.filter(resource =>
      resourceTypes.includes(resource.resourceType)
    );
    if (existingResources.length > 0) {
      await onAvailable(existingResources, { areExistingResources: true });
    }
  }

  _stopListening(resourceType) {
    if (!this._listenedResources.has(resourceType)) {
      throw new Error(
        `Stopped listening for resource '${resourceType}' that isn't being listened to`
      );
    }
    this._listenedResources.delete(resourceType);

    this._cache = this._cache.filter(
      cachedResource => cachedResource.resourceType !== resourceType
    );

    if (
      !this.rootFront.isDestroyed() &&
      this.rootFront.traits.resources?.[resourceType]
    ) {
      this.rootFront.unwatchResources([resourceType]);
    }
  }
}

RootResourceCommand.TYPES = RootResourceCommand.prototype.TYPES = {
  EXTENSIONS_BGSCRIPT_STATUS: "extensions-backgroundscript-status",
};
RootResourceCommand.ALL_TYPES = RootResourceCommand.prototype.ALL_TYPES = Object.values(
  RootResourceCommand.TYPES
);
module.exports = RootResourceCommand;
