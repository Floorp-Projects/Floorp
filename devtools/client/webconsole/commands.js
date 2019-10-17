/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ObjectClient = require("devtools/shared/client/object-client");
const LongStringClient = require("devtools/shared/client/long-string-client");

class ConsoleCommands {
  constructor({ debuggerClient, proxy, threadFront, currentTarget }) {
    this.debuggerClient = debuggerClient;
    this.proxy = proxy;
    this.threadFront = threadFront;
    this.currentTarget = currentTarget;
  }

  evaluateJSAsync(expression, options) {
    return this.proxy.webConsoleClient.evaluateJSAsync(expression, options);
  }

  createObjectClient(object) {
    return new ObjectClient(this.debuggerClient, object);
  }

  createLongStringClient(object) {
    return new LongStringClient(this.debuggerClient, object);
  }

  releaseActor(actor) {
    if (!actor) {
      return null;
    }

    return this.debuggerClient.release(actor);
  }

  async fetchObjectProperties(grip, ignoreNonIndexedProperties) {
    const client = new ObjectClient(this.currentTarget.client, grip);
    const { iterator } = await client.enumProperties({
      ignoreNonIndexedProperties,
    });
    const { ownProperties } = await iterator.slice(0, iterator.count);
    return ownProperties;
  }

  async fetchObjectEntries(grip) {
    const client = new ObjectClient(this.currentTarget.client, grip);
    const { iterator } = await client.enumEntries();
    const { ownProperties } = await iterator.slice(0, iterator.count);
    return ownProperties;
  }

  timeWarp(executionPoint) {
    return this.threadFront.timeWarp(executionPoint);
  }
}

module.exports = ConsoleCommands;
