/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test the Name property.
 */
addUiaTask(
  `
<button id="button">before</button>
<div id="div">div</div>
  `,
  async function testName(browser) {
    await definePyVar("doc", `getDocUia()`);
    await assignPyVarToUiaWithId("button");
    is(
      await runPython(`button.CurrentName`),
      "before",
      "button has correct name"
    );
    await assignPyVarToUiaWithId("div");
    is(await runPython(`div.CurrentName`), "", "div has no name");

    info("Setting aria-label on button");
    await setUpWaitForUiaPropEvent("Name", "button");
    await invokeSetAttribute(browser, "button", "aria-label", "after");
    await waitForUiaEvent();
    ok(true, "Got Name prop change event on button");
    is(
      await runPython(`button.CurrentName`),
      "after",
      "button has correct name"
    );
  }
);
