/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

async function testIsFocusable(pyVar, isFocusable) {
  const result = await runPython(`${pyVar}.CurrentIsKeyboardFocusable`);
  if (isFocusable) {
    ok(result, `${pyVar} is focusable`);
  } else {
    ok(!result, `${pyVar} isn't focusable`);
  }
}

async function testHasFocus(pyVar, hasFocus) {
  const result = await runPython(`${pyVar}.CurrentHasKeyboardFocus`);
  if (hasFocus) {
    ok(result, `${pyVar} has focus`);
  } else {
    ok(!result, `${pyVar} doesn't have focus`);
  }
}

addUiaTask(
  `
<button id="button1">button1</button>
<p id="p">p</p>
<button id="button2">button2</button>
  `,
  async function (browser) {
    await definePyVar("doc", `getDocUia()`);
    await testIsFocusable("doc", true);
    await testHasFocus("doc", true);

    await assignPyVarToUiaWithId("button1");
    await testIsFocusable("button1", true);
    await testHasFocus("button1", false);
    info("Focusing button1");
    await setUpWaitForUiaEvent("AutomationFocusChanged", "button1");
    await invokeFocus(browser, "button1");
    await waitForUiaEvent();
    ok(true, "Got AutomationFocusChanged event on button1");
    await testHasFocus("button1", true);

    await assignPyVarToUiaWithId("p");
    await testIsFocusable("p", false);
    await testHasFocus("p", false);

    await assignPyVarToUiaWithId("button2");
    await testIsFocusable("button2", true);
    await testHasFocus("button2", false);
    info("Focusing button2");
    await setUpWaitForUiaEvent("AutomationFocusChanged", "button2");
    await invokeFocus(browser, "button2");
    await waitForUiaEvent();
    ok(true, "Got AutomationFocusChanged event on button2");
    await testHasFocus("button2", true);
    await testHasFocus("button1", false);
  }
);
