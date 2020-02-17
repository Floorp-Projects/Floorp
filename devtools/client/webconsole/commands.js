/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class ConsoleCommands {
  constructor({ devToolsClient, proxy, threadFront, currentTarget }) {
    this.devToolsClient = devToolsClient;
    this.proxy = proxy;
    this.threadFront = threadFront;
    this.currentTarget = currentTarget;
  }

  async evaluateJSAsync(expression, options = {}) {
    const { selectedNodeFront, webConsoleFront, selectedObjectActor } = options;
    let front = this.proxy.webConsoleFront;

    // Defer to the selected paused thread front
    if (webConsoleFront) {
      front = webConsoleFront;
    }

    // If there's a selectedObjectActor option, this means the user intend to do a
    // given action on a specific object, so it should take precedence over selected
    // node front.
    if (selectedObjectActor) {
      const objectFront = this.devToolsClient.getFrontByID(selectedObjectActor);
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
