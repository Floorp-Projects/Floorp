/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that warning messages emitted when taking a screenshot are displayed in the UI.

const exampleNetDocument = `http://example.net/document-builder.sjs`;
const exampleComDocument = `http://example.com/document-builder.sjs`;

const TEST_URL = `${exampleNetDocument}?html=
  <style>
    body {
      margin: 0;
      height: 10001px;
    }
    iframe {
      height: 50px;
      border:none;
      display: block;
    }
  </style>
  <iframe
    src="${exampleNetDocument}?html=<body style='margin:0;height:30px;background:rgb(255,0,0)'></body>"
    id="same-origin" onload="document.body.classList.add('same-origin-loaded')"></iframe>
  <iframe
    src="${exampleComDocument}?html=<body style='margin:0;height:30px;background:rgb(0,255,0)'></body>"
    id="remote" onload="document.body.classList.add('remote-origin-loaded')"></iframe>`;

addRDMTask(
  TEST_URL,
  async function({ ui, browser, manager }) {
    const { toolWindow } = ui;
    const { document } = toolWindow;

    info("Wait until the iframes are loaded");
    await waitUntil(() =>
      SpecialPowers.spawn(ui.getViewportBrowser(), [], async function() {
        const { classList } = content.document.body;
        return (
          classList.contains("same-origin-loaded") &&
          classList.contains("remote-loaded")
        );
      })
    );

    info(
      "Set a big viewport and high dpr so the screenshot dpr gets downsized"
    );
    // The viewport can't be bigger than 9999×9999
    await setViewportSize(ui, manager, 9999, 9999);
    const dpr = 3;
    await selectDevicePixelRatio(ui, dpr);
    await waitForDevicePixelRatio(ui, dpr);
    // Wait for a tick so the page can be re-rendered.
    await waitForTick();

    info("Click the screenshot button");
    const onScreenshotDownloaded = waitUntilScreenshot();
    const screenshotButton = document.getElementById("screenshot-button");
    screenshotButton.click();

    const filePath = await onScreenshotDownloaded;
    ok(filePath, "The screenshot was taken");

    const image = new Image();
    const onImageLoad = once(image, "load");
    image.src = OS.Path.toFileURI(filePath);
    await onImageLoad;

    info("Check that the same-origin iframe is rendered in the screenshot");
    await checkImageColorAt({
      image,
      y: 10,
      expectedColor: `rgb(255, 0, 0)`,
      label: "The same-origin iframe is rendered properly in the screenshot",
    });

    info("Check that the remote iframe is rendered in the screenshot");
    await checkImageColorAt({
      image,
      y: 60,
      expectedColor: `rgb(0, 255, 0)`,
      label: "The remote iframe is rendered properly in the screenshot",
    });

    info(
      "Check that a warning message was displayed to indicate the dpr was changed"
    );

    const box = gBrowser.getNotificationBox(browser);
    await waitUntil(() => box.currentNotification);

    const notificationEl = box.currentNotification;
    ok(notificationEl, "Notification should be visible");
    is(
      notificationEl.textContent,
      "The device pixel ratio was reduced to 1 as the resulting image was too large",
      "The expected warning was displayed"
    );

    //Remove the downloaded screenshot file
    await OS.File.remove(filePath);
    await resetDownloads();
  },
  { waitForDeviceList: true }
);
