/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Filters "stores-update" response to only include events for
// the storage type we desire
function getFilteredStorageEvents(updates, storageType) {
  const filteredUpdate = Object.create(null);

  // updateType will be "added", "changed", or "deleted"
  for (const updateType in updates) {
    if (updates[updateType][storageType]) {
      if (!filteredUpdate[updateType]) {
        filteredUpdate[updateType] = {};
      }
      filteredUpdate[updateType][storageType] =
        updates[updateType][storageType];
    }
  }

  return Object.keys(filteredUpdate).length > 0 ? filteredUpdate : null;
}

// This is a mixin that provides all shared cored between storage legacy
// listeners
function makeStorageLegacyListener(storageKey, storageType) {
  return async function({
    targetCommand,
    targetType,
    targetFront,
    onAvailable,
    onUpdated,
    onDestroyed,
  }) {
    if (!targetFront.isTopLevel) {
      return;
    }

    const storageFront = await targetFront.getFront("storage");
    const storageTypes = await storageFront.listStores();

    // Initialization
    const storage = storageTypes[storageKey];

    // extension storage might not be available
    if (!storage) {
      return;
    }

    storage.resourceType = storageType;
    storage.resourceKey = storageKey;
    // storage resources are singletons, and thus we can set their ID to their
    // storage type
    storage.resourceId = storageType;
    onAvailable([storage]);

    // Maps global events from `storageFront` shared for all storage-types,
    // down to storage-type's specific front `storage`.
    // Any item in the store gets updated
    storageFront.on("stores-update", response => {
      response = getFilteredStorageEvents(response, storageKey);
      if (!response) {
        return;
      }
      storage.emit("single-store-update", {
        changed: response.changed,
        added: response.added,
        deleted: response.deleted,
      });
    });

    // When a store gets cleared
    storageFront.on("stores-cleared", response => {
      const cleared = response[storageKey];

      if (!cleared) {
        return;
      }

      storage.emit("single-store-cleared", {
        clearedHostsOrPaths: cleared,
      });
    });
  };
}

module.exports = { makeStorageLegacyListener };
