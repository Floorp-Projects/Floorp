/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const exampleNetDocument = `http://example.net/document-builder.sjs`;
const exampleComDocument = `http://example.com/document-builder.sjs`;

const TEST_URL = `${exampleNetDocument}?html=
  <iframe
    src="${exampleNetDocument}?html=<div style='width:30px;height:30px;background:rgb(255,0,0)'></div>"
    id="same-origin"></iframe>
  <iframe
    src="${exampleComDocument}?html=<div style='width:25px;height:10px;background:rgb(0,255,0)'></div>"
    id="remote"></iframe>`;

// Test that the "Screenshot Node" feature works with a node inside an iframe.
add_task(async function() {
  const { inspector, toolbox } = await openInspectorForURL(encodeURI(TEST_URL));

  info("Select the red node");
  await selectNodeInFrames(["#same-origin", "div"], inspector);

  info(
    "Take a screenshot of the red div in the same origin iframe node and verify it looks as expected"
  );
  const redScreenshot = await takeNodeScreenshot(inspector);
  await assertSingleColorScreenshotImage(redScreenshot, 30, 30, {
    r: 255,
    g: 0,
    b: 0,
  });

  info("Select the green node");
  await selectNodeInFrames(["#remote", "div"], inspector);
  info(
    "Take a screenshot of the green div in the remote iframe node and verify it looks as expected"
  );
  const greenScreenshot = await takeNodeScreenshot(inspector);
  await assertSingleColorScreenshotImage(greenScreenshot, 25, 10, {
    r: 0,
    g: 255,
    b: 0,
  });
  await toolbox.destroy();
});
