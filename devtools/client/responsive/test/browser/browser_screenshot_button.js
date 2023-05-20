/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test global screenshot button

const TEST_URL = "data:text/html;charset=utf-8,";

addRDMTask(TEST_URL, async function ({ ui }) {
  const { toolWindow } = ui;
  const { store, document } = toolWindow;

  info("Click the screenshot button");
  const screenshotButton = document.getElementById("screenshot-button");
  screenshotButton.click();

  const whenScreenshotSucceeded = waitUntilScreenshot();

  const filePath = await whenScreenshotSucceeded;
  const image = new Image();
  image.src = PathUtils.toFileURI(filePath);

  await once(image, "load");

  // We have only one viewport at the moment
  const viewport = store.getState().viewports[0];
  const ratio = window.devicePixelRatio;

  is(
    image.width,
    viewport.width * ratio,
    "screenshot width has the expected width"
  );

  is(
    image.height,
    viewport.height * ratio,
    "screenshot width has the expected height"
  );

  await IOUtils.remove(filePath);
  await resetDownloads();
});
