/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test that the eyedropper opens correctly even when the page defines CSP headers.

const TEST_URI = URL_ROOT + "doc_inspector_csp.html";

add_task(async function () {
  const { inspector, highlighterTestFront } = await openInspectorForURL(
    TEST_URI
  );

  const toggleButton = inspector.panelDoc.querySelector(
    "#inspector-eyedropper-toggle"
  );
  toggleButton.click();
  await TestUtils.waitForCondition(() =>
    highlighterTestFront.isEyeDropperVisible()
  );

  ok(true, "Eye dropper is visible");

  await checkEyeDropperColorAt(
    highlighterTestFront,
    5,
    5,
    "#ff0000",
    "The eyedropper holds the expected color for the top-level element"
  );

  info("Hide the eyedropper");
  toggleButton.click();
  await TestUtils.waitForCondition(async () => {
    const visible = await highlighterTestFront.isEyeDropperVisible();
    return !visible;
  });
});
