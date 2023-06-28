/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TYPES,
} = require("resource://devtools/shared/commands/resource/resource-command.js");

/**
 * This collector class is dedicated to recording additional metadata necessary
 * to build HAR files. The actual request data will be provided by the
 * netmonitor which is already monitoring for requests.
 *
 * The only purpose of this class is to record additional document and network
 * events, which will help to assign requests to individual pages.
 *
 * It should be created and destroyed by the main netmonitor data collector.
 */
class HarMetadataCollector {
  #commands;
  #initialURL;
  #navigationRequests;

  constructor(commands) {
    this.#commands = commands;
  }

  /**
   * Stop recording and clear the state.
   */
  destroy() {
    this.clear();

    this.#commands.resourceCommand.unwatchResources([TYPES.NETWORK_EVENT], {
      onAvailable: this.#onResourceAvailable,
    });
  }

  /**
   * Reset the current state.
   */
  clear() {
    // Keep the initial URL for requests captured before the first recorded
    // navigation.
    this.#initialURL = this.#commands.targetCommand.targetFront.url;
    this.#navigationRequests = [];
  }

  /**
   * Start recording additional events for HAR files building.
   */
  async connect() {
    this.clear();

    await this.#commands.resourceCommand.watchResources([TYPES.NETWORK_EVENT], {
      onAvailable: this.#onResourceAvailable,
    });
  }

  getHarData() {
    return {
      initialURL: this.#initialURL,
      navigationRequests: this.#navigationRequests,
    };
  }

  #onResourceAvailable = resources => {
    for (const resource of resources) {
      if (
        resource.resourceType === TYPES.NETWORK_EVENT &&
        resource.isNavigationRequest
      ) {
        this.#navigationRequests.push(resource);
      }
    }
  };
}

exports.HarMetadataCollector = HarMetadataCollector;
