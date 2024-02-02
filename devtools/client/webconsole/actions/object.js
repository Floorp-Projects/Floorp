/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyServiceGetter(
  this,
  "clipboardHelper",
  "@mozilla.org/widget/clipboardhelper;1",
  "nsIClipboardHelper"
);

function storeAsGlobal(actor) {
  return async ({ commands, hud }) => {
    const evalString = `{ let i = 0;
      while (this.hasOwnProperty("temp" + i) && i < 1000) {
        i++;
      }
      this["temp" + i] = _self;
      "temp" + i;
    }`;

    const res = await commands.scriptCommand.execute(evalString, {
      selectedObjectActor: actor,
      // Prevent any type of breakpoint when evaluating this code
      disableBreaks: true,
      // Ensure always overriding "$0" and "_self" console command, even if the page implements a variable with the same name.
      preferConsoleCommandsOverLocalSymbols: true,
    });

    // Select the adhoc target in the console.
    if (hud.toolbox) {
      const objectFront = commands.client.getFrontByID(actor);
      if (objectFront) {
        const targetActorID = objectFront.targetFront?.actorID;
        if (targetActorID) {
          hud.toolbox.selectTarget(targetActorID);
        }
      }
    }

    hud.focusInput();
    hud.setInputValue(res.result);
  };
}

function copyMessageObject(actor, variableText) {
  return async ({ commands }) => {
    if (actor) {
      // The Debugger.Object of the OA will be bound to |_self| during evaluation.
      // See server/actors/webconsole/eval-with-debugger.js `evalWithDebugger`.
      const res = await commands.scriptCommand.execute("copy(_self)", {
        selectedObjectActor: actor,
        // Prevent any type of breakpoint when evaluating this code
        disableBreaks: true,
        // ensure always overriding "$0" and "_self" console command, even if the page implements a variable with the same name.
        preferConsoleCommandsOverLocalSymbols: true,
      });

      clipboardHelper.copyString(res.helperResult.value);
    } else {
      clipboardHelper.copyString(variableText);
    }
  };
}

module.exports = {
  storeAsGlobal,
  copyMessageObject,
};
