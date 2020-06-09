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

  getTargetActor(browsingContextID) {
    for (const actor of browsingContextTargetActors) {
      if (actor.browsingContextID == browsingContextID) {
        return actor;
      }
    }
    return null;
  },
};
