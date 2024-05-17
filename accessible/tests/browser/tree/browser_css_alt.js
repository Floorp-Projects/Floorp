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
<h1 id="attr" style='content: ${IMAGE} / attr(data-alt)' data-alt="replaced">attr</h1>
<h1 id="attrFallback" style='content: ${IMAGE} / attr(data-alt, "fallback")'>attrFallback</h1>
<h1 id="mixed" style='content: ${IMAGE} / "re" attr(data-alt, "fallback") attr(missing, "ed")' data-alt="plac">mixed</h1>
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
    const attr = findAccessibleChildByID(docAcc, "attr");
    testAccessibleTree(attr, {
      role: ROLE_HEADING,
      name: "replaced",
      children: [],
    });
    const attrFallback = findAccessibleChildByID(docAcc, "attrFallback");
    testAccessibleTree(attrFallback, {
      role: ROLE_HEADING,
      name: "fallback",
      children: [],
    });
    testAccessibleTree(findAccessibleChildByID(docAcc, "mixed"), {
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

    info("Changing attr data-alt");
    changed = waitForEvent(EVENT_NAME_CHANGE, attr);
    await invokeSetAttribute(browser, "attr", "data-alt", "REPLACED");
    await changed;
    testAccessibleTree(attr, {
      role: ROLE_HEADING,
      name: "REPLACED",
      children: [],
    });

    info("Changing attrFallback data-alt");
    changed = waitForEvent(EVENT_NAME_CHANGE, attrFallback);
    await invokeSetAttribute(browser, "attrFallback", "data-alt", "replaced");
    await changed;
    testAccessibleTree(attrFallback, {
      role: ROLE_HEADING,
      name: "replaced",
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
  #mixed::before {
    content: ${IMAGE} / "be" attr(data-alt);
  }
</style>
<h1 id="noAlt">noAlt</h1>
<h1 id="strings" class="strings1">inside</h1>
<h1 id="mixed" data-alt="fore">inside</h1>
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
    const mixed = findAccessibleChildByID(docAcc, "mixed");
    testAccessibleTree(mixed, {
      role: ROLE_HEADING,
      name: "before inside",
      children: [
        { role: ROLE_GRAPHIC, name: "before" },
        { role: ROLE_TEXT_LEAF, name: "inside" },
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

    info("Changing mixed data-alt");
    changed = waitForEvent(EVENT_NAME_CHANGE, mixed);
    await invokeSetAttribute(browser, "mixed", "data-alt", "FORE");
    await changed;
    testAccessibleTree(mixed, {
      role: ROLE_HEADING,
      name: "beFORE inside",
      children: [
        { role: ROLE_GRAPHIC, name: "beFORE" },
        { role: ROLE_TEXT_LEAF, name: "inside" },
      ],
    });
  },
  { chrome: true, topLevel: true }
);

/**
 * Test CSS text content in pseudo-elements.
 */
addAccessibleTask(
  `
<style>
  #noAlt::before {
    content: "before";
  }
  .strings1::before {
    content: "before" / "a" "lt";
  }
  .strings2::before {
    content: "before" / "A" "LT";
  }
  #mixed::before {
    content: "before" / "a" attr(data-alt);
  }
</style>
<h1 id="noAlt">noAlt</h1>
<h1 id="strings" class="strings1">inside</h1>
<h1 id="mixed" data-alt="lt">inside</h1>
  `,
  async function testTextPseudo(browser, docAcc) {
    testAccessibleTree(findAccessibleChildByID(docAcc, "noAlt"), {
      role: ROLE_HEADING,
      name: "beforenoAlt",
      children: [
        { role: ROLE_STATICTEXT, name: "before" },
        { role: ROLE_TEXT_LEAF, name: "noAlt" },
      ],
    });
    const strings = findAccessibleChildByID(docAcc, "strings");
    testAccessibleTree(strings, {
      role: ROLE_HEADING,
      name: "altinside",
      children: [
        { role: ROLE_STATICTEXT, name: "alt" },
        { role: ROLE_TEXT_LEAF, name: "inside" },
      ],
    });
    const mixed = findAccessibleChildByID(docAcc, "mixed");
    testAccessibleTree(mixed, {
      role: ROLE_HEADING,
      name: "altinside",
      children: [
        { role: ROLE_STATICTEXT, name: "alt" },
        { role: ROLE_TEXT_LEAF, name: "inside" },
      ],
    });

    info("Changing strings class to strings2");
    let changed = waitForEvent(EVENT_NAME_CHANGE, strings);
    await invokeSetAttribute(browser, "strings", "class", "strings2");
    await changed;
    testAccessibleTree(strings, {
      role: ROLE_HEADING,
      name: "ALTinside",
      children: [
        { role: ROLE_STATICTEXT, name: "ALT" },
        { role: ROLE_TEXT_LEAF, name: "inside" },
      ],
    });

    info("Changing mixed data-alt");
    changed = waitForEvent(EVENT_NAME_CHANGE, mixed);
    await invokeSetAttribute(browser, "mixed", "data-alt", "LT");
    await changed;
    testAccessibleTree(mixed, {
      role: ROLE_HEADING,
      name: "aLTinside",
      children: [
        { role: ROLE_STATICTEXT, name: "aLT" },
        { role: ROLE_TEXT_LEAF, name: "inside" },
      ],
    });
  },
  { chrome: true, topLevel: true }
);
