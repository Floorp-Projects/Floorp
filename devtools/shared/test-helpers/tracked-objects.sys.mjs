/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test-only module in order to register objects later inspected by
// the allocation tracker (in the same folder).
//
// We are going to store a weak reference to the passed objects,
// in order to prevent holding them in memory.
// Allocation tracker will then print detailed information
// about why these objects are still allocated.

const objects = [];

/**
 * Request to track why the given object is kept in memory,
 * later on, when retrieving all the watched object via getAllNodeIds.
 */
export function track(obj) {
  // We store a weak reference, so that we do force keeping the object in memory!!
  objects.push(Cu.getWeakReference(obj));
}

/**
 * Return the NodeId's of all the objects passed via `track()` method.
 *
 * NodeId's are used by spidermonkey memory API to designates JS objects in head snapshots.
 */
export function getAllNodeIds() {
  // Filter out objects which have been freed already
  return (
    objects
      .map(weak => weak.get())
      .filter(obj => !!obj)
      // Convert objects from here instead of from allocation tracker in order
      // to be from the shared system compartment and avoid trying to compute the NodeId
      // of a wrapper!
      .map(ChromeUtils.getObjectNodeId)
  );
}

/**
 * Used by tests to clear all tracked objects
 */
export function clear() {
  objects.length = 0;
}
