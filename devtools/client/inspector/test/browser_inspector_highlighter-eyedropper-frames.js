/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test that the eyedropper works on a page with iframes.

const TOP_LEVEL_BACKGROUND_COLOR = "#ff0000";
const SAME_ORIGIN_FRAME_BACKGROUND_COLOR = "#00ee00";
const REMOTE_FRAME_BACKGROUND_COLOR = "#0000dd";

const HTML = `
<style>
  body {
    height: 100vh;
    background: ${TOP_LEVEL_BACKGROUND_COLOR};
    margin: 0;
  }

  div, iframe {
    border: none;
    display: block;
    height: 100px;
    text-align: center;
  }
</style>
<div>top-level element</div>
<iframe src="http://example.com/document-builder.sjs?html=<style>body {background:${encodeURIComponent(
  SAME_ORIGIN_FRAME_BACKGROUND_COLOR
)};text-align: center;}</style><body>same origin iframe</body>"></iframe>
<iframe src="http://example.org/document-builder.sjs?html=<style>body {background:${encodeURIComponent(
  REMOTE_FRAME_BACKGROUND_COLOR
)};text-align: center;}</style><body>remote iframe</body>"></iframe>
`;
const TEST_URI = `http://example.com/document-builder.sjs?html=${encodeURIComponent(
  HTML
)}`;

add_task(async function() {
  const { inspector, testActor } = await openInspectorForURL(TEST_URI);
  const inspectorFrontActorID = inspector.inspectorFront.actorID;

  const toggleButton = inspector.panelDoc.querySelector(
    "#inspector-eyedropper-toggle"
  );
  toggleButton.click();
  await TestUtils.waitForCondition(() =>
    testActor.isEyeDropperVisible(inspectorFrontActorID)
  );

  ok(true, "Eye dropper is visible");

  const checkColorAt = (...args) =>
    checkEyeDropperColorAt(testActor, inspectorFrontActorID, ...args);

  //   The content page has the following layout:
  //
  //  +------------------------------------+
  //  |   top level div (#ff0000)          | 100px
  //  +------------------------------------+
  //  |   same origin iframe (#00ee00)     | 100px
  //  +------------------------------------+
  //  |   remote iframe (#0000dd)          | 100px
  //  +------------------------------------+

  await checkColorAt(
    50,
    50,
    TOP_LEVEL_BACKGROUND_COLOR,
    "The eyedropper holds the expected color for the top-level element"
  );

  await checkColorAt(
    50,
    150,
    SAME_ORIGIN_FRAME_BACKGROUND_COLOR,
    "The eyedropper holds the expected color for the same-origin iframe"
  );

  await checkColorAt(
    50,
    250,
    REMOTE_FRAME_BACKGROUND_COLOR,
    "The eyedropper holds the expected color for the remote iframe"
  );

  info("Hide the eyedropper");
  toggleButton.click();
  await TestUtils.waitForCondition(() =>
    testActor
      .isEyeDropperVisible(inspectorFrontActorID)
      .then(visible => !visible)
  );
});

/**
 * Move the mouse on the content page at the x,y position and check the color displayed
 * in the eyedropper label.
 *
 * @param {TestActorFront} testActorFront
 * @param {String} inspectorActorID: The inspector actorID we'll use to retrieve the eyedropper
 * @param {Number} x
 * @param {Number} y
 * @param {String} expectedColor: Hexa string of the expected color
 * @param {String} assertionDescription
 */
async function checkEyeDropperColorAt(
  testActorFront,
  inspectorActorID,
  x,
  y,
  expectedColor,
  assertionDescription
) {
  info(`Move mouse to ${x},${y}`);
  await testActorFront.synthesizeMouse({
    selector: ":root",
    x,
    y,
    options: { type: "mousemove" },
  });

  const colorValue = await testActorFront.getEyeDropperColorValue(
    inspectorActorID
  );
  is(colorValue, expectedColor, assertionDescription);
}
