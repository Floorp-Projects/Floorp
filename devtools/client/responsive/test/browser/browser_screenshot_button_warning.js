/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that warning messages emitted when taking a screenshot are displayed in the UI.

const TEST_URL = `http://example.net/document-builder.sjs?html=
  <style>
    body {
      margin: 0;
      height: 10001px;
    }
  </style>Hello world`;

addRDMTask(
  TEST_URL,
  async function ({ ui, browser, manager }) {
    const { toolWindow } = ui;
    const { document } = toolWindow;

    info(
      "Set a big viewport and high dpr so the screenshot dpr gets downsized"
    );
    // The viewport can't be bigger than 9999×9999
    await setViewportSize(ui, manager, 9999, 9999);
    const dpr = 3;
    await selectDevicePixelRatio(ui, dpr);
    await waitForDevicePixelRatio(ui, dpr);

    info("Click the screenshot button");
    const onScreenshotDownloaded = waitUntilScreenshot();
    const screenshotButton = document.getElementById("screenshot-button");
    screenshotButton.click();

    const filePath = await onScreenshotDownloaded;
    ok(filePath, "The screenshot was taken");

    info(
      "Check that a warning message was displayed to indicate the dpr was changed"
    );

    const box = gBrowser.getNotificationBox(browser);
    await waitUntil(() => box.currentNotification);

    const notificationEl = box.currentNotification;
    ok(notificationEl, "Notification should be visible");
    is(
      notificationEl.messageText.textContent.trim(),
      "The device pixel ratio was reduced to 1 as the resulting image was too large",
      "The expected warning was displayed"
    );

    //Remove the downloaded screenshot file
    await IOUtils.remove(filePath);
    await resetDownloads();
  },
  { waitForDeviceList: true }
);
