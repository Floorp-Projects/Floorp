/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = `data:text/html;charset=utf8,
  <div id="blue-node" style="width:30px;height:11000px;background:rgb(0, 0, 255)"></div>`;

// Test taking a screenshot of a tall node displays a warning message in the notification box.
add_task(async function () {
  const { inspector, toolbox } = await openInspectorForURL(encodeURI(TEST_URL));

  info("Select the blue node");
  await selectNode("#blue-node", inspector);

  info("Take a screenshot of the blue node and verify it looks as expected");
  const blueScreenshot = await takeNodeScreenshot(inspector);
  await assertSingleColorScreenshotImage(blueScreenshot, 30, 10000, {
    r: 0,
    g: 0,
    b: 255,
  });

  info(
    "Check that a warning message was displayed to indicate the screenshot was truncated"
  );
  const notificationBox = await waitFor(() =>
    toolbox.doc.querySelector(".notificationbox")
  );

  const message = notificationBox.querySelector(".notification").textContent;
  ok(
    message.startsWith("The image was cut off"),
    `The warning message is rendered as expected (${message})`
  );

  await toolbox.destroy();
});
