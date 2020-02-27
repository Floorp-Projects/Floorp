/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class ConsoleCommands {
  constructor({ devToolsClient, hud, threadFront }) {
    this.devToolsClient = devToolsClient;
    this.hud = hud;
    this.threadFront = threadFront;
  }

  getFrontByID(id) {
    return this.devToolsClient.getFrontByID(id);
  }

  async evaluateJSAsync(expression, options = {}) {
    const {
      selectedNodeFront,
      selectedThreadFront,
      frameActor,
      selectedObjectActor,
    } = options;
    let front = await this.hud.currentTarget.getFront("console");

    // Defer to the selected paused thread front
    if (frameActor) {
      const frameFront = this.getFrontByID(frameActor);
      if (frameFront) {
        front = await frameFront.targetFront.getFront("console");
      }
    }

    // NOTE: once we handle the other tasks in console evaluation,
    // all of the implicit actions like pausing, selecting a frame in the inspector,
    // etc will update the selected thread and we will no longer need to support these other
    // cases.
    if (selectedThreadFront) {
      front = await selectedThreadFront.targetFront.getFront("console");

      // If there's a selectedObjectActor option, this means the user intend to do a
      // given action on a specific object, so it should take precedence over selected
      // node front.
    } else if (selectedObjectActor) {
      const objectFront = this.getFrontByID(selectedObjectActor);
      if (objectFront) {
        front = await objectFront.targetFront.getFront("console");
      }
    } else if (selectedNodeFront) {
      // Defer to the selected node's thread console front
      front = await selectedNodeFront.targetFront.getFront("console");
      options.selectedNodeActor = selectedNodeFront.actorID;
    }

    return front.evaluateJSAsync(expression, options);
  }

  timeWarp(executionPoint) {
    return this.threadFront.timeWarp(executionPoint);
  }
}

module.exports = ConsoleCommands;
