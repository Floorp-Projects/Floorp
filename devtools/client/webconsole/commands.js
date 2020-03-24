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
      selectedThreadFront,
      frameActor,
      selectedObjectActor,
      selectedNodeActor,
    } = options;
    let front = await this.hud.currentTarget.getFront("console");
    const selectedActor = selectedObjectActor || selectedNodeActor;

    // Defer to the selected paused thread front
    // NOTE: when the context selector is enabled, this will no longer be needed
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

    // Get the selected thread's, webconsole front so that we can
    // evaluate expressions against it.
    if (selectedThreadFront) {
      front = await selectedThreadFront.targetFront.getFront("console");
    } else if (selectedActor) {
      const selectedFront = this.getFrontByID(selectedActor);
      if (selectedFront) {
        front = await selectedFront.targetFront.getFront("console");
      }
    }

    return front.evaluateJSAsync(expression, options);
  }

  timeWarp(executionPoint) {
    return this.threadFront.timeWarp(executionPoint);
  }
}

module.exports = ConsoleCommands;
