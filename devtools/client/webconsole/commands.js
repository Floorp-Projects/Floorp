/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class ConsoleCommands {
  constructor({ debuggerClient, proxy, threadFront, currentTarget }) {
    this.debuggerClient = debuggerClient;
    this.proxy = proxy;
    this.threadFront = threadFront;
    this.currentTarget = currentTarget;
  }

  evaluateJSAsync(expression, options = {}) {
    const { selectedNodeFront, webConsoleFront } = options;
    let front = this.proxy.webConsoleFront;

    // Defer to the selected paused thread front
    if (webConsoleFront) {
      front = webConsoleFront;
    }

    // Defer to the selected node's thread console front
    if (selectedNodeFront) {
      front = selectedNodeFront.targetFront.activeConsole;
      options.selectedNodeActor = selectedNodeFront.actorID;
    }

    return front.evaluateJSAsync(expression, options);
  }

  timeWarp(executionPoint) {
    return this.threadFront.timeWarp(executionPoint);
  }
}

module.exports = ConsoleCommands;
