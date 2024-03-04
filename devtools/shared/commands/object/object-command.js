/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * The ObjectCommand helps inspecting and managing lifecycle
 * of all inspected JavaScript objects.
 */
class ObjectCommand {
  constructor({ commands, descriptorFront, watcherFront }) {
    this.#commands = commands;
  }
  #commands = null;

  /**
   * Release a set of object actors all at once.
   *
   * @param {Array<ObjectFront>} frontsToRelease
   *        List of fronts for the object to release.
   */
  async releaseObjects(frontsToRelease) {
    // First group all object fronts per target
    const actorsPerTarget = new Map();
    const promises = [];
    for (const frontToRelease of frontsToRelease) {
      const { targetFront } = frontToRelease;
      // If the front is already destroyed, its target front will be nullified.
      if (!targetFront) {
        continue;
      }

      let actorIDsToRemove = actorsPerTarget.get(targetFront);
      if (!actorIDsToRemove) {
        actorIDsToRemove = [];
        actorsPerTarget.set(targetFront, actorIDsToRemove);
      }
      actorIDsToRemove.push(frontToRelease.actorID);
      frontToRelease.destroy();
    }

    // Then release all fronts by bulk per target
    for (const [targetFront, actorIDs] of actorsPerTarget) {
      const objectsManagerFront = await targetFront.getFront("objects-manager");
      promises.push(objectsManagerFront.releaseObjects(actorIDs));
    }

    await Promise.all(promises);
  }
}

module.exports = ObjectCommand;
