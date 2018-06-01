/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test global exit button

const TEST_URL = "data:text/html;charset=utf-8,";

const { OS } = require("resource://gre/modules/osfile.jsm");

function waitUntilScreenshot() {
  return new Promise(async function(resolve) {
    const { Downloads } = require("resource://gre/modules/Downloads.jsm");
    const list = await Downloads.getList(Downloads.ALL);

    const view = {
      onDownloadAdded: download => {
        download.whenSucceeded().then(() => {
          resolve(download.target.path);
          list.removeView(view);
        });
      }
    };

    await list.addView(view);
  });
}

addRDMTask(TEST_URL, async function({ ui: {toolWindow} }) {
  const { store, document } = toolWindow;

  // Wait until the viewport has been added
  await waitUntilState(store, state => state.viewports.length == 1);

  info("Click the screenshot button");
  const screenshotButton = document.getElementById("global-screenshot-button");
  screenshotButton.click();

  const whenScreenshotSucceeded = waitUntilScreenshot();

  const filePath = await whenScreenshotSucceeded;
  const image = new Image();
  image.src = OS.Path.toFileURI(filePath);

  await once(image, "load");

  // We have only one viewport at the moment
  const viewport = store.getState().viewports[0];
  const ratio = window.devicePixelRatio;

  is(image.width, viewport.width * ratio,
    "screenshot width has the expected width");

  is(image.height, viewport.height * ratio,
    "screenshot width has the expected height");

  await OS.File.remove(filePath);
});
