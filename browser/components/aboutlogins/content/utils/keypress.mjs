/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { withSimpleController } from "./controllers.mjs";

const useKeyEvent =
  keyEventName => (target, keyCombination, callback, options) => {
    const parts = keyCombination.split("+");
    const keys = parts.map(part => part.toLowerCase());
    const modifiers = ["ctrl", "alt", "shift", "meta"];

    const handleKeyEvent = event => {
      const isModifierCorrect = modifiers.every(
        modifier => keys.includes(modifier) === event[`${modifier}Key`]
      );

      const actualKey = keys.find(key => !modifiers.includes(key));

      // We check the code value rather than key since key value
      // can be missleading without prevent default e.g. option + N = ñ
      const isKeyCorrect =
        event.code.toLowerCase() === `key${actualKey}`.toLowerCase() ||
        event.code.toLowerCase() === actualKey.toLowerCase();

      if (isModifierCorrect && isKeyCorrect) {
        if (options?.preventDefault) {
          event.preventDefault();
        }
        callback?.(event);
      }
    };

    target.addEventListener(keyEventName, handleKeyEvent);

    return () => {
      target.removeEventListener(keyEventName, handleKeyEvent);
    };
  };

export const handleKeyPress = (host, ...args) =>
  new (withSimpleController(host, useKeyEvent("keydown"), ...args))();
