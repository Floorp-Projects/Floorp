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
  #initialTargetTitle;
  #navigationRequests;
  #targetTitlesPerURL;

  constructor(commands) {
    this.#commands = commands;
  }

  /**
   * Stop recording and clear the state.
   */
  destroy() {
    this.clear();

    this.#commands.resourceCommand.unwatchResources(
      [TYPES.DOCUMENT_EVENT, TYPES.NETWORK_EVENT],
      {
        onAvailable: this.#onResourceAvailable,
      }
    );
  }

  /**
   * Reset the current state.
   */
  clear() {
    this.#navigationRequests = [];
    this.#targetTitlesPerURL = new Map();
    this.#initialTargetTitle = this.#commands.targetCommand.targetFront.title;
  }

  /**
   * Start recording additional events for HAR files building.
   */
  async connect() {
    this.clear();

    await this.#commands.resourceCommand.watchResources(
      [TYPES.DOCUMENT_EVENT, TYPES.NETWORK_EVENT],
      {
        onAvailable: this.#onResourceAvailable,
      }
    );
  }

  getHarData() {
    return {
      initialTargetTitle: this.#initialTargetTitle,
      navigationRequests: this.#navigationRequests,
      targetTitlesPerURL: this.#targetTitlesPerURL,
    };
  }

  #onResourceAvailable = resources => {
    for (const resource of resources) {
      if (resource.resourceType === TYPES.DOCUMENT_EVENT) {
        if (
          resource.name === "dom-complete" &&
          resource.targetFront.isTopLevel
        ) {
          this.#targetTitlesPerURL.set(
            resource.targetFront.url,
            resource.targetFront.title
          );
        }
      } else if (resource.resourceType === TYPES.NETWORK_EVENT) {
        if (resource.isNavigationRequest) {
          this.#navigationRequests.push(resource);
        }
      }
    }
  };
}

exports.HarMetadataCollector = HarMetadataCollector;
