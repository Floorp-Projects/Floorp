/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class ConsoleCommands {
  constructor({ devToolsClient, hud }) {
    this.devToolsClient = devToolsClient;
    this.hud = hud;
  }

  getFrontByID(id) {
    return this.devToolsClient.getFrontByID(id);
  }

  async evaluateJSAsync(expression, options = {}) {
    const { selectedObjectActor, selectedNodeActor, frameActor } = options;
    let front = await this.hud.currentTarget.getFront("console");

    const selectedActor =
      selectedObjectActor || selectedNodeActor || frameActor;

    if (selectedActor) {
      const selectedFront = this.getFrontByID(selectedActor);
      if (selectedFront) {
        front = await selectedFront.targetFront.getFront("console");
      }
    }

    return front.evaluateJSAsync(expression, options);
  }
}

module.exports = ConsoleCommands;
