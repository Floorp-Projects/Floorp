/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class ConsoleCommands {
  constructor({ commands }) {
    this.commands = commands;
  }

  getFrontByID(id) {
    return this.commands.client.getFrontByID(id);
  }

  async evaluateJSAsync(expression, options = {}) {
    const {
      selectedObjectActor,
      selectedNodeActor,
      frameActor,
      selectedTargetFront,
    } = options;

    let targetFront = this.commands.targetCommand.targetFront;

    const selectedActor =
      selectedObjectActor || selectedNodeActor || frameActor;

    if (selectedTargetFront) {
      targetFront = selectedTargetFront;
    } else if (selectedActor) {
      const selectedFront = this.getFrontByID(selectedActor);
      if (selectedFront) {
        targetFront = selectedFront.targetFront;
      }
    }

    const front = await targetFront.getFront("console");
    return front.evaluateJSAsync(expression, options);
  }
}

module.exports = ConsoleCommands;
