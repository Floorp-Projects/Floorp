/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_screenshot_node.js */

"use strict";

loadHelperScript("helper_screenshot_node.js");

const TEST_URL = `data:text/html;charset=utf8,
  <iframe
    src="data:text/html;charset=utf8,
      <div style='width:30px;height:30px;background:rgb(255, 0, 0)'></div>"></iframe>`;

// Test that the "Screenshot Node" feature works with a node inside an iframe.
add_task(async function() {
  const { inspector, toolbox } = await openInspectorForURL(encodeURI(TEST_URL));

  info("Select the red node");
  const redNode = await getNodeFrontInFrame("div", "iframe", inspector);
  await selectNode(redNode, inspector);

  info("Take a screenshot of the red node and verify it looks as expected");
  const redScreenshot = await takeNodeScreenshot(inspector);
  await assertSingleColorScreenshotImage(redScreenshot, 30, 30, {
    r: 255,
    g: 0,
    b: 0,
  });

  await toolbox.destroy();
});
