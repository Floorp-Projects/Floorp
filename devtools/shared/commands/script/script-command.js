/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class ScriptCommand {
  constructor({ commands }) {
    this._commands = commands;
  }

  /**
   * Execute a JavaScript expression.
   *
   * @param {String} expression: The code you want to evaluate.
   * @param {Object} options: Options for evaluation:
   * @param {Object} options.frameActor: a FrameActor ID. The actor holds a reference to
   *        a Debugger.Frame. This option allows you to evaluate the string in the frame
   *        of the given FrameActor.
   * @param {String} options.url: the url to evaluate the script as. Defaults to "debugger eval code".
   * @param {TargetFront} options.selectedTargetFront: When passed, the expression will be
   *        evaluated in the context of the target (as opposed to the default, top-level one).
   * @param {String} options.selectedNodeActor: A NodeActor ID that may be used by helper
   *        functions that can reference the currently selected node in the Inspector, like $0.
   * @param {String} options.selectedObjectActor: the actorID of a given objectActor.
   *        This is used by context menu entries to get a reference to an object, in order
   *        to perform some operation on it (copy it, store it as a global variable, …).
   * @param {Number} options.innerWindowID: An optional window id to be used for the evaluation,
   *        instead of the regular webConsoleActor.evalWindow.
   *        This is used by functions that may want to evaluate in a different window (for
   *        example a non-remote iframe), like getting the elements of a given document.
   * @param {object} options.mapped: An optional object indicating if the original expression
   *        entered by the users have been modified
   * @param {boolean} options.mapped.await: true if the expression was a top-level await
   *        expression that was wrapped in an async-iife
   *
   * @return {Promise}: A promise that resolves with the response.
   */
  async execute(expression, options = {}) {
    const {
      selectedObjectActor,
      selectedNodeActor,
      frameActor,
      selectedTargetFront,
    } = options;

    let targetFront = this._commands.targetCommand.targetFront;

    const selectedActor =
      selectedObjectActor || selectedNodeActor || frameActor;

    if (selectedTargetFront) {
      targetFront = selectedTargetFront;
    } else if (selectedActor) {
      const selectedFront = this._commands.client.getFrontByID(selectedActor);
      if (selectedFront) {
        targetFront = selectedFront.targetFront;
      }
    }

    const front = await targetFront.getFront("console");

    return front.evaluateJSAsync(expression, options);
  }
}

module.exports = ScriptCommand;
