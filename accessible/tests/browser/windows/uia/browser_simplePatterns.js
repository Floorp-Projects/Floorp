/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test the Invoke pattern.
 */
addUiaTask(
  `
<button id="button">button</button>
<p id="p">p</p>
  `,
  async function testInvoke() {
    await definePyVar("doc", `getDocUia()`);
    await assignPyVarToUiaWithId("button");
    await definePyVar("pattern", `getUiaPattern(button, "Invoke")`);
    ok(await runPython(`bool(pattern)`), "button has Invoke pattern");
    info("Calling Invoke on button");
    // The button will get focus when it is clicked.
    let focused = waitForEvent(EVENT_FOCUS, "button");
    // The UIA -> IA2 proxy doesn't fire the Invoked event.
    if (gIsUiaEnabled) {
      await setUpWaitForUiaEvent("Invoke_Invoked", "button");
    }
    await runPython(`pattern.Invoke()`);
    await focused;
    ok(true, "button got focus");
    if (gIsUiaEnabled) {
      await waitForUiaEvent();
      ok(true, "button got Invoked event");
    }

    let hasPattern = await runPython(`
      p = findUiaByDomId(doc, "p")
      return bool(getUiaPattern(p, "Invoke"))
    `);
    ok(!hasPattern, "p doesn't have Invoke pattern");
  }
);
