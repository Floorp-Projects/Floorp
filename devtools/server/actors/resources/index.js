/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TYPES = {
  CONSOLE_MESSAGE: "console-message",
  CSS_MESSAGE: "css-message",
  ERROR_MESSAGE: "error-message",
  PLATFORM_MESSAGE: "platform-message",
};
exports.TYPES = TYPES;

// Helper dictionary, which will contain all data specific to each resource type.
// - `path` is the absolute path to the module defining the Resource Watcher class,
// - `parentProcessResource` is an optional boolean to be set to true for resources to be
//    watched from the parent process.
// Also see the following for loop which will add new attributes for each type:
// - `watchers` is a weak map which will store Resource Watchers
//    (i.e. devtools/server/actors/resources/ class instances)
//    keyed by target actor -or- watcher actor.
// - `WatcherClass` is a shortcut to the Resource Watcher module.
//    Each module exports a Resource Watcher class.
const Resources = {
  [TYPES.CONSOLE_MESSAGE]: {
    path: "devtools/server/actors/resources/console-messages",
  },
  [TYPES.CSS_MESSAGE]: {
    path: "devtools/server/actors/resources/css-messages",
  },
  [TYPES.ERROR_MESSAGE]: {
    path: "devtools/server/actors/resources/error-messages",
  },
  [TYPES.PLATFORM_MESSAGE]: {
    path: "devtools/server/actors/resources/platform-messages",
  },
};

for (const resource of Object.values(Resources)) {
  resource.watchers = new WeakMap();

  loader.lazyRequireGetter(resource, "WatcherClass", resource.path);
}

/**
 * Start watching for a new list of resource types.
 * This will also emit all already existing resources before resolving.
 *
 * @param Actor watcherOrTargetActor
 *        Either a WatcherActor or a TargetActor which can be listening to a resource.
 *        WatcherActor will be used for resources listened from the parent process,
 *        and TargetActor will be used for resources listened from the content process.
 *        This actor:
 *          - defines what context to observe (browsing context, process, worker, ...)
 *            Via browsingContextID, windows, docShells attributes for the target actor.
 *            Via browserId or browserElement attributes for the watcher actor.
 *          - exposes `notifyResourceAvailable` method to be notified about the available resources
 * @param Array<String> resourceTypes
 *        List of all type of resource to listen to.
 */
function watchResources(watcherOrTargetActor, resourceTypes) {
  for (const resourceType of resourceTypes) {
    if (!(resourceType in Resources)) {
      throw new Error(`Unsupported resource type '${resourceType}'`);
    }
    // Pull all info about this resource type from `Resources` global object
    const { watchers, WatcherClass } = Resources[resourceType];

    // Ignore resources we're already listening to
    if (watchers.has(watcherOrTargetActor)) {
      continue;
    }

    const watcher = new WatcherClass(watcherOrTargetActor, {
      onAvailable: watcherOrTargetActor.notifyResourceAvailable,
    });
    watchers.set(watcherOrTargetActor, watcher);
  }
}
function getParentProcessResourceTypes(resourceTypes) {
  return resourceTypes.filter(resourceType => {
    if (!(resourceType in Resources)) {
      throw new Error(`Unsupported resource type '${resourceType}'`);
    }
    return !!Resources[resourceType].parentProcessResource;
  });
}
function getContentProcessResourceTypes(resourceTypes) {
  return resourceTypes.filter(resourceType => {
    if (!(resourceType in Resources)) {
      throw new Error(`Unsupported resource type '${resourceType}'`);
    }
    return !Resources[resourceType].parentProcessResource;
  });
}

/**
 * See `watchResources` jsdoc.
 *
 * This one is to be called from the target process/thread.
 * This is called by DevToolsFrameChild.
 */
function watchTargetResources(targetActor, resourceTypes) {
  const contentProcessTypes = getContentProcessResourceTypes(resourceTypes);
  watchResources(targetActor, contentProcessTypes);
}
exports.watchTargetResources = watchTargetResources;

/**
 * See `watchResources` jsdoc.
 * This one is to be called from the parent process.
 * This is called by WatcherActor.
 *
 * @return Array<String>
 *         List of all resource types that aren't watched from the parent process.
 */
function watchParentProcessResources(watcherActor, resourceTypes) {
  const parentProcessTypes = getParentProcessResourceTypes(resourceTypes);
  watchResources(watcherActor, parentProcessTypes);

  return resourceTypes.filter(
    resource => !parentProcessTypes.includes(resource)
  );
}
exports.watchParentProcessResources = watchParentProcessResources;

/**
 * Stop watching for a list of resource types.
 *
 * @param Actor watcherOrTargetActor
 *        The related actor, already passed to watchTargetResources
 * @param Array<String> resourceTypes
 *        List of all type of resource to stop listening to.
 */
function unwatchResources(watcherOrTargetActor, resourceTypes) {
  for (const resourceType of resourceTypes) {
    if (!(resourceType in Resources)) {
      throw new Error(`Unsupported resource type '${resourceType}'`);
    }
    // Pull all info about this resource type from `Resources` global object
    const { watchers } = Resources[resourceType];

    const watcher = watchers.get(watcherOrTargetActor);
    if (watcher) {
      watcher.destroy();
      watchers.delete(watcherOrTargetActor);
    }
  }
}

/**
 * See `unwatchResources` jsdoc.
 * This one is to be called from the target process/thread.
 * This is called by DevToolsFrameChild.
 */
function unwatchTargetResources(targetActor, resourceTypes) {
  const contentProcessTypes = getContentProcessResourceTypes(resourceTypes);
  unwatchResources(targetActor, contentProcessTypes);
}
exports.unwatchTargetResources = unwatchTargetResources;

/**
 * See `unwatchResources` jsdoc.
 * This one is to be called from the parent process.
 * This is called by WatcherActor.
 *
 * @return Array<String>
 *         List of all resource types that aren't watched from the parent process.
 */
function unwatchParentProcessResources(watcherActor, resourceTypes) {
  const parentProcessTypes = getParentProcessResourceTypes(resourceTypes);
  unwatchResources(watcherActor, parentProcessTypes);

  return resourceTypes.filter(
    resource => !parentProcessTypes.includes(resource)
  );
}
exports.unwatchParentProcessResources = unwatchParentProcessResources;

/**
 * Stop watching for all watched resources on a given actor.
 *
 * @param Actor watcherOrTargetActor
 *        The related actor, already passed to watchTargetResources or watchParentProcessResources.
 */
function unwatchAllTargetResources(watcherOrTargetActor) {
  for (const { watchers } of Object.values(Resources)) {
    const watcher = watchers.get(watcherOrTargetActor);
    if (watcher) {
      watcher.destroy();
      watchers.delete(watcherOrTargetActor);
    }
  }
}
exports.unwatchAllTargetResources = unwatchAllTargetResources;

/**
 * If we are watching for the given resource type,
 * return the current ResourceWatcher instance used by this target actor
 * in order to observe this resource type.
 *
 * @param Actor watcherOrTargetActor
 *        Either a WatcherActor or a TargetActor which can be listening to a resource.
 *        WatcherActor will be used for resources listened from the parent process,
 *        and TargetActor will be used for resources listened from the content process.
 * @param String resourceType
 *        The resource type to query
 * @return ResourceWatcher
 *         The resource watcher instance, defined in devtools/server/actors/resources/
 */
function getResourceWatcher(watcherOrTargetActor, resourceType) {
  if (!(resourceType in Resources)) {
    throw new Error(`Unsupported resource type '${resourceType}'`);
  }
  // Pull the watchers Map for this resource type from `Resources` global object
  const { watchers } = Resources[resourceType];

  return watchers.get(watcherOrTargetActor);
}
exports.getResourceWatcher = getResourceWatcher;
