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
let xpcShellTargetActor = null;

var TargetActorRegistry = {
  registerTargetActor(targetActor) {
    browsingContextTargetActors.add(targetActor);
  },

  unregisterTargetActor(targetActor) {
    browsingContextTargetActors.delete(targetActor);
  },

  registerXpcShellTargetActor(targetActor) {
    xpcShellTargetActor = targetActor;
  },

  unregisterXpcShellTargetActor(targetActor) {
    xpcShellTargetActor = null;
  },

  /**
   * Return the first target actor matching the passed browser element id. Returns null if
   * no matching target actors could be found.
   *
   * @param {Integer} browserId: The browserId to retrieve targets for. Pass null to
   *                             retrieve the parent process targets.
   * @param {String} connectionPrefix: Optional prefix, in order to select only actor
   *                                   related to the same connection. i.e. the same client.
   * @returns {TargetActor|null}
   */
  getTargetActor(browserId, connectionPrefix) {
    return this.getTargetActors(browserId, connectionPrefix)[0] || null;
  },

  /**
   * Return the target actors matching the passed browser element id.
   * In some scenarios, the registry can have multiple target actors for a given
   * browserId (e.g. the regular DevTools content toolbox + DevTools WebExtensions targets).
   *
   * @param {Integer} browserId: The browserId to retrieve targets for. Pass null to
   *                             retrieve the parent process targets.
   * @param {String} connectionPrefix: Optional prefix, in order to select only actor
   *                                   related to the same connection. i.e. the same client.
   * @returns {Array<TargetActor>}
   */
  getTargetActors(browserId, connectionPrefix) {
    const actors = [];
    for (const actor of browsingContextTargetActors) {
      if (
        ((!connectionPrefix || actor.actorID.startsWith(connectionPrefix)) &&
          actor.browserId == browserId) ||
        (browserId === null && actor.typeName === "parentProcessTarget")
      ) {
        actors.push(actor);
      }
    }
    return actors;
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

    // The xpcshell target actor also lives in the "parent process", even if it
    // is an instance of ContentProcessTargetActor.
    if (xpcShellTargetActor) {
      return xpcShellTargetActor;
    }

    return null;
  },
};
