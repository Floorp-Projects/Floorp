/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/states.js */
loadScripts(
  { name: "role.js", dir: MOCHITESTS_DIR },
  { name: "states.js", dir: MOCHITESTS_DIR }
);

addAccessibleTask(
  `<input id="textbox" value="hello"/>`,
  async function(browser, fissionDocAcc, contentDocAcc) {
    const textbox = findAccessibleChildByID(fissionDocAcc, "textbox");
    testStates(textbox, STATE_FOCUSABLE, 0, STATE_FOCUSED);

    let onFocus = waitForEvent(EVENT_FOCUS, textbox);
    textbox.takeFocus();
    await onFocus;

    testStates(textbox, STATE_FOCUSABLE | STATE_FOCUSED, 0);
  },
  { topLevel: false, iframe: true }
);
