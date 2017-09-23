/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test the creation of the viewport infobar and makes sure if resizes correctly

const TEST_URL = "data:text/html;charset=utf-8," +
                 "<div style='position:absolute;left: 0; top: 0; " +
                 "width: 20px; height: 50px'></div>";

const ID = "rulers-highlighter-";

var {Toolbox} = require("devtools/client/framework/toolbox");

add_task(function* () {
  let { inspector, testActor } = yield openInspectorForURL(TEST_URL);
  let front = inspector.inspector;

  let highlighter = yield front.getHighlighterByType("RulersHighlighter");

  yield isShown(highlighter, inspector, testActor);
  yield hasRightLabelsContent(highlighter, inspector, testActor);
  yield resizeInspector(highlighter, inspector, testActor);
  yield hasRightLabelsContent(highlighter, inspector, testActor);

  yield highlighter.finalize();
});

function* isShown(highlighterFront, inspector, testActor) {
  info("Checking that the viewport infobar is displayed");
  // the rulers doesn't need any node, but as highligher it seems mandatory
  // ones, so the body is given
  let body = yield getNodeFront("body", inspector);
  yield highlighterFront.show(body);

  let hidden = yield testActor.getHighlighterNodeAttribute(
    `${ID}viewport-infobar-container`, "hidden", highlighterFront);

  isnot(hidden, "true", "viewport infobar is visible after show");
}

function* hasRightLabelsContent(highlighterFront, inspector, testActor) {
  info("Checking the rulers dimension tooltip have the proper text");

  let dimensionText = yield testActor.getHighlighterNodeTextContent(
    `${ID}viewport-infobar-container`, highlighterFront);

  let windowDimensions = yield testActor.getWindowDimensions();
  let windowHeight = Math.round(windowDimensions.height);
  let windowWidth = Math.round(windowDimensions.width);
  let windowText = windowHeight + "px \u00D7 " + windowWidth + "px";

  is(dimensionText, windowText, "Dimension text was created successfully");
}

function* resizeInspector(highlighterFront, inspector, testActor) {
  info("Docking the toolbox to the side of the browser to change the window size");
  let toolbox = inspector.toolbox;
  yield toolbox.switchHost(Toolbox.HostType.SIDE);
}
