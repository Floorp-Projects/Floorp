/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

const IMAGE =
  'url("https://example.com/a11y/accessible/tests/mochitest/moz.png")';

/**
 * Test CSS content replacing an element.
 */
addAccessibleTask(
  `
<style>
  .twoStrings1 {
    content: ${IMAGE} / "re" "placed";
  }
  .twoStrings2 {
    content: ${IMAGE} / "RE" "PLACED";
  }
</style>
<h1 id="noAlt" style='content: ${IMAGE};'>noAlt</h1>
<h1 id="oneString" style='content: ${IMAGE} / "replaced";'>oneString</h1>
<h1 id="twoStrings" class="twoStrings1">twoStrings</h1>
  `,
  async function testReplacing(browser, docAcc) {
    testAccessibleTree(findAccessibleChildByID(docAcc, "noAlt"), {
      role: ROLE_HEADING,
      children: [],
    });
    const oneString = findAccessibleChildByID(docAcc, "oneString");
    testAccessibleTree(oneString, {
      role: ROLE_HEADING,
      name: "replaced",
      children: [],
    });
    const twoStrings = findAccessibleChildByID(docAcc, "twoStrings");
    testAccessibleTree(twoStrings, {
      role: ROLE_HEADING,
      name: "replaced",
      children: [],
    });

    info("Changing oneString content style");
    let changed = waitForEvent(EVENT_NAME_CHANGE, oneString);
    await invokeSetStyle(
      browser,
      "oneString",
      "content",
      `${IMAGE} / "REPLACED"`
    );
    await changed;
    testAccessibleTree(oneString, {
      role: ROLE_HEADING,
      name: "REPLACED",
      children: [],
    });

    info("Changing twoStrings class to twoStrings2");
    changed = waitForEvent(EVENT_NAME_CHANGE, twoStrings);
    await invokeSetAttribute(browser, "twoStrings", "class", "twoStrings2");
    await changed;
    testAccessibleTree(twoStrings, {
      role: ROLE_HEADING,
      name: "REPLACED",
      children: [],
    });
  },
  { chrome: true, topLevel: true }
);

/**
 * Test CSS image content in pseudo-elements.
 */
addAccessibleTask(
  `
<style>
  #noAlt::before, #noAlt::after {
    content: ${IMAGE};
  }
  .strings1::before {
    content: ${IMAGE} / "be" "fore";
  }
  .strings2::before {
    content: ${IMAGE} / "BE" "FORE";
  }
  #strings::after {
    content: ${IMAGE} / "af" "ter";
  }
</style>
<h1 id="noAlt">noAlt</h1>
<h1 id="strings" class="strings1">inside</h1>
  `,
  async function testImagePseudo(browser, docAcc) {
    testAccessibleTree(findAccessibleChildByID(docAcc, "noAlt"), {
      role: ROLE_HEADING,
      children: [{ role: ROLE_TEXT_LEAF, name: "noAlt" }],
    });
    const strings = findAccessibleChildByID(docAcc, "strings");
    testAccessibleTree(strings, {
      role: ROLE_HEADING,
      name: "before inside after",
      children: [
        { role: ROLE_GRAPHIC, name: "before" },
        { role: ROLE_TEXT_LEAF, name: "inside" },
        { role: ROLE_GRAPHIC, name: "after" },
      ],
    });

    info("Changing strings class to strings2");
    let changed = waitForEvent(EVENT_NAME_CHANGE, strings);
    await invokeSetAttribute(browser, "strings", "class", "strings2");
    await changed;
    testAccessibleTree(strings, {
      role: ROLE_HEADING,
      name: "BEFORE inside after",
      children: [
        { role: ROLE_GRAPHIC, name: "BEFORE" },
        { role: ROLE_TEXT_LEAF, name: "inside" },
        { role: ROLE_GRAPHIC, name: "after" },
      ],
    });
  },
  { chrome: true, topLevel: true }
);
