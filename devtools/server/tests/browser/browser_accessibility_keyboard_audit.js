/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Checks functionality around text label audit for the AccessibleActor.
 */

const {
  accessibility: {
    AUDIT_TYPE: { KEYBOARD },
    SCORES: { FAIL, WARNING },
    ISSUE_TYPE: {
      [KEYBOARD]: {
        FOCUSABLE_NO_SEMANTICS,
        FOCUSABLE_POSITIVE_TABINDEX,
        INTERACTIVE_NO_ACTION,
        INTERACTIVE_NOT_FOCUSABLE,
        NO_FOCUS_VISIBLE,
      },
    },
  },
} = require("devtools/shared/constants");

add_task(async function() {
  const { target, walker, accessibility } = await initAccessibilityFrontForUrl(
    `${MAIN_DOMAIN}doc_accessibility_keyboard_audit.html`
  );

  const a11yWalker = await accessibility.getWalker();
  await accessibility.enable();

  const tests = [
    [
      "Focusable element (styled button) with no semantics.",
      "#button-1",
      { score: WARNING, issue: FOCUSABLE_NO_SEMANTICS },
    ],
    ["Element (styled button) with no semantics.", "#button-2", null],
    [
      "Container element for out of order focusable element.",
      "#input-container",
      null,
    ],
    [
      "Interactive element with focus out of order (-1).",
      "#input-1",
      {
        score: FAIL,
        issue: INTERACTIVE_NOT_FOCUSABLE,
      },
    ],
    [
      "Interactive element with focus out of order (-1) when disabled.",
      "#input-2",
      null,
    ],
    ["Interactive element when disabled.", "#input-3", null],
    ["Focusable interactive element.", "#input-4", null],
    [
      "Interactive accesible (link with no attributes) with no accessible actions.",
      "#link-1",
      {
        score: FAIL,
        issue: INTERACTIVE_NO_ACTION,
      },
    ],
    ["Interactive accessible (link with valid hred).", "#link-2", null],
    ["Interactive accessible with no tabindex.", "#button-3", null],
    [
      "Interactive accessible with -1 tabindex.",
      "#button-4",
      {
        score: FAIL,
        issue: INTERACTIVE_NOT_FOCUSABLE,
      },
    ],
    ["Interactive accessible with 0 tabindex.", "#button-5", null],
    [
      "Interactive accessible with 1 tabindex.",
      "#button-6",
      { score: WARNING, issue: FOCUSABLE_POSITIVE_TABINDEX },
    ],
    [
      "Focusable ARIA button with no focus styling.",
      "#focusable-1",
      { score: WARNING, issue: NO_FOCUS_VISIBLE },
    ],
    ["Focusable ARIA button with focus styling.", "#focusable-2", null],
    ["Focusable ARIA button with browser styling.", "#focusable-3", null],
  ];

  for (const [description, selector, expected] of tests) {
    info(description);
    const node = await walker.querySelector(walker.rootNode, selector);
    const front = await a11yWalker.getAccessibleFor(node);
    const audit = await front.audit({ types: [KEYBOARD] });
    Assert.deepEqual(
      audit[KEYBOARD],
      expected,
      `Audit result for ${selector} is correct.`
    );
  }

  await accessibility.disable();
  await waitForA11yShutdown();
  await target.destroy();
  gBrowser.removeCurrentTab();
});
