/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/webconsole/test/browser/shared-head.js",
  this
);

// The mask image is served from example.com while the test page is served from
// example.org.
const MASK_SRC = URL_ROOT_COM_SSL + "square_svg.sjs";
const STYLE_ATTRIBUTE = `mask-image: url("${MASK_SRC}"); width:10px; height: 10px; background: red;`;
const TEST_URL = `https://example.org/document-builder.sjs?html=<div style='${STYLE_ATTRIBUTE}'>`;

// Used to assert screenshot colors.
const RED = { r: 255, g: 0, b: 0 };

add_task(async function () {
  await addTab(TEST_URL);
  const { inspector, toolbox, view } = await openRuleView();

  info("Open the splitconsole to check for CORS messages");
  await toolbox.toggleSplitConsole();

  await selectNode("div", inspector);

  info("Take a node screenshot, mask is applied, should be red");
  const beforeScreenshot = await takeNodeScreenshot(inspector);
  await assertSingleColorScreenshotImage(beforeScreenshot, 10, 10, RED);

  info("Update a property from the rule view");
  const heightProperty = getTextProperty(view, 0, { height: "10px" });
  await setProperty(view, heightProperty, "11px");

  info("Wait until the style has been applied in the content page");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector("div").style.height == "11px"
    );
  });

  // Wait for some time in case the image needs to be reloaded, and to allow
  // error messages (if any) to be rendered.
  await wait(1000);

  info("Take another screenshot, mask should still apply, should be red");
  const afterScreenshot = await takeNodeScreenshot(inspector);
  await assertSingleColorScreenshotImage(afterScreenshot, 10, 11, RED);

  const hud = toolbox.getPanel("webconsole").hud;
  ok(
    !findMessageByType(hud, "Cross-Origin Request Blocked", ".error"),
    "No message was logged about a CORS issue"
  );

  info("Close split console");
  await toolbox.toggleSplitConsole();
});
