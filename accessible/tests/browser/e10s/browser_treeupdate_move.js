/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
/* import-globals-from ../../mochitest/states.js */
loadScripts(
  { name: "role.js", dir: MOCHITESTS_DIR },
  { name: "states.js", dir: MOCHITESTS_DIR }
);

/**
 * Test moving Accessibles:
 * 1. A moved Accessible keeps the same Accessible.
 * 2. If the moved Accessible is focused, it remains focused.
 * 3. A child of the moved Accessible also keeps the same Accessible.
 * 4. A child removed at the same time as the move gets shut down.
 */
addAccessibleTask(
  `
<div id="scrollable" role="presentation" style="height: 1px;">
  <div contenteditable id="textbox" role="textbox">
    <h1 id="heading">Heading</h1>
    <p id="para">Para</p>
  </div>
  <iframe id="iframe" src="https://example.com/"></iframe>
</div>
  `,
  async function (browser, docAcc) {
    const textbox = findAccessibleChildByID(docAcc, "textbox");
    const heading = findAccessibleChildByID(docAcc, "heading");
    const para = findAccessibleChildByID(docAcc, "para");
    const iframe = findAccessibleChildByID(docAcc, "iframe");
    const iframeDoc = iframe.firstChild;
    ok(iframeDoc, "iframe contains a document");

    let focused = waitForEvent(EVENT_FOCUS, textbox);
    textbox.takeFocus();
    await focused;
    testStates(textbox, STATE_FOCUSED, 0, 0, EXT_STATE_DEFUNCT);

    let reordered = waitForEvent(EVENT_REORDER, docAcc);
    await invokeContentTask(browser, [], () => {
      // scrollable wasn't in the a11y tree, but this will force it to be created.
      // textbox will be moved inside it.
      content.document.getElementById("scrollable").style.overflow = "scroll";
      content.document.getElementById("heading").remove();
    });
    await reordered;
    // Despite the move, ensure textbox is still alive and is focused.
    testStates(textbox, STATE_FOCUSED, 0, 0, EXT_STATE_DEFUNCT);
    // Ensure para (a child of textbox) is also still alive.
    ok(!isDefunct(para), "para is alive");
    // heading was a child of textbox, but was removed when textbox
    // was moved. Ensure it is dead.
    ok(isDefunct(heading), "heading is dead");
    // Ensure the iframe and its embedded document are alive.
    ok(!isDefunct(iframe), "iframe is alive");
    ok(!isDefunct(iframeDoc), "iframeDoc is alive");
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);

/**
 * Test that moving a subtree containing an iframe doesn't cause assertions or
 * crashes. Note that aria-owns moves Accessibles even if it is set before load.
 */
addAccessibleTask(
  `
<div id="container">
  <iframe id="iframe"></iframe>
  <div aria-owns="iframe"></div>
</div>
  `,
  async function (browser, docAcc) {
    const container = findAccessibleChildByID(docAcc, "container");
    testAccessibleTree(container, {
      SECTION: [{ SECTION: [{ INTERNAL_FRAME: [{ DOCUMENT: [] }] }] }],
    });
  },
  { topLevel: true }
);
