/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test that the eye dropper works as expected when the page is zoomed-in.

const COLOR_1 = "#aa0000";
const COLOR_2 = "#0bb000";
const COLOR_3 = "#00cc00";
const COLOR_4 = "#000dd0";

const HTML = `
<style>
  body {
    margin: 0;
  }
  div {
    height: 50px;
    width: 50px;
  }
</style>
<div style="background-color: ${COLOR_1};"></div>
<div style="background-color: ${COLOR_2};"></div>
<div style="background-color: ${COLOR_3};"></div>
<div style="background-color: ${COLOR_4};"></div>`;
const TEST_URI = `http://example.com/document-builder.sjs?html=${encodeURIComponent(
  HTML
)}`;

add_task(async function () {
  const { inspector, highlighterTestFront } = await openInspectorForURL(
    TEST_URI
  );

  info("Zoom in the page");
  setContentPageZoomLevel(2);

  const toggleButton = inspector.panelDoc.querySelector(
    "#inspector-eyedropper-toggle"
  );
  toggleButton.click();
  await TestUtils.waitForCondition(() =>
    highlighterTestFront.isEyeDropperVisible()
  );

  ok(true, "Eye dropper is visible");

  const checkColorAt = (...args) =>
    checkEyeDropperColorAt(highlighterTestFront, ...args);

  // ⚠️ Note that we need to check the regular position, not the zoomed-in ones.

  await checkColorAt(
    25,
    10,
    COLOR_1,
    "The eyedropper holds the expected color for the first div"
  );

  await checkColorAt(
    25,
    60,
    COLOR_2,
    "The eyedropper holds the expected color for the second div"
  );

  await checkColorAt(
    25,
    110,
    COLOR_3,
    "The eyedropper holds the expected color for the third div"
  );

  await checkColorAt(
    25,
    160,
    COLOR_4,
    "The eyedropper holds the expected color for the fourth div"
  );

  info("Hide the eyedropper");
  toggleButton.click();
  await TestUtils.waitForCondition(async () => {
    const visible = await highlighterTestFront.isEyeDropperVisible();
    return !visible;
  });
  setContentPageZoomLevel(1);
});
