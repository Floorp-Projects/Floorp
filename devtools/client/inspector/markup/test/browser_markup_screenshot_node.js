/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = `data:text/html;charset=utf8,
  <div id="blue-node" style="width:30px;height:30px;background:rgb(0, 0, 255)"></div>`;

// Test that the "Screenshot Node" feature works with a regular node in the main document.
add_task(async function () {
  const { inspector, toolbox } = await openInspectorForURL(encodeURI(TEST_URL));

  info("Select the blue node");
  await selectNode("#blue-node", inspector);

  info("Take a screenshot of the blue node and verify it looks as expected");
  const blueScreenshot = await takeNodeScreenshot(inspector);
  await assertSingleColorScreenshotImage(blueScreenshot, 30, 30, {
    r: 0,
    g: 0,
    b: 255,
  });

  await toolbox.destroy();
});
