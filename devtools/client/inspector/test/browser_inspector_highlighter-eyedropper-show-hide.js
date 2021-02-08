/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test the basic structure of the eye-dropper highlighter.

add_task(async function() {
  const { inspector, testActor } = await openInspectorForURL(
    "data:text/html;charset=utf-8,eye-dropper test"
  );
  const inspectorFrontActorID = inspector.inspectorFront.actorID;

  info("Checking that the eyedropper is hidden by default");
  const eyeDropperVisible = await testActor.isEyeDropperVisible(
    inspectorFrontActorID
  );
  is(eyeDropperVisible, false, "The eyedropper is hidden by default");

  const toggleButton = inspector.panelDoc.querySelector(
    "#inspector-eyedropper-toggle"
  );

  info("Display the eyedropper by clicking on the inspector toolbar button");
  toggleButton.click();
  await TestUtils.waitForCondition(() =>
    testActor.isEyeDropperVisible(inspectorFrontActorID)
  );
  ok(true, "Eye dropper is visible after clicking the button in the inspector");

  const style = await testActor.getEyeDropperElementAttribute(
    inspectorFrontActorID,
    "root",
    "style"
  );
  is(style, "top:100px;left:100px;", "The eyedropper is correctly positioned");

  info("Hide the eyedropper by clicking on the inspector toolbar button again");
  toggleButton.click();
  await TestUtils.waitForCondition(() =>
    testActor
      .isEyeDropperVisible(inspectorFrontActorID)
      .then(visible => !visible)
  );
  ok(true, "Eye dropper is not visible anymore");
});
