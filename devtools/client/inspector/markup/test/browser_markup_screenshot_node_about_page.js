/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = `about:preferences`;

// Test that the "Screenshot Node" feature works in about:preferences (See Bug 1691349).

function hexToCSS(hex) {
  if (!hex) {
    return null;
  }
  const rgba = InspectorUtils.colorToRGBA(hex);
  info(`rgba: '${JSON.stringify(rgba)}'`);
  // Drop off the 'a' component since the color will be opaque
  return `rgb(${rgba.r}, ${rgba.g}, ${rgba.b})`;
}

add_task(async function() {
  const { inspector, toolbox } = await openInspectorForURL(TEST_URL);

  info("Select the main content node");
  await selectNode(".main-content", inspector);

  let inContentPageBackgroundColor = await getComputedStyleProperty(
    ":root",
    null,
    "--in-content-page-background"
  );
  inContentPageBackgroundColor = inContentPageBackgroundColor.trim();

  info("Take a screenshot of the element and verify it looks as expected");
  const image = await takeNodeScreenshot(inspector);
  // We only check that we have the right background color, since it would be difficult
  // to assert the look of any other area in the page.
  await checkImageColorAt({
    image,
    x: 0,
    y: 0,
    expectedColor: hexToCSS(inContentPageBackgroundColor),
    label: "The screenshot was taken",
  });

  await toolbox.destroy();
});
