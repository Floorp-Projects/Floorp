/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TargetActorRegistry"];

// Keep track of all BrowsingContext target actors.
// This is especially used to track the actors using Message manager connector,
// or the ones running in the parent process.
// Top level actors, like tab's top level target or parent process target
// are still using message manager in order to avoid being destroyed on navigation.
// And because of this, these actors aren't using JS Window Actor.
const browsingContextTargetActors = new Set();

var TargetActorRegistry = {
  registerTargetActor(targetActor) {
    browsingContextTargetActors.add(targetActor);
  },

  unregisterTargetActor(targetActor) {
    browsingContextTargetActors.delete(targetActor);
  },

  /**
   * Return the target actor matching the passed browser element id. Returns null if
   * no matching target actors could be found.
   *
   * @param {Integer} browserId
   * @returns {TargetActor|null}
   */
  getTargetActor(browserId) {
    for (const actor of browsingContextTargetActors) {
      if (
        actor.browserId == browserId ||
        (browserId === null && actor.typeName === "parentProcessTarget")
      ) {
        return actor;
      }
    }
    return null;
  },

  /**
   * Return the parent process target actor. Returns null if it couldn't be found.
   *
   * @returns {TargetActor|null}
   */
  getParentProcessTargetActor() {
    for (const actor of browsingContextTargetActors) {
      if (actor.typeName === "parentProcessTarget") {
        return actor;
      }
    }

    return null;
  },
};
