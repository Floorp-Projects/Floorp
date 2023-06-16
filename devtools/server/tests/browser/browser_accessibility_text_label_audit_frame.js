/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Checks functionality around text label audit for the AccessibleActor that is
 * created for frame elements.
 */

const {
  accessibility: {
    AUDIT_TYPE: { TEXT_LABEL },
    SCORES: { FAIL },
    ISSUE_TYPE: {
      [TEXT_LABEL]: { FRAME_NO_NAME },
    },
  },
} = require("resource://devtools/shared/constants.js");

add_task(async function () {
  const { target, walker, a11yWalker, parentAccessibility } =
    await initAccessibilityFrontsForUrl(
      `${MAIN_DOMAIN}doc_accessibility_text_label_audit_frame.html`
    );

  const tests = [
    ["Frame with no name", "#frame-1", { score: FAIL, issue: FRAME_NO_NAME }],
    ["Frame with aria-label", "#frame-2", null],
  ];

  for (const [description, selector, expected] of tests) {
    info(description);
    const node = await walker.querySelector(walker.rootNode, selector);
    const front = await a11yWalker.getAccessibleFor(node);
    const audit = await front.audit({ types: [TEXT_LABEL] });
    Assert.deepEqual(
      audit[TEXT_LABEL],
      expected,
      `Audit result for ${selector} is correct.`
    );
  }

  await waitForA11yShutdown(parentAccessibility);
  await target.destroy();
  gBrowser.removeCurrentTab();
});
