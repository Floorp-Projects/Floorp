/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test the creation of the viewport infobar and makes sure if resizes correctly

const TEST_URL =
  "data:text/html;charset=utf-8," +
  "<div style='position:absolute;left: 0; top: 0; " +
  "width: 20px; height: 50px'></div>";

const ID = "viewport-size-highlighter-";

var { Toolbox } = require("resource://devtools/client/framework/toolbox.js");

add_task(async function () {
  const { inspector, highlighterTestFront } = await openInspectorForURL(
    TEST_URL
  );
  const front = inspector.inspectorFront;

  const highlighter = await front.getHighlighterByType(
    "ViewportSizeHighlighter"
  );

  await isVisibleAfterShow(highlighter, inspector, highlighterTestFront);
  await hasRightLabelsContent(highlighter, highlighterTestFront);
  await resizeInspector(inspector);
  await hasRightLabelsContent(highlighter, highlighterTestFront);
  await isHiddenAfterHide(highlighter, inspector, highlighterTestFront);

  await highlighter.finalize();
});

async function isVisibleAfterShow(
  highlighterFront,
  inspector,
  highlighterTestFront
) {
  info("Checking that the viewport infobar is displayed");
  // the rulers doesn't need any node, but as highligher it seems mandatory
  // ones, so the body is given
  const body = await getNodeFront("body", inspector);
  await highlighterFront.show(body);

  const hidden = await isViewportInfobarHidden(
    highlighterFront,
    highlighterTestFront
  );
  ok(!hidden, "viewport infobar is visible after show");
}

async function isHiddenAfterHide(
  highlighterFront,
  inspector,
  highlighterTestFront
) {
  info("Checking that the viewport infobar is hidden after disabling");
  await highlighterFront.hide();

  const hidden = await isViewportInfobarHidden(
    highlighterFront,
    highlighterTestFront
  );
  ok(hidden, "viewport infobar is hidden after hide");
}

async function hasRightLabelsContent(highlighterFront, highlighterTestFront) {
  const windowDimensions = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => {
      const { require } = ChromeUtils.importESModule(
        "resource://devtools/shared/loader/Loader.sys.mjs"
      );
      const {
        getWindowDimensions,
      } = require("resource://devtools/shared/layout/utils.js");
      return getWindowDimensions(content);
    }
  );
  const windowHeight = Math.round(windowDimensions.height);
  const windowWidth = Math.round(windowDimensions.width);
  const windowText = windowWidth + "px \u00D7 " + windowHeight + "px";

  info("Wait until the rulers dimension tooltip have the proper text");
  await asyncWaitUntil(async () => {
    const dimensionText =
      await highlighterTestFront.getHighlighterNodeTextContent(
        `${ID}viewport-infobar-container`,
        highlighterFront
      );
    return dimensionText == windowText;
  }, 100);
}

async function resizeInspector(inspector) {
  info(
    "Docking the toolbox to the side of the browser to change the window size"
  );
  const toolbox = inspector.toolbox;
  await toolbox.switchHost(Toolbox.HostType.RIGHT);

  // Wait for some time to avoid measuring outdated window dimensions.
  await wait(100);
}

async function isViewportInfobarHidden(highlighterFront, highlighterTestFront) {
  const hidden = await highlighterTestFront.getHighlighterNodeAttribute(
    `${ID}viewport-infobar-container`,
    "hidden",
    highlighterFront
  );
  return hidden === "true";
}
