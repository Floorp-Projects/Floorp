/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TYPES = {
  CONSOLE_MESSAGE: "console-message",
  PLATFORM_MESSAGE: "platform-message",
};
exports.TYPES = TYPES;

// Helper dictionary, which will contain all data specific to each resource type.
// See following for loop which will add new attributes for each type.
const Resources = {
  [TYPES.CONSOLE_MESSAGE]: {
    path: "devtools/server/actors/resources/console-messages",
  },
  [TYPES.PLATFORM_MESSAGE]: {
    path: "devtools/server/actors/resources/platform-messages",
  },
};

for (const resource of Object.values(Resources)) {
  // For each resource type, we have:
  // - a weak map which will store Resource Watchers
  // (i.e. devtools/server/actors/resources/ class instances)
  // keyed by target actor.
  resource.watchers = new WeakMap();

  // - a shortcut to the Resource Watcher module.
  // Each module export a Resource Watcher class.
  loader.lazyRequireGetter(resource, "WatcherClass", resource.path);
}

/**
 * Start watching for a new list of resource types.
 * This will also emit all already existing resources before resolving.
 *
 * This is called by DevToolsFrameChild.
 *
 * @param TargetActor targetActor
 *        The related target actor, it:
 *          - defines what context to observe (browsing context, process, worker, ...)
 *          - exposes `onResourceAvailable` method to be notified about the available resources
 * @param Array<String> resourceTypes
 *        List of all type of resource to listen to.
 */
function watchTargetResources(targetActor, resourceTypes) {
  for (const resourceType of resourceTypes) {
    if (!(resourceType in Resources)) {
      throw new Error(`Unsupported resource type '${resourceType}'`);
    }
    // Pull all info about this resource type from `Resources` global object
    const { watchers, WatcherClass } = Resources[resourceType];

    // Ignore resources we're already listening to
    if (watchers.has(targetActor)) {
      continue;
    }

    const watcher = new WatcherClass(targetActor, {
      onAvailable: targetActor.onResourceAvailable,
    });
    watchers.set(targetActor, watcher);
  }
}
exports.watchTargetResources = watchTargetResources;

/**
 * Stop watching for a list of resource types.
 *
 * This is called by DevToolsFrameChild.
 *
 * @param TargetActor targetActor
 *        The related target actor, already passed to watchTargetResources
 * @param Array<String> resourceTypes
 *        List of all type of resource to stop listening to.
 */
function unwatchTargetResources(targetActor, resourceTypes) {
  for (const resourceType of resourceTypes) {
    if (!(resourceType in Resources)) {
      throw new Error(`Unsupported resource type '${resourceType}'`);
    }
    // Pull all info about this resource type from `Resources` global object
    const { watchers } = Resources[resourceType];

    const watcher = watchers.get(targetActor);
    watcher.destroy();
    watchers.delete(resourceType);
  }
}
exports.unwatchTargetResources = unwatchTargetResources;

/**
 * If we are watching for the given resource type,
 * return the current ResourceWatcher instance used by this target actor
 * in order to observe this resource type.
 *
 * @param String resourceType
 *        The resource type to query
 * @return ResourceWatcher
 *         The resource watcher instance, defined in devtools/server/actors/resources/
 */
function getResourceWatcher(targetActor, resourceType) {
  if (!(resourceType in Resources)) {
    throw new Error(`Unsupported resource type '${resourceType}'`);
  }
  // Pull the watchers Map for this resource type from `Resources` global object
  const { watchers } = Resources[resourceType];

  return watchers.get(targetActor);
}
exports.getResourceWatcher = getResourceWatcher;
