/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

/**
 * Test changing the left/top CSS properties.
 */
addAccessibleTask(
  `
<div id="div" style="position: relative; left: 0px; top: 0px; width: fit-content;">
  test
</div>
  `,
  async function (browser, docAcc) {
    await testBoundsWithContent(docAcc, "div", browser);
    info("Changing left");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("div").style.left = "200px";
    });
    await waitForContentPaint(browser);
    await testBoundsWithContent(docAcc, "div", browser);
    info("Changing top");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("div").style.top = "200px";
    });
    await waitForContentPaint(browser);
    await testBoundsWithContent(docAcc, "div", browser);
  },
  { chrome: true, topLevel: true, iframe: true }
);

/**
 * Test moving one element by inserting a second element before it such that the
 * second element doesn't reflow.
 */
addAccessibleTask(
  `
<div id="reflowContainer">
  <p>1</p>
  <p id="reflow2" hidden>2</p>
  <p id="reflow3">3</p>
</div>
<p id="noReflow">noReflow</p>
  `,
  async function (browser, docAcc) {
    for (const id of ["reflowContainer", "reflow3", "noReflow"]) {
      await testBoundsWithContent(docAcc, id, browser);
    }
    // Show p2, which will reflow everything inside "reflowContainer", but just
    // move "noReflow" down without reflowing it.
    info("Showing p2");
    let shown = waitForEvent(EVENT_SHOW, "reflow2");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("reflow2").hidden = false;
    });
    await waitForContentPaint(browser);
    await shown;
    for (const id of ["reflowContainer", "reflow2", "reflow3", "noReflow"]) {
      await testBoundsWithContent(docAcc, id, browser);
    }
  },
  { chrome: true, topLevel: true, remoteIframe: true }
);

/**
 * Test bounds when an Accessible is re-parented.
 */
addAccessibleTask(
  `
<div id="container">
  <p style="height: 300px;">1</p>
  <div class="pParent">
    <p id="p2">2</p>
  </div>
</div>
  `,
  async function (browser, docAcc) {
    const paraTree = { PARAGRAPH: [{ TEXT_LEAF: [] }] };
    const container = findAccessibleChildByID(docAcc, "container");
    testAccessibleTree(container, { SECTION: [paraTree, paraTree] });
    await testBoundsWithContent(docAcc, "p2", browser);
    // Add a click listener to the div containing p2. This will cause an
    // Accessible to be created for that div, which didn't have one before.
    info("Adding click listener to pParent");
    let reordered = waitForEvent(EVENT_REORDER, container);
    await invokeContentTask(browser, [], () => {
      content.document
        .querySelector(".pParent")
        .addEventListener("click", function () {});
    });
    await reordered;
    testAccessibleTree(container, {
      SECTION: [paraTree, { SECTION: [paraTree] }],
    });
    await testBoundsWithContent(docAcc, "p2", browser);
  },
  { chrome: true, topLevel: true, remoteIframe: true }
);

/**
 * Test the bounds of items in an inline list with content that offsets the
 * origin of the list's bounding box (creating an IB split within the UL frame).
 */
addAccessibleTask(
  `
  <style>
    ul,li {
      display:inline;
      list-style-type:none;
      list-style-position:inside;
      margin:0;
      padding:0;
    }
    </style>
    <div id="container" style="background:green; max-width: 400px;">List of information: <ul id="list"><li id="one">item one</li> | <li id="two">item two</li> | <li id="three">item three</li> | <li id="four">item four</li> | <li id="five">item five</li></ul></div>
  `,
  async function (browser, docAcc) {
    await testBoundsWithContent(docAcc, "list", browser);
    await testBoundsWithContent(docAcc, "one", browser);
    await testBoundsWithContent(docAcc, "two", browser);
    await testBoundsWithContent(docAcc, "three", browser);
    await testBoundsWithContent(docAcc, "four", browser);
    await testBoundsWithContent(docAcc, "five", browser);
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
  }
);
