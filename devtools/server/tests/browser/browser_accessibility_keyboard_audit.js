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
        MOUSE_INTERACTIVE_ONLY,
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
    ["Interactive accessible (link with valid href).", "#link-2", null],
    ["Interactive accessible (link with # as href).", "#link-3", null],
    [
      "Interactive accessible (link with empty string as href).",
      "#link-4",
      null,
    ],
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
    [
      "Not focusable, non-semantic element that has a click handler.",
      "#mouse-only-1",
      { score: FAIL, issue: MOUSE_INTERACTIVE_ONLY },
    ],
    [
      "Focusable, non-semantic element that has a click handler.",
      "#focusable-4",
      { score: WARNING, issue: FOCUSABLE_NO_SEMANTICS },
    ],
    [
      "Not focusable, ARIA button that has a click handler.",
      "#button-7",
      { score: FAIL, issue: INTERACTIVE_NOT_FOCUSABLE },
    ],
    ["Focusable, ARIA button with a click handler.", "#button-8", null],
    ["Regular image, no keyboard checks should flag an issue.", "#img-1", null],
    [
      "Image with a longdesc (accessible will have showlongdesc action).",
      "#img-2",
      null,
    ],
    [
      "Clickable image with a longdesc (accessible will have click and showlongdesc actions).",
      "#img-3",
      { score: FAIL, issue: MOUSE_INTERACTIVE_ONLY },
    ],
    [
      "Clickable image (accessible will have click action).",
      "#img-4",
      { score: FAIL, issue: MOUSE_INTERACTIVE_ONLY },
    ],
    ["Focusable button with aria-haspopup.", "#buttonmenu-1", null],
    [
      "Not focusable aria button with aria-haspopup.",
      "#buttonmenu-2",
      {
        score: FAIL,
        issue: INTERACTIVE_NOT_FOCUSABLE,
      },
    ],
    ["Focusable checkbox.", "#checkbox-1", null],
    ["Focusable select element size > 1", "#listbox-1", null],
    ["Focusable select element with one option", "#combobox-1", null],
    ["Focusable select element with no options", "#combobox-2", null],
    ["Focusable select element with two options", "#combobox-3", null],
    [
      "Non-focusable aria combobox with one aria option.",
      "#editcombobox-1",
      null,
    ],
    ["Non-focusable aria combobox with no options.", "#editcombobox-2", null],
    ["Focusable aria combobox with no options.", "#editcombobox-3", null],
    [
      "Non-focusable aria switch",
      "#switch-1",
      {
        score: FAIL,
        issue: INTERACTIVE_NOT_FOCUSABLE,
      },
    ],
    ["Focusable aria switch", "#switch-2", null],
    [
      "Combobox list that is visible (has focusable state)",
      "#owned_listbox",
      null,
    ],
    [
      "Mouse interactive, label that contains form element (linked)",
      "#label-1",
      null,
    ],
    ["Mouse interactive label for external element (linked)", "#label-2", null],
    ["Not interactive unlinked label", "#label-3", null],
    [
      "Not interactive unlinked label with folloing form element",
      "#label-4",
      null,
    ],
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

  info("Text leaf inside a link (jump action is propagated to the text link)");
  let node = await walker.querySelector(walker.rootNode, "#link-5");
  let parent = await a11yWalker.getAccessibleFor(node);
  let front = (await parent.children())[0];
  let audit = await front.audit({ types: [KEYBOARD] });
  Assert.deepEqual(
    audit[KEYBOARD],
    null,
    "Text leafs are excluded from semantics rule."
  );

  info("Combobox list that is invisible");
  node = await walker.querySelector(walker.rootNode, "#combobox-1");
  parent = await a11yWalker.getAccessibleFor(node);
  front = (await parent.children())[0];
  audit = await front.audit({ types: [KEYBOARD] });
  Assert.deepEqual(
    audit[KEYBOARD],
    null,
    "Combobox lists (invisible) are excluded from semantics rule."
  );

  await accessibility.disable();
  await waitForA11yShutdown();
  await target.destroy();
  gBrowser.removeCurrentTab();
});
