/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ObjectFront = require("devtools/shared/fronts/object");

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

  createObjectFront(object) {
    return new ObjectFront(this.debuggerClient, object);
  }

  createLongStringFront(object) {
    return this.proxy.webConsoleFront.longString(object);
  }

  releaseActor(actor) {
    if (!actor) {
      return null;
    }

    const objFront = this.debuggerClient.getFrontByID(actor);
    if (objFront) {
      return objFront.release();
    }

    // In case there's no object front, use the client's release method.
    return this.debuggerClient.release(actor).catch(() => {});
  }

  async fetchObjectProperties(grip, ignoreNonIndexedProperties) {
    const client = new ObjectFront(this.currentTarget.client, grip);
    const iterator = await client.enumProperties({
      ignoreNonIndexedProperties,
    });
    const { ownProperties } = await iterator.slice(0, iterator.count);
    return ownProperties;
  }

  async fetchObjectEntries(grip) {
    const client = new ObjectFront(this.currentTarget.client, grip);
    const iterator = await client.enumEntries();
    const { ownProperties } = await iterator.slice(0, iterator.count);
    return ownProperties;
  }

  timeWarp(executionPoint) {
    return this.threadFront.timeWarp(executionPoint);
  }
}

module.exports = ConsoleCommands;
