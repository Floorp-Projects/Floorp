/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  objectsManagerSpec,
} = require("resource://devtools/shared/specs/objects-manager.js");

/**
 * This actor is a singleton per Target which allows interacting with JS Object
 * inspected by DevTools. Typically from the Console or Debugger.
 */
class ObjectsManagerActor extends Actor {
  constructor(conn) {
    super(conn, objectsManagerSpec);
  }

  /**
   * Release Actors by bulk by specifying their actor IDs.
   * (Passing the whole Front [i.e. Actor's form] would be more expensive than passing only their IDs)
   *
   * @param {Array<string>} actorIDs
   *        List of all actor's IDs to release.
   */
  releaseObjects(actorIDs) {
    for (const actorID of actorIDs) {
      const actor = this.conn.getActor(actorID);
      // Note that release will also typically call Actor's destroy and unregister the actor from its Pool
      if (actor) {
        actor.release();
      }
    }
  }
}

exports.ObjectsManagerActor = ObjectsManagerActor;
