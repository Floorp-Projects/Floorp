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
  return async ({ client, hud }) => {
    const evalString = `{ let i = 0;
      while (this.hasOwnProperty("temp" + i) && i < 1000) {
        i++;
      }
      this["temp" + i] = _self;
      "temp" + i;
    }`;

    const res = await client.evaluateJSAsync(evalString, {
      selectedObjectActor: actor,
    });

    // Select the adhoc target in the console.
    if (hud.toolbox) {
      const objectFront = client.getFrontByID(actor);
      if (objectFront) {
        const threadActorID = objectFront.targetFront?.threadFront?.actorID;
        if (threadActorID) {
          hud.toolbox.selectThread(threadActorID);
        }
      }
    }

    hud.focusInput();
    hud.setInputValue(res.result);
  };
}

function copyMessageObject(actor, variableText) {
  return async ({ client }) => {
    if (actor) {
      // The Debugger.Object of the OA will be bound to |_self| during evaluation.
      // See server/actors/webconsole/eval-with-debugger.js `evalWithDebugger`.
      const res = await client.evaluateJSAsync("copy(_self)", {
        selectedObjectActor: actor,
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
