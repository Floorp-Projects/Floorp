/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test the creation of the viewport infobar and makes sure if resizes correctly

const TEST_URL =
  "data:text/html;charset=utf-8," +
  "<div style='position:absolute;left: 0; top: 0; " +
  "width: 20px; height: 50px'></div>";

const ID = "rulers-highlighter-";

var { Toolbox } = require("devtools/client/framework/toolbox");

add_task(async function() {
  const { inspector, testActor } = await openInspectorForURL(TEST_URL);
  const front = inspector.inspectorFront;

  const highlighter = await front.getHighlighterByType("RulersHighlighter");

  await isVisibleAfterShow(highlighter, inspector, testActor);
  await hasRightLabelsContent(highlighter, inspector, testActor);
  await resizeInspector(highlighter, inspector, testActor);
  await hasRightLabelsContent(highlighter, inspector, testActor);
  await isHiddenAfterHide(highlighter, inspector, testActor);

  await highlighter.finalize();
});

async function isVisibleAfterShow(highlighterFront, inspector, testActor) {
  info("Checking that the viewport infobar is displayed");
  // the rulers doesn't need any node, but as highligher it seems mandatory
  // ones, so the body is given
  const body = await getNodeFront("body", inspector);
  await highlighterFront.show(body);

  const hidden = await isViewportInfobarHidden(highlighterFront, testActor);
  ok(!hidden, "viewport infobar is visible after show");
}

async function isHiddenAfterHide(highlighterFront, inspector, testActor) {
  info("Checking that the viewport infobar is hidden after disabling");
  await highlighterFront.hide();

  const hidden = await isViewportInfobarHidden(highlighterFront, testActor);
  ok(hidden, "viewport infobar is hidden after hide");
}

async function hasRightLabelsContent(highlighterFront, inspector, testActor) {
  const windowDimensions = await testActor.getWindowDimensions();
  const windowHeight = Math.round(windowDimensions.height);
  const windowWidth = Math.round(windowDimensions.width);
  const windowText = windowWidth + "px \u00D7 " + windowHeight + "px";

  info("Wait until the rulers dimension tooltip have the proper text");
  await asyncWaitUntil(async () => {
    const dimensionText = await testActor.getHighlighterNodeTextContent(
      `${ID}viewport-infobar-container`,
      highlighterFront
    );
    return dimensionText == windowText;
  }, 100);
}

async function resizeInspector(highlighterFront, inspector, testActor) {
  info(
    "Docking the toolbox to the side of the browser to change the window size"
  );
  const toolbox = inspector.toolbox;
  await toolbox.switchHost(Toolbox.HostType.RIGHT);

  // Wait for some time to avoid measuring outdated window dimensions.
  await wait(100);
}

async function isViewportInfobarHidden(highlighterFront, testActor) {
  const hidden = await testActor.getHighlighterNodeAttribute(
    `${ID}viewport-infobar-container`,
    "hidden",
    highlighterFront
  );
  return hidden === "true";
}
